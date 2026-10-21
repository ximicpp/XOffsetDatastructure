// Stable host owner identity (V2: the host page before the payload names the
// owner) and callback-safe allocator bookkeeping. No timer.
#define XOFFSET_OWNER_LOOKUP_DIAGNOSTICS 1
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>
#if defined(_WIN32)
#include <malloc.h>
#endif
#include "xoffsetdatastructure.hpp"

namespace host {
std::atomic<std::size_t> allocations{0}, live{0};
thread_local int fail_after = -1;
thread_local unsigned calls = 0;
thread_local void (*callback)() = nullptr;
thread_local bool capture_next = false;
thread_local std::size_t capture_size = 0;
thread_local void* captured = nullptr;
thread_local void* delete_address = nullptr;
thread_local unsigned delete_calls = 0;
using DeleteCallback = void (*)() noexcept;
thread_local DeleteCallback delete_callback = nullptr;
DeleteCallback take_delete_callback(void* p) noexcept {
    if (p && p == delete_address) {
        delete_address = nullptr; // callback may allocate/free/reenter recursively
        ++delete_calls;
        return delete_callback;
    }
    return nullptr;
}
void handler() { ++calls; if (callback) callback(); }
// alignment 0: ordinary new (malloc, released by free). Any aligned new uses the
// aligned allocator, so release_aligned matches it (_aligned_free on Windows).
void* allocate(std::size_t n, std::size_t alignment = 0) {
    for (;;) {
        bool fail = fail_after == 0;
        if (fail) fail_after = -1;
        else if (fail_after > 0) --fail_after;
        void* p = nullptr;
        if (!fail) {
            if (alignment == 0) p = std::malloc(n ? n : 1);
            else {
                const auto bytes = ((n ? n : 1) + alignment - 1) / alignment * alignment;
#if defined(_WIN32)
                p = _aligned_malloc(bytes, alignment);
#else
                p = std::aligned_alloc(alignment, bytes);
#endif
            }
        }
        if (p) {
            ++allocations; ++live;
            if (capture_next && (!capture_size || capture_size == n)) {
                captured = p; capture_next = false;
            }
            return p;
        }
        if (auto fn = std::get_new_handler()) fn(); else throw std::bad_alloc();
    }
}
void release(void* p) noexcept {
    if (p) {
        auto after = take_delete_callback(p);
        --live; std::free(p);
        if (after) after();
    }
}
void release_aligned(void* p) noexcept {
    if (p) {
        auto after = take_delete_callback(p);
        --live;
#if defined(_WIN32)
        _aligned_free(p);
#else
        std::free(p);
#endif
        if (after) after();
    }
}
}
void* operator new(std::size_t n) { return host::allocate(n); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { host::release(p); }
void operator delete[](void* p) noexcept { host::release(p); }
void operator delete(void* p, std::size_t) noexcept { host::release(p); }
void operator delete[](void* p, std::size_t) noexcept { host::release(p); }
void* operator new(std::size_t n, std::align_val_t a) { return host::allocate(n, std::size_t(a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void operator delete(void* p, std::align_val_t) noexcept { host::release_aligned(p); }
void operator delete[](void* p, std::align_val_t) noexcept { host::release_aligned(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { host::release_aligned(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { host::release_aligned(p); }


namespace xo = XOffsetDatastructure;
namespace fd = xo::fixed_detail;
namespace dt = xo::detail;
struct OwnerRoot { xo::XVector<std::uint32_t> numbers; xo::XString name; };
XOFFSET_REGISTER_SCHEMA_NAME(OwnerRoot, "test.v5.OwnerIdentity.v1")
struct OtherOwnerRoot { xo::XString other; };
XOFFSET_REGISTER_SCHEMA_NAME(OtherOwnerRoot, "test.v5.OtherOwnerIdentity.v1")

namespace {
void need(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
auto policy() { return xo::XBuffer::maximum_capacity(4u << 20).with_growth(xo::XGrowPolicy::Automatic); }
auto make_buffer(std::size_t initial = 4096) { return xo::XBuffer::create<OwnerRoot>(initial, policy()); }
auto binding(xo::XBuffer& buffer) { return buffer.root<OwnerRoot>().numbers.binding(); }
// V2 owner lookup: one read of the host page preceding the payload base.
xo::XBufferCore* owner_of(const fd::allocator_ref& ref) {
    return xo::XBufferCore::owner_from_arena_noexcept(ref.segment);
}
bool in_payload(const xo::XBufferCore& owner, const void* p, std::size_t bytes) {
    const auto base = reinterpret_cast<std::uintptr_t>(owner.get_address());
    const auto at = reinterpret_cast<std::uintptr_t>(p);
    return owner.get_address() && at >= base && at - base <= owner.get_size() &&
           bytes <= owner.get_size() - (at - base);
}
bool registry_and_tls_untouched() {
    const auto& s = dt::owner_lookup_stats;
    return s.entries == 0 && s.cache_hits == 0 && s.cold_lookups == 0 && s.dead_records == 0 &&
           s.slow_revalidations == 0;
}
xo::XBuffer* source = nullptr;
xo::XBuffer* target = nullptr;
xo::XBuffer* auxiliary = nullptr;
fd::allocator_ref active{};
std::array<void*, 2> reentrant_freed{};
enum class Action { move, swap, release, reenter, reclaim_reenter, register_reenter };
Action action{};
void callback() {
    switch (action) {
    case Action::move: *target = std::move(*source); break;
    case Action::swap: source->swap(*target); break;
    case Action::release: *source = xo::XBuffer{}; break;
    case Action::reenter: {
        auto* p = fd::allocate_dynamic_bytes(active, 24, 8);
        std::memset(p, 0x31, 24); fd::retire_dynamic_bytes(active, p, 24); break;
    }
    case Action::reclaim_reenter: {
        auto* a = active.segment->allocate_aligned(24, 8);
        auto* b = active.segment->allocate_aligned(32, 8);
        reentrant_freed = {a, b};
        active.segment->deallocate(a, 24); active.segment->deallocate(b, 32); break;
    }
    case Action::register_reenter:
        auxiliary->root<OwnerRoot>().numbers.resize(9, 72u);
        *target = make_buffer();
        break;
    }
}
struct HandlerScope {
    std::new_handler old;
    explicit HandlerScope(Action which) : old(std::set_new_handler(host::handler)) {
        action = which; host::callback = callback; host::calls = 0;
    }
    ~HandlerScope() {
        host::fail_after = -1; host::callback = nullptr; std::set_new_handler(old);
    }
};

fd::allocator_ref delete_binding{};
xo::XBuffer* delete_expected_owner = nullptr;
bool delete_ok = false;
std::uint32_t delete_expected_word = 0;
void check_delete_owner() noexcept {
    delete_ok = false;
    try {
        auto* owner = owner_of(delete_binding);
        if (owner != static_cast<xo::XBufferCore*>(delete_expected_owner) ||
            static_cast<void*>(owner->data()) != static_cast<void*>(delete_binding.segment)) return;
        auto& root = delete_expected_owner->root<OwnerRoot>();
        if (root.numbers.empty() || root.numbers[0] != delete_expected_word) return;
        auto* p = fd::allocate_dynamic_bytes(delete_binding, 24, 8);
        std::memset(p, 0x61, 24); fd::retire_dynamic_bytes(delete_binding, p, 24);
        delete_ok = true;
    } catch (...) { delete_ok = false; }
}
void begin_capture(std::size_t matching_bytes = 0) {
    host::captured = nullptr; host::capture_size = matching_bytes; host::capture_next = true;
}
void arm_delete(void* p, void (*fn)() noexcept) {
    need(p != nullptr, "delete callback target was not captured");
    host::delete_address = p; host::delete_callback = fn; host::delete_calls = 0; delete_ok = false;
}
void disarm_delete() noexcept { host::delete_address = nullptr; host::delete_callback = nullptr; }

void hot_lookup_is_nonallocating_and_does_not_change_payload() {
    auto buffer = make_buffer(); auto a = binding(buffer);
    need(owner_of(a) == &buffer, "host page does not name the owner");
    const auto bytes = buffer.copy_bytes();
    dt::reset_owner_lookup_stats();
    const auto before = host::allocations.load(), live = host::live.load();
    host::fail_after = 0;
    for (unsigned i = 0; i != 4096; ++i)
        need(&xo::XBufferCore::owner_from_arena(a.segment) == &buffer, "hot lookup returned another owner");
    need(host::fail_after == 0, "hot lookup attempted host allocation"); host::fail_after = -1;
    need(host::allocations.load() == before && host::live.load() == live, "hot lookup allocated or released");
    need(registry_and_tls_untouched(), "V2 owner lookup used the TLS cache/registry path");
    need(buffer.copy_bytes() == bytes, "host identity changed persisted bytes");
    // Allocation and retirement resolve the owner the same way.
    auto* warm = fd::allocate_dynamic_bytes(a, 24, 8); fd::retire_dynamic_bytes(a, warm, 24);
    dt::reset_owner_lookup_stats();
    for (unsigned i = 0; i != 64; ++i) {
        auto* p = fd::allocate_dynamic_bytes(a, 24, 8);
        need(p == warm, "exact-size free-table reuse differs");
        fd::retire_dynamic_bytes(a, p, 24);
    }
    need(registry_and_tls_untouched(), "allocate/retire consulted the TLS cache/registry path");
}

void lookup_ignores_other_owners_and_registration_failures() {
    auto seed = make_buffer(); auto ref = binding(seed);
    std::array<xo::XBuffer, 6> others;
    for (auto& item : others) item = make_buffer(); // (V1: evicted seed from its bounded TLS cache)
    // V2 retirement records the block in the host-side free table; warm the
    // 32-byte bin so the measured allocate/retire pair needs no host memory.
    auto* warm = fd::allocate_dynamic_bytes(ref, 32, 8); fd::retire_dynamic_bytes(ref, warm, 32);
    dt::reset_owner_lookup_stats(); const auto before = host::allocations.load();
    host::fail_after = 0; dt::fail_next_registry_registration();
    auto* raw = fd::allocate_dynamic_bytes(ref, 32, 8);
    fd::retire_dynamic_bytes(ref, raw, 32);
    need(host::fail_after == 0 && host::allocations.load() == before, "owner lookup allocated");
    host::fail_after = -1;
    need(raw == warm && seed.free_table_complete() && seed.free_block_count() == 1, "free-table reuse differs");
    need(registry_and_tls_untouched(), "owner lookup consulted the registry");
    bool caught = false; try { (void)make_buffer(); } catch (const xo::XBadAlloc&) { caught = true; }
    dt::clear_registry_failures(); need(caught, "lookup consumed registration-only failure");
    for (int point : {0, 1}) {
        host::fail_after = point; caught = false;
        try { (void)make_buffer(); } catch (const xo::XBadAlloc&) { caught = true; }
        host::fail_after = -1;
        need(caught, "record/node registration host failure not exercised");
        auto* p = fd::allocate_dynamic_bytes(ref, 24, 8); fd::retire_dynamic_bytes(ref, p, 24);
        need(owner_of(ref) == &seed, "failed registration changed an existing owner");
    }
}

void transfers_and_deterministic_reused_payload_base() {
    auto first = make_buffer(), second = make_buffer(); auto ref = binding(first);
    first.root<OwnerRoot>().numbers.resize(3, 91u);
    auto* payload = first.data(); const auto before = host::allocations.load();
    xo::XBuffer moved(std::move(first));
    need(owner_of(ref) == &moved, "move construction did not publish the new owner");
    moved.swap(second);
    need(owner_of(ref) == &second && owner_of(binding(moved)) == &moved, "swap did not publish both owners");
    first = std::move(second); first.swap(first); first = std::move(first);
    need(host::allocations.load() == before, "move/swap allocated owner identity");
    need(first.data() == payload && first.root<OwnerRoot>().numbers[0] == 91, "move/swap routed payload incorrectly");
    need(owner_of(ref) == &first, "owner slot does not name the final owner");
    auto* p = fd::allocate_dynamic_bytes(ref, 24, 8); fd::retire_dynamic_bytes(ref, p, 24);
    need(first.free_block_count() == 1, "retirement reached a stale owner");
    xo::XBufferPool pool({1, 4u << 20, 4u << 20});
    const auto cap = policy().with_pool(pool);
    auto reused = xo::XBuffer::create<OwnerRoot>(4096, cap);
    const auto address = reused.data(); need(owner_of(binding(reused)) == &reused, "pooled owner not published");
    reused = xo::XBuffer{};
    need(xo::XBufferCore::owner_from_arena_noexcept(address) == nullptr, "pooled reservation retained a dead owner");
    reused = xo::XBuffer::create<OwnerRoot>(4096, cap);
    need(reused.data() == address, "pool did not exercise exact base reuse");
    auto again = binding(reused);
    need(owner_of(again) == &reused, "reused base resolved to a stale owner");
    p = fd::allocate_dynamic_bytes(again, 24, 8);
    std::memset(p, 0x57, 24); fd::retire_dynamic_bytes(again, p, 24);
    need(reused.free_block_count() == 1, "reused-base retirement reached a stale owner");
    // A frozen snapshot is immutable: its payload names no allocation owner.
    auto typed = xo::TypedXBuffer<OwnerRoot>::create(4096, policy());
    typed.root().numbers.resize(2, 5u);
    const void* base = typed.data();
    auto frozen = xo::freeze_verified(std::move(typed));
    need(static_cast<const void*>(frozen.segments()[1].data()) == base &&
         xo::XBufferCore::owner_from_arena_noexcept(base) == nullptr, "frozen payload still names an owner");
}

void callback_during_allocation_bookkeeping() {
    // G: a failed no-growth attempt constructs no diagnostic, and growth itself
    // reaches no host allocation, so new_handler has no callback point.
    {
        auto from = make_buffer(); from.root<OwnerRoot>().numbers.resize(3, 29u); auto ref = binding(from);
        source = &from; target = &from; auxiliary = &from; active = ref;
        HandlerScope handler(Action::reenter); host::fail_after = 0;
        void* raw = fd::allocate_dynamic_bytes(ref, 65536, 8);
        need(host::calls == 0 && host::fail_after == 0, "growth probe reached a host allocation/new_handler");
        host::fail_after = -1;
        need(raw && from.root<OwnerRoot>().numbers[0] == 29, "growth probe allocation failed");
        std::memset(raw, 0x43, 65536); fd::retire_dynamic_bytes(ref, raw, 65536);
    }
    // V2's allocation-path host request (V1: release_cached_blocks' host
    // reservation): splitting a larger free block pushes its remainder into
    // the host free table. That new_handler may transfer, release or reenter
    // the owner; the allocation must commit to, and return memory of, the
    // actual payload owner, or fail closed once that payload is released.
    for (Action which : {Action::move, Action::swap, Action::release, Action::reenter}) {
        auto from = make_buffer(), to = make_buffer(), unrelated = make_buffer();
        from.root<OwnerRoot>().numbers.resize(3, 29u); auto ref = binding(from);
        auto* larger = fd::allocate_dynamic_bytes(ref, 48, 8); fd::retire_dynamic_bytes(ref, larger, 48);
        need(from.free_block_count() == 1 && from.free_block_bytes() == 48, "fixture free block missing");
        source = &from; target = &to; auxiliary = &unrelated; active = ref;
        void* raw = nullptr; bool caught = false;
        {
            HandlerScope handler(which); host::fail_after = 0;
            try { raw = fd::allocate_dynamic_bytes(ref, 32, 8); }
            catch (const xo::XException&) { caught = true; }
            need(host::calls == 1, "split-remainder new_handler not exercised");
        }
        if (which == Action::release) {
            need(caught && from.data() == nullptr, "released payload identity revived/read");
        } else {
            auto& current = which == Action::move || which == Action::swap ? to : from;
            need(!caught && raw && in_payload(current, raw, 32) && current.root<OwnerRoot>().numbers[0] == 29,
                 "allocation after callback owner transfer");
            need(raw == larger, "split did not hand out the front of the larger free block");
            std::memset(raw, 0x43, 32); fd::retire_dynamic_bytes(ref, raw, 32);
            const std::uint64_t expected = which == Action::reenter ? 72 : 48;
            need(current.free_table_complete() && current.free_block_bytes() == expected,
                 "split remainder lost or committed to a stale owner");
            auto loaded = xo::XBuffer::load_verified<OwnerRoot>(current.save_verified<OwnerRoot>(), policy());
            need(loaded.root<OwnerRoot>().numbers[2] == 29, "post-callback owner failed verified round trip");
        }
        dt::clear_vm_failures();
    }
}

void callback_during_free_table_retirement() {
    // V1 Core reclamation reserved a host vector; V2 retirement creates/grows
    // the host free table. That new_handler may transfer, release or reenter.
    for (Action which : {Action::move, Action::swap, Action::release, Action::reclaim_reenter}) {
        auto from = make_buffer(), to = make_buffer(); auto ref = binding(from);
        auto* p = ref.segment->allocate_aligned(32, 8);
        need(from.free_block_count() == 0, "fixture already has a free table entry");
        source = &from; target = &to; active = ref;
        {
            HandlerScope handler(which); host::fail_after = 0;
            ref.segment->deallocate(p, 32); // the first retirement creates the host table
            need(host::calls == 1, "free-table retirement new_handler not exercised");
        }
        if (which == Action::release) { need(from.data() == nullptr, "retirement revived released owner"); continue; }
        auto& current = which == Action::move || which == Action::swap ? to : from;
        need(static_cast<void*>(current.data()) == static_cast<void*>(ref.segment), "retirement payload transfer");
        auto* q = ref.segment->allocate_aligned(32, 8);
        if (which == Action::reclaim_reenter) {
            auto* second = ref.segment->allocate_aligned(32, 8);
            need((q == p && second == reentrant_freed[1]) || (q == reentrant_freed[1] && second == p),
                 "outer or reentrant retired block lost after reentry");
            auto* third = ref.segment->allocate_aligned(24, 8);
            need(third == reentrant_freed[0], "reentrant 24-byte retirement lost after outer retirement");
            ref.segment->deallocate(third, 24); ref.segment->deallocate(second, 32);
        } else {
            need(q == p, "retirement committed into stale Core instead of actual payload owner");
        }
        ref.segment->deallocate(q, 32);
        need(current.free_table_complete(), "retirement left the actual owner's free table incomplete");
        auto loaded = xo::XBuffer::load_verified<OwnerRoot>(current.save_verified<OwnerRoot>(), policy());
        (void)loaded.root<OwnerRoot>();
    }
}

void free_table_host_oom_is_recorded_and_recomputed() {
    // V2 replacement of V1's best-effort Core cache under host OOM: a lost
    // retirement marks the host table incomplete and saving recomputes holes.
    auto b = make_buffer(); auto ref = binding(b);
    auto* p = fd::allocate_dynamic_bytes(ref, 24, 8);
    host::fail_after = 0; fd::retire_dynamic_bytes(ref, p, 24); host::fail_after = -1;
    need(!b.free_table_complete(), "host OOM during retirement was not recorded");
    const auto wire = b.save_verified<OwnerRoot>();
    need(b.free_table_complete() && b.free_block_count() == 1 && b.free_block_bytes() == 24,
         "save did not recompute the lost hole");
    auto loaded = xo::XBuffer::load_verified<OwnerRoot>(wire, policy());
    need(loaded.free_block_count() == 1 && loaded.free_block_bytes() == 24, "recomputed hole not on the wire");
    auto* again = fd::allocate_dynamic_bytes(ref, 24, 8);
    need(again == p, "recomputed hole not reusable");
    fd::retire_dynamic_bytes(ref, again, 24);
}

void registry_new_handler_reentry_is_unlocked() {
    auto other = make_buffer(), nested = make_buffer(); auxiliary = &other; target = &nested;
    for (int point : {0, 1}) {
        HandlerScope handler(Action::register_reenter); host::fail_after = point;
        auto b = make_buffer();
        need(host::calls == 1 && other.root<OwnerRoot>().numbers.size() == 9,
             "registration node/control allocation reentry not exercised");
        auto ref = binding(b); auto* p = fd::allocate_dynamic_bytes(ref, 24, 8); fd::retire_dynamic_bytes(ref, p, 24);
        need(owner_of(ref) == &b && owner_of(binding(nested)) == &nested, "reentrant registration owner differs");
    }
}

void delete_callbacks_observe_complete_move() {
    // Target's actual free-table allocation is captured, not a guessed size or
    // the earlier registry-node delete. Its delete fires in the move gap.
    {
        auto from = make_buffer();
        auto to = xo::XBuffer::create<OtherOwnerRoot>(4096, policy());
        from.root<OwnerRoot>().numbers.resize(3, 313u);
        auto* p = to.arena()->allocate_aligned(32, 8);
        begin_capture(); to.arena()->deallocate(p, 32);
        need(!host::capture_next, "target free-table allocation not captured");
        auto* old_table = host::captured;
        delete_binding = binding(from); delete_expected_owner = &to; delete_expected_word = 313;
        const auto from_epoch = from.epoch(), to_epoch = to.epoch();
        arm_delete(old_table, check_delete_owner); to = std::move(from);
        need(host::delete_calls == 1 && delete_ok, "delete callback saw partial Core/schema/free-table transfer");
        disarm_delete();
        need(to.epoch() == to_epoch + 1 && from.epoch() == from_epoch + 1, "move epoch changed more than once");
        need(to.root<OwnerRoot>().numbers[0] == 313, "incoming schema not transferred");
    }
    // A sole pool State is another callback boundary in VM move assignment.
    // Capture its preallocated owner-slot vector, then drop every external pool
    // handle. Destruction must wait until the incoming Core is fully published.
    {
        auto from = make_buffer(); from.root<OwnerRoot>().numbers.resize(3, 419u);
        xo::XBuffer to;
        begin_capture(sizeof(dt::VirtualMemoryStorage));
        {
            xo::XBufferPool pool({1, 4u << 20, 4u << 20});
            to = xo::XBuffer::create<OtherOwnerRoot>(4096, policy().with_pool(pool));
        }
        need(!host::capture_next, "pool's reserved owner-vector not captured");
        auto* pool_vector = host::captured;
        delete_binding = binding(from); delete_expected_owner = &to; delete_expected_word = 419;
        arm_delete(pool_vector, check_delete_owner); to = std::move(from);
        need(host::delete_calls == 1 && delete_ok, "pool delete callback saw partial Core transfer");
        disarm_delete();
    }
}

void alias_roundtrip_and_thread_exit() {
    auto a = make_buffer(), b = make_buffer(); const std::array<std::uint32_t,4> words{5,9,13,17};
    auto& root = a.root<OwnerRoot>(); root.numbers.assign_range(std::span<const std::uint32_t>(words));
    root.numbers.assign_range({root.numbers.data()+1, 3}); root.numbers.resize(23, root.numbers[0]);
    b.root<OwnerRoot>().numbers.assign_range({root.numbers.data(), root.numbers.size()});
    root.name.assign("cross-arena source"); b.root<OwnerRoot>().name.assign(root.name.view());
    auto wire = b.save_verified<OwnerRoot>(); auto loaded = xo::XBuffer::load_verified<OwnerRoot>(wire, policy());
    need(loaded.root<OwnerRoot>().numbers.size() == 23 && loaded.root<OwnerRoot>().numbers[22] == 9 &&
         loaded.root<OwnerRoot>().name == "cross-arena source", "alias/cross-arena reload contents");
    std::array<std::thread, 8> workers;
    std::atomic<unsigned> misrouted{0};
    for (unsigned id=0; id!=workers.size(); ++id) workers[id]=std::thread([id, &misrouted] {
        for (unsigned n=0;n!=64;++n) {
            auto x=make_buffer(), y=make_buffer(); auto ref=binding(x); x.swap(y);
            if (owner_of(ref) != &y) ++misrouted;
            auto* p=fd::allocate_dynamic_bytes(ref,24,8); std::memset(p,int(id),24);fd::retire_dynamic_bytes(ref,p,24);
            if (y.free_block_count() != 1) ++misrouted;
        }
    });
    for (auto& thread : workers) thread.join();
    need(misrouted.load() == 0, "concurrent swaps misrouted host-page owner lookup");
    // V2 lookups create no host records (V1 bounded them to four inert TLS
    // records); registry nodes are erased as owners leave scope.
    for (unsigned i=0;i!=32;++i) { auto x=make_buffer(); (void)xo::XBufferCore::owner_from_arena(binding(x).segment); }
    auto baseline=host::live.load();
    for (unsigned i=0;i!=512;++i) { auto x=make_buffer(); (void)xo::XBufferCore::owner_from_arena(binding(x).segment); }
    need(host::live.load() <= baseline, "dead host records accumulated across owner lifetimes");
    static_assert(sizeof(xo::XFixedString)==16 && sizeof(xo::XFixedVector<std::uint32_t>)==16);
    static_assert(sizeof(xo::XBufferCore::arena_header)==48);
}

xo::XBuffer late_owner;
bool late_armed = false;
struct LateCheck {
    ~LateCheck() {
        if (!late_armed) return;
        // After main's thread-locals are destroyed: the V2 allocation path
        // reads only the host page, never a destroyed TLS cache.
        try {
            auto ref=binding(late_owner);
            need(owner_of(ref) == &late_owner, "late global destructor resolved another owner");
            auto* p=fd::allocate_dynamic_bytes(ref,24,8);std::memset(p,0x17,24);fd::retire_dynamic_bytes(ref,p,24);
            auto loaded=xo::XBuffer::load_verified<OwnerRoot>(late_owner.save_verified<OwnerRoot>(), policy());
            need(loaded.root<OwnerRoot>().numbers[2]==617, "late global round trip differs");
        } catch (...) { std::abort(); }
    }
} late_check;
}

int main() {
    struct Group { const char* name; void (*run)(); };
    const Group groups[] = {
        {"hot lookup", hot_lookup_is_nonallocating_and_does_not_change_payload},
        {"other owners/registration failures", lookup_ignores_other_owners_and_registration_failures},
        {"transfers/reused base/freeze", transfers_and_deterministic_reused_payload_base},
        {"allocation bookkeeping callbacks", callback_during_allocation_bookkeeping},
        {"free-table retirement callbacks", callback_during_free_table_retirement},
        {"free-table host OOM", free_table_host_oom_is_recorded_and_recomputed},
        {"registry new_handler reentry", registry_new_handler_reentry_is_unlocked},
        {"alias/thread exit", alias_roundtrip_and_thread_exit},
        {"delete callbacks", delete_callbacks_observe_complete_move},
    };
    // Every group runs, so one failing contract does not hide the others.
    unsigned failed = 0;
    for (const auto& group : groups) {
        try { group.run(); }
        catch (const std::exception& e) { ++failed; std::cerr << group.name << ": " << e.what() << '\n'; }
        host::fail_after = -1; host::callback = nullptr; disarm_delete();
    }
    if (failed) return 1;
    try {
        late_owner=make_buffer(); late_owner.root<OwnerRoot>().numbers.resize(3, 617u);
        late_armed=true;
        std::cout << "9 owner identity groups passed; late global destructor armed; Core bytes "
                  << sizeof(xo::XBufferCore) << "; no timing\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
