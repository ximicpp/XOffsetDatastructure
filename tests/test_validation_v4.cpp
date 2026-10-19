// Functional/resource/oracle tests only. No clock, benchmark, or fixture-specific
// tuning. The global new injector is the unchanged v3 one-shot implementation.
#include "validation_global_new_failure.hpp"
#include <xoffsetdatastructure.hpp>
#include "bitmap_test_support.hpp"
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace xo=XOffsetDatastructure;
namespace wd=xo::detail;
namespace {
void need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Resource final:std::pmr::memory_resource {
    struct Entry{void* p{};std::size_t bytes{},alignment{};};
    std::array<Entry,64> entries{};
    std::size_t allocations{},deallocations{},requested{},released{},live{},peak{};
    ~Resource(){if(live||requested!=released||allocations!=deallocations)std::abort();}
    void* do_allocate(std::size_t bytes,std::size_t alignment)override{
        auto* p=std::pmr::new_delete_resource()->allocate(bytes,alignment);
        auto slot=std::find_if(entries.begin(),entries.end(),[](const auto& x){return !x.p;});
        if(slot==entries.end()){std::pmr::new_delete_resource()->deallocate(p,bytes,alignment);throw std::runtime_error("resource ledger full");}
        *slot={p,bytes,alignment};++allocations;requested+=bytes;live+=bytes;peak=std::max(peak,live);return p;
    }
    void do_deallocate(void* p,std::size_t bytes,std::size_t alignment)override{
        auto slot=std::find_if(entries.begin(),entries.end(),[&](const auto& x){return x.p==p;});
        if(slot==entries.end()||slot->bytes!=bytes||slot->alignment!=alignment)std::abort();
        std::pmr::new_delete_resource()->deallocate(p,bytes,alignment);*slot={};++deallocations;released+=bytes;live-=bytes;
    }
    bool do_is_equal(const std::pmr::memory_resource& x)const noexcept override{return this==&x;}
    void balanced()const{need(!live&&requested==released&&allocations==deallocations,"resource requests/deallocations unbalanced");}
};
struct State {
    bitmap_test::Snapshot coverage;
    std::size_t dense_prefix{},dense_bytes{};
    bool operator==(const State&)const=default;
};
State save(const wd::WireValidationByteIndex& x){return {bitmap_test::snapshot(x),x.dense_prefix_bytes(),x.dense_requested_bytes()};}
using Ranges=std::vector<std::pair<std::size_t,std::size_t>>;
bool overlaps(const Ranges& ranges,std::size_t b,std::size_t e){
    if(b==e)return false;
    for(auto [l,h]:ranges)if(l<e&&b<h)return true;
    return false;
}
void reg(bool free,const void* p,std::size_t bytes,wd::WireValidationContext& ctx){
    if(free)wd::register_free_range(p,bytes,ctx,"v4 bounds");
    else wd::register_owned_range(p,bytes,ctx,"v4 bounds");
}
std::size_t oracle_operations{},priority_cases{},real_host_failures{},threshold_cases{},typed_rejections{};

void independent_oracle(){
    // mode0 default dense; mode1 paged; mode2 a short existing dense prefix and
    // paged tail; mode3 different representations in the two real context sets.
    for(unsigned mode=0;mode<4;++mode){
        alignas(64)std::array<char,32771> arena{};
        wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+arena.size(),
            .owned_ranges=wd::WireValidationByteIndex{&ctx.range_resource,mode!=1},
            .free_ranges=wd::WireValidationByteIndex{&ctx.range_resource,mode==0||mode==2}};
        Ranges owned,free;
        if(mode==2){ctx.owned_ranges.insert_disjoint(8,9,4095);owned.emplace_back(8,9);}
        std::uint64_t random=0x9876543210abcdefULL+mode;
        for(std::size_t i=0;i<2048;++i){
            random^=random<<13;random^=random>>7;random^=random<<17;
            const bool f=(random&1)!=0;
            const std::size_t b=(random>>1)%(arena.size()+1),bytes=(random>>20)%97;
            const char* error=nullptr;
            if(bytes){
                if(bytes>arena.size()-b)error="v4 bounds";
                else if(overlaps(owned,b,b+bytes))error=f?"wire validation detected free-list overlap with live ranges":"wire validation detected overlapping dynamic ranges";
                else if(overlaps(free,b,b+bytes))error=f?"wire validation detected overlapping free-list ranges":"wire validation detected live/free overlap";
            }
            const auto before_owned=save(ctx.owned_ranges),before_free=save(ctx.free_ranges);
            bool caught=false;
            try{reg(f,arena.data()+b,bytes,ctx);}
            catch(const xo::XException& e){need(error&&std::string_view(e.what())==error,"independent interval oracle category/order differs");caught=true;}
            need(caught==(error!=nullptr),"independent interval oracle acceptance differs");
            if(caught)need(save(ctx.owned_ranges)==before_owned&&save(ctx.free_ranges)==before_free,"reject changed exact representation state");
            else if(bytes)(f?free:owned).emplace_back(b,b+bytes);
            bitmap_test::same(ctx.owned_ranges,owned,arena.size());bitmap_test::same(ctx.free_ranges,free,arena.size());
            ++oracle_operations;
        }
    }
}

void global_priority(){
    for(bool dense:{false,true})for(bool inserting_free:{false,true}){
        alignas(64)std::array<char,16384> arena{};
        wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+arena.size(),
            .owned_ranges=wd::WireValidationByteIndex{&ctx.range_resource,dense},
            .free_ranges=wd::WireValidationByteIndex{&ctx.range_resource,dense}};
        reg(true,arena.data()+1,1,ctx);reg(false,arena.data()+12287,2,ctx);
        const auto own=save(ctx.owned_ranges),fre=save(ctx.free_ranges);
        bool caught=false;
        // Free conflict occurs first in address order, owned occurs three pages
        // later; a fused address-order walk must still report the owned error.
        try{reg(inserting_free,arena.data(),12289,ctx);}
        catch(const xo::XException& e){caught=std::string_view(e.what())==(inserting_free?"wire validation detected free-list overlap with live ranges":"wire validation detected overlapping dynamic ranges");}
        need(caught&&save(ctx.owned_ranges)==own&&save(ctx.free_ranges)==fre,"global owned priority lost or rejection mutated state");
        need(wd::WireValidationByteIndex::overlap_priority(ctx.owned_ranges,ctx.free_ranges,0,12289)==1,"pair helper priority differs");++priority_cases;
    }
}

void thresholds_and_summary(){
    constexpr auto limit=wd::WireValidationByteIndex::dense_extent_limit;
    for(auto extent:{std::size_t{1},std::size_t{4095},std::size_t{4096},limit-1,limit,limit+1}){
        Resource resource;
        {
            wd::WireValidationByteIndex x(&resource);x.insert_disjoint(extent-1,extent,extent);
            need(x.extent()==extent&&x.size()==1&&x.word_at((extent-1)/64)==(std::uint64_t{1}<<((extent-1)%64)),"threshold last-byte coverage wrong");
            need(x.word_at(extent/64+(extent%64!=0))==0&&!x.overlaps(extent,extent+1),"padding became admitted coverage");
            if(extent<=limit){
                const auto pages=extent/4096+(extent%4096!=0);
                const auto bytes=pages*528;
                need(x.dense_prefix_bytes()==pages*4096&&x.dense_requested_bytes()==bytes&&resource.requested==bytes&&resource.allocations==1&&x.directory_entries()==0&&x.allocated_pages()==0,"dense exact allocation or budget differs");
                need(bytes<=8u*1024u*1024u+2048u,"dense hard budget exceeded");
            }else{
                need(x.dense_prefix_bytes()==0&&x.allocated_pages()==1,"large sparse arena unexpectedly dense");
                need(resource.requested==(extent/4096+(extent%4096!=0))*sizeof(std::uintptr_t)+528,"paged sparse formula differs");
            }
            ++threshold_cases;
        }
        resource.balanced();
    }
    Resource resource;
    {
        wd::WireValidationByteIndex x(&resource);x.insert_disjoint(4095,4096,8193);
        need(x.overlaps(0,4096)&&!x.overlaps(0,4095),"nonempty summary lost full-page/partial distinction");
        x.insert_disjoint(0,4095,8193);
        for(std::size_t w=0;w<64;++w)need(x.word_at(w)==UINT64_MAX,"dense full-page coverage wrong");
        need(x.size()==2&&x.allocated_pages()==0&&resource.allocations==1,"dense page grew a shadow page");
    }
    resource.balanced();
}

void actual_new_failures(){
    // Cold dense array is one real host allocation. An adjacent warm insertion
    // has none; it must not invent a refill failure at the former v3 Page point.
    {
        Resource resource;wd::WireValidationByteIndex x(&resource);const auto before=save(x);const auto live=fail_new::live();bool caught=false;
        {fail_new::Arm arm(0);try{x.insert_disjoint(1,2,8193);}catch(const std::bad_alloc&){caught=true;}}
        need(caught&&fail_new::hits==1&&fail_new::attempts==1&&fail_new::live()==live,"cold dense real new failure not reached once");
        need(save(x)==before&&resource.live==0,"cold dense failure committed/leaked");
        x.insert_disjoint(1,2,8193);
        {fail_new::Arm arm(0);x.insert_disjoint(2,3,8193);}
        need(!fail_new::attempts&&!fail_new::hits&&x.size()==2,"warm dense insertion requested a fabricated host failure");++real_host_failures;
    }
    for(bool crosses_dense:{false,true}){
        const long points=crosses_dense?2:3;
        for(long point=0;point<points;++point){
            const auto outside=fail_new::live();
            {
                Resource resource;wd::WireValidationByteIndex x(&resource);x.insert_disjoint(8,9,4095);
                const auto before=save(x);const auto live_bytes=resource.live;const auto live=fail_new::live();
                const std::size_t b=crosses_dense?4095:8191,e=crosses_dense?8193:12289,extent=16384;
                bool caught=false;
                {fail_new::Arm arm(point);try{x.insert_disjoint(b,e,extent);}catch(const std::bad_alloc&){caught=true;}}
                need(caught&&fail_new::hits==1&&fail_new::attempts==std::size_t(point+1)&&fail_new::live()==live,"dense/tail real resource point or cleanup differs");
                need(save(x)==before&&resource.live==live_bytes,"dense-prefix/tail prepare partially committed");
                x.insert_disjoint(b,e,extent);
                need(x.dense_prefix_bytes()==4096&&x.size()==2&&x.overlaps(8,9)&&x.overlaps(b,e)&&!x.overlaps(b-1,b),"dense prefix migration/overlap after recovery");
                ++real_host_failures;
            }
            need(fail_new::live()==outside,"dense/tail host allocation leaked on destruction");
        }
    }
    // The second representation of an actual registration context may fail,
    // without changing the already committed owned coverage or publishing free.
    for(bool target_free:{false,true}){
        alignas(64)std::array<char,8193> a{};Resource resource;
        wd::WireValidationContext ctx{.base=a.data(),.end=a.data()+a.size(),
            .owned_ranges=wd::WireValidationByteIndex{&resource},.free_ranges=wd::WireValidationByteIndex{&resource}};
        reg(!target_free,a.data()+8,8,ctx);const auto own=save(ctx.owned_ranges),fre=save(ctx.free_ranges);const auto live=fail_new::live();bool caught=false;
        {fail_new::Arm arm(0);try{reg(target_free,a.data()+64,8,ctx);}catch(const std::bad_alloc&){caught=true;}}
        need(caught&&fail_new::hits==1&&fail_new::attempts==1&&fail_new::live()==live,"second bitmap actual registration OOM absent");
        need(save(ctx.owned_ranges)==own&&save(ctx.free_ranges)==fre,"second representation OOM changed context state");
        reg(target_free,a.data()+64,8,ctx);++real_host_failures;
    }
}
}

struct V4Scalar { std::uint32_t a{};std::int64_t b{};std::uint16_t table[3]{}; };
struct V4NestedBool { V4Scalar plain;bool bits[2]{}; };
struct V4ValidationRoot {
    V4Scalar no_checks[128]{};
    bool flags[3]{};
    xo::XString name;
    xo::XVector<V4Scalar> plain;
    xo::XVector<bool> booleans;
    xo::XMap<std::uint32_t,bool> sorted;
};
XOFFSET_REGISTER_SCHEMA_NAME(V4ValidationRoot,"tests.v4.complete-validation")
static_assert(!wd::wire_value_needs_validation_v<std::uint8_t>);
static_assert(!wd::wire_value_needs_validation_v<V4Scalar>);
static_assert(!wd::wire_value_needs_validation_v<V4Scalar[17]>);
static_assert(wd::wire_value_needs_validation_v<bool>);
static_assert(wd::wire_value_needs_validation_v<bool[2][3]>);
static_assert(wd::wire_value_needs_validation_v<V4NestedBool>);
static_assert(wd::wire_value_needs_validation_v<xo::XVector<std::uint8_t>>);
static_assert(wd::wire_value_needs_validation_v<xo::XSet<std::uint32_t>>);
static_assert(wd::wire_value_needs_validation_v<xo::XMap<std::uint32_t,std::uint32_t>>);
static_assert(wd::wire_value_needs_validation_v<xo::XKeyValue<std::uint32_t,bool>>);
namespace {
template<class T>T read_at(const std::string& s,std::size_t p){T x;std::memcpy(&x,s.data()+p,sizeof(x));return x;}
template<class T>void write_at(std::string& s,std::size_t p,T x){std::memcpy(s.data()+p,&x,sizeof(x));}
void reject(const std::string& wire,std::string_view expected){
    bool caught=false;try{auto b=xo::XBuffer::load_verified<V4ValidationRoot>(wire);}
    catch(const xo::XException& e){caught=std::string_view(e.what())==expected;}
    need(caught,"typed validation check or first-error category was skipped");++typed_rejections;
}
void trait_and_typed_checks(){
    auto buffer=xo::XBuffer::create<V4ValidationRoot>(32768);auto& root=buffer.root<V4ValidationRoot>();
    root.name="verified";root.plain.resize(64);root.booleans.resize(3);root.booleans[1]=true;
    root.sorted.try_emplace(1,true);root.sorted.try_emplace(2,false);
    root.no_checks[127].a=123;root.plain[63].b=456;
    const auto original=buffer.save_verified<V4ValidationRoot>();auto h=read_at<xo::XWireHeaderV1>(original,0);
    const std::size_t rp=sizeof(h)+h.root_offset;
    {
        auto loaded=xo::XBuffer::load_verified<V4ValidationRoot>(original,
            xo::XBuffer::maximum_capacity(1u<<20).with_growth(xo::XGrowPolicy::Automatic));
        auto& r=loaded.root<V4ValidationRoot>();need(r.no_checks[127].a==123&&r.plain[63].b==456&&r.booleans[1],"no-op traversal changed ordinary values");
        r.name="still mutable";auto again=xo::XBuffer::load_verified<V4ValidationRoot>(loaded.save_verified<V4ValidationRoot>());need(again.root<V4ValidationRoot>().name.view()=="still mutable","trait path lost mutable reload");
    }
    {
        auto bad=original;write_at(bad,rp+offsetof(V4ValidationRoot,flags)+2,std::uint8_t{2});
        // Later invalid relative binding must not displace the earlier bool.
        write_at(bad,rp+offsetof(V4ValidationRoot,name)+offsetof(xo::XFixedString,arena_delta),INT32_MAX);
        reject(bad,"wire validation: invalid bool representation");
    }
    {
        auto bad=original;const auto origin=rp+offsetof(V4ValidationRoot,booleans)+offsetof(xo::XFixedVector<bool>,data_delta);
        const auto data=std::size_t(std::int64_t(origin)+read_at<std::int32_t>(bad,origin));write_at(bad,data+1,std::uint8_t{2});
        reject(bad,"wire validation: invalid bool representation");
    }
    {
        using Pair=xo::XKeyValue<std::uint32_t,bool>;
        auto bad=original;const auto origin=rp+offsetof(V4ValidationRoot,sorted)+offsetof(xo::XFixedVector<Pair>,data_delta);
        const auto data=std::size_t(std::int64_t(origin)+read_at<std::int32_t>(bad,origin));
        write_at(bad,data+sizeof(Pair)+offsetof(Pair,first),std::uint32_t{1});
        reject(bad,"wire validation: map keys not strictly ordered");
        write_at(bad,data+offsetof(Pair,second),std::uint8_t{2});
        reject(bad,"wire validation: invalid bool representation");
    }
    {
        auto bad=original;write_at(bad,rp+offsetof(V4ValidationRoot,plain)+offsetof(xo::XFixedVector<V4Scalar>,arena_delta),INT32_MAX);
        reject(bad,"wire validation: relative pointer out of bounds");
    }
}
void alignment_fallback(){
    alignas(64)std::array<char,256>a{};wd::WireValidationContext ctx{.base=a.data(),.end=a.data()+a.size()};
    for(std::size_t alignment:{1,3,6,8,16,64})for(std::size_t offset=0;offset<64;++offset){
        bool caught=false;try{wd::validate_storage_range(a.data()+offset,8,alignment,ctx,"alignment oracle");}
        catch(const xo::XException& e){caught=std::string_view(e.what())=="alignment oracle";}
        need(caught==((reinterpret_cast<std::uintptr_t>(a.data()+offset)%alignment)!=0),"positive generic alignment fallback changed");
    }
    wd::validate_storage_range(nullptr,0,3,ctx,"zero");
}
}
int main(){
    try{
        independent_oracle();global_priority();thresholds_and_summary();actual_new_failures();trait_and_typed_checks();alignment_fallback();
        std::cout<<"{\"status\":\"passed\",\"independent_oracle_operations\":"<<oracle_operations<<",\"global_owned_priority_cases\":"<<priority_cases<<",\"actual_global_new_failure_points\":"<<real_host_failures<<",\"extent_threshold_cases\":"<<threshold_cases<<",\"typed_bool_binding_order_rejections\":"<<typed_rejections<<",\"performance_samples\":0}\n";
        return 0;
    }catch(const std::exception& e){fail_new::watching=false;fail_new::fail_after=-1;std::cerr<<e.what()<<'\n';return 1;}
}
