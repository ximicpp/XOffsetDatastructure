// Process-wide canonical receive window ("W-fixed"): receive storage lives in
// one compile-time-addressed reservation whose single std::byte[] array is the
// provenance root of every canonical pointer.
//
// Variants (same source, separate targets):
//   default                         full window (reserved lazily by the first
//                                   receive); optional --reserve-failure
//   XOFFSET_TEST_WINDOW_SMALL       window of 4 granules (exhaustion)
//   XOFFSET_TEST_WINDOW_DISABLED    XOFFSET_CANONICAL_WINDOW_BYTES == 0
#include "xoffsetdatastructure.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace xo = XOffsetDatastructure;
namespace xd = XOffsetDatastructure::detail;
using Window = xd::CanonicalWindow;
using Storage = xd::VirtualMemoryStorage;

struct WindowRoot {
    std::uint64_t marker{};
    xo::XString label;
    xo::XVector<std::uint32_t> values;
};
XOFFSET_REGISTER_SCHEMA_NAME(WindowRoot, "receive.canonical-window.fixture")
namespace XOffsetDatastructure {
template <> struct wire_record_layout<WindowRoot> { static constexpr bool ordinary_members = true; };
}
using Buffer = xo::TypedXBuffer<WindowRoot>;

namespace {
void need(bool p, const char* why) { if (!p) throw std::runtime_error(why); }

std::uintptr_t addr(const void* p) { return reinterpret_cast<std::uintptr_t>(p); }
bool in_window(const void* p, std::size_t n) {
    const auto off = addr(p) - Window::base;
    return Window::enabled && off <= Window::bytes && n <= Window::bytes - off;
}
template <class F> bool throws_bad_alloc(F&& f) {
    try { f(); } catch (const std::bad_alloc&) { return true; } catch (const xo::XBadAlloc&) { return true; }
    return false;
}

auto policy() { return xo::XBuffer::maximum_capacity(131072).with_growth(xo::XGrowPolicy::Automatic); }
std::string wire() {
    auto b = Buffer::create(32768, policy());
    auto& r = b.root();
    r.marker = 0x5eed;
    r.label = "canonical window";
    r.values.resize(6);
    for (unsigned i = 0; i < 6; ++i) r.values[i] = 11 + i;
    return b.save_verified();
}
Buffer receive(const std::string& bytes) {
    const auto body = bytes.size() - sizeof(xo::XWireHeaderV1);
    auto t = xo::begin_verified_receive<WindowRoot>(body, policy());
    auto op = t.acquire_operation();
    auto h = op.header_bytes(), p = op.payload_bytes();
    std::memcpy(h.data(), bytes.data(), h.size());
    std::memcpy(p.data(), bytes.data() + h.size(), p.size());
    op.complete(h.size(), p.size());
    return xo::finish_verified(t);
}
void check(Buffer& b) {
    auto& r = b.root();
    need(r.marker == 0x5eed && r.label == "canonical window" && r.values.size() == 6 && r.values[5] == 16,
         "received values differ");
}

void ordinary_outside_window() {
    auto b = Buffer::create(8192, policy());
    b.root().marker = 3;
    b.root().values.resize(3);
    b.root().values[2] = 9;
    need(!in_window(b.get_address(), 1), "ordinary buffer placed inside the canonical window");
    need(xo::XBufferCore::canonical_bytes_at(b.get_address(), addr(b.root().values.data()), 4) == nullptr,
         "ordinary buffer resolved canonically");
    Storage plain(4096, 1 << 20);
    need(!plain.in_canonical_window() && !in_window(plain.data(), 1) && !plain.canonical_byte_origin(),
         "ordinary storage placed inside the canonical window");
}

struct VmPoolGuard { ~VmPoolGuard() { xd::clear_vm_failures(); } };

bool touches_window(const void* p, std::size_t n) {
    if (!Window::enabled) return false;
    const auto first = addr(p), last = first + n;
    return first <= Window::base + Window::bytes && last >= Window::base;
}

// Ordinary create/mutate/save/load/mutate round trip; every arena must stay
// clear of the window range unless it is a receive slot.
void ordinary_full_lifecycle() {
    auto b = Buffer::create(16384, policy());
    auto& r = b.root();
    r.marker = 0x5eed;
    r.label = "canonical window";
    r.values.resize(6);
    for (unsigned i = 0; i < 6; ++i) r.values[i] = 11 + i;
    need(!touches_window(b.get_address(), b.maximum_capacity()), "created arena touches the window range");
    const auto bytes = b.save_verified();
    auto loaded = Buffer::load_verified(bytes, policy());
    need(!touches_window(loaded.get_address(), loaded.maximum_capacity()), "loaded arena touches the window range");
    check(loaded);
    loaded.root().label = "canonical window, mutated after load with a longer string";
    for (unsigned i = 0; i < 200; ++i) loaded.root().values.push_back(i);
    need(loaded.root().values.size() == 206 && loaded.root().values[205] == 199 &&
         loaded.root().label == "canonical window, mutated after load with a longer string",
         "ordinary mutation after load differs");
    auto again = Buffer::load_verified(loaded.save_verified(), policy());
    need(again.root().values.size() == 206 && again.root().values[100] == 94, "ordinary reload differs");
    std::vector<Storage> many;
    for (int i = 0; i < 64; ++i) {
        many.emplace_back(0, std::size_t(1 + i % 7) << 20);
        need(!touches_window(many.back().data(), many.back().reserved_bytes()), "ordinary storage touches the window range");
    }
}

// G1: growth zeroes only bytes that may be dirty; everything newly exposed
// must still read as zero.
bool all_zero(const char* p, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) if (p[i] != 0) return false;
    return true;
}
void g1_zeroing() {
    const std::size_t max = 1 << 20, n = 256 << 10, small = 1000;
    {   // shrink + regrow
        Storage s(0, max);
        need(s.grow_to(n), "grow failed");
        std::memset(s.data(), 0x5a, n);
        s.resize_logical(small);
        need(s.grow_to(n) && all_zero(s.data() + small, n - small) && s.data()[small - 1] == 0x5a,
             "shrink+regrow exposed stale bytes");
        need(s.grow_to(max) && all_zero(s.data() + small, max - small), "growth beyond old high-water mark not zero");
    }
    {   // pool reuse
        VmPoolGuard guard;
        xd::VmStoragePool pool({2, 4 * max, 4 * max});
        {
            Storage a(0, max, pool);
            need(a.grow_to(n), "pooled grow failed");
            std::memset(a.data(), 0x6b, n);
        }
        need(pool.stats().cached_blocks == 1, "pooled owner not cached");
        Storage b(0, max, pool);
        need(pool.stats().hits == 1, "pool reuse not exercised");
        need(b.grow_to(n) && all_zero(b.data(), n), "pool reuse exposed stale bytes");
        std::memset(b.data(), 0x6c, 64);
        b.resize_logical(0);
        need(b.grow_to(64) && all_zero(b.data(), 64), "pooled regrow exposed stale bytes");
    }
    {   // from_bytes then growth (partial and full copies)
        std::vector<char> input(5000, 'x');
        auto s = Storage::from_bytes(input.data(), input.size(), max);
        need(s.size() == input.size() && s.data()[4999] == 'x', "from_bytes contents differ");
        need(s.grow_to(n) && all_zero(s.data() + input.size(), n - input.size()), "growth after from_bytes not zero");
        s.resize_logical(10);
        need(s.grow_to(input.size()) && all_zero(s.data() + 10, input.size() - 10), "regrow over copied bytes not zero");
        std::vector<char> full(64 << 10, 'y');
        auto f = Storage::from_bytes(full.data(), full.size(), full.size());
        f.resize_logical(100);
        need(f.grow_to(full.size()) && all_zero(f.data() + 100, full.size() - 100), "full-copy regrow not zero");
    }
    {   // tail reclamation lowers the high-water mark
        Storage s(0, max);
        need(s.grow_to(max), "grow failed");
        std::memset(s.data(), 0x7d, max);
        s.resize_logical(small);
        auto result = s.reclaim_tail();
        need(result.complete(), "reclaim failed");
        need(s.grow_to(max) && all_zero(s.data() + small, max - small), "regrow after reclaim not zero");
    }
}

#if defined(XOFFSET_TEST_WINDOW_DISABLED)
static_assert(!Window::enabled);
void run() {
    auto empty = Storage::for_receive(0, 0);
    need(!empty.data() && !empty.in_canonical_window(), "empty receive storage changed");
    need(throws_bad_alloc([] { auto s = Storage::for_receive(16, 4096); (void)s; }),
         "disabled window did not reject receive storage");
    need(throws_bad_alloc([] { auto t = xo::begin_verified_receive<WindowRoot>(64, policy()); (void)t; }),
         "disabled window did not reject verified receive");
    need(xo::XBufferCore::canonical_bytes_at(nullptr, 0x580000000000ull, 1) == nullptr,
         "disabled window resolved an address");
    ordinary_outside_window();
    ordinary_full_lifecycle();
    g1_zeroing();
    need(xo::XBufferCore::canonical_bytes_at_internal(nullptr, 0x580000000000ull, 1) == nullptr,
         "disabled window internal resolver resolved an address");
    std::cout << "canonical window disabled: receive rejected, ordinary storage and G1 zeroing unaffected\n";
}
#elif defined(XOFFSET_TEST_WINDOW_SMALL)
static_assert(Window::enabled && Window::slots == 4);
void run() {
    const std::size_t g = Window::granule;
    need(Window::used_granules() == 0, "fresh small window not empty");
    std::vector<Storage> live;
    for (int i = 0; i < 4; ++i) live.push_back(Storage::for_receive(0, g));
    need(Window::used_granules() == 4, "four granule slots not accounted");
    for (auto& s : live) need(s.in_canonical_window() && in_window(s.data(), g), "slot outside small window");
    need(throws_bad_alloc([&] { auto s = Storage::for_receive(0, g); (void)s; }), "exhausted window accepted receive");
    need(throws_bad_alloc([&] { auto s = Storage::for_receive(0, 5 * g); (void)s; }), "oversized receive accepted");
    const void* freed = live[1].data();
    live[1] = Storage{};
    need(Window::used_granules() == 3, "release did not return the slot");
    need(throws_bad_alloc([&] { auto s = Storage::for_receive(0, 2 * g); (void)s; }),
         "fragmented window returned a non-contiguous slot");
    auto again = Storage::for_receive(0, g);
    need(again.data() == freed, "freed slot was not reused");
    live.clear();
    again = Storage{};
    need(Window::used_granules() == 0, "small window not empty after release");
    auto big = Storage::for_receive(0, 4 * g);
    need(addr(big.data()) == Window::base, "whole-window slot misplaced");
    big = Storage{};
    ordinary_outside_window();
    std::cout << "canonical window small: exhaustion, fragmentation and reuse passed\n";
}
#else
void boundaries() {
    auto* root = xd::canonical_window_array.load(std::memory_order_acquire);
    need(root && addr(root) == Window::base, "window array differs from base");
    using Core = xo::XBufferCore;
    need(Core::canonical_bytes_at(nullptr, Window::base - 1, 1) == nullptr, "below-window address resolved");
    need(Core::canonical_bytes_at(nullptr, Window::base, 1) == root, "window base not resolved");
    need(Core::canonical_bytes_at(nullptr, Window::base + Window::bytes, 0) == root + Window::bytes,
         "window one-past-end not resolved");
    need(Core::canonical_bytes_at(nullptr, Window::base + Window::bytes - 1, 2) == nullptr,
         "range crossing window end resolved");
    need(Core::canonical_bytes_at(nullptr, Window::base + Window::bytes, 1) == nullptr, "past-window address resolved");
    need(Core::canonical_bytes_at(nullptr, 0, 1) == nullptr && Core::canonical_bytes_at(nullptr, ~std::uintptr_t{0}, 1) == nullptr,
         "extreme addresses resolved");
    // With the window reserved, the load-free internal resolver is identical.
    const std::pair<std::uintptr_t, std::size_t> probes[] = {
        {Window::base - 1, 1}, {Window::base, 1}, {Window::base + 4096, 64}, {Window::base + Window::bytes, 0},
        {Window::base + Window::bytes - 1, 2}, {Window::base + Window::bytes, 1}, {0, 1}, {~std::uintptr_t{0}, 1}};
    for (auto [target, n] : probes)
        need(Core::canonical_bytes_at_internal(nullptr, target, n) == Core::canonical_bytes_at(nullptr, target, n),
             "internal and public resolvers differ while the window is reserved");
}

void storage_in_window_and_reuse() {
    const auto baseline = Window::used_granules();
    auto a = Storage::for_receive(100, 3 << 20);
    auto b = Storage::for_receive(10, 4096);
    need(a.in_canonical_window() && in_window(a.data(), a.maximum_capacity()) &&
         reinterpret_cast<char*>(a.canonical_byte_origin()) == a.data(), "receive storage outside window");
    need(a.canonical_byte_origin() == xd::canonical_window_array.load() + (addr(a.data()) - Window::base),
         "receive origin not derived from the window array");
    need(b.in_canonical_window() && in_window(b.data(), b.maximum_capacity()), "second receive outside window");
    need(addr(a.data()) + a.reserved_bytes() <= addr(b.data()) || addr(b.data()) + b.reserved_bytes() <= addr(a.data()),
         "receive slots overlap");
    need(a.accessible_bytes() == a.reserved_bytes() && a.size() == 100, "receive accessible extent changed");
    need(Window::used_granules() == baseline + 3 + 1, "granule accounting differs");
    const auto* old = a.data();
    const auto max = a.maximum_capacity();
    std::memset(a.data(), 0xab, max); // dirty the whole slot, including beyond logical
    a = Storage{};
    need(Window::used_granules() == baseline + 1, "release did not return granules");
    auto c = Storage::for_receive(0, max);
    need(c.data() == old, "first-fit did not reuse the freed slot");
    for (std::size_t i = 0; i < max; i += 1024)
        need(c.data()[i] == 0, "reused slot exposed stale bytes");
    // Moves keep the slot; pools never cache window slots and receive never
    // takes pooled ordinary storage.
    xd::VmStoragePool pool({4, 64u << 20, 64u << 20});
    {
        auto d = Storage::for_receive(0, 1 << 20, pool);
        need(d.in_canonical_window(), "pooled receive left the window");
    }
    need(pool.stats().cached_blocks == 0 && pool.stats().hits == 0 && pool.stats().misses == 0,
         "window slot interacted with a storage pool");
    {
        Storage ordinary(0, 1 << 20, pool);
    }
    need(pool.stats().cached_blocks == 1, "ordinary pooled storage not cached");
    auto e = Storage::for_receive(0, 1 << 20, pool);
    need(e.in_canonical_window() && pool.stats().cached_blocks == 1 && pool.stats().hits == 0,
         "receive took pooled ordinary storage");
    Storage moved(std::move(e));
    need(moved.in_canonical_window() && !e.in_canonical_window(), "move lost the window slot");
    // Tail reclamation stays inside the slot and keeps it a window slot.
    auto f = Storage::for_receive(16, 1 << 20);
    need(f.reclaim_tail().complete() && f.accessible_bytes() < f.reserved_bytes() && f.in_canonical_window(),
         "receive tail reclamation differs");
    need(f.grow_to(f.maximum_capacity()) && f.data()[f.maximum_capacity() - 1] == 0, "receive regrowth differs");
}

void live_equivalence(const std::string& input) {
    std::vector<Buffer> owners;
    for (int i = 0; i < 5; ++i) owners.push_back(receive(input));
    for (auto& b : owners) {
        check(b);
        const auto* base = static_cast<const char*>(b.get_address());
        need(in_window(base, b.maximum_capacity()), "received payload outside window");
        using Core = xo::XBufferCore;
        const auto values = addr(b.root().values.data());
        const std::pair<std::uintptr_t, std::size_t> probes[] = {
            {addr(base), 1}, {addr(base) + 64, 8}, {values, 6 * sizeof(std::uint32_t)},
            {addr(base) + b.maximum_capacity(), 0}, {addr(base) + b.maximum_capacity() - 1, 1},
            {addr(&b.root()), sizeof(WindowRoot)}};
        for (auto [target, n] : probes) {
            auto* hot = Core::canonical_bytes_at(&b.root(), target, n);
            auto* reg = Core::canonical_bytes_at_active(&b.root(), target, n);
            need(hot && hot == reg && addr(hot) == target, "window and registry recovery differ for a live record");
        }
        auto binding = b.root().values.binding();
        auto* p = xo::fixed_detail::allocate_dynamic_bytes(binding, 24, 8);
        xo::fixed_detail::retire_dynamic_bytes(binding, p, 24);
        auto* q = xo::fixed_detail::allocate_dynamic_bytes(binding, 24, 8);
        need(p == q, "window free-list reuse differs");
        xo::fixed_detail::retire_dynamic_bytes(binding, q, 24);
        b.root().values.push_back(17);
        need(b.root().values.size() == 7 && b.root().values[6] == 17, "received mutation differs");
    }
}

void concurrent_slots() {
    constexpr int threads = 6, rounds = 200;
    std::vector<std::thread> pool;
    std::vector<int> failures(threads, 0);
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([t, &failures] {
            try {
                for (int i = 0; i < rounds; ++i) {
                    auto s = Storage::for_receive(64, std::size_t(1 + (i + t) % 3) << 20);
                    if (!s.in_canonical_window()) { ++failures[t]; continue; }
                    std::memset(s.data(), t + 1, 64);
                    for (int k = 0; k < 64; ++k) if (s.data()[k] != char(t + 1)) ++failures[t];
                }
            } catch (...) { ++failures[t]; }
        });
    }
    for (auto& th : pool) th.join();
    for (int f : failures) need(f == 0, "concurrent window slot use failed");
}

bool window_untouched() {
    return !xd::canonical_window_array.load() && !xd::canonical_window_attempted.load();
}

// Force the kernel-chosen address into the window range while the window is
// not reserved: the unconditional overlap guard must move the reservation out
// and release the colliding mapping (PROT_NONE path and full-copy RW path).
void collision_relocates(const char* phase) {
    for (int full_copy = 0; full_copy != 2; ++full_copy) {
        const auto moved = xd::canonical_window_relocations.load();
        xd::fail_next_vm_operation(xd::VmOperation::WindowCollide);
        std::vector<char> input(1 << 20, 'z');
        Storage s = full_copy ? Storage::from_bytes(input.data(), input.size(), input.size())
                              : Storage(0, 1 << 20);
        need(xd::canonical_window_relocations.load() == moved + 1, phase);
        need(s.data() && !touches_window(s.data(), s.reserved_bytes()), "colliding ordinary storage not relocated");
        need(!full_copy || (s.size() == input.size() && s.data()[input.size() - 1] == 'z'), "relocated copy differs");
        Storage t(0, 1 << 20);
        need(!touches_window(t.data(), t.reserved_bytes()), "ordinary storage entered the window after collision");
    }
}

// W7: ordinary create/mutate/save/load/reload and raw storage never reserve
// the window (lazy reservation at the first receive) and never touch it.
void ordinary_only_leaves_window_unreserved(const std::string& input) {
    need(window_untouched(), "ordinary create/save reserved the window");
    {
        auto loaded = Buffer::load_verified(input, policy());
        check(loaded);
        loaded.root().values.push_back(77);
        auto again = Buffer::load_verified(loaded.save_verified(), policy());
        need(again.root().values.size() == 7 && again.root().values[6] == 77, "ordinary reload differs");
        Storage plain(0, 1 << 20);
        auto copy = Storage::from_bytes(input.data(), input.size(), 1 << 20);
        need(!touches_window(loaded.get_address(), loaded.maximum_capacity()) &&
             !touches_window(plain.data(), plain.reserved_bytes()) &&
             !touches_window(copy.data(), copy.reserved_bytes()), "ordinary storage touches the window range");
    }
    ordinary_full_lifecycle();
    need(window_untouched(), "ordinary load/storage reserved the window");
    collision_relocates("window collision hook was not exercised before the reservation");
    need(window_untouched(), "collision relocation reserved the window");
}

void reserve_failure_mode() {
    // Ordinary storage before the first receive: never reserves, never lands
    // in the window range.
    need(window_untouched(), "window touched before any VM storage existed");
    ordinary_only_leaves_window_unreserved(wire());
    // First window use in this process: inject the one-time reservation failure.
    need(!xd::canonical_window_array.load(), "window reserved before injection");
    xd::fail_next_vm_operation(xd::VmOperation::WindowReserve);
    need(throws_bad_alloc([] { auto s = Storage::for_receive(16, 4096); (void)s; }),
         "reservation failure did not reject receive");
    need(throws_bad_alloc([] { auto s = Storage::for_receive(16, 4096); (void)s; }),
         "reservation was retried after failure");
    need(throws_bad_alloc([] { auto t = xo::begin_verified_receive<WindowRoot>(64, policy()); (void)t; }),
         "verified receive accepted an unavailable window");
    need(xd::canonical_window_attempted.load() && !xd::canonical_window_array.load(),
         "failed reservation not final");
    need(xo::XBufferCore::canonical_bytes_at(nullptr, Window::base, 1) == nullptr &&
         xo::XBufferCore::canonical_bytes_at(nullptr, Window::base + Window::bytes, 0) == nullptr,
         "unreserved window resolved an address");
    ordinary_outside_window();
    ordinary_full_lifecycle();
    g1_zeroing();
    // After the failed reservation the guard still relocates colliding
    // mappings; the released range is free again, but a receive still fails
    // (the window attempt is final) and ordinary storage still avoids it.
    collision_relocates("window collision hook was not exercised after the failed reservation");
    need(throws_bad_alloc([] { auto s = Storage::for_receive(16, 4096); (void)s; }),
         "receive succeeded after a final failed reservation");
    const auto bytes = wire(); // ordinary create/save path still works
    need(!bytes.empty(), "ordinary save failed");
    std::cout << "canonical window reservation failure: receive rejected once and for all, ordinary unaffected\n";
}

// Default window size: 32 GiB on macOS (reservation cost scales with
// size there), 256 GiB on the other enabled platforms. 1 MiB granules.
#if defined(__APPLE__)
static_assert(Window::bytes == (32ull << 30) && Window::slots == 32768 && Window::granule == (1u << 20),
              "macOS default canonical window is 32 GiB of 1 MiB granules");
#else
static_assert(Window::bytes == (256ull << 30) && Window::granule == (1u << 20),
              "non-macOS default canonical window stays 256 GiB");
#endif

#if defined(__APPLE__)
// The 32 GiB macOS window holds exactly 512 receives at the default 64 MiB
// maximum capacity; the next one is rejected, and releases make room again.
void default_capacity_receives() {
    const std::size_t cap = xo::XBufferCore::DEFAULT_MAXIMUM_CAPACITY;
    static_assert(xo::XBufferCore::DEFAULT_MAXIMUM_CAPACITY == (64ull << 20));
    need(Window::used_granules() == 0, "capacity probe needs an empty window");
    std::vector<Storage> slots; slots.reserve(Window::bytes / cap);
    for (std::size_t i = 0; i < Window::bytes / cap; ++i) {
        slots.push_back(Storage::for_receive(0, cap));
        need(slots.back().in_canonical_window(), "default-capacity receive outside the window");
    }
    need(slots.size() == 512 && Window::used_granules() == Window::slots, "window did not hold 512 x 64 MiB");
    need(throws_bad_alloc([&] { auto s = Storage::for_receive(0, cap); (void)s; }),
         "513th default-capacity receive was accepted");
    slots.pop_back();
    auto again = Storage::for_receive(0, cap);
    need(again.in_canonical_window(), "released default-capacity slot was not reusable");
    again = Storage{}; slots.clear();
    need(Window::used_granules() == 0, "window not empty after capacity probe");
}
#endif

void run() {
    static_assert(Window::enabled);
    need(window_untouched(), "window touched before any VM storage existed");
    const auto input = wire();
    ordinary_only_leaves_window_unreserved(input);
    {   // Lazy reservation: the first receive reserves the window.
        auto first = receive(input);
        check(first);
        need(xd::canonical_window_array.load() && xd::canonical_window_attempted.load() &&
             in_window(first.get_address(), 1), "first receive did not reserve the window");
    }
    {   // While the window is reserved the kernel cannot honour the collision
        // hint; it may place the mapping right after the window (touching its
        // closed range), which the unconditional guard then relocates.
        const auto moved = xd::canonical_window_relocations.load();
        xd::fail_next_vm_operation(xd::VmOperation::WindowCollide);
        Storage s(0, 1 << 20);
        const auto relocated = xd::canonical_window_relocations.load() - moved;
        need(!s.in_canonical_window() && !touches_window(s.data(), s.reserved_bytes()) && relocated <= 1,
             "reserved window admitted ordinary storage");
    }
    {
        auto first = receive(input);
        check(first);
    }
    boundaries();
#if defined(__APPLE__)
    default_capacity_receives();
#endif
    storage_in_window_and_reuse();
    live_equivalence(input);
    concurrent_slots();
    ordinary_outside_window();
    ordinary_full_lifecycle();
    g1_zeroing();
    std::cout << "canonical window: lazy reservation at first receive, placement, equivalence, reuse, pool isolation, concurrency, G1 passed\n";
}
#endif
} // namespace

int main(int argc, char** argv) {
    try {
#if !defined(XOFFSET_TEST_WINDOW_SMALL) && !defined(XOFFSET_TEST_WINDOW_DISABLED)
        if (argc == 2 && std::string_view(argv[1]) == "--reserve-failure") { reserve_failure_mode(); return 0; }
#endif
        if (argc != 1) throw std::runtime_error("unsupported arguments");
        (void)argv;
        run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
