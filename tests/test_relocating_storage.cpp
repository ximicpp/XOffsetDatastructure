// XGrowPolicy::Relocating storage (3.6 growth policy, PR 1): a 1 MiB first
// reservation that is not zeroed, growth in place inside it, make_room()/grow()
// moving the payload to a larger reservation (servers: the whole maximum;
// XOFFSET_RELOCATE_STEP_BYTES: that step, then doubling), small first
// reservations moving through larger slots, address-space fallback, clone,
// save/load and policy changes. Built twice: as is, and with a 4 MiB step
// (the phone rule) as test_relocating_storage_step.
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct RelocRoot {
    std::uint64_t id{};
    XString name;
    XVector<std::uint32_t> values;
    XVector<XString> labels;
    XMap<std::int32_t, std::int32_t> map;
};
XOFFSET_RECORD_DERIVED(RelocRoot, (), id, name, values, labels, map)
XOFFSET_REGISTER_SCHEMA_NAME(RelocRoot, "test.RelocRoot")

namespace {
int failures = 0;
void need(bool ok, const char* what) {
    if (!ok) {
        std::cout << "  FAIL: " << what << "\n";
        ++failures;
    }
}

constexpr auto kReserve = XBufferCore::RELOCATING_RESERVE;
constexpr bool kSlots = XBufferCore::SLOT_MAX != 0;

// Pushes `n` values through raw references, making room first whenever the
// next push could need a new block (what a proxy's slow path does).
bool push_values(XBuffer& b, std::uint32_t from, std::uint32_t n) {
    for (std::uint32_t i = 0; i < n; ++i) {
        auto* r = &b.root<RelocRoot>();
        if (r->values.size() == r->values.capacity()) {
            // the regrown block: at most twice the elements plus slack
            const std::size_t bytes = (std::size_t(r->values.capacity()) * 2 + 16) * sizeof(std::uint32_t) + 8;
            if (b.make_room(bytes) != XErrc::ok) return false;
            r = &b.root<RelocRoot>();
        }
        r->values.push_back(from + i);
    }
    return true;
}

bool values_are(XBuffer& b, std::uint32_t from, std::uint32_t n) {
    auto& r = b.root<RelocRoot>();
    if (r.values.size() != n) return false;
    for (std::uint32_t i = 0; i < n; ++i)
        if (r.values[i] != from + i) return false;
    return true;
}

void fill_misc(XBuffer& b) {
    auto& r = b.root<RelocRoot>();
    r.id = 77;
    r.name = std::string(300, 'n');
    for (int i = 0; i < 8; ++i) r.labels.emplace_back() = "label-" + std::to_string(i);
    for (int i = 0; i < 100; ++i) r.map.try_emplace(i * 7 % 101, i);
}

bool misc_ok(XBuffer& b) {
    auto& r = b.root<RelocRoot>();
    if (r.id != 77 || r.name != std::string(300, 'n') || r.labels.size() != 8 || r.map.size() != 100) return false;
    for (int i = 0; i < 8; ++i)
        if (r.labels[i] != "label-" + std::to_string(i)) return false;
    for (int i = 0; i < 100; ++i) {
        auto it = r.map.find(i * 7 % 101);
        if (it == r.map.end() || it->second != i) return false;
    }
    return true;
}

void test_first_reservation() {
    std::cout << "first reservation\n";
    auto b = XBuffer::create<RelocRoot>(256, XBuffer::relocating());
    need(b.growth_policy() == XGrowPolicy::Relocating, "policy");
    need(b.maximum_capacity() == XBufferCore::MAX_PAYLOAD, "default maximum is the format limit");
    need(!b.address_stable(), "a relocating buffer is not address stable");
    need(b.reservation_limit() >= kReserve, "first reservation holds RELOCATING_RESERVE");
    if (kSlots) {
        need(b.storage_kind() == XStorageKind::MovableSlot, "first reservation is a slot");
        need(b.reservation_limit() == kReserve, "exactly the 1 MiB slot");
    }
    // In place up to the reservation: no move, no epoch change.
    const char* base = b.data();
    const auto epoch = b.epoch();
    fill_misc(b);
    auto& r = b.root<RelocRoot>();
    std::uint32_t pushed = 0;
    while (b.get_size() + 64 * 1024 < b.reservation_limit()) r.values.push_back(pushed++);
    need(b.data() == base && b.epoch() == epoch, "growth inside the reservation keeps the address");
    need(values_are(b, 0, pushed) && misc_ok(b), "content after in-place growth");
#if !XOFFSET_NO_EXCEPTIONS
    // A raw container operation cannot move the buffer: past the reservation
    // it fails (XBadAlloc) and changes nothing.
    bool threw = false;
    try {
        for (;;) r.values.push_back(pushed++);
    } catch (const XBadAlloc&) {
        threw = true;
        --pushed;
    }
    need(threw, "a raw push past the reservation fails");
    need(b.data() == base && values_are(b, 0, pushed) && misc_ok(b), "a failed raw push changes nothing");
#endif
}

void test_make_room_moves() {
    std::cout << "make_room moves\n";
    auto b = XBuffer::create<RelocRoot>(256, XBuffer::relocating());
    fill_misc(b);
    const char* base = b.data();
    const auto epoch = b.epoch();
    need(b.make_room(1000) == XErrc::ok && b.data() == base, "small room in place");
    const std::uint32_t n = 1u << 20; // 4 MiB of values: past the first reservation
    need(push_values(b, 5, n), "push through make_room");
    need(b.data() != base && b.epoch() != epoch, "the buffer moved");
    need(values_are(b, 5, n) && misc_ok(b), "content after the move");
    if constexpr (XBufferCore::RELOCATE_STEP == 0) {
        need(b.storage_kind() == XStorageKind::Vm, "moved to VM");
        need(b.reservation_limit() == b.maximum_capacity(), "one move: the reservation is the maximum");
        const char* moved = b.data();
        need(push_values(b, 5 + n, 4 * n), "push 16 MiB more");
        need(b.data() == moved, "no second move");
        need(values_are(b, 5, 5 * n), "content after more growth");
    } else {
        need(b.reservation_limit() >= XBufferCore::RELOCATE_STEP, "moved to at least the step");
        const auto first = b.reservation_limit();
        need(push_values(b, 5 + n, 8 * n), "push 32 MiB more");
        need(b.reservation_limit() >= 2 * first, "doubled");
        need(values_are(b, 5, 9 * n), "content after doubling");
    }
    need(b.make_room(XBufferCore::MAX_PAYLOAD) == XErrc::capacity_exceeded, "past the maximum");
}

void test_small_reserve() {
    std::cout << "small first reservation\n";
    auto b = XBuffer::create<RelocRoot>(256, XBuffer::relocating().with_reserve(4096));
    if (kSlots) need(b.reservation_limit() == 4096 - 64, "a 4 KiB slot");
    std::vector<std::size_t> limits{b.reservation_limit()};
    std::uint32_t pushed = 0;
    for (int step = 0; step < 12; ++step) {
        const std::uint32_t more = 1000u << step;
        need(push_values(b, pushed, more), "push");
        pushed += more;
        if (b.reservation_limit() != limits.back()) limits.push_back(b.reservation_limit());
    }
    need(values_are(b, 0, pushed), "content through the moves");
    if (kSlots) {
        need(limits.size() >= 3, "several moves");
        bool slot_steps = true;
        for (std::size_t i = 1; i < limits.size() && limits[i] <= kReserve; ++i)
            slot_steps = slot_steps && limits[i] > limits[i - 1];
        need(slot_steps, "growing slot classes");
    }
}

void test_small_maximum() {
    std::cout << "small maximum\n";
    const std::size_t cap = 16u << 20;
    auto b = XBuffer::create<RelocRoot>(256, XBuffer::relocating(cap));
    need(push_values(b, 0, 1000000), "4 MB of values under a 16 MiB maximum");
    need(b.reservation_limit() == cap || XBufferCore::RELOCATE_STEP != 0, "moved to the maximum");
    need(!push_values(b, 1000000, 4000000), "past the maximum fails");
    need(b.make_room(cap) == XErrc::capacity_exceeded, "capacity_exceeded");
    need(b.root<RelocRoot>().values.size() >= 1000000, "content kept");
}

void test_address_space_fallback() {
    std::cout << "address space fallback\n";
    auto b = XBuffer::create<RelocRoot>(256, XBuffer::relocating());
    fill_misc(b);
    need(push_values(b, 0, 50000), "push inside the slot");
    need(b.storage_kind() == XStorageKind::MovableSlot || !kSlots, "still in the slot");
    // The move's first reservation (the maximum) is refused: the next, half
    // the size, is taken.
    const auto limit = b.reservation_limit();
    detail::fail_next_vm_operation(detail::VmOperation::Reserve);
    need(b.make_room(limit) == XErrc::ok, "fallback move");
    detail::clear_vm_failures();
    need(b.reservation_limit() < b.maximum_capacity() && b.reservation_limit() >= limit,
         "a smaller reservation");
    need(values_are(b, 0, 50000) && misc_ok(b), "content after the fallback move");
    need(push_values(b, 50000, 1u << 22), "push more");
    need(values_are(b, 0, 50000 + (1u << 22)), "content");
}

void test_failure_changes_nothing() {
    std::cout << "failed move\n";
    const std::size_t cap = 3u << 20;
    auto b = XBuffer::create<RelocRoot>(256, XBuffer::relocating(cap));
    fill_misc(b);
    need(push_values(b, 0, 50000), "push");
    need(b.storage_kind() == XStorageKind::MovableSlot || !kSlots, "still in the slot");
    const char* base = b.data();
    const auto limit = b.reservation_limit();
    const auto epoch = b.epoch();
    // Room for nearly the whole maximum: the first attempt (the maximum) is
    // refused at its reservation, the second (exactly the need) at its first
    // page; there is no third.
    const auto used = b.arena()->used_end;
    detail::fail_next_vm_operation(detail::VmOperation::Reserve);
    detail::fail_next_vm_operation(detail::VmOperation::Enable);
    const auto result = b.make_room(cap - used - 4096);
    detail::clear_vm_failures();
    need(result == XErrc::out_of_storage, "out_of_storage");
    need(b.data() == base && b.reservation_limit() == limit && b.epoch() == epoch, "nothing moved");
    need(values_are(b, 0, 50000) && misc_ok(b), "content intact");
    need(b.make_room(cap - used - 4096) == XErrc::ok, "works once the OS gives memory again");
    need(values_are(b, 0, 50000) && misc_ok(b), "content after the move");
}

void test_wire_and_clone() {
    std::cout << "wire, clone, load\n";
    auto a = XBuffer::create<RelocRoot>(256, XBuffer::relocating());
    auto ref = XBuffer::create<RelocRoot>(256, XBuffer::maximum_capacity(64u << 20).with_growth(XGrowPolicy::Automatic));
    fill_misc(a);
    fill_misc(ref);
    need(push_values(a, 3, 400000), "push");
    auto& rr = ref.root<RelocRoot>();
    for (std::uint32_t i = 0; i < 400000; ++i) rr.values.push_back(3 + i);
    need(a.storage_kind() == XStorageKind::Vm || XBufferCore::RELOCATE_STEP != 0 || !kSlots, "moved");
    const auto wa = a.save_verified<RelocRoot>();
    const auto wr = ref.save_verified<RelocRoot>();
#if !defined(XOFFSET_STRESS_RELOCATE)
    need(wa == wr, "same wire as an Automatic buffer");
#else
    // Stress mode regrows a Relocating buffer's containers into fresh blocks:
    // another layout of the same content.
    (void)wr;
    auto same = XBuffer::load_verified<RelocRoot>(wa);
    need(values_are(same, 3, 400000), "same content as an Automatic buffer");
#endif

    auto c = a.clone();
    need(c.growth_policy() == XGrowPolicy::Relocating && c.maximum_capacity() == a.maximum_capacity(), "clone keeps the policy");
    need(values_are(c, 3, 400000) && misc_ok(c), "clone content");
    need(push_values(c, 3 + 400000, 100000) && values_are(c, 3, 500000), "clone grows");

    auto l = XBuffer::load_verified<RelocRoot>(wa, XBuffer::relocating());
    need(l.growth_policy() == XGrowPolicy::Relocating, "load keeps the policy");
    need(values_are(l, 3, 400000) && misc_ok(l), "loaded content");
    need(push_values(l, 3 + 400000, 1u << 20) && values_are(l, 3, 400000 + (1u << 20)), "loaded buffer grows");

    // A small wire loads into the first slot.
    auto s = XBuffer::create<RelocRoot>(256, XBuffer::relocating());
    fill_misc(s);
    const auto ws = s.save_verified<RelocRoot>();
    auto ls = XBuffer::load_verified<RelocRoot>(ws, XBuffer::relocating());
    if (kSlots) need(ls.storage_kind() == XStorageKind::MovableSlot && ls.reservation_limit() == kReserve, "small load in a slot");
    need(misc_ok(ls), "small load content");

    // Reload onto a relocating buffer whose reservation is too small moves it.
    auto t = XBuffer::create<RelocRoot>(256, XBuffer::relocating().with_reserve(4096));
    t.reload_verified<RelocRoot>(wa);
    need(values_are(t, 3, 400000) && misc_ok(t), "reload past the reservation");
}

void test_moves_and_policy() {
    std::cout << "buffer moves and policy changes\n";
    auto a = XBuffer::create<RelocRoot>(256, XBuffer::relocating());
    fill_misc(a);
    need(push_values(a, 0, 300000), "push");
    XBuffer m(std::move(a));
    need(values_are(m, 0, 300000) && misc_ok(m), "moved-to XBuffer");
    need(push_values(m, 300000, 1u << 20) && values_are(m, 0, 300000 + (1u << 20)), "moved-to XBuffer relocates");
    XBuffer s = XBuffer::create<RelocRoot>(256, XBuffer::relocating());
    s.swap(m);
    need(values_are(s, 0, 300000 + (1u << 20)), "swapped");

    // Relocating -> Automatic: a reservation of the whole maximum (one move).
    auto p = XBuffer::create<RelocRoot>(256, XBuffer::relocating(32u << 20));
    fill_misc(p);
    p.set_growth_policy(XGrowPolicy::Automatic);
    need(p.address_stable() && p.reservation_limit() == p.maximum_capacity(), "pinned for Automatic");
    auto& r = p.root<RelocRoot>();
    for (std::uint32_t i = 0; i < 2000000; ++i) r.values.push_back(i);
    need(values_are(p, 0, 2000000) && misc_ok(p), "Automatic grows in place");
    // Automatic -> Relocating keeps the storage.
    p.set_growth_policy(XGrowPolicy::Relocating);
    need(!p.address_stable() && p.reservation_limit() == p.maximum_capacity(), "Relocating keeps the reservation");

    // grow() moves a relocating buffer too.
    auto g = XBuffer::create<RelocRoot>(256, XBuffer::relocating());
    fill_misc(g);
    need(g.grow(2u << 20), "grow past the slot");
    need(g.get_size() >= (2u << 20) && misc_ok(g), "grown");
}

void test_slot_budget_accounting() {
    std::cout << "slot retention accounting\n";
    if (!kSlots) return;
    {
        std::vector<XBuffer> many;
        for (int i = 0; i < 200; ++i) {
            many.push_back(XBuffer::create<RelocRoot>(256, XBuffer::relocating()));
            many.back().root<RelocRoot>().name = std::string(100, 'x');
        }
    }
    XBufferCore::trim_slot_storage();
    const auto st = XBufferCore::slot_storage_stats();
    // Freed slots count what they dirtied (a page each), far below 200 MiB nominal.
    need(st.global_free_dirty_bytes <= st.global_free_bytes, "dirty <= nominal");
    need(st.global_free_dirty_bytes < (64u << 20), "the budget counts dirtied bytes");
}

void test_invalid_requests() {
    std::cout << "invalid requests\n";
#if !XOFFSET_NO_EXCEPTIONS
    bool threw = false;
    try {
        auto b = XBuffer::create<RelocRoot>(256, XBuffer::relocating().with_storage(XStorage::Pinned));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    need(threw, "Pinned is refused");
    threw = false;
    try {
        XBufferPool pool({4, 4u << 26, 64u << 20});
        auto b = XBuffer::create<RelocRoot>(256, XBuffer::relocating(64u << 20).with_pool(pool));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    need(threw, "a pool is refused");
#endif
    // Other policies: make_room grows in place within the storage.
    auto e = XBuffer::create<RelocRoot>(4096, XBuffer::maximum_capacity(64u << 20).with_growth(XGrowPolicy::Automatic));
    need(e.make_room(10u << 20) == XErrc::ok && e.get_size() >= (10u << 20), "Automatic make_room");
    need(e.make_room(100u << 20) == XErrc::capacity_exceeded, "Automatic past the maximum");
}
} // namespace

int main() {
    std::cout << "RELOCATING_RESERVE=" << kReserve << " RELOCATE_STEP=" << XBufferCore::RELOCATE_STEP
              << " SLOT_MAX=" << XBufferCore::SLOT_MAX << "\n";
    test_first_reservation();
    test_make_room_moves();
    test_small_reserve();
    test_small_maximum();
    test_address_space_fallback();
    test_failure_changes_nothing();
    test_wire_and_clone();
    test_moves_and_policy();
    test_slot_budget_accounting();
    test_invalid_requests();
    if (failures) {
        std::cout << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "all relocating storage tests passed\n";
    return 0;
}
