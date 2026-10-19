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

// The complete real global-new/delete hook prefix is added from the pinned
// prior fault test before this fresh prototype is frozen. No native time test.
#include <array>
#include <iostream>
#include <limits>
#include <string_view>
#include "bitmap_test_support.hpp"
namespace xo=XOffsetDatastructure;
namespace wd=XOffsetDatastructure::detail;
namespace {
void need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::size_t cases{},host_failure_points{},second_boundary_failures{};
void exact_byte_and_page_boundaries(){
    alignas(64)std::array<char,16387> arena{};
    wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+arena.size()};
    std::vector<std::pair<std::size_t,std::size_t>> owned,free;
    auto add=[&](bool f,std::size_t b,std::size_t e){
        if(f)wd::register_free_range(arena.data()+b,e-b,ctx,"bitmap bounds");
        else wd::register_owned_range(arena.data()+b,e-b,ctx,"bitmap bounds");
        (f?free:owned).emplace_back(b,e);
        bitmap_test::same(ctx.owned_ranges,owned,arena.size());
        bitmap_test::same(ctx.free_ranges,free,arena.size());
    };
    add(false,0,1);add(true,1,63);add(false,63,64);add(true,64,65);
    add(false,4095,4096);add(true,4096,4097);add(false,8191,8192);
    add(true,8192,8193);add(false,16386,16387);
    const auto old_owned=bitmap_test::snapshot(ctx.owned_ranges),old_free=bitmap_test::snapshot(ctx.free_ranges);
    for(bool f:{false,true}){
        bool caught=false;
        try{if(f)wd::register_free_range(arena.data(),8193,ctx,"bitmap bounds");else wd::register_owned_range(arena.data(),8193,ctx,"bitmap bounds");}
        catch(const xo::XException& e){caught=std::string_view(e.what())==(f?"wire validation detected free-list overlap with live ranges":"wire validation detected overlapping dynamic ranges");}
        need(caught,"mixed occupied span did not keep owned-first error priority");
        need(bitmap_test::snapshot(ctx.owned_ranges)==old_owned&&bitmap_test::snapshot(ctx.free_ranges)==old_free,"rejection changed bitmap/count/extent");
    }
    need(ctx.owned_ranges.word_at((arena.size()+63)/64)==0,"last word escaped backing span");++cases;
}
void full_pages_and_no_promotion(){
    wd::WireValidationByteIndex full{std::pmr::new_delete_resource(), false};
    full.insert_disjoint(4096,12288,16384);
    need(full.allocated_pages()==0&&full.directory_entries()==4,"full pages allocated partial storage");
    need(!full.overlaps(0,4096)&&full.overlaps(4096,4097)&&full.overlaps(12287,12288)&&!full.overlaps(12288,16384),"full-page sentinel lost exact boundaries");
    for(std::size_t w=64;w<192;++w)need(full.word_at(w)==(std::numeric_limits<std::uint64_t>::max)(),"full-page coverage mismatch");
    wd::WireValidationByteIndex partial{std::pmr::new_delete_resource(), false};
    for(std::size_t i=0;i<4096;++i){need(!partial.overlaps(i,i+1),"adjacent byte rejected");partial.insert_disjoint(i,i+1,4096);}
    need(partial.size()==4096&&partial.allocated_pages()==1,"partial-page storage was lost on becoming full");
    for(std::size_t w=0;w<64;++w)need(partial.word_at(w)==(std::numeric_limits<std::uint64_t>::max)(),"incremental full-page coverage mismatch");++cases;
}
void all_three_prepare_failures(bool grow){
    for(long point=0;point<3;++point){
        const auto outside=fail_new::live();
        {
            wd::WireValidationByteIndex index{std::pmr::new_delete_resource(), false};
            if(grow)index.insert_disjoint(8,9,4096);
            const auto before=bitmap_test::snapshot(index);const auto live=fail_new::live();
            const std::size_t b=grow?8191:4095,e=grow?12289:8193,extent=grow?16384:12288;
            bool caught=false;
            {fail_new::Arm arm(point);try{index.insert_disjoint(b,e,extent);}catch(const std::bad_alloc&){caught=true;}}
            need(caught&&fail_new::hits==1&&fail_new::attempts==std::size_t(point+1)&&!fail_new::overflow,"real directory/boundary new failure not reached");
            need(fail_new::live()==live&&bitmap_test::snapshot(index)==before,"failed prepare changed extent/count/directory/coverage or leaked");
            if(point==2)++second_boundary_failures;
            index.insert_disjoint(b,e,extent);
            need(index.size()==before.registrations+1&&index.extent()==extent&&index.allocated_pages()==before.pages+2,"failed prepare did not recover");
            need(index.overlaps(b,b+1)&&index.overlaps(e-1,e)&&!index.overlaps(b-1,b)&&!index.overlaps(e,e+1),"prepared endpoints lost byte boundaries");
            if(grow)need(index.overlaps(8,9),"directory expansion lost old coverage");++host_failure_points;
        }
        need(fail_new::live()==outside,"index destruction leaked real C++ allocations");
    }
    ++cases;
}
void real_context_second_edge_failure(){
    alignas(64)std::array<char,16384> arena{};
    // The actual production registration function is exercised with a direct
    // new/delete-backed index so the second prepared Page has a real host point.
    // The normal per-context monotonic cold/refill tests remain separate.
    wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+arena.size(),
        .owned_ranges=wd::WireValidationByteIndex{std::pmr::new_delete_resource(), false}};
    const auto owned=bitmap_test::snapshot(ctx.owned_ranges),free=bitmap_test::snapshot(ctx.free_ranges);
    const auto before=fail_new::live();bool caught=false;
    {fail_new::Arm arm(2);try{wd::register_owned_range(arena.data()+4095,4098,ctx,"bitmap bounds");}catch(const std::bad_alloc&){caught=true;}}
    need(caught&&fail_new::hits==1&&fail_new::attempts==3,"actual context second-edge host failure absent");
    need(bitmap_test::snapshot(ctx.owned_ranges)==owned&&bitmap_test::snapshot(ctx.free_ranges)==free&&fail_new::live()==before,"actual registration partially committed after second edge failure");
    wd::register_owned_range(arena.data()+4095,4098,ctx,"bitmap bounds");
    need(ctx.owned_ranges.size()==1&&ctx.owned_ranges.allocated_pages()==2,"actual registration recovery failed");++second_boundary_failures;++host_failure_points;++cases;
}
struct RejectResource final:std::pmr::memory_resource{
    std::size_t attempts{},bytes{};
    void* do_allocate(std::size_t n,std::size_t)override{++attempts;bytes=n;throw std::bad_alloc();}
    void do_deallocate(void*,std::size_t,std::size_t)noexcept override{std::abort();}
    bool do_is_equal(const std::pmr::memory_resource& x)const noexcept override{return this==&x;}
};
void arithmetic_and_prevalidation(){
    RejectResource reject;wd::WireValidationByteIndex index(&reject);
    bool caught=false;try{index.insert_disjoint(1,2,(std::numeric_limits<std::size_t>::max)());}catch(const std::bad_alloc&){caught=true;}
    const auto slots=(std::numeric_limits<std::size_t>::max)()/4096+1;
    need(caught&&reject.attempts==1&&reject.bytes==slots*sizeof(std::uintptr_t)&&index.empty()&&index.extent()==0,"directory ceil/multiplication wrapped or failed state changed");
    alignas(64)std::array<char,64> arena{};wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+arena.size(),.owned_ranges=wd::WireValidationByteIndex{&reject}};
    const auto calls=reject.attempts;
    for(auto address:{reinterpret_cast<std::uintptr_t>(arena.data())-1,(std::numeric_limits<std::uintptr_t>::max)()}){
        bool bad=false;try{wd::register_owned_range(reinterpret_cast<const void*>(address),8,ctx,"bitmap bad address");}
        catch(const xo::XException& e){bad=std::string_view(e.what())=="bitmap bad address";}
        need(bad&&reject.attempts==calls,"bounds were not checked before allocation/bitmap access");
    }
    wd::register_owned_range(nullptr,(std::numeric_limits<std::size_t>::max)(),ctx,"no-op");
    wd::register_owned_range(reinterpret_cast<const void*>((std::numeric_limits<std::uintptr_t>::max)()),0,ctx,"no-op");
    need(reject.attempts==calls&&ctx.owned_ranges.empty(),"null/zero behavior changed");++cases;
}
}
int main(){try{
    exact_byte_and_page_boundaries();full_pages_and_no_promotion();all_three_prepare_failures(false);all_three_prepare_failures(true);real_context_second_edge_failure();arithmetic_and_prevalidation();
    std::cout<<"{\"status\":\"passed\",\"bitmap_cases\":"<<cases<<",\"actual_host_failure_points\":"<<host_failure_points<<",\"second_boundary_failures\":"<<second_boundary_failures<<",\"performance_samples\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
