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


// O1-specific supplements. Run the unchanged full-project range oracle and
// typed-load all-new failpoint test as well. No timing or allocator substitution.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <xoffsetdatastructure.hpp>
#include "bitmap_test_support.hpp"
using namespace XOffsetDatastructure;
namespace wd = XOffsetDatastructure::detail;
struct HintAliasRoot { XString left, right; XVector<std::uint32_t> samples; };
XOFFSET_REGISTER_SCHEMA_NAME(HintAliasRoot, "test.HintAliasRoot.O1.v1")
namespace {
void need(bool b, const char* why) { if (!b) throw std::runtime_error(why); }
enum class Result { ok, bounds, oo, of, fo, ff };
constexpr const char* bounds_message = "O1-test: invalid range";
struct Interval { std::size_t begin, end; };
struct Oracle {
    std::vector<Interval> owned, free;
    static bool overlaps(const std::vector<Interval>& old, std::size_t b, std::size_t e) {
        for (auto x : old) if (b < x.end && x.begin < e) return true;
        return false;
    }
    Result insert(bool f, std::uintptr_t addr, std::size_t bytes, std::uintptr_t base, std::size_t extent) {
        if (!addr || !bytes) return Result::ok;
        if (addr < base || addr-base > extent || bytes > extent-(addr-base)) return Result::bounds;
        const auto b=static_cast<std::size_t>(addr-base), e=b+bytes;
        if (overlaps(owned,b,e)) return f ? Result::fo : Result::oo;
        if (overlaps(free,b,e)) return f ? Result::ff : Result::of;
        (f ? free : owned).push_back({b,e}); return Result::ok;
    }
};
Result category(std::string_view s) {
    if (s==bounds_message) return Result::bounds;
    if (s=="wire validation detected overlapping dynamic ranges") return Result::oo;
    if (s=="wire validation detected live/free overlap") return Result::of;
    if (s=="wire validation detected free-list overlap with live ranges") return Result::fo;
    if (s=="wire validation detected overlapping free-list ranges") return Result::ff;
    throw std::runtime_error("unexpected error category");
}
std::size_t operations{}, hint_checks{}, allocation_failures{}, alias_rejections{};
struct Fixture {
    alignas(64) std::array<char,128> arena{};
    wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+96};
    Oracle expected;
    static void same(const wd::WireValidationByteIndex& actual, const std::vector<Interval>& want) {
        bitmap_test::same(actual,want,128);
    }
    Result raw(bool f,std::uintptr_t address,std::size_t bytes) {
        const auto old_owned=bitmap_test::snapshot(ctx.owned_ranges), old_free=bitmap_test::snapshot(ctx.free_ranges);
        const auto want=expected.insert(f,address,bytes,reinterpret_cast<std::uintptr_t>(arena.data()),96);
        auto got=Result::ok;
        try {
            if(f)wd::register_free_range(reinterpret_cast<const void*>(address),bytes,ctx,bounds_message);
            else wd::register_owned_range(reinterpret_cast<const void*>(address),bytes,ctx,bounds_message);
        } catch(const XException& e) { got=category(e.what()); }
        need(want==got,"unsorted pairwise result differs");
        if(got!=Result::ok)need(bitmap_test::snapshot(ctx.owned_ranges)==old_owned && bitmap_test::snapshot(ctx.free_ranges)==old_free,"rejection changed bitmap coverage");
        same(ctx.owned_ranges,expected.owned);same(ctx.free_ranges,expected.free);++operations;return got;
    }
    Result step(bool f,std::size_t b,std::size_t n) { return raw(f,reinterpret_cast<std::uintptr_t>(arena.data())+b,n); }
};
void hints() {
    using Index=wd::WireValidationRangeIndex;
    // This also detects an accidental public three-argument overload ambiguity.
    bool(*legacy)(const Index&,std::size_t,std::size_t)=&wd::wire_range_overlaps;
    Index index{{8,16},{24,32},{48,64}};
    for(std::size_t b=0;b<=80;++b)for(std::size_t e=b;e<=80;++e) {
        bool overlap=false;
        if(b!=e)for(auto x:index) if(b<x.second && x.first<e)overlap=true;
        auto expected=index.end();
        if(b!=e)for(auto it=index.begin();it!=index.end();++it) if(it->first>=b){expected=it;break;}
        auto hint=index.cend();
        need(wd::wire_range_overlaps_with_hint(index,b,e,hint)==overlap,"hint overlap differs from pairwise scan");
        need(hint==expected,"hint differs from linear insertion position");
        need(legacy(index,b,e)==overlap,"legacy helper differs");++hint_checks;
    }
    const auto high=(std::numeric_limits<std::size_t>::max)();
    Index top{{high-16,high-8}};auto h=top.cend();
    need(!wd::wire_range_overlaps_with_hint(top,high-8,high,h)&&h==top.end(),"high boundary adjacency rejected");
    need(wd::wire_range_overlaps_with_hint(top,high-9,high,h),"high overlap accepted");
}
void adversarial_registration() {
    // Every order and ownership split exercises insertion before/between/after.
    std::array<std::size_t,4> order{0,1,2,3};
    do { for(unsigned mask=0;mask<16;++mask) {
        Fixture f;
        for(auto n:order) f.step((mask>>n)&1,8+n*16,8);
        for(bool isfree:{false,true}) {
            f.step(isfree,8,1);f.step(isfree,10,2);f.step(isfree,7,3);
            f.step(isfree,7,60); // May span both classes; owned must win.
            f.raw(isfree,0,999);f.step(isfree,0,0);f.step(isfree,96,0);
            f.step(isfree,96,1);f.step(isfree,8,(std::numeric_limits<std::size_t>::max)());
            f.raw(isfree,(std::numeric_limits<std::uintptr_t>::max)(),8);
        }
    }} while(std::next_permutation(order.begin(),order.end()));
    for(bool f:{false,true}) {
        Fixture x;x.step(false,16,8);x.step(true,40,8);
        need(x.step(f,20,24)==(f?Result::fo:Result::oo),"owned-first priority lost");
        need(x.step(f,39,10)==(f?Result::ff:Result::of),"free-only category lost");
        need(x.step(f,16,100)==Result::bounds,"bounds did not precede overlap");
        x.step(f,0,16);x.step(f,24,16);x.step(f,48,48); // exact half-open adjacency
    }
}
void allocation_failure() {
    // Keep all sixteen actual host failure scenarios on explicit paged indexes at fresh bitmap-page
    // prepare boundaries. The independent legacy hints test above is unchanged.
    for(bool f:{false,true})for(bool seeded:{false,true})for(std::size_t b:{0u,4096u,8192u,12288u}) {
        const auto outer=fail_new::live();
        {
            alignas(64)std::array<char,32768> arena{};
            wd::WireValidationContext ctx{.base=arena.data(),.end=arena.data()+arena.size(),
                .owned_ranges=wd::WireValidationByteIndex{&ctx.range_resource, false},
                .free_ranges=wd::WireValidationByteIndex{&ctx.free_range_resource, false}};
            if(seeded) {
                wd::register_owned_range(arena.data()+28680,8,ctx,bounds_message);
                wd::register_owned_range(arena.data()+28712,8,ctx,bounds_message);
                wd::register_free_range(arena.data()+28696,8,ctx,bounds_message);
                wd::register_free_range(arena.data()+28728,8,ctx,bounds_message);
            }
            bool exhausted=false;
            for(std::size_t fill=0;fill<65536;++fill){
                try{fail_new::Arm arm(0);(void)(f?ctx.free_range_resource:ctx.range_resource).allocate(1,1);}
                catch(const std::bad_alloc&){exhausted=true;break;}
                need(fail_new::attempts==0&&fail_new::hits==0,"priming made an unfailed host request");
            }
            need(exhausted,"test failed to exhaust existing resource space");
            const auto owned=bitmap_test::snapshot(ctx.owned_ranges),free=bitmap_test::snapshot(ctx.free_ranges);
            const auto before=fail_new::live();bool caught=false;
            {fail_new::Arm arm(0);try{
                if(f)wd::register_free_range(arena.data()+b,4,ctx,bounds_message);
                else wd::register_owned_range(arena.data()+b,4,ctx,bounds_message);
            }catch(const std::bad_alloc&){caught=true;}}
            need(caught&&fail_new::attempts==1&&fail_new::hits==1&&!fail_new::overflow,"actual bitmap upstream allocation was not failed");
            need(fail_new::live()==before,"failed bitmap allocation leaked");
            need(bitmap_test::snapshot(ctx.owned_ranges)==owned&&bitmap_test::snapshot(ctx.free_ranges)==free,"failed prepare changed exact bitmap/count/extent");
            if(f)wd::register_free_range(arena.data()+b,4,ctx,bounds_message);
            else wd::register_owned_range(arena.data()+b,4,ctx,bounds_message);
            need((f?ctx.free_ranges:ctx.owned_ranges).size()==(f?free.registrations:owned.registrations)+1,"bitmap recovery failed");
            ++allocation_failures;
        }
        need(fail_new::live()==outer,"bitmap fixture cleanup leaked");
    }
}
void malicious_typed_alias() {
    auto buffer=XBuffer::create<HintAliasRoot>(1u<<20);
    auto& root=buffer.root<HintAliasRoot>();
    root.left=std::string(64,'l');root.right=std::string(64,'r');
    for(std::uint32_t i=0;i<16;++i)root.samples.push_back(i*17);
    const auto delta_field_offset=static_cast<std::size_t>(reinterpret_cast<const char*>(&root.right.data_delta)-buffer.data());
    const auto left_data_offset=static_cast<std::size_t>(root.left.data()-buffer.data());
    const auto wire=buffer.save_verified<HintAliasRoot>();auto evil=wire;
    const auto delta=static_cast<std::int32_t>(static_cast<std::ptrdiff_t>(left_data_offset)-static_cast<std::ptrdiff_t>(delta_field_offset));
    need(sizeof(XWireHeaderV1)+delta_field_offset+sizeof(delta)<=evil.size(),"alias fixture field not in wire");
    std::memcpy(evil.data()+sizeof(XWireHeaderV1)+delta_field_offset,&delta,sizeof(delta));
    const auto unchanged=evil;bool rejected=false;
    try {auto loaded=XBuffer::load_verified<HintAliasRoot>(evil);(void)loaded;}
    catch(const XException& e) {need(category(e.what())==Result::oo,"malicious alias error category changed");rejected=true;}
    need(rejected && evil==unchanged,"malicious alias accepted or input changed");++alias_rejections;
    auto good=XBuffer::load_verified<HintAliasRoot>(wire);const auto& restored=good.root<HintAliasRoot>();
    need(restored.left.view()==std::string(64,'l') && restored.right.view()==std::string(64,'r') && restored.samples.size()==16,"valid typed recovery differs");
    for(std::uint32_t i=0;i<16;++i)need(restored.samples[i]==i*17,"typed sample changed");
}
}
int main() {
    try {hints();adversarial_registration();allocation_failure();malicious_typed_alias();
        std::cout<<"{\"o1_range_hint\":\"passed\",\"operations\":"<<operations
                 <<",\"hint_checks\":"<<hint_checks<<",\"real_bad_alloc_points\":"<<allocation_failures
                 <<",\"typed_alias_rejections\":"<<alias_rejections<<",\"performance_samples\":0}\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
