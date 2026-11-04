#include "xoffset_image_builder.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <vector>
#include <sys/resource.h>
namespace xo=XOffsetDatastructure; namespace im=xo::image;
using Clock=std::chrono::steady_clock;
struct BenchLeaf { std::uint64_t id{}; xo::XString name; xo::XVector<std::uint32_t> values; };
struct BenchRoot { std::uint64_t marker{}; xo::XVector<BenchLeaf> records; xo::XVector<std::uint8_t> blob; };
XOFFSET_REGISTER_SCHEMA_NAME(BenchRoot,"bench.MemmoveImage.v1")
namespace XOffsetDatastructure {
template<> struct wire_record_layout<BenchRoot>{static constexpr bool ordinary_members=true;};
template<> struct wire_record_layout<BenchLeaf>{static constexpr bool ordinary_members=true;};
}
static_assert(im::supported_v<BenchRoot>);
volatile std::uint64_t sink;
constexpr auto dense=xo::XSaveOptions{.emission=xo::XWireEmission::dense};
double ns(Clock::time_point t){return std::chrono::duration<double,std::nano>(Clock::now()-t).count();}
auto make(std::size_t target,std::size_t nodes,bool holes){
 auto b=xo::TypedXBuffer<BenchRoot>::create(32768,
   xo::XBuffer::maximum_capacity(64*1024*1024).with_growth(xo::XGrowPolicy::Automatic));
 auto& r=b.root();r.marker=7;r.records.resize(nodes);
 for(std::size_t i=0;i<nodes;++i){auto& e=r.records[i];e.id=i+19;e.name="a long record name used to create blocks";e.values.resize(8);for(unsigned j=0;j<8;++j)e.values[j]=i+j;}
 if(holes)for(auto& e:r.records)e.name.reserve(128);
 const auto before=sizeof(xo::XWireHeaderV2)+b.arena()->used_end-xo::XWIRE_PAYLOAD_BEGIN+8*b.free_block_count();
 if(before>target)throw std::runtime_error("fixture exceeds target");
 r.blob.resize(target-before);
 for(std::size_t i=0;i<r.blob.size();++i)r.blob[i]=std::uint8_t(i);
 return b;
}
std::uint64_t native_sum(const BenchRoot& r){
 std::uint64_t sum=r.marker;
 for(const auto& e:r.records){sum+=e.id+e.name.size();for(auto n:e.values)sum+=n;}
 if(!r.blob.empty())sum+=r.blob[0]+r.blob[r.blob.size()-1];return sum;
}
std::uint64_t proxy_sum(im::RecordView<BenchRoot> r){
 std::uint64_t sum=r.get<0>();auto rows=r.get<1>();
 for(std::size_t i=0;i<rows.size();++i){auto e=rows[i];sum+=e.get<0>()+e.get<1>().size();auto values=e.get<2>();for(std::size_t j=0;j<values.size();++j)sum+=values[j];}
 auto blob=r.get<2>();if(blob.size())sum+=blob[0]+blob[blob.size()-1];return sum;
}
__attribute__((noinline)) std::uint64_t bare_copy(std::byte* dst,const std::byte* src,std::size_t n){
 std::memmove(dst,src,n);asm volatile(""::"r"(dst):"memory");return std::to_integer<unsigned char>(dst[40]);
}
__attribute__((noinline)) std::uint64_t packed_copy(const xo::PackedWire<BenchRoot>& src,std::span<std::byte> dst){
 xo::copy_packed_into(src,dst);asm volatile(""::"r"(dst.data()):"memory");return std::to_integer<unsigned char>(dst[40]);
}
__attribute__((noinline)) std::uint64_t image_copy(const xo::PackedWire<BenchRoot>& src,im::PreparedImageSlot<BenchRoot>& dst){
 auto view=im::copy_image_into(src,dst);asm volatile(""::"r"(view.bytes().data()):"memory");return view.root().get<0>();
}
__attribute__((noinline)) xo::PackedWire<BenchRoot> seal_image(im::ImageBuilder<BenchRoot>& builder){
 return std::move(builder).seal();
}
auto controlled(std::size_t target,std::size_t nodes,double& build_ns,double& seal_ns){
 auto start=Clock::now();im::ImageBuilder<BenchRoot> builder(target);auto root=builder.root();root.get<0>().set(7);
 auto rows=root.get<1>().resize(nodes);
 for(std::size_t i=0;i<nodes;++i){auto e=rows[i];e.get<0>().set(i+19);e.get<1>().assign("a long record name used to create blocks");auto values=e.get<2>().resize(8);for(unsigned j=0;j<8;++j)values[j].set(i+j);}
 std::vector<std::uint8_t> blob(target-builder.wire_size());for(std::size_t i=0;i<blob.size();++i)blob[i]=std::uint8_t(i);
 root.get<2>().assign(std::span<const std::uint8_t>(blob));
 build_ns=ns(start);start=Clock::now();auto packed=seal_image(builder);seal_ns=ns(start);return packed;
}
struct Context{
 xo::VerifiedSnapshot<BenchRoot> snapshot;
 xo::PackedWire<BenchRoot> packed;
 im::PreparedImageSlot<BenchRoot> slot;
 xo::TypedXBuffer<BenchRoot> reload;
 std::vector<std::byte> output;
 std::byte* slot_address{};
 std::string wire,emit;
 Context(xo::VerifiedSnapshot<BenchRoot> s,xo::PackedWire<BenchRoot> p):snapshot(std::move(s)),packed(std::move(p)),slot(packed.bytes().size()),
 reload(xo::TypedXBuffer<BenchRoot>::create(32768,xo::XBuffer::maximum_capacity(64*1024*1024).with_growth(xo::XGrowPolicy::Automatic))),output(packed.bytes().size()){
  wire.assign(reinterpret_cast<const char*>(packed.bytes().data()),packed.bytes().size());emit.reserve(wire.size());reload.reload_verified(wire);
  {auto v=im::copy_image_into(packed,slot);slot_address=const_cast<std::byte*>(v.bytes().data());}
 }
};
template<class F>double timed(F f,std::size_t iterations){
 std::uint64_t sum=0;auto start=Clock::now();
 for(std::size_t i=0;i<iterations;++i){sum+=f();asm volatile("":::"memory");}
 const auto elapsed=ns(start);sink=sum;return elapsed/iterations;
}
double measure(int mode,Context& c,std::size_t it){
 switch(mode){
 case 0:case 1:return timed([&]{return bare_copy(c.output.data(),c.packed.bytes().data(),c.output.size());},it);
 case 2:return timed([&]{return packed_copy(c.packed,c.output);},it);
 case 3:return timed([&]{return image_copy(c.packed,c.slot);},it);
 case 4:return timed([&]{xo::emit_dense_into(c.snapshot,c.output);return std::to_integer<unsigned char>(c.output[40]);},it);
 case 5:return timed([&]{c.reload.save_verified_into(c.emit,dense);return std::uint64_t(c.emit[40]);},it);
 case 6:return timed([&]{c.reload.reload_verified(c.wire);return c.reload.root().marker;},it);
 case 7:return timed([&]{auto t=c.reload.begin_verified_reload<BenchRoot>(c.wire.size()+8-c.snapshot.segments()[2].size(),c.snapshot.segments()[2].size()/8);
  auto parts=c.snapshot.segments();std::memcpy(t.header_bytes().data(),parts[0].data(),parts[0].size());
  std::memcpy(t.payload_bytes().data(),parts[1].data(),parts[1].size());if(parts[2].size())std::memcpy(t.hole_bytes().data(),parts[2].data(),parts[2].size());
  t.complete(t.header_bytes().size(),t.payload_bytes().size(),t.hole_bytes().size(),0);t.finish();return c.reload.root().marker;},it);
 case 8:return timed([&]{c.reload.reload_verified(c.wire);return native_sum(c.reload.root());},it);
 case 9:return timed([&]{auto v=im::copy_image_into(c.packed,c.slot);return proxy_sum(v.root());},it);
 case 10:return timed([&]{return native_sum(c.snapshot.root());},it);
 case 11:{auto v=im::copy_image_into(c.packed,c.slot);return timed([&]{return proxy_sum(v.root());},it);}
 case 12:return timed([&]{return bare_copy(c.slot_address,c.packed.bytes().data(),c.output.size());},it);
 default:std::abort();
 }
}
int main(int argc,char**argv){
 bool aa=argc>1&&std::string_view(argv[1])=="aa";unsigned seed=argc>2?std::stoul(argv[2]):0;
 const char* names[]={"raw_a","raw_b","packed","image","dense_span","old_save","verified_reload","direct_reload","verified_read","image_read","native_query","proxy_query","raw_slot"};
 struct Shape{std::size_t bytes,nodes;bool holes;};
 const Shape shapes[]={{128,0,false},{1024,0,false},{4096,0,false},{65536,0,false},{1048576,0,false},{16777216,0,false},{1048576,512,false},{1048576,512,true}};
 std::cout<<"{\"phase\":\""<<(aa?"aa":"comparison")<<"\",\"seed\":"<<seed<<",\"cases\":[";
 bool first=true;
 for(auto shape:shapes){
  auto t=Clock::now();auto b=make(shape.bytes,shape.nodes,shape.holes);double build_ns=ns(t);
  t=Clock::now();auto s=xo::freeze_verified(std::move(b));double freeze_ns=ns(t);
  t=Clock::now();auto p=xo::pack_dense(s);double pack_ns=ns(t);
  double controlled_build_ns=0,seal_ns=0;
  if(!shape.holes)p=controlled(shape.bytes,shape.nodes,controlled_build_ns,seal_ns);
  if(p.bytes().size()!=shape.bytes)throw std::runtime_error("fixture size differs");
  const auto n=p.bytes().size();t=Clock::now();Context c(s,p);double destination_ns=ns(t);
  {auto v=im::copy_image_into(p,c.slot);if(proxy_sum(v.root())!=native_sum(s.root()))throw std::runtime_error("query mismatch");}
  const std::size_t it=std::clamp<std::size_t>(64*1024*1024/n,4,65536);
  std::vector<std::vector<double>> samples(13);
  const int modes=aa?2:13;
  for(int mode=0;mode<modes;++mode)measure(mode,c,std::min<std::size_t>(it,64));
  for(unsigned block=0;block<12;++block){
   for(int k=0;k<modes;++k){int mode=(block+seed)%2?k:modes-1-k;samples[mode].push_back(measure(mode,c,it));}
  }
  if(!first)std::cout<<",";first=false;
  std::cout<<"{\"bytes\":"<<n<<",\"nodes\":"<<shape.nodes<<",\"holes\":"<<s.segments()[2].size()/8<<",\"iterations\":"<<it
   <<",\"build_ns\":"<<build_ns<<",\"freeze_ns\":"<<freeze_ns<<",\"pack_ns\":"<<pack_ns<<",\"controlled_build_ns\":"<<controlled_build_ns<<",\"seal_ns\":"<<seal_ns<<",\"context_prepare_ns\":"<<destination_ns<<",\"samples\":{";
  for(int mode=0;mode<modes;++mode){if(mode)std::cout<<",";std::cout<<"\""<<names[mode]<<"\":[";for(std::size_t i=0;i<samples[mode].size();++i){if(i)std::cout<<",";std::cout<<samples[mode][i];}std::cout<<"]";}
  std::cout<<"}}";
 }
 rusage usage{};getrusage(RUSAGE_SELF,&usage);
 std::cout<<"],\"resources\":{\"process_peak_rss_bytes\":"<<usage.ru_maxrss<<",\"minor_faults\":"<<usage.ru_minflt<<",\"major_faults\":"<<usage.ru_majflt<<"}}\n";
}
