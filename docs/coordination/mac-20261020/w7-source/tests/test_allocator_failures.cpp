// Real global-new failures, armed only around the allocator operation under test.
#include <array>
#include <cstdlib>
#include <new>
#include <iostream>
#include <cstring>
#include "allocator_model_support.hpp"
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;
using allocator_model::require;
namespace { thread_local long fail_after=-1; thread_local bool watch=false; thread_local std::size_t attempts=0,hits=0; }
void* operator new(std::size_t n) {
    if(watch) { ++attempts; if(fail_after==0) {fail_after=-1;++hits;throw std::bad_alloc();} if(fail_after>0)--fail_after; }
    if(auto* p=std::malloc(n?n:1))return p; throw std::bad_alloc();
}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
struct AllocatorFailureRoot {XVector<std::uint32_t> values;};
XOFFSET_REGISTER_SCHEMA_NAME(AllocatorFailureRoot,"test.AllocatorFailureRoot")
namespace {
struct Outcome {std::size_t attempts,hits,free;};
Outcome exercise(int kind,long n) {
    auto b=XBuffer::create<AllocatorFailureRoot>(1024*1024,XBuffer::maximum_capacity(1024*1024));
    auto* guard=b.arena()->allocate_aligned(64,16); std::memset(guard,0x5d,64);
    auto* state=b.find_allocator_storage<XAllocatorStateV1>(); require(state,"missing state");
    std::array<void*,128> blocks{};
    for(auto& p:blocks) p=b.arena()->allocate_aligned(256,16);
    if(kind==1) for(auto p:blocks) fixed_detail::retire_dynamic_bytes(fixed_detail::allocator_ref_of(state),p,256);
    attempts=hits=0;fail_after=n;watch=true;
    if(kind==0) for(auto p:blocks)b.arena()->deallocate(p);
    else fixed_detail::release_cached_blocks(state);
    watch=false;fail_after=-1; auto result=Outcome{attempts,hits,b.get_free_memory()};
    for(unsigned j=0;j<64;++j)require(static_cast<unsigned char*>(guard)[j]==0x5d,"host OOM changed live data");
    std::array<void*,128> reused{}; std::vector<allocator_model::Interval> ranges;ranges.reserve(129);
    for(auto& p:reused) { p=b.arena()->allocate_aligned(128,16); auto off=static_cast<char*>(p)-b.data();
        XBufferCore::AllocationHeader h;std::memcpy(&h,static_cast<char*>(p)-16,16);
        ranges.push_back({std::uint64_t(off)-16,std::uint64_t(off)+h.user_bytes,0,0}); }
    auto off=static_cast<char*>(guard)-b.data();ranges.push_back({std::uint64_t(off)-16,std::uint64_t(off)+64,0,0});
#ifdef S3_PROBE
    require(b.s3_validate_index(),"post-OOM invalid index");
    b.s3_visit_free([&](auto off,auto size,auto align) {
        XBufferCore::AllocationHeader h;std::memcpy(&h,b.data()+off-16,16);
        require(h.user_offset==off && h.user_bytes==size && h.total_bytes==16ull+size && h.alignment==align,"post-OOM stale free reference");
        ranges.push_back({off-16ull,off+std::uint64_t(size),0,1});
    });
#endif
    allocator_model::validate(ranges,b.arena()->used_end);
    require(state->free_bytes==0,"transfer left duplicate wire owners");
    auto frame=b.save_verified<AllocatorFailureRoot>(); auto loaded=XBuffer::load_verified<AllocatorFailureRoot>(frame);
    require(loaded.root<AllocatorFailureRoot>().values.empty(),"post-failure wire corruption");
    return result;
}
void lazy_load() {
    auto b=XBuffer::create<AllocatorFailureRoot>(65536);
    b.root<AllocatorFailureRoot>().values.push_back(77);auto frame=b.save_verified<AllocatorFailureRoot>();
    auto loaded=XBuffer::load_verified<AllocatorFailureRoot>(frame,XBuffer::maximum_capacity(65536));
    loaded.grow(4096);auto* p=loaded.arena()->allocate_aligned(64,16);
    attempts=hits=0;fail_after=0;watch=true;loaded.arena()->deallocate(p);watch=false;fail_after=-1;
    require(hits==1,"post-load lazy metadata failure not reached");
    require(loaded.root<AllocatorFailureRoot>().values[0]==77,"post-load OOM changed root");
}
// G: the failed no-growth attempt inside allocate_dynamic_bytes is a probe,
// not a thrown XBadAlloc: no diagnostic exception string is allocated before
// the unchanged release-then-grow recovery, whose order and errors are kept.
auto growth_policy(bool automatic) {
    auto cap=XBuffer::maximum_capacity(1024*1024);
    return automatic ? cap.with_growth(XGrowPolicy::Automatic) : cap;
}
void growth_probe_without_host_allocation() {
    for(long point:{-1L,0L}) {
        auto b=XBuffer::create<AllocatorFailureRoot>(4096,growth_policy(true));
        auto* state=b.find_allocator_storage<XAllocatorStateV1>(); require(state,"missing state");
        require(state->free_bytes==0,"fixture unexpectedly has cached blocks");
        auto ref=fixed_detail::allocator_ref_of(state); const auto size=b.get_size();
        attempts=hits=0;fail_after=point;watch=true;
        void* p=nullptr;
        try { p=fixed_detail::allocate_dynamic_bytes(ref,static_cast<std::uint32_t>(size),8); }
        catch(...) { watch=false;fail_after=-1;throw; }
        watch=false;fail_after=-1;
        require(p && b.get_size()>size,"growth allocation did not grow");
        require(attempts==0 && hits==0,"growth signalling attempted a host allocation");
        std::memset(p,0x47,size);
        auto frame=b.save_verified<AllocatorFailureRoot>(); (void)XBuffer::load_verified<AllocatorFailureRoot>(frame);
    }
}
void growth_probe_releases_cached_blocks_first() {
    for(bool automatic:{true,false}) {
        auto b=XBuffer::create<AllocatorFailureRoot>(4096,growth_policy(automatic));
        auto* state=b.find_allocator_storage<XAllocatorStateV1>(); require(state,"missing state");
        auto ref=fixed_detail::allocator_ref_of(state);
        auto* small=fixed_detail::allocate_dynamic_bytes(ref,256,8); fixed_detail::retire_dynamic_bytes(ref,small,256);
        require(state->free_bytes==256,"fixture cached block missing");
        const auto epoch=state->epoch; const auto size=b.get_size();
        bool bad=false; void* p=nullptr;
        try { p=fixed_detail::allocate_dynamic_bytes(ref,static_cast<std::uint32_t>(size),8); }
        catch(const XBadAlloc&) { bad=true; }
        require(state->free_bytes==0 && state->epoch==epoch+1,"cached blocks were not released exactly once before growth");
        for(auto head:state->freelist_head) require(head==0,"release retained a free-list head");
        if(automatic) require(!bad && p && b.get_size()>size,"automatic growth after release failed");
        else require(bad && !p && b.get_size()==size,"explicit policy must still end with XBadAlloc");
        require(b.arena()->allocate_aligned(200,8)==small,"released block was not handed to the Core free list");
    }
}
}
int main() {try {
    std::size_t points=0,max_lost=0;
    for(int kind=0;kind<2;++kind) {
        auto normal=exercise(kind,-1); require(normal.attempts>0,"no real metadata allocations observed");
        for(std::size_t n=0;n<normal.attempts;++n) {auto r=exercise(kind,n);require(r.hits==1,"requested fail point not hit");
            require(r.free<=normal.free,"failure invented free bytes");max_lost=std::max(max_lost,normal.free-r.free);++points;}
    }
    lazy_load(); growth_probe_without_host_allocation(); growth_probe_releases_cached_blocks_first(); std::cout<<"{\"status\":\"passed\",\"actual_failure_points\":"<<points+1<<",\"max_unreclaimed_bytes\":"<<max_lost<<"}\n";
} catch(const std::exception& e) {watch=false;std::cerr<<e.what()<<'\n';return 1;} }
