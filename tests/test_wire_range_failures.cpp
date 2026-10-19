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

#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include "../xoffsetdatastructure.hpp"
#include "bitmap_test_support.hpp"
using namespace XOffsetDatastructure;
namespace wd = XOffsetDatastructure::detail;
struct RangeFailureRecord {
    std::uint32_t id{};
    XString name;
    XVector<std::uint32_t> values;
    XMap<std::int32_t, XString> attrs;
};
struct RangeFailureRoot {
    std::uint32_t marker{};
    XString title;
    XVector<RangeFailureRecord> records;
    XVector<std::uint32_t> retired;
    XString barrier;
};
XOFFSET_REGISTER_SCHEMA_NAME(RangeFailureRoot, "test.RangeFailureRoot.online-index-v1")
namespace {
void need(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
struct Summary {
    std::size_t direct_points{}, first_upstream_chunk_request_bytes{}, graph_new_calls{}, typed_load_new_calls{};
    std::size_t typed_bad_alloc{}, typed_xbad_alloc{}, chunk_sized_requests{};
} summary;
std::array<std::size_t, fail_new::trace_capacity> observed_graph_requests{};
void register_one(bool free, const char* address, wd::WireValidationContext& ctx) {
    if (free) wd::register_free_range(address, 8, ctx, "failure-test invalid free range");
    else wd::register_owned_range(address, 8, ctx, "failure-test invalid owned range");
}
void direct_bitmap_prepare_failures() {
    // Explicit paged indexes: empty contexts fail the first upstream request. Nonempty contexts first
    // accept warm nodes, then fail the first actual monotonic refill request.
    for (bool free : {false, true}) for (bool nonempty : {false, true}) {
        const auto outside = fail_new::live();
        {
            alignas(64) std::array<char, 131072> arena{};
            wd::WireValidationContext ctx{.base = arena.data(), .end = arena.data() + arena.size(),
                .owned_ranges=wd::WireValidationByteIndex{&ctx.range_resource, false},
                .free_ranges=wd::WireValidationByteIndex{&ctx.free_range_resource, false}};
            if (nonempty) {
                wd::register_owned_range(arena.data() + 8, 8, ctx, "test owned");
                wd::register_free_range(arena.data() + 32, 8, ctx, "test free");
            }
            bool caught = false;
            std::size_t warm_nodes = 0;
            for (std::size_t i = 0; i < 4096; ++i) {
                const auto before_owned = bitmap_test::snapshot(ctx.owned_ranges), before_free = bitmap_test::snapshot(ctx.free_ranges);
                const auto live_before = fail_new::live();
                const auto* address = arena.data() + 64 + i * 16;
                try { fail_new::Arm arm(0); register_one(free, address, ctx); }
                catch (const std::bad_alloc&) { caught = true; }
                if (!caught) {
                    need(fail_new::attempts == 0 && fail_new::hits == 0,
                        "warm monotonic node unexpectedly requested host allocation");
                    ++warm_nodes;
                    continue;
                }
                need(fail_new::hits == 1 && fail_new::attempts == 1 && !fail_new::overflow,
                    "actual monotonic upstream new request was not failed once");
                need(bitmap_test::snapshot(ctx.owned_ranges) == before_owned && bitmap_test::snapshot(ctx.free_ranges) == before_free,
                    "upstream bad_alloc changed registered range state");
                need(fail_new::live() == live_before, "failed upstream request leaked a C++ allocation");
                if (!summary.first_upstream_chunk_request_bytes) summary.first_upstream_chunk_request_bytes = fail_new::requests[0];
                register_one(free, address, ctx);
                need((free ? ctx.free_ranges : ctx.owned_ranges).overlaps(64 + i * 16, 72 + i * 16),
                    "normal insertion did not recover after upstream failure");
                ++summary.direct_points;
                break;
            }
            need(caught && (!nonempty || warm_nodes > 0),
                "empty/nonempty actual chunk boundary was not reached");
        }
        need(fail_new::live() == outside, "range context destruction leaked C++ allocations");
    }
}
std::string make_wire() {
    auto buffer = XBuffer::create<RangeFailureRoot>(1u << 20);
    auto& root = buffer.root<RangeFailureRoot>(); root.marker = 0x1234abcd; root.title = "range-failure-root";
    root.records.reserve(8);
    for (std::uint32_t i = 0; i < 8; ++i) {
        auto& record = root.records.emplace_back(); record.id = i + 1;
        record.name = std::string(80, static_cast<char>('a' + i)); record.values.reserve(32);
        for (std::uint32_t j = 0; j < 32; ++j) record.values.push_back(0xa5000000u + i * 32 + j);
        record.attrs.reserve(2); record.attrs.try_emplace(-3, "lower"); record.attrs.try_emplace(9, "upper");
    }
    // Retire a non-tail allocation so real typed load also creates the free index.
    root.retired.reserve(8); root.barrier = std::string(96, 'z'); root.retired.reserve(256);
    need(buffer.allocator_state()->free_bytes > 0, "fixture did not retain a real free-list allocation");
    return buffer.save_verified<RangeFailureRoot>();
}
void values(XBuffer& buffer) {
    const auto& root = buffer.root<RangeFailureRoot>();
    need(root.marker == 0x1234abcd && root.title.view() == "range-failure-root", "typed reload root differs");
    need(root.records.size() == 8 && root.retired.empty() && root.barrier.size() == 96, "typed reload container counts differ");
    for (const auto c : root.barrier.view()) need(c == 'z', "typed reload barrier differs");
    for (std::uint32_t i = 0; i < 8; ++i) {
        const auto& record = root.records[i];
        need(record.id == i + 1 && record.name.size() == 80 && record.values.size() == 32 && record.attrs.size() == 2, "typed reload record metadata differs");
        for (const auto c : record.name.view()) need(c == static_cast<char>('a' + i), "typed reload name differs");
        for (std::uint32_t j = 0; j < 32; ++j) need(record.values[j] == 0xa5000000u + i * 32 + j, "typed reload sample differs");
        auto lower = record.attrs.find(-3), upper = record.attrs.find(9);
        need(lower != record.attrs.end() && upper != record.attrs.end() && lower->second.view() == "lower" && upper->second.view() == "upper", "typed reload attributes differ");
    }
}
void typed_load_failures() {
    // Both copies exist before any arm; source bytes never belong to loaded state.
    const auto wire = make_wire(); const auto immutable_copy = wire;
    {
        auto warm = XBuffer::load_verified<RangeFailureRoot>(wire); values(warm);
        need(warm.save_verified<RangeFailureRoot>() == wire, "normal typed reload changed normalized wire");
        // This isolated real graph check has no constructor/VM/registry work.
        // Its observed new sequence comes from the monotonic upstream chunks in validation.
        {
            const auto live_before = fail_new::live();
            { fail_new::Arm arm(-1); wd::verify_structural_wire_graph(warm.root<RangeFailureRoot>(), warm); }
            need(fail_new::hits == 0 && !fail_new::overflow && fail_new::attempts > 0, "isolated structural traversal did not request upstream chunks");
            summary.graph_new_calls = fail_new::attempts;
            observed_graph_requests = fail_new::requests;
            for (std::size_t i = 0; i < fail_new::attempts; ++i)
                need(observed_graph_requests[i] > 0, "isolated structural graph used a zero-sized upstream request");
            need(fail_new::live() == live_before, "successful structural graph retained C++ allocations");
        }
    }
    const auto outside = fail_new::live();
    {
        // Baseline is a functional inventory, not a performance sample.
        { fail_new::Arm arm(-1); auto loaded = XBuffer::load_verified<RangeFailureRoot>(wire); }
        need(fail_new::hits == 0 && !fail_new::overflow && fail_new::attempts >= summary.graph_new_calls, "typed-load baseline allocation inventory invalid");
        summary.typed_load_new_calls = fail_new::attempts;
        const auto baseline_requests = fail_new::requests;
        // bind_schema and return perform no further new; the final observed
        // requests must be the already isolated structural upstream-chunk sequence.
        const auto graph_start = summary.typed_load_new_calls - summary.graph_new_calls;
        for (std::size_t i = graph_start; i < summary.typed_load_new_calls; ++i)
            need(baseline_requests[i] == observed_graph_requests[i - graph_start], "typed-load upstream chunk sequence suffix changed");
        need(fail_new::live() == outside, "normal typed-load scope retained C++ allocations");
        for (std::size_t point = 0; point < summary.typed_load_new_calls; ++point) {
            const auto live_before = fail_new::live(); bool caught = false, standard = false;
            try { fail_new::Arm arm(static_cast<long>(point)); auto loaded = XBuffer::load_verified<RangeFailureRoot>(wire); }
            catch (const std::bad_alloc&) { caught = standard = true; ++summary.typed_bad_alloc; }
            catch (const XBadAlloc&) { caught = true; ++summary.typed_xbad_alloc; }
            need(caught && fail_new::hits == 1 && !fail_new::overflow && fail_new::attempts >= point + 1, "requested real typed-load failpoint was not reached exactly once");
            if (point >= graph_start) need(standard, "structural bitmap-chunk failure did not propagate std::bad_alloc");
            if (baseline_requests[point] == summary.first_upstream_chunk_request_bytes) ++summary.chunk_sized_requests;
            need(fail_new::live() == live_before, "failed typed-load RAII scope leaked a C++ allocation");
            need(wire == immutable_copy, "failed typed load modified the original wire");
            {
                auto recovered = XBuffer::load_verified<RangeFailureRoot>(wire); values(recovered);
                need(recovered.save_verified<RangeFailureRoot>() == immutable_copy, "fresh recovery load changed canonical normalized wire bytes");
            }
            need(fail_new::live() == live_before && wire == immutable_copy, "recovery scope retained resources or changed the wire");
        }
    }
    need(fail_new::live() == outside, "typed-load sweep retained C++ allocations");
}
}
int main() {
    try {
        direct_bitmap_prepare_failures(); typed_load_failures();
        std::cout << "{\"schema\":\"wire-range-real-new-failure-bitmap-v3\",\"status\":\"passed\",\"direct_empty_nonempty_owned_free_points\":" << summary.direct_points
                  << ",\"first_upstream_chunk_request_bytes\":" << summary.first_upstream_chunk_request_bytes << ",\"isolated_structural_graph_new_calls\":" << summary.graph_new_calls
                  << ",\"typed_load_observed_new_points\":" << summary.typed_load_new_calls << ",\"typed_std_bad_alloc\":" << summary.typed_bad_alloc
                  << ",\"typed_XBadAlloc\":" << summary.typed_xbad_alloc << ",\"chunk_sized_requests_not_stack_attribution\":" << summary.chunk_sized_requests
                  << ",\"original_wire_unchanged_and_reloaded_after_each_failure\":true,\"CXX_allocation_scope_leaks\":0,\"does_not_cover\":[\"all_malloc\",\"all_OS_VM_failures\",\"multi_operation_transactions\"],\"performance_samples\":0}\n";
        return 0;
    } catch (const std::exception& error) { fail_new::watching = false; fail_new::fail_after = -1; std::cerr << error.what() << '\n'; return 1; }
    catch (...) { fail_new::watching = false; fail_new::fail_after = -1; std::cerr << "unexpected range failure-test exception\n"; return 1; }
}
