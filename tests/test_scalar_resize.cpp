// v4 scalar default resize: required value initialization, no timer.
#include <array>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>
#include "xoffsetdatastructure.hpp"

namespace xo = XOffsetDatastructure;
namespace fd = xo::fixed_detail;
namespace dt = xo::detail;
namespace host_failure {
bool fail_once = false;
unsigned handler_calls = 0;
xo::XBuffer* from = nullptr;
xo::XBuffer* to = nullptr;
void move_owner_handler() {
    ++handler_calls;
    *to = std::move(*from);
}
}
// Deterministic host-allocation failure using the ordinary new-handler retry
// protocol. It is armed only around one failing scalar tail grow. In the
// locked libc++ toolchain the XBadAlloc message allocates after grow_to has
// returned false; moving Core there must not revive a stale owner cache.
void* operator new(std::size_t n) {
    for (;;) {
        if (host_failure::fail_once) host_failure::fail_once = false;
        else if (void* p = std::malloc(n ? n : 1)) return p;
        if (auto h = std::get_new_handler()) h(); else throw std::bad_alloc();
    }
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
struct ScalarChild { xo::XVector<std::uint32_t> numbers; xo::XVector<std::uint8_t> bytes; };
struct ScalarRoot {
    xo::XVector<std::uint32_t> numbers;
    xo::XVector<std::uint8_t> bytes;
    xo::XVector<double> reals;
    xo::XString blocker;
    xo::XVector<ScalarChild> children;
};
XOFFSET_RECORD_DERIVED(ScalarChild, (), numbers, bytes)
XOFFSET_RECORD_DERIVED(ScalarRoot, (), numbers, bytes, reals, blocker, children)
XOFFSET_REGISTER_SCHEMA_NAME(ScalarRoot, "test.v4.scalar-resize.v1")
namespace {
void need(bool value, const char* what) { if (!value) throw std::runtime_error(what); }
auto policy(std::size_t maximum = 16u << 20) {
    return xo::XBuffer::maximum_capacity(maximum).with_growth(xo::XGrowPolicy::Automatic);
}
auto make_buffer(std::size_t initial = 1u << 20) { return xo::XBuffer::create<ScalarRoot>(initial, policy()); }
template<class T> auto descriptor(const T& value) {
    static_assert(sizeof(T) == 16);
    std::array<unsigned char, 16> bytes{}; std::memcpy(bytes.data(), &value, 16); return bytes;
}
template<class T> std::vector<T> values(const xo::XVector<T>& v) {
    return v.size() ? std::vector<T>(v.begin(), v.end()) : std::vector<T>{};
}
// V2 has no allocator operation counters (FreelistIndex diagnostics are gone).
// Allocator operations are observed through the host free table instead:
// live bytes = used_end - free bytes, and a retired block is an exact hole.
using Holes = std::vector<std::pair<std::uint32_t, std::uint32_t>>;
Holes holes(const xo::XBufferCore& b) {
    Holes out; b.for_each_free_block([&](xo::XBufferCore::WireHole h) { out.emplace_back(h.offset, h.bytes); }); return out;
}
std::uint64_t live(const xo::XBufferCore& b) { return b.arena()->used_end - b.free_block_bytes(); }
std::uint32_t extent(std::size_t bytes) { return static_cast<std::uint32_t>((bytes + 7u) & ~std::size_t(7)); }
bool has_hole(const xo::XBufferCore& b, const void* p, std::uint32_t bytes) {
    const auto offset = static_cast<std::uint32_t>(static_cast<const char*>(p) - b.data());
    for (auto [o, n] : holes(b)) if (o == offset && n == bytes) return true;
    return false;
}
void roundtrip(xo::XBuffer& b) {
    const auto n = values(b.root<ScalarRoot>().numbers);
    const auto v = values(b.root<ScalarRoot>().bytes);
    auto loaded = xo::XBuffer::load_verified<ScalarRoot>(b.save_verified<ScalarRoot>(), policy());
    need(values(loaded.root<ScalarRoot>().numbers) == n && values(loaded.root<ScalarRoot>().bytes) == v,
         "scalar roundtrip bytes");
    auto& root = loaded.root<ScalarRoot>();
    const auto old = root.numbers.size(); root.numbers.resize(old + 7);
    for (unsigned i = old; i != root.numbers.size(); ++i) need(root.numbers[i] == 0, "loaded suffix not initialized");
    auto again = xo::XBuffer::load_verified<ScalarRoot>(loaded.save_verified<ScalarRoot>(), policy());
    need(values(again.root<ScalarRoot>().numbers) == values(root.numbers), "loaded mutation contents");
}
void physical_tail_and_required_initialization() {
    auto b = make_buffer(); auto& root = b.root<ScalarRoot>();
    root.numbers.resize(4); for (unsigned i = 0; i != 4; ++i) root.numbers[i] = 100 + i;
    auto* pointer = root.numbers.data(); const auto holes_before = holes(b); const auto used_before = b.arena()->used_end;
    root.numbers.resize(33);
    need(root.numbers.data() == pointer && root.numbers.capacity() == 33, "scalar physical-tail expansion");
    // In-place tail growth: the free table is untouched and used_end moves by exactly the extent delta.
    need(holes(b) == holes_before && b.arena()->used_end == used_before + extent(33 * 4) - extent(4 * 4),
         "physical-tail expansion allocated or retired");
    for (unsigned i = 0; i != 33; ++i) need(root.numbers[i] == (i < 4 ? 100 + i : 0), "tail prefix/suffix");
    root.reals.resize(3); root.reals[0] = 1.5; auto* real_pointer = root.reals.data(); root.reals.resize(11);
    need(root.reals.data() == real_pointer && root.reals[0] == 1.5, "floating tail expansion");
    for (unsigned i = 3; i != 11; ++i) need(root.reals[i] == double{}, "floating value initialization");
    roundtrip(b);
}
void blocked_tail_and_in_capacity() {
    auto b = make_buffer(); auto& root = b.root<ScalarRoot>();
    root.numbers.resize(4, 29u); root.blocker.assign(std::string(129, 'b'));
    auto* before = root.numbers.data(); const auto live_before = live(b); root.numbers.resize(31);
    need(root.numbers.data() != before && root.numbers.capacity() == 31, "non-tail must use fresh path");
    // Exactly one exact 124 -> 128-byte allocation and one retire of the old 16-byte block.
    need(live(b) == live_before + extent(31 * 4) - extent(4 * 4) && has_hole(b, before, extent(4 * 4)),
         "blocked scalar fresh operation count");
    for (unsigned i = 0; i != 31; ++i) need(root.numbers[i] == (i < 4 ? 29u : 0u), "blocked prefix/suffix");
    auto* fresh = root.numbers.data(); root.numbers.resize(2); root.numbers.resize(30);
    need(root.numbers.data() == fresh && root.numbers.capacity() == 31, "in-capacity resize changed storage");
    need(root.numbers[0] == 29 && root.numbers[1] == 29, "shrink prefix");
    for (unsigned i = 2; i != 30; ++i) need(root.numbers[i] == 0, "regrown elements not initialized");
    root.numbers.resize(0); root.numbers.resize(0); root.numbers.resize(8);
    for (auto value : root.numbers) need(value == 0, "empty regrowth not initialized");
    roundtrip(b);
}
void fill_alias_and_nested_default_keep_existing_semantics() {
    auto b = make_buffer(); auto& root = b.root<ScalarRoot>(); root.numbers.resize(2, 83u);
    root.numbers.resize(19, root.numbers[0]);
    for (auto v : root.numbers) need(v == 83, "fill alias failed");
    root.children.resize(2); root.children[0].numbers.resize(4, 91u); root.children[0].bytes.resize(5, std::uint8_t{7});
    root.children.resize(5, root.children[0]);
    for (unsigned i = 2; i != 5; ++i) {
        need(root.children[i].numbers.size() == 4 && root.children[i].bytes.size() == 5, "nested fill sizes");
        for (auto v : root.children[i].numbers) need(v == 91, "nested fill alias values");
        for (auto v : root.children[i].bytes) need(v == 7, "nested bytes values");
        need(root.children[i].numbers.arena() == root.children[0].numbers.arena(), "nested binding");
    }
    root.children.resize(8);
    for (unsigned i = 5; i != 8; ++i)
        need(root.children[i].numbers.empty() && root.children[i].numbers.is_bound(), "nontrivial default path");
    auto loaded = xo::XBuffer::load_verified<ScalarRoot>(b.save_verified<ScalarRoot>(), policy());
    need(values(loaded.root<ScalarRoot>().children[4].numbers) == values(root.children[4].numbers), "nested reload");
    roundtrip(b);
}
void fixed_and_vm_failure_strong_guarantee() {
    auto fixed = xo::XBuffer::create<ScalarRoot>(32768, xo::XBuffer::maximum_capacity(32768));
    auto& v = fixed.root<ScalarRoot>().numbers; v.resize(4, 17u);
    const auto old = descriptor(v); bool caught = false;
    try { v.resize(32768); } catch (const xo::XBadAlloc&) { caught = true; }
    need(caught && descriptor(v) == old && values(v) == std::vector<std::uint32_t>(4, 17), "fixed OOM changed vector");
    for (bool blocked : {false, true}) for (auto operation : {dt::VmOperation::Enable, dt::VmOperation::Prepare}) {
        auto b = make_buffer(4096); auto& root = b.root<ScalarRoot>(); root.numbers.resize(4, 41u);
        if (blocked) root.blocker.assign(std::string(127, 'x'));
        const auto d = descriptor(root.numbers); const auto expected = values(root.numbers);
        dt::clear_vm_failures(); dt::fail_next_vm_operation(operation); caught = false;
        try { root.numbers.resize(65536); } catch (const xo::XBadAlloc&) { caught = true; }
        dt::clear_vm_failures();
        need(caught && descriptor(root.numbers) == d && values(root.numbers) == expected, "VM failure changed vector");
        root.numbers.resize(65536);
        need(root.numbers[0] == 41 && root.numbers[65535] == 0, "VM recovery values");
    }
}
void size_guard_and_owner_lifetimes() {
    xo::XVector<std::uint32_t> unbound; unbound.resize(0); need(unbound.empty(), "unbound empty resize");
    bool caught = false; try { unbound.resize(std::numeric_limits<std::size_t>::max()); }
    catch (const xo::XBadAlloc&) { caught = true; }
    need(caught && !unbound.is_bound(), "count overflow ordering");
    auto a = make_buffer(); a.root<ScalarRoot>().numbers.resize(3, 7u);
    auto moved = std::move(a); auto& v = moved.root<ScalarRoot>().numbers; v.resize(19);
    need(v[0] == 7 && v[18] == 0, "owner move");
    auto other = make_buffer(); other.root<ScalarRoot>().numbers.resize(2, 81u);
    swap(moved, other); other.root<ScalarRoot>().numbers.resize(39);
    need(other.root<ScalarRoot>().numbers[0] == 7 && other.root<ScalarRoot>().numbers[38] == 0, "owner swap");
    moved = make_buffer(); moved.root<ScalarRoot>().numbers.resize(17);
    for (auto x : moved.root<ScalarRoot>().numbers) need(x == 0, "release/replacement state");
    roundtrip(other); roundtrip(moved);
}
void new_handler_owner_move_during_failed_tail_growth() {
    auto source = make_buffer(4096); xo::XBuffer receiver;
    auto& v = source.root<ScalarRoot>().numbers; v.resize(4, 57u);
    const auto d = descriptor(v); const auto content = values(v);
    host_failure::from = &source; host_failure::to = &receiver; host_failure::handler_calls = 0;
    auto previous = std::set_new_handler(host_failure::move_owner_handler);
    dt::clear_vm_failures(); dt::fail_next_vm_operation(dt::VmOperation::Prepare);
    host_failure::fail_once = true; bool caught = false;
    try { v.resize(65536); } catch (const xo::XBadAlloc&) { caught = true; }
    catch (...) {
        host_failure::fail_once = false; dt::clear_vm_failures(); std::set_new_handler(previous); throw;
    }
    host_failure::fail_once = false; dt::clear_vm_failures(); std::set_new_handler(previous);
    need(caught && host_failure::handler_calls == 1, "new-handler Core move was not exercised");
    auto& after = receiver.root<ScalarRoot>().numbers;
    need(descriptor(after) == d && values(after) == content, "callback move changed failed scalar resize");
    after.resize(65536); need(after[0] == 57 && after[65535] == 0, "owner after callback move unusable");
    roundtrip(receiver);
}
}
int main() {
    try {
        physical_tail_and_required_initialization(); blocked_tail_and_in_capacity();
        fill_alias_and_nested_default_keep_existing_semantics(); fixed_and_vm_failure_strong_guarantee();
        size_guard_and_owner_lifetimes();
        new_handler_owner_move_during_failed_tail_growth();
        std::cout << "6 scalar resize groups passed; no timing\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
