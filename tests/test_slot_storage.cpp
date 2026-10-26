// R1 slot storage: the storage rule, movable grow(), reload and direct
// reload onto slots, owners without storage, zeroing of reused slots, wire
// equality with VM storage, failure atomicity and cross-thread release.
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct SlotRoot {
    std::uint64_t id{};
    XString name;
    XVector<std::uint32_t> values;
    XVector<XString> labels;
};
XOFFSET_REGISTER_SCHEMA_NAME(SlotRoot, "test.SlotRoot")

namespace {
void need(bool ok, const char* what) { if (!ok) throw std::runtime_error(what); }

constexpr auto kDefault = XBufferCore::DEFAULT_MAXIMUM_CAPACITY;
constexpr auto kSlotMax = XBufferCore::SLOT_MAX;

XBufferCore::MaximumCapacity pinned(std::size_t bytes = kDefault) {
    return XBuffer::maximum_capacity(bytes).with_storage(XStorage::Pinned);
}

template <class F>
bool throws_invalid(F&& f) {
    try { f(); } catch (const std::invalid_argument&) { return true; }
    return false;
}

// The same mutation sequence for every storage: allocations, a freed block
// (a hole), string growth and a vector that outgrows its first block.
void fill(XBuffer& b, std::uint64_t id, std::size_t n) {
    auto& r = b.root<SlotRoot>();
    r.id = id;
    r.name = "slot-" + std::to_string(id) + std::string(48, 'n');
    auto* hole = b.arena()->allocate_aligned(120, 8);
    std::memset(hole, 0xcd, 120);
    b.arena()->deallocate(hole, 120);
    for (std::size_t i = 0; i < n; ++i) r.values.push_back(std::uint32_t(id * 7 + i));
    for (std::size_t i = 0; i < 4; ++i) r.labels.emplace_back() = "label-" + std::to_string(i) + std::string(i * 9, 'l');
}

void check(XBuffer& b, std::uint64_t id, std::size_t n) {
    auto& r = b.root<SlotRoot>();
    need(r.id == id, "id");
    need(r.name == "slot-" + std::to_string(id) + std::string(48, 'n'), "name");
    need(r.values.size() == n, "values size");
    for (std::size_t i = 0; i < n; ++i) need(r.values[i] == std::uint32_t(id * 7 + i), "values");
    need(r.labels.size() >= 4 && r.labels[3] == "label-3" + std::string(27, 'l'), "labels");
}

// Grows an Explicit buffer by hand until `n` values fit.
void fill_explicit(XBuffer& b, std::uint64_t id, std::size_t n) {
    for (;;) {
        try {
            fill(b, id, n);
            return;
        } catch (const XBadAlloc&) {
            // Start over on a reset root: partial content is not reused.
            const auto size = b.get_size();
            need(b.grow(size), "explicit grow");
            (void)b.reset<SlotRoot>();
        }
    }
}

// The transport side of begin_verified_reload: header, extent table,
// payload (dense or extent by extent), hole table.
void direct_reload(XBuffer& b, const std::string& w) {
    XWireHeaderV2 h{};
    std::memcpy(&h, w.data(), sizeof(h));
    auto t = b.begin_verified_reload<SlotRoot>(h.used_bytes, h.hole_count, h.extent_count);
    std::size_t at = 0;
    auto take = [&](std::byte* to, std::size_t n) { std::memcpy(to, w.data() + at, n); at += n; };
    take(t.header_bytes().data(), t.header_bytes().size());
    const auto table = t.extent_bytes();
    if (!table.empty()) take(table.data(), table.size());
    if (table.empty()) {
        take(t.payload_bytes().data(), t.payload_bytes().size());
    } else {
        for (std::size_t i = 0; i < table.size() / 8; ++i) {
            std::uint32_t e[2];
            std::memcpy(e, table.data() + i * 8, 8);
            take(t.payload_bytes().data() + e[0], e[1]);
        }
    }
    if (!t.hole_bytes().empty()) take(t.hole_bytes().data(), t.hole_bytes().size());
    need(at == w.size(), "test transport framing");
    t.complete(t.header_bytes().size(), t.payload_bytes().size(), t.hole_bytes().size(), table.size());
    t.finish();
}

void storage_rule() {
    XBuffer by_size(4096);
    need(by_size.storage_kind() == XStorageKind::MovableSlot && !by_size.address_stable(), "default -> movable slot");
    need(by_size.maximum_capacity() == kDefault, "default maximum");
    need(by_size.reserved_bytes() == 8192 && by_size.accessible_bytes() == 8192 - 64, "8 KiB class");
    need(reinterpret_cast<std::uintptr_t>(by_size.get_address()) % 64 == 0, "payload 64-aligned");

    auto created = XBuffer::create<SlotRoot>(4096, XBuffer::maximum_capacity(8 * 1024 * 1024));
    need(created.storage_kind() == XStorageKind::MovableSlot, "README create -> movable slot");

    for (auto growth : {XGrowPolicy::Explicit, XGrowPolicy::Automatic}) {
        XBuffer small(4096, XBuffer::maximum_capacity(64 * 1024).with_growth(growth));
        need(small.storage_kind() == XStorageKind::PinnedSlot && small.address_stable(), "small maximum -> pinned slot");
        // 64 KiB + the header needs the 128 KiB class.
        need(small.reserved_bytes() == 128 * 1024, "pinned slot holds the maximum");
        XBuffer at_max(4096, XBuffer::maximum_capacity(kSlotMax).with_growth(growth));
        need(at_max.storage_kind() == XStorageKind::PinnedSlot, "SLOT_MAX -> pinned slot");
    }

    XBuffer automatic(4096, XBuffer::maximum_capacity(256 * 1024 * 1024).with_growth(XGrowPolicy::Automatic));
    need(automatic.storage_kind() == XStorageKind::Vm, "Automatic, large maximum -> VM");
    XBuffer large_initial(kSlotMax + 1);
    need(large_initial.storage_kind() == XStorageKind::Vm, "initial > SLOT_MAX -> VM");
    XBufferPool pool(XBufferPool::Limits{2, 1u << 26, 1u << 26});
    XBuffer pooled(4096, XBuffer::maximum_capacity(1024 * 1024).with_pool(pool));
    need(pooled.storage_kind() == XStorageKind::Vm, "pool -> VM");
    XBuffer pinned_small(4096, pinned(64 * 1024));
    need(pinned_small.storage_kind() == XStorageKind::Vm, "Pinned -> VM");

    const auto movable = XBuffer::maximum_capacity(kDefault).with_storage(XStorage::Movable);
    XBuffer explicit_movable(4096, movable);
    need(explicit_movable.storage_kind() == XStorageKind::MovableSlot, "Movable request");
    need(throws_invalid([&] { XBuffer b(4096, movable.with_growth(XGrowPolicy::Automatic)); }),
         "Movable + Automatic + large maximum refused");
    need(throws_invalid([&] { XBuffer b(4096, movable.with_pool(pool)); }), "Movable + pool refused");
    need(throws_invalid([&] { XBuffer b(kSlotMax + 1, movable); }), "Movable + large initial refused");

    // Loads take the rule with the used size as the initial size.
    const auto wire = XBuffer::create<SlotRoot>(4096).save_verified<SlotRoot>();
    auto loaded = XBuffer::load_verified<SlotRoot>(wire);
    need(loaded.storage_kind() == XStorageKind::MovableSlot, "load_verified without maximum -> movable slot");
    auto loaded_auto = XBuffer::load_verified<SlotRoot>(wire,
        XBuffer::maximum_capacity(256u << 20).with_growth(XGrowPolicy::Automatic));
    need(loaded_auto.storage_kind() == XStorageKind::Vm, "Automatic load -> VM");
    auto raw = XBuffer::load_unverified(wire.substr(sizeof(XWireHeaderV2)));
    need(raw.storage_kind() == XStorageKind::MovableSlot, "unverified load -> movable slot");
}

void movable_grow() {
    auto b = XBuffer::create<SlotRoot>(1024);
    fill_explicit(b, 3, 40);
    auto handle = b.handle<SlotRoot>();
    const auto holes = b.free_block_count();
    const auto hole_bytes = b.free_block_bytes();
    need(holes > 0, "the sequence leaves a hole");
    const auto wire_before = b.save_verified<SlotRoot>();
    const auto* address = b.get_address();
    const auto epoch = b.epoch();
    const auto used = b.arena()->used_end;
    const auto old_class = b.reserved_bytes();

    need(b.grow(b.accessible_bytes()), "grow past the slot");
    need(b.get_address() != address && b.epoch() != epoch, "moved, epoch changed");
    need(b.reserved_bytes() > old_class && b.storage_kind() == XStorageKind::MovableSlot, "larger slot");
    need(b.arena()->used_end == used, "used_end kept");
    need(b.free_block_count() == holes && b.free_block_bytes() == hole_bytes, "free table kept");
    for (auto i = used; i < b.get_size(); ++i) need(b.data()[i] == 0, "bytes above used_end read zero");
    check(b, 3, 40);
    need(handle->id == 3, "handle reads the moved root");
    need(handle.get() == &b.root<SlotRoot>(), "handle re-resolves");
    need(b.save_verified<SlotRoot>() == wire_before, "same wire after the move");

    // The moved owner keeps allocating: the owner word moved with it.
    // (save_verified shrank the logical size to used_end.)
    need(b.grow(1024), "room after save");
    b.root<SlotRoot>().labels.emplace_back() = std::string(300, 'z');
    need(b.root<SlotRoot>().labels.back() == std::string(300, 'z'), "allocation after move");

    // Inside the slot grow() does not move.
    const auto* stay = b.get_address();
    const auto stay_epoch = b.epoch();
    need(b.grow(8), "grow inside slot");
    need(b.get_address() == stay && b.epoch() == stay_epoch, "in-slot grow keeps address");

    // Past SLOT_MAX the owner moves to VM and is pinned from then on.
    need(b.grow(kSlotMax), "grow past SLOT_MAX");
    need(b.storage_kind() == XStorageKind::Vm && b.address_stable(), "promoted to VM");
    need(b.maximum_capacity() == kDefault, "maximum kept");
    const auto* vm = b.get_address();
    need(b.grow(1u << 20) && b.get_address() == vm, "VM grow keeps address");
    check(b, 3, 40);

    // A grow over the maximum fails and changes nothing.
    auto c = XBuffer::create<SlotRoot>(1024);
    const auto* c_address = c.get_address();
    need(!c.grow(kDefault), "over maximum refused");
    need(c.get_address() == c_address && c.has_root<SlotRoot>(), "unchanged after refusal");
}

void grow_failures_are_atomic() {
    auto b = XBuffer::create<SlotRoot>(1024);
    fill_explicit(b, 5, 10);
    const auto bytes = b.copy_bytes();
    const auto* address = b.get_address();
    const auto epoch = b.epoch();
    detail::fail_next_vm_operation(detail::VmOperation::Reserve);
    need(!b.grow(b.accessible_bytes()), "slot acquisition failure");
    detail::clear_vm_failures();
    need(b.get_address() == address && b.epoch() == epoch && b.copy_bytes() == bytes, "unchanged");
    detail::fail_next_registry_registration();
    need(!b.grow(b.accessible_bytes()), "registration failure");
    detail::clear_registry_failures();
    need(b.get_address() == address && b.epoch() == epoch && b.copy_bytes() == bytes, "unchanged");
    check(b, 5, 10);
    need(b.grow(b.accessible_bytes()), "grow after failures");
    check(b, 5, 10);
}

void wire_equal_across_storage() {
    for (const std::size_t n : {std::size_t(3), std::size_t(500), std::size_t(20000)}) {
        auto vm = XBuffer::create<SlotRoot>(4096, pinned().with_growth(XGrowPolicy::Automatic));
        auto pinned_slot = XBuffer::create<SlotRoot>(4096,
            XBuffer::maximum_capacity(kSlotMax).with_growth(XGrowPolicy::Automatic));
        auto movable = XBuffer::create<SlotRoot>(1024);
        fill(vm, n, n);
        if (n * 4 + 4096 < kSlotMax / 2) fill(pinned_slot, n, n);
        fill_explicit(movable, n, n);
        const auto wire = vm.save_verified<SlotRoot>();
        if (n * 4 + 4096 < kSlotMax / 2) need(pinned_slot.save_verified<SlotRoot>() == wire, "pinned slot wire == VM wire");
        need(movable.save_verified<SlotRoot>() == wire, "movable slot wire == VM wire");
        auto back = XBuffer::load_verified<SlotRoot>(wire);
        check(back, n, n);
        need(back.save_verified<SlotRoot>() == wire, "reload re-emits");
    }
}

void reload_onto_slots() {
    const auto small = [] { auto b = XBuffer::create<SlotRoot>(4096, pinned()); fill(b, 1, 5); return b.save_verified<SlotRoot>(); }();
    const auto mid = [] { auto b = XBuffer::create<SlotRoot>(4096, pinned().with_growth(XGrowPolicy::Automatic)); fill(b, 2, 5000); return b.save_verified<SlotRoot>(); }();
    const auto big = [] { auto b = XBuffer::create<SlotRoot>(4096, pinned().with_growth(XGrowPolicy::Automatic)); fill(b, 4, 90000); return b.save_verified<SlotRoot>(); }();

    auto b = XBuffer::load_verified<SlotRoot>(small);
    need(b.storage_kind() == XStorageKind::MovableSlot, "small load on a slot");
    const auto* address = b.get_address();
    b.reload_verified<SlotRoot>(small);
    need(b.get_address() == address, "fitting reload stays");
    check(b, 1, 5);
    b.reload_verified<SlotRoot>(mid);
    need(b.get_address() != address && b.storage_kind() == XStorageKind::MovableSlot, "larger reload moves to a larger slot");
    check(b, 2, 5000);
    need(b.save_verified<SlotRoot>() == mid, "mid re-emits");
    const auto* mid_address = b.get_address();
    b.reload_verified<SlotRoot>(small);
    need(b.get_address() == mid_address, "no downgrade on a smaller reload");
    check(b, 1, 5);
    for (auto i = b.arena()->used_end; i < b.get_size(); ++i) need(b.data()[i] == 0, "old bytes above used_end cleared");
    b.reload_verified<SlotRoot>(big);
    need(b.storage_kind() == XStorageKind::Vm, "reload above SLOT_MAX moves to VM");
    check(b, 4, 90000);
    need(b.save_verified<SlotRoot>() == big, "big re-emits");

    // A pinned slot never moves: its slot holds the whole maximum.
    auto p = XBuffer::load_verified<SlotRoot>(small, XBuffer::maximum_capacity(kSlotMax));
    need(p.storage_kind() == XStorageKind::PinnedSlot, "pinned slot");
    const auto* p_address = p.get_address();
    p.reload_verified<SlotRoot>(mid);
    need(p.get_address() == p_address, "pinned slot reload stays");
    check(p, 2, 5000);

    // Direct reload: begin moves a too-small slot before handing out spans.
    auto d = XBuffer::load_verified<SlotRoot>(small);
    const auto* d_address = d.get_address();
    for (const auto* wire : {&mid, &small, &mid}) {
        direct_reload(d, *wire);
        need(d.save_verified<SlotRoot>() == *wire, "direct reload on slot re-emits");
    }
    need(d.get_address() != d_address, "direct reload moved to a larger slot");
    check(d, 2, 5000);
}

void owners_without_storage() {
    const auto wire = [] { auto b = XBuffer::create<SlotRoot>(4096); fill_explicit(b, 9, 30); return b.save_verified<SlotRoot>(); }();
    XBuffer empty;
    empty.reload_verified<SlotRoot>(wire);
    need(empty.storage_kind() == XStorageKind::MovableSlot && empty.maximum_capacity() == kDefault, "storage on demand");
    check(empty, 9, 30);

    XBuffer direct;
    direct_reload(direct, wire);
    check(direct, 9, 30);
    need(direct.save_verified<SlotRoot>() == wire, "direct reload on a new owner re-emits");

    // A moved-from buffer has no storage either.
    XBuffer moved(std::move(direct));
    direct.reload_verified<SlotRoot>(wire);
    check(direct, 9, 30);
    check(moved, 9, 30);

    // Without a maximum the M1 default limit applies.
    bool refused = false;
    try {
        XBuffer none;
        (void)none.begin_verified_reload<SlotRoot>(kDefault + 8);
    } catch (const XBadAlloc&) { refused = true; }
    need(refused, "on-demand storage keeps the default maximum");
}

void reused_slots_read_zero() {
    // Free a dirty slot, then take the same class again (the thread cache
    // hands back the most recent slot first).
    const void* dirty = nullptr;
    {
        XBuffer b(3000);
        std::memset(b.data() + 48, 0xee, b.get_size() - 48);
        need(b.grow(b.accessible_bytes() - b.get_size()), "fill the slot");
        std::memset(b.data() + 48, 0xee, b.get_size() - 48);
        dirty = b.get_address();
    }
    XBuffer again(3000);
    need(again.get_address() == dirty, "same slot reused");
    for (std::size_t i = 48; i < again.get_size(); ++i) need(again.data()[i] == 0, "reused slot cleared");
    need(again.grow(again.accessible_bytes() - again.get_size()), "grow in reused slot");
    for (std::size_t i = 48; i < again.get_size(); ++i) need(again.data()[i] == 0, "grown range cleared");
}

void growth_policy_switch_pins() {
    auto b = XBuffer::create<SlotRoot>(1024);
    fill_explicit(b, 11, 20);
    const auto epoch = b.epoch();
    b.set_growth_policy(XGrowPolicy::Automatic);
    need(b.storage_kind() == XStorageKind::Vm && b.epoch() != epoch, "Automatic pins the owner on VM");
    check(b, 11, 20);
    auto& r = b.root<SlotRoot>();
    const auto* address = b.get_address();
    for (std::uint32_t i = 0; i < 100000; ++i) r.values.push_back(i);
    need(b.get_address() == address && &b.root<SlotRoot>() == &r, "Automatic growth in place");
}

void reset_and_swap() {
    auto a = XBuffer::create<SlotRoot>(1024);
    fill_explicit(a, 12, 50);
    const auto* address = a.get_address();
    (void)a.reset<SlotRoot>();
    need(a.get_address() == address && a.root<SlotRoot>().values.empty(), "reset keeps the slot");

    auto vm = XBuffer::create<SlotRoot>(4096, pinned());
    fill(vm, 13, 50);
    fill_explicit(a, 12, 50);
    a.swap(vm);
    check(a, 13, 50);
    check(vm, 12, 50);
    need(a.storage_kind() == XStorageKind::Vm && vm.storage_kind() == XStorageKind::MovableSlot, "kind follows storage");
    vm.root<SlotRoot>().labels.emplace_back() = "after swap";
    need(vm.grow(vm.accessible_bytes()), "grow after swap");
    need(vm.root<SlotRoot>().labels.back() == "after swap", "content after swap and move");
}

void cross_thread_release() {
    std::vector<XBuffer> made;
    std::thread producer([&] {
        for (int i = 0; i < 2000; ++i) {
            auto b = XBuffer::create<SlotRoot>(1024 + (i % 7) * 4096);
            b.root<SlotRoot>().id = std::uint64_t(i);
            made.push_back(std::move(b));
        }
    });
    producer.join();
    std::thread consumer([&] {
        for (std::size_t i = 0; i < made.size(); ++i) need(made[i].root<SlotRoot>().id == i, "cross-thread content");
        made.clear();
    });
    consumer.join();
    const auto stats = XBufferCore::slot_storage_stats();
    need(stats.chunks > 0 && stats.global_free_slots > 0, "consumer thread exit returned its cache");
    XBufferCore::trim_slot_storage();
    XBuffer after(30000);
    for (std::size_t i = 48; i < after.get_size(); ++i) need(after.data()[i] == 0, "slot after trim reads zero");
}

void compaction_onto_slot() {
    auto b = XBuffer::create<SlotRoot>(1024);
    fill_explicit(b, 14, 300);
    auto compacted = XCompactor::compact<SlotRoot>(b);
    need(compacted.storage_kind() == XStorageKind::MovableSlot, "compaction target on a slot");
    check(compacted, 14, 300);
}
} // namespace

int main() {
    if (kSlotMax == 0) {
        std::cout << "Slot storage disabled at build time; skipped\n";
        return 0;
    }
    const char* stage = "";
    try {
        stage = "storage_rule"; storage_rule();
        stage = "movable_grow"; movable_grow();
        stage = "grow_failures_are_atomic"; grow_failures_are_atomic();
        stage = "wire_equal_across_storage"; wire_equal_across_storage();
        stage = "reload_onto_slots"; reload_onto_slots();
        stage = "owners_without_storage"; owners_without_storage();
        stage = "reused_slots_read_zero"; reused_slots_read_zero();
        stage = "growth_policy_switch_pins"; growth_policy_switch_pins();
        stage = "reset_and_swap"; reset_and_swap();
        stage = "cross_thread_release"; cross_thread_release();
        stage = "compaction_onto_slot"; compaction_onto_slot();
    } catch (const std::exception& e) {
        std::cerr << "test_slot_storage FAILED in " << stage << ": " << e.what() << "\n";
        return 1;
    }
    std::cout << "Slot storage tests passed\n";
    return 0;
}
