// Real, one-shot global C++ new failures in range registration and typed load.
// Fixture encoding, expected values, and logging are outside armed scopes.
// This does not inject malloc, mmap/OS failures or promise multi-operation rollback.
#include <array>
#include <atomic>
#include <cstdlib>
#include <limits>
#include <new>
#if defined(_WIN32)
#include <malloc.h>
#endif
namespace fail_new {
constexpr std::size_t trace_capacity = 4096;
std::atomic<long long> outstanding_blocks{0};
thread_local bool watching = false;
thread_local long fail_after = -1;
thread_local std::size_t attempts = 0, hits = 0;
thread_local bool overflow = false;
thread_local std::array<std::size_t, trace_capacity> requests{};
void check(std::size_t bytes) {
    if (!watching) return;
    if (attempts < requests.size()) requests[attempts] = bytes; else overflow = true;
    ++attempts;
    if (fail_after == 0) { fail_after = -1; ++hits; throw std::bad_alloc(); }
    if (fail_after > 0) --fail_after;
}
void* ordinary(std::size_t bytes) {
    check(bytes);
    if (auto* p = std::malloc(bytes ? bytes : 1)) { outstanding_blocks.fetch_add(1, std::memory_order_relaxed); return p; }
    throw std::bad_alloc();
}
void* aligned(std::size_t bytes, std::align_val_t requested) {
    check(bytes);
    const auto alignment = static_cast<std::size_t>(requested);
    bytes = bytes ? bytes : 1;
    if (!alignment || bytes > (std::numeric_limits<std::size_t>::max)() - (alignment - 1)) throw std::bad_alloc();
    bytes = (bytes + alignment - 1) / alignment * alignment;
#if defined(_WIN32)
    auto* p = _aligned_malloc(bytes, alignment);
#else
    auto* p = std::aligned_alloc(alignment, bytes);
#endif
    if (p) { outstanding_blocks.fetch_add(1, std::memory_order_relaxed); return p; }
    throw std::bad_alloc();
}
void release(void* p) noexcept {
    if (!p) return;
    outstanding_blocks.fetch_sub(1, std::memory_order_relaxed); std::free(p);
}
void release_aligned(void* p) noexcept {
    if (!p) return;
    outstanding_blocks.fetch_sub(1, std::memory_order_relaxed);
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
struct Arm {
    explicit Arm(long point) { attempts = hits = 0; overflow = false; fail_after = point; watching = true; }
    ~Arm() { watching = false; fail_after = -1; }
    Arm(const Arm&) = delete; Arm& operator=(const Arm&) = delete;
};
long long live() { return outstanding_blocks.load(std::memory_order_relaxed); }
}
void* operator new(std::size_t n) { return fail_new::ordinary(n); }
void* operator new[](std::size_t n) { return fail_new::ordinary(n); }
void* operator new(std::size_t n, std::align_val_t a) { return fail_new::aligned(n, a); }
void* operator new[](std::size_t n, std::align_val_t a) { return fail_new::aligned(n, a); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { try { return ::operator new(n); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { try { return ::operator new[](n); } catch (...) { return nullptr; } }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { try { return ::operator new(n, a); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { try { return ::operator new[](n, a); } catch (...) { return nullptr; } }
void operator delete(void* p) noexcept { fail_new::release(p); }
void operator delete[](void* p) noexcept { fail_new::release(p); }
void operator delete(void* p, std::size_t) noexcept { fail_new::release(p); }
void operator delete[](void* p, std::size_t) noexcept { fail_new::release(p); }
void operator delete(void* p, std::align_val_t) noexcept { fail_new::release_aligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { fail_new::release_aligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { fail_new::release_aligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { fail_new::release_aligned(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { fail_new::release(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { fail_new::release(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { fail_new::release_aligned(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { fail_new::release_aligned(p); }


// V3 bitmap resource regression. No clocks or benchmark workload.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory_resource>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>
#include <xoffsetdatastructure.hpp>
#include "bitmap_test_support.hpp"
using namespace XOffsetDatastructure;
namespace wd=XOffsetDatastructure::detail;
namespace {
void need(bool b,const char* why){if(!b)throw std::runtime_error(why);}
struct TrackingResource final:std::pmr::memory_resource {
    struct Block{void* p{};std::size_t bytes{},alignment{};};
    std::array<Block,2048> blocks{};
    std::pmr::memory_resource* upstream;
    std::size_t attempts{},failures{},allocations{},deallocations{},allocated_bytes{},freed_bytes{},live{},live_bytes{},peak_bytes{};
    bool fail_next=false;
    void(*callback)(void*)=nullptr;void* argument{};
    explicit TrackingResource(std::pmr::memory_resource* u=std::pmr::new_delete_resource()):upstream(u){}
    void* do_allocate(std::size_t bytes,std::size_t alignment)override{
        ++attempts;
        if(callback){auto fn=callback;callback=nullptr;fn(argument);}
        if(fail_next){fail_next=false;++failures;throw std::bad_alloc();}
        void* p;
        try{p=upstream->allocate(bytes,alignment);}catch(...){++failures;throw;}
        Block* slot=nullptr;for(auto& b:blocks)if(!b.p){slot=&b;break;}
        if(!slot){upstream->deallocate(p,bytes,alignment);throw std::runtime_error("test ledger capacity exhausted");}
        *slot={p,bytes,alignment};++allocations;++live;allocated_bytes+=bytes;live_bytes+=bytes;
        peak_bytes=std::max(peak_bytes,live_bytes);return p;
    }
    void do_deallocate(void* p,std::size_t bytes,std::size_t alignment)noexcept override{
        Block* slot=nullptr;for(auto& b:blocks)if(b.p==p){slot=&b;break;}
        if(!slot||slot->bytes!=bytes||slot->alignment!=alignment)std::abort();
        upstream->deallocate(p,bytes,alignment);*slot={};++deallocations;--live;freed_bytes+=bytes;live_bytes-=bytes;
    }
    bool do_is_equal(const std::pmr::memory_resource& o)const noexcept override{return this==&o;}
    void balanced()const{need(!live&&!live_bytes&&allocations==deallocations&&allocated_bytes==freed_bytes,"upstream allocation scope did not clean up");}
};
using Saved=std::vector<std::pair<std::size_t,std::size_t>>;
auto save(const wd::WireValidationByteIndex& m){return bitmap_test::snapshot(m);}
void one(bool free,const char* p,std::size_t n,wd::WireValidationContext& c){
    if(free)wd::register_free_range(p,n,c,"PMR-test: bounds");else wd::register_owned_range(p,n,c,"PMR-test: bounds");
}
std::size_t oracle_operations{},failure_boundaries{},real_new_failures{},logical_nodes{},host_chunks{},logical_bytes{},host_bytes{};
void lazy_default_and_isolation(){
    static_assert(std::is_aggregate_v<wd::WireValidationContext>);
    static_assert(!std::is_copy_constructible_v<wd::WireValidationContext>);
    static_assert(!std::is_move_constructible_v<wd::WireValidationContext>);
    struct DefaultGuard{std::pmr::memory_resource* old=std::pmr::set_default_resource(std::pmr::null_memory_resource());~DefaultGuard(){std::pmr::set_default_resource(old);}} guard;
    alignas(64)std::array<char,256>a{};
    wd::WireValidationContext ctx{.base=a.data(),.end=a.data()+a.size()};
    need(ctx.range_resource.upstream_resource()==std::pmr::new_delete_resource()&&ctx.free_range_resource.upstream_resource()==std::pmr::new_delete_resource(),"context captured global default PMR resource");
    need(ctx.owned_ranges.resource()==&ctx.range_resource&&ctx.free_ranges.resource()==&ctx.free_range_resource,"bitmap indexes lack their independent context resources");
    one(false,a.data()+8,8,ctx);one(true,a.data()+32,8,ctx);
    need(ctx.owned_ranges.size()==1&&ctx.free_ranges.size()==1,"default-resource isolation failed");
}
void independent_rejection_oracle(){
    alignas(64)std::array<char,4096>a{};TrackingResource upstream;
    {
        wd::WireValidationContext ctx{.base=a.data(),.end=a.data()+a.size(),
          .range_resource=std::pmr::monotonic_buffer_resource{4096,&upstream},.free_range_resource=std::pmr::monotonic_buffer_resource{4096,&upstream}};
        need(upstream.allocations==0,"monotonic constructor allocated eagerly");
        Saved owned,free;
        auto overlaps=[](const Saved& v,std::size_t b,std::size_t e){for(auto x:v)if(b<x.second&&x.first<e)return true;return false;};
        std::uint64_t seed=0x9e3779b97f4a7c15ULL;
        for(std::size_t i=0;i<2048;++i){
            seed^=seed<<13;seed^=seed>>7;seed^=seed<<17;
            const bool f=seed&1;const auto b=std::size_t((seed>>8)%4200),n=std::size_t((seed>>24)%48);
            const char* expected=nullptr;
            if(n){if(b>a.size()||n>a.size()-std::min(b,a.size()))expected="PMR-test: bounds";
             else if(overlaps(owned,b,b+n))expected=f?"wire validation detected free-list overlap with live ranges":"wire validation detected overlapping dynamic ranges";
             else if(overlaps(free,b,b+n))expected=f?"wire validation detected overlapping free-list ranges":"wire validation detected live/free overlap";}
            auto before_owned=save(ctx.owned_ranges),before_free=save(ctx.free_ranges);const auto calls=upstream.attempts;bool rejected=false;
            try{one(f,reinterpret_cast<const char*>(reinterpret_cast<std::uintptr_t>(a.data())+b),n,ctx);}
            catch(const XException& e){need(expected&&std::string_view(e.what())==expected,"pairwise oracle error category/order differs");rejected=true;}
            need(rejected==(expected!=nullptr),"pairwise oracle acceptance differs");
            if(rejected){need(save(ctx.owned_ranges)==before_owned&&save(ctx.free_ranges)==before_free,"rejection changed bitmap coverage");need(upstream.attempts==calls,"rejected interval allocated an upstream chunk");}
            else if(n)(f?free:owned).emplace_back(b,b+n);
            need(ctx.owned_ranges.size()==owned.size()&&ctx.free_ranges.size()==free.size(),"oracle cardinality differs");
            bitmap_test::same(ctx.owned_ranges,owned,a.size());
            bitmap_test::same(ctx.free_ranges,free,a.size());
            ++oracle_operations;
        }
    }
    upstream.balanced();
}
void real_upstream_failure(){
    for(bool f:{false,true})for(bool seeded:{false,true}){
        const auto outside=fail_new::live();
        {
            alignas(64)std::array<char,131072>a{};TrackingResource upstream;
            {
                wd::WireValidationContext ctx{.base=a.data(),.end=a.data()+a.size(),
                  .range_resource=std::pmr::monotonic_buffer_resource{4096,&upstream},.free_range_resource=std::pmr::monotonic_buffer_resource{4096,&upstream},
                   .owned_ranges=wd::WireValidationByteIndex{&ctx.range_resource, false},
                   .free_ranges=wd::WireValidationByteIndex{&ctx.free_range_resource, false}};
                if(seeded){one(false,a.data()+8,8,ctx);one(true,a.data()+24,8,ctx);}
                bool caught=false;std::size_t in_chunk_success=0;
                for(std::size_t i=0;i<4096;++i){
                    auto owned=save(ctx.owned_ranges),free=save(ctx.free_ranges);auto live=fail_new::live();const auto calls=upstream.allocations;
                    auto* target=a.data()+64+i*16;
                    try{fail_new::Arm arm(0);one(f,target,8,ctx);}catch(const std::bad_alloc&){caught=true;}
                    if(caught){
                        need(fail_new::hits==1&&fail_new::attempts==1&&!fail_new::overflow,"did not fail a real upstream global new call");
                        need(upstream.allocations==calls&&save(ctx.owned_ranges)==owned&&save(ctx.free_ranges)==free,"upstream OOM changed committed ranges");
                        need(fail_new::live()==live,"upstream OOM leaked a host allocation");
                        one(f,target,8,ctx);need(upstream.allocations==calls+1,"same insertion did not recover with one chunk");
                        ++real_new_failures;break;
                    }
                    need(fail_new::hits==0&&fail_new::attempts==0,"unexpected host allocation between chunk boundaries");++in_chunk_success;
                }
                need(caught&&(!seeded||in_chunk_success>0),"real empty/nonempty chunk boundary was not exercised");
            }
            upstream.balanced();
        }
        need(fail_new::live()==outside,"real upstream failure scope leaked");
    }
}
void logical_and_host_layers(){
    alignas(64)std::array<char,16384>a{};TrackingResource upstream;
    {
        std::pmr::monotonic_buffer_resource arena{4096,&upstream};TrackingResource objects{&arena};
        {
            wd::WireValidationContext ctx{.base=a.data(),.end=a.data()+a.size(),
              .owned_ranges=wd::WireValidationByteIndex{&objects},.free_ranges=wd::WireValidationByteIndex{&objects}};
            for(std::size_t i=0;i<1000;++i)one(i&1,a.data()+i*8,4,ctx);
            need(ctx.owned_ranges.size()+ctx.free_ranges.size()==1000,"bitmap registered-range count differs");
            need(objects.allocations>0&&objects.allocations<1000,"bitmap still allocates per registered range");
            need(objects.allocations==2&&objects.allocated_bytes==4224&&upstream.allocations>0&&upstream.allocations<=objects.allocations,"dense object requests and actual host chunks differ from the exact fixture budget");
            logical_nodes=objects.allocations;logical_bytes=objects.allocated_bytes;host_chunks=upstream.allocations;host_bytes=upstream.allocated_bytes;
        }
        objects.balanced();need(upstream.live>0,"index destruction prematurely released monotonic chunks");
    }
    upstream.balanced();
}
struct Reentry{wd::WireValidationContext* inner;std::size_t calls{};};
void enter_inner(void* p){auto& r=*static_cast<Reentry*>(p);++r.calls;one(false,r.inner->base+16,8,*r.inner);}
void reentry_and_resource_failure(){
    alignas(64)std::array<char,4096> a{},b{};TrackingResource inner_up,outer_up;
    {
        wd::WireValidationContext inner{.base=a.data(),.end=a.data()+a.size(),.range_resource=std::pmr::monotonic_buffer_resource{4096,&inner_up},.free_range_resource=std::pmr::monotonic_buffer_resource{4096,&inner_up}};
        Reentry r{&inner};outer_up.callback=enter_inner;outer_up.argument=&r;
        {
            wd::WireValidationContext outer{.base=b.data(),.end=b.data()+b.size(),.range_resource=std::pmr::monotonic_buffer_resource{4096,&outer_up},.free_range_resource=std::pmr::monotonic_buffer_resource{4096,&outer_up}};
            outer_up.fail_next=true;bool caught=false;
            try{one(false,b.data()+8,8,outer);}catch(const std::bad_alloc&){caught=true;}
            need(caught&&outer.owned_ranges.empty()&&outer.free_ranges.empty()&&outer_up.live==0,"upstream resource failure committed partial state");
            need(r.calls==1&&inner.owned_ranges.overlaps(16,24)&&inner_up.live>0,"reentrant inner context was lost");
            one(false,b.data()+8,8,outer);need(outer.owned_ranges.overlaps(8,16),"outer resource failure did not recover");++failure_boundaries;
        }
        outer_up.balanced();need(inner.owned_ranges.overlaps(16,24),"outer cleanup affected live inner context");one(true,a.data()+64,8,inner);
    }
    inner_up.balanced();outer_up.balanced();
}
}
int main(){try{
    lazy_default_and_isolation();independent_rejection_oracle();real_upstream_failure();logical_and_host_layers();reentry_and_resource_failure();
    std::cout<<"{\"status\":\"passed\",\"oracle\":\"unsorted-pairwise\",\"oracle_operations\":"<<oracle_operations
      <<",\"real_global_new_chunk_failures\":"<<real_new_failures<<",\"upstream_resource_failure_boundaries\":"<<failure_boundaries
      <<",\"logical_bitmap_object_requests\":"<<logical_nodes<<",\"logical_requested_bytes\":"<<logical_bytes
      <<",\"actual_upstream_chunk_requests\":"<<host_chunks<<",\"actual_upstream_requested_bytes\":"<<host_bytes
      <<",\"all_scopes_live_zero\":true,\"performance_samples\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
