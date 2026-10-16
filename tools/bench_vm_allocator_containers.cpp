#include <chrono>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <string>
#include <array>
#include <stdexcept>
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;
struct AllocatorContainerRoot {std::uint64_t marker{};XVector<std::uint32_t> values;XString text;XVector<XVector<std::uint32_t>> rows;};
XOFFSET_REGISTER_SCHEMA_NAME(AllocatorContainerRoot,"bench.allocator.containers")
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void mix(std::uint64_t& h,std::uint64_t value){h=(h^value)*1099511628211ull;}
}
int main(int argc,char** argv){try {
    std::string shape="interleaved";std::size_t count=200000,cycles=4;bool pooled=false;
    for(int i=1;i<argc;++i){require(i+1<argc,"missing flag value");std::string k=argv[i],v=argv[++i];if(k=="--shape")shape=v;else if(k=="--count")count=std::stoull(v);else if(k=="--cycles")cycles=std::stoull(v);else if(k=="--pool")pooled=std::stoull(v)!=0;else throw std::runtime_error("unknown flag");}
    require((shape=="sequential"||shape=="interleaved")&&count>0&&count<=1000000&&cycles>0&&cycles<=16,"work budget");
    const std::array<std::string,4> texts={std::string(32,'a'),std::string(256,'b'),std::string(4096,'c'),std::string(64,'d')};
    XBufferPool pool=pooled?XBufferPool(XBufferPool::Limits{2,32*1024*1024,32*1024*1024}):XBufferPool{};
    double elapsed=0;std::uint64_t checksum=713,used=0,accessible=0;const auto row_count=(count+511)/512;
    for(std::size_t cycle=0;cycle<cycles;++cycle){
        auto b=XBuffer::create<AllocatorContainerRoot>(4096,XBuffer::maximum_capacity(16*1024*1024).with_growth(XGrowPolicy::Automatic).with_pool(pool));
        auto& r=b.root<AllocatorContainerRoot>();r.marker=713;r.rows.reserve(row_count);const auto* base=b.get_address();
        auto secondary=[&](std::size_t i){if(i%256==0)r.text=texts[(i/256)%4];if(i%512==0){auto& row=r.rows.emplace_back();for(unsigned j=0;j<32;++j)row.push_back(std::uint32_t(i*19+j));}};
        const auto start=std::chrono::steady_clock::now();
        for(std::size_t i=0;i<count;++i){r.values.push_back(std::uint32_t(i*7+cycle));if(shape=="interleaved")secondary(i);}
        if(shape=="sequential")for(std::size_t i=0;i<count;++i)secondary(i);
        elapsed+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        require(b.get_address()==base&&b.get_size()<=b.maximum_capacity(),"owner/hard limit changed");
        auto verify=[&](const AllocatorContainerRoot& value,bool hash){require(value.marker==713&&value.values.size()==count&&value.rows.size()==row_count,"shape changed");
            require(value.text==texts[((count-1)/256)%4],"string changed");
            for(std::size_t i=0;i<count;++i){require(value.values[i]==std::uint32_t(i*7+cycle),"vector changed");if(hash)mix(checksum,value.values[i]);}
            for(std::size_t i=0;i<row_count;++i){require(value.rows[i].size()==32,"nested size changed");for(unsigned j=0;j<32;++j){require(value.rows[i][j]==std::uint32_t(i*512*19+j),"nested value changed");if(hash)mix(checksum,value.rows[i][j]);}}
            if(hash)for(char c:texts[((count-1)/256)%4])mix(checksum,std::uint8_t(c));};
        verify(r,true);used=std::max<std::uint64_t>(used,b.arena()->used_end);accessible=std::max<std::uint64_t>(accessible,b.accessible_bytes());
        auto wire=b.save_verified<AllocatorContainerRoot>();auto loaded=XBuffer::load_verified<AllocatorContainerRoot>(wire);verify(loaded.root<AllocatorContainerRoot>(),false);
    }
    std::cout<<std::setprecision(17)<<"{\"workload\":\"containers\",\"shape\":\""<<shape<<"\",\"pool\":"<<int(pooled)<<",\"count\":"<<count<<",\"cycles\":"<<cycles<<",\"operations\":"<<count*cycles<<",\"semantic_checksum\":"<<checksum<<",\"elapsed_ms\":"<<elapsed<<",\"used_end_max\":"<<used<<",\"accessible_max\":"<<accessible<<"}\n";
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;} }
