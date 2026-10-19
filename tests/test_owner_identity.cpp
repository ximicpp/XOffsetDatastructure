// Stable host owner records and callback-safe reclamation. No timer.
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
void* allocate(std::size_t n, std::size_t alignment = alignof(std::max_align_t)) {
    for (;;) {
        bool fail = fail_after == 0;
        if (fail) fail_after = -1;
        else if (fail_after > 0) --fail_after;
        void* p = nullptr;
        if (!fail) {
            if (alignment <= alignof(std::max_align_t)) p = std::malloc(n ? n : 1);
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
xo::XBuffer* source = nullptr;
xo::XBuffer* target = nullptr;
xo::XBuffer* auxiliary = nullptr;
fd::allocator_ref active{};
void* pending_retire = nullptr;
std::array<void*, 2> reentrant_freed{};
enum class Action { move, swap, release, reenter, retire, reclaim_reenter, register_reenter };
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
    case Action::retire: fd::retire_dynamic_bytes(active, pending_retire, 32); break;
    case Action::reclaim_reenter: {
        auto* a = active.segment->allocate_aligned(24, 8);
        auto* b = active.segment->allocate_aligned(32, 8);
        reentrant_freed = {a, b};
        active.segment->deallocate(a); active.segment->deallocate(b); break;
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
xo::XBuffer* delete_move_source = nullptr;
xo::XBuffer* delete_move_target = nullptr;
bool delete_ok = false, delete_require_cold = false;
std::uint32_t delete_expected_word = 0;
void check_delete_owner() noexcept {
    delete_ok = false;
    try {
        const auto cold_before = dt::owner_lookup_stats.cold_lookups;
        auto& owner = delete_binding.segment->freelist_runtime_owner();
        if (&owner != static_cast<xo::XBufferCore*>(delete_expected_owner) ||
            static_cast<void*>(owner.data()) != static_cast<void*>(delete_binding.segment)) return;
        auto& root = delete_expected_owner->root<OwnerRoot>();
        if (root.numbers.empty() || root.numbers[0] != delete_expected_word) return;
        auto* p = fd::allocate_dynamic_bytes(delete_binding, 24, 8);
        std::memset(p, 0x61, 24); fd::retire_dynamic_bytes(delete_binding, p, 24);
        if (delete_require_cold && dt::owner_lookup_stats.cold_lookups <= cold_before) return;
        delete_ok = true;
    } catch (...) { delete_ok = false; }
}
void move_during_delete_then_check() noexcept {
    *delete_move_target = std::move(*delete_move_source);
    delete_expected_owner = delete_move_target;
    check_delete_owner();
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
    a.segment->freelist_runtime_owner();
    const auto bytes = buffer.copy_bytes();
    dt::reset_owner_lookup_stats(); const auto before = host::allocations.load();
    host::fail_after = 0;
    for (unsigned i = 0; i != 4096; ++i) a.segment->freelist_runtime_owner();
    need(host::fail_after == 0, "hot lookup attempted host allocation"); host::fail_after = -1;
    need(host::allocations.load() == before, "hot lookup allocated");
    need(dt::owner_lookup_stats.entries == 4096 && dt::owner_lookup_stats.cache_hits == 4096 &&
         dt::owner_lookup_stats.cold_lookups == 0, "hot lookup retained registry mutex path");
    need(buffer.copy_bytes() == bytes, "host identity changed persisted bytes");
}

void cold_lookup_pending_registration_and_registration_oom() {
    auto seed = make_buffer(); auto ref = binding(seed);
    std::array<xo::XBuffer, 6> others;
    for (auto& item : others) item = make_buffer(); // evict seed from bounded cache
    dt::reset_owner_lookup_stats(); const auto before = host::allocations.load();
    host::fail_after = 0; dt::fail_next_registry_registration();
    auto* raw = fd::allocate_dynamic_bytes(ref, 32, 8);
    fd::retire_dynamic_bytes(ref, raw, 32);
    need(host::fail_after == 0 && host::allocations.load() == before, "cold lookup allocated");
    host::fail_after = -1;
    need(dt::owner_lookup_stats.cold_lookups == 1, "cold lookup not exercised");
    bool caught = false; try { (void)make_buffer(); } catch (const xo::XBadAlloc&) { caught = true; }
    dt::clear_registry_failures(); need(caught, "lookup consumed registration-only failure");
    for (int point : {0, 1}) {
        host::fail_after = point; caught = false;
        try { (void)make_buffer(); } catch (const xo::XBadAlloc&) { caught = true; }
        host::fail_after = -1;
        need(caught, "record/node registration host failure not exercised");
        auto* p = fd::allocate_dynamic_bytes(ref, 24, 8); fd::retire_dynamic_bytes(ref, p, 24);
    }
}

void transfers_and_deterministic_reused_payload_base() {
    auto first = make_buffer(), second = make_buffer(); auto ref = binding(first);
    first.root<OwnerRoot>().numbers.resize(3, 91u);
    auto* payload = first.data(); const auto before = host::allocations.load();
    xo::XBuffer moved(std::move(first)); moved.swap(second);
    first = std::move(second); first.swap(first); first = std::move(first);
    need(host::allocations.load() == before, "move/swap allocated owner identity");
    need(first.data() == payload && first.root<OwnerRoot>().numbers[0] == 91, "move/swap routed payload incorrectly");
    auto* p = fd::allocate_dynamic_bytes(ref, 24, 8); fd::retire_dynamic_bytes(ref, p, 24);
    xo::XBufferPool pool({1, 4u << 20, 4u << 20});
    const auto cap = policy().with_pool(pool);
    auto reused = xo::XBuffer::create<OwnerRoot>(4096, cap);
    const auto address = reused.data(); binding(reused).segment->freelist_runtime_owner();
    reused = xo::XBuffer{};
    dt::reset_owner_lookup_stats();
    reused = xo::XBuffer::create<OwnerRoot>(4096, cap);
    need(reused.data() == address, "pool did not exercise exact base reuse");
    auto again = binding(reused); p = fd::allocate_dynamic_bytes(again, 24, 8);
    std::memset(p, 0x57, 24); fd::retire_dynamic_bytes(again, p, 24);
    need(dt::owner_lookup_stats.dead_records != 0, "old record not observed inert at reused base");
}

void callback_during_allocation_diagnostic() {
    for (Action which : {Action::move, Action::swap, Action::release, Action::reenter}) {
        auto from = make_buffer(), to = make_buffer(), unrelated = make_buffer();
        from.root<OwnerRoot>().numbers.resize(3, 29u); auto ref = binding(from);
        source = &from; target = &to; auxiliary = &unrelated; active = ref;
        HandlerScope handler(which); host::fail_after = 0;
        void* raw = nullptr; bool caught = false;
        try { raw = fd::allocate_dynamic_bytes(ref, 65536, 8); }
        catch (const xo::XException&) { caught = true; }
        need(host::calls == 1, "allocation diagnostic new_handler not exercised");
        if (which == Action::release) {
            need(caught && from.data() == nullptr, "released payload identity revived/read");
        } else {
            auto& current = which == Action::move || which == Action::swap ? to : from;
            need(!caught && raw && current.root<OwnerRoot>().numbers[0] == 29, "allocation after callback owner transfer");
            std::memset(raw, 0x43, 65536); fd::retire_dynamic_bytes(ref, raw, 65536);
        }
        dt::clear_vm_failures();
    }
}

void callback_during_core_reclamation() {
    for (Action which : {Action::move, Action::swap, Action::release, Action::reclaim_reenter}) {
        auto from = make_buffer(), to = make_buffer(); auto ref = binding(from);
        auto* p = ref.segment->allocate_aligned(32, 8);
        source = &from; target = &to; active = ref;
        dt::reset_owner_lookup_stats(); HandlerScope handler(which); host::fail_after = 0;
        ref.segment->deallocate(p);
        need(host::calls == 1, "Core reclamation new_handler not exercised");
        if (which == Action::release) need(from.data() == nullptr, "Core reclaim released owner");
        else {
            auto& current = which == Action::move || which == Action::swap ? to : from;
            auto* q = ref.segment->allocate_aligned(32, 8);
            need(q == p || (which == Action::reclaim_reenter && q == reentrant_freed[1]),
                 "reclamation committed into stale Core instead of actual payload owner");
            if (which == Action::reclaim_reenter) {
                auto* second = ref.segment->allocate_aligned(32, 8);
                need(second == p && second != q, "outer reclaimed block lost after reentry");
                ref.segment->deallocate(second);
            }
            ref.segment->deallocate(q);
            need(static_cast<void*>(current.data()) == static_cast<void*>(ref.segment), "Core reclaim payload transfer");
            if (which == Action::reclaim_reenter)
                need(dt::owner_lookup_stats.reclamation_restarts != 0, "reentrant capacity change not rechecked");
        }
    }
}

void callback_during_detached_cache_release_and_host_oom() {
    for (Action which : {Action::move, Action::swap, Action::release, Action::retire}) {
        auto from = make_buffer(), to = make_buffer(); auto ref = binding(from);
        auto* a = fd::allocate_dynamic_bytes(ref, 24, 8);
        auto* b = fd::allocate_dynamic_bytes(ref, 32, 8);
        pending_retire = fd::allocate_dynamic_bytes(ref, 32, 8);
        fd::retire_dynamic_bytes(ref, a, 24); fd::retire_dynamic_bytes(ref, b, 32);
        source = &from; target = &to; active = ref;
        HandlerScope handler(which); host::fail_after = 0;
        fd::release_cached_blocks(ref.state);
        need(host::calls == 1, "detached release new_handler not exercised");
        if (which == Action::release) need(from.data() == nullptr, "detached release accessed dead state");
        else if (which == Action::retire) {
            need(ref.state->free_bytes == 32, "outer release erased reentrant retirement accounting");
            auto* p = fd::allocate_dynamic_bytes(ref, 32, 8);
            need(p == pending_retire, "reentrant new list was erased by outer release");
            fd::retire_dynamic_bytes(ref, p, 32);
        } else need(ref.state->free_bytes == 0, "detached cache not empty after transfer");
    }
    auto b = make_buffer(); auto ref = binding(b);
    auto* p = fd::allocate_dynamic_bytes(ref, 24, 8); fd::retire_dynamic_bytes(ref, p, 24);
    host::fail_after = 0; fd::release_cached_blocks(ref.state); host::fail_after = -1;
    need(ref.state->free_bytes == 0, "best-effort Core host OOM left inconsistent allocator cache");
    p = fd::allocate_dynamic_bytes(ref, 24, 8); fd::retire_dynamic_bytes(ref, p, 24);
}

void registry_new_handler_reentry_is_unlocked() {
    auto other = make_buffer(), nested = make_buffer(); auxiliary = &other; target = &nested;
    for (int point : {0, 1}) {
        HandlerScope handler(Action::register_reenter); host::fail_after = point;
        auto b = make_buffer();
        need(host::calls == 1 && other.root<OwnerRoot>().numbers.size() == 9,
             "registration node/control allocation reentry not exercised");
        auto ref = binding(b); auto* p = fd::allocate_dynamic_bytes(ref, 24, 8); fd::retire_dynamic_bytes(ref, p, 24);
    }
}

void delete_callbacks_observe_complete_move() {
    // Target's actual free-vector allocation is captured, not a guessed size or
    // the earlier registry-node delete. This fires exactly in the v1 move gap.
    {
        auto from = make_buffer();
        auto to = xo::XBuffer::create<OtherOwnerRoot>(4096, policy());
        from.root<OwnerRoot>().numbers.resize(3, 313u);
        auto* p = to.arena()->allocate_aligned(32, 8);
        begin_capture(); to.arena()->deallocate(p);
        need(!host::capture_next, "target free-vector allocation not captured");
        auto* old_vector = host::captured;
        delete_binding = binding(from); delete_expected_owner = &to; delete_expected_word = 313;
        delete_require_cold = false;
        const auto from_epoch = from.epoch(), to_epoch = to.epoch();
        arm_delete(old_vector, check_delete_owner); to = std::move(from);
        need(host::delete_calls == 1 && delete_ok, "delete callback saw partial Core/schema/free-vector transfer");
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

void delete_callback_during_cold_cache_eviction() {
    auto incoming = make_buffer(); incoming.root<OwnerRoot>().numbers.resize(3, 521u);
    auto receiver = xo::XBuffer{}; auto ref = binding(incoming);
    std::array<xo::XBuffer, 4> first;
    for (auto& item : first) item = make_buffer(); // incoming is now cold
    xo::XBufferPool pool({1, 4u << 20, 4u << 20}); // retain victim mapping, avoid base reuse
    begin_capture(); auto victim = xo::XBuffer::create<OwnerRoot>(4096, policy().with_pool(pool));
    need(!host::capture_next, "victim record allocation not captured");
    auto* control = host::captured; victim = xo::XBuffer{};
    std::array<xo::XBuffer, 3> after;
    for (auto& item : after) item = make_buffer(); // next replacement is dead victim's slot
    delete_binding = ref; delete_expected_word = 521; delete_require_cold = false;
    delete_move_source = &incoming; delete_move_target = &receiver;
    arm_delete(control, move_during_delete_then_check);
    auto& actual = ref.segment->freelist_runtime_owner();
    need(host::delete_calls == 1 && delete_ok && &actual == static_cast<xo::XBufferCore*>(&receiver),
         "cold cache eviction returned owner cached before delete callback");
    disarm_delete();
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
    for (unsigned id=0; id!=workers.size(); ++id) workers[id]=std::thread([id] {
        for (unsigned n=0;n!=64;++n) {
            auto x=make_buffer(), y=make_buffer(); auto ref=binding(x); x.swap(y);
            auto* p=fd::allocate_dynamic_bytes(ref,24,8); std::memset(p,int(id),24);fd::retire_dynamic_bytes(ref,p,24);
        }
    });
    for (auto& thread : workers) thread.join();
    // Only four inert TLS records may survive owner destruction, not one per
    // historic payload. Registry nodes are erased as owners leave scope.
    for (unsigned i=0;i!=32;++i) { auto x=make_buffer(); binding(x).segment->freelist_runtime_owner(); }
    auto baseline=host::live.load();
    for (unsigned i=0;i!=512;++i) { auto x=make_buffer(); binding(x).segment->freelist_runtime_owner(); }
    need(host::live.load() <= baseline+4, "dead host records accumulated without bounded TLS ownership");
    static_assert(sizeof(xo::XFixedString)==16 && sizeof(xo::XFixedVector<std::uint32_t>)==16);
    static_assert(sizeof(xo::XAllocatorStateV1)==88 && sizeof(xo::XBufferCore::arena_header)==48);
}

xo::XBufferPool late_pool;
xo::XBuffer late_owner;
bool late_armed = false;
struct LateCheck {
    ~LateCheck() {
        if (!late_armed) return;
        try {
            need(host::delete_calls == 1 && delete_ok, "TLS record delete callback did not use safe late cold lookup");
            disarm_delete();
            auto ref=binding(late_owner); const auto before=dt::owner_lookup_stats.cold_lookups;
            auto* p=fd::allocate_dynamic_bytes(ref,24,8);fd::retire_dynamic_bytes(ref,p,24);
            need(dt::owner_lookup_stats.cold_lookups > before, "late global destructor reused destroyed TLS cache");
        } catch (...) { std::abort(); }
    }
} late_check;
}

int main() {
    try {
        hot_lookup_is_nonallocating_and_does_not_change_payload();
        cold_lookup_pending_registration_and_registration_oom();
        transfers_and_deterministic_reused_payload_base();
        callback_during_allocation_diagnostic(); callback_during_core_reclamation();
        callback_during_detached_cache_release_and_host_oom();
        registry_new_handler_reentry_is_unlocked(); alias_roundtrip_and_thread_exit();
        delete_callbacks_observe_complete_move(); delete_callback_during_cold_cache_eviction();
        late_owner=make_buffer(); late_owner.root<OwnerRoot>().numbers.resize(3, 617u);
        late_pool=xo::XBufferPool({1, 4u << 20, 4u << 20});
        begin_capture();
        auto late_victim=xo::XBuffer::create<OwnerRoot>(4096, policy().with_pool(late_pool));
        need(!host::capture_next, "late victim control not captured");
        auto* late_control=host::captured; late_victim=xo::XBuffer{};
        delete_binding=binding(late_owner); delete_expected_owner=&late_owner;
        delete_expected_word=617; delete_require_cold=true;
        arm_delete(late_control, check_delete_owner); late_armed=true;
        std::cout << "10 owner identity groups passed; TLS delete and late global destructor armed; Core bytes "
                  << sizeof(xo::XBufferCore) << "; no timing\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
