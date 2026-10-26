// D5: the small-bin front in the VM host prefix must not change any
// allocation decision. The same random operation sequence runs on a VM
// owner (which has the front) and on a slot owner (which does not): every
// returned offset, the canonical hole list, the free counts and the saved
// wire must agree, through the member and the fixed_detail paths, splits,
// alignment padding, reload, load, reset, clone, clone_into and swap. Also:
// a pooled mapping does not leak its last owner's front, and a full row
// spills to the table, where a host OOM is recorded like any lost entry.
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <new>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "../xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

namespace {
thread_local long fail_after = -1;
thread_local bool watch = false;
thread_local std::size_t attempts = 0, hits = 0;
}
void* operator new(std::size_t n) {
    if (watch) {
        ++attempts;
        if (fail_after == 0) { fail_after = -1; ++hits; throw std::bad_alloc(); }
        if (fail_after > 0) --fail_after;
    }
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

struct FrontRoot {
    std::uint64_t id{};
    XString name;
    XVector<std::uint32_t> values;
};
XOFFSET_REGISTER_SCHEMA_NAME(FrontRoot, "test.FrontRoot")

namespace {
void need(bool ok, const std::string& what) {
    if (!ok) throw std::runtime_error(what);
}

constexpr std::size_t kMax = 192 * 1024;
using Holes = std::vector<XBufferCore::WireHole>;

Holes holes_of(const XBufferCore& b) {
    Holes out;
    b.for_each_free_block([&](XBufferCore::WireHole h) { out.push_back(h); });
    return out;
}

Holes holes_at_least(const XBufferCore& b, std::uint32_t min_bytes) {
    Holes out;
    b.for_each_free_block_at_least(min_bytes, [&](XBufferCore::WireHole h) { out.push_back(h); });
    return out;
}

bool same(const Holes& a, const Holes& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].offset != b[i].offset || a[i].bytes != b[i].bytes) return false;
    return true;
}

XBufferCore::MaximumCapacity vm_cap() { return XBuffer::maximum_capacity(kMax).with_storage(XStorage::Pinned); }
XBufferCore::MaximumCapacity slot_cap() { return XBuffer::maximum_capacity(kMax); }

struct Block {
    std::uint32_t offset, bytes;
};

// One buffer of each kind under the same operations.
struct Pair {
    XBuffer vm, slot;
    std::vector<Block> live; // raw blocks, same offsets in both
    std::size_t compared = 0;

    void check_kinds(const std::string& where) const {
        need(vm.storage_kind() == XStorageKind::Vm, where + ": vm side is not VM");
        need(slot.storage_kind() != XStorageKind::Vm, where + ": slot side is VM");
    }

    void compare(const std::string& where) {
        need(vm.arena()->used_end == slot.arena()->used_end, where + ": used_end");
        need(vm.free_block_count() == slot.free_block_count(), where + ": free_block_count");
        need(vm.free_block_bytes() == slot.free_block_bytes(), where + ": free_block_bytes");
        need(vm.get_free_memory() == slot.get_free_memory(), where + ": get_free_memory");
        need(vm.free_table_complete() == slot.free_table_complete(), where + ": complete");
        need(same(holes_of(vm), holes_of(slot)), where + ": canonical holes");
        for (std::uint32_t m : {1u, 24u, 200u, 384u, 392u, 5000u})
            need(same(holes_at_least(vm, m), holes_at_least(slot, m)), where + ": holes at least");
        const auto used = static_cast<std::size_t>(vm.arena()->used_end);
        need(std::memcmp(vm.data(), slot.data(), used) == 0, where + ": payload bytes");
        ++compared;
    }
};

std::uint32_t pick_size(std::mt19937_64& rng) {
    const auto r = rng() % 100;
    if (r < 70) return std::uint32_t(1 + rng() % 384);  // the front's bins
    if (r < 90) return std::uint32_t(385 + rng() % 3712); // other bins
    return std::uint32_t(4097 + rng() % 12000);           // large blocks
}

// Both buffers take the same request through the same path; both must
// agree on the outcome and the offset.
bool allocate(Pair& p, std::mt19937_64& rng, const std::string& where) {
    const auto bytes = pick_size(rng);
    const std::size_t alignment = rng() % 20 == 0 ? (rng() % 2 ? 16 : 64) : 8;
    const bool direct = rng() % 2;
    auto one = [&](XBuffer& b) -> std::int64_t {
        try {
            void* q = direct ? fixed_detail::allocate_dynamic_bytes(fixed_detail::allocator_ref{b.arena()}, bytes, alignment)
                             : b.arena()->allocate_aligned(bytes, alignment);
            std::memset(q, 0x5a, bytes);
            return static_cast<char*>(q) - b.data();
        } catch (const XBadAlloc&) {
            return -1;
        }
    };
    const auto a = one(p.vm), b = one(p.slot);
    need(a == b, where + ": allocation placement differs (" + std::to_string(a) + " vs " + std::to_string(b) +
                     ", " + std::to_string(bytes) + " B)");
    if (a < 0) return false;
    p.live.push_back({std::uint32_t(a), bytes});
    return true;
}

void retire(Pair& p, std::mt19937_64& rng) {
    const auto i = rng() % p.live.size();
    const auto blk = p.live[i];
    p.live[i] = p.live.back();
    p.live.pop_back();
    const bool direct = rng() % 2;
    for (XBuffer* b : {&p.vm, &p.slot}) {
        void* q = b->data() + blk.offset;
        if (direct) fixed_detail::retire_dynamic_bytes(fixed_detail::allocator_ref{b->arena()}, q, blk.bytes);
        else b->arena()->deallocate(q, blk.bytes);
    }
}

void mutate_root(Pair& p, std::mt19937_64& rng) {
    const auto n = rng() % 40;
    const auto len = rng() % 300;
    const auto v = std::uint32_t(rng());
    for (XBuffer* b : {&p.vm, &p.slot}) {
        auto& r = b->root<FrontRoot>();
        try {
            for (std::size_t k = 0; k < n; ++k) r.values.push_back(v + std::uint32_t(k));
            if (n % 3 == 0 && !r.values.empty()) r.values.erase(r.values.begin());
            r.name = std::string(len, char('a' + len % 26));
        } catch (const XBadAlloc&) {
        }
    }
}

std::string save_both(Pair& p, const std::string& where) {
    const auto a = p.vm.save_verified<FrontRoot>();
    const auto b = p.slot.save_verified<FrontRoot>();
    need(a == b, where + ": saved wires differ");
    p.compare(where + " after save");
    return a;
}

void drain(Pair& p, std::mt19937_64& rng) {
    while (!p.live.empty()) retire(p, rng);
}

void differential(std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    Pair p{XBuffer::create<FrontRoot>(kMax, vm_cap()), XBuffer::create<FrontRoot>(kMax, slot_cap()), {}};
    p.check_kinds("create");
    for (int round = 0; round < 60; ++round) {
        const auto where = "seed " + std::to_string(seed) + " round " + std::to_string(round);
        const auto ops = 50 + rng() % 400;
        for (std::size_t k = 0; k < ops; ++k) {
            const auto r = rng() % 100;
            if (r < 50 || p.live.empty()) {
                if (!allocate(p, rng, where)) drain(p, rng);
            } else if (r < 92) {
                retire(p, rng);
            } else {
                mutate_root(p, rng);
            }
            if (k % 37 == 0) p.compare(where + " op " + std::to_string(k));
        }
        p.compare(where + " before drain");
        drain(p, rng);
        p.compare(where + " drained");
        const auto wire = save_both(p, where);
        switch (rng() % 7) {
        case 0: // continue on the same owners
            break;
        case 1:
            p.vm.reload_verified<FrontRoot>(wire);
            p.slot.reload_verified<FrontRoot>(wire);
            break;
        case 2:
            p.vm = XBuffer::load_verified<FrontRoot>(wire, vm_cap());
            p.slot = XBuffer::load_verified<FrontRoot>(wire, slot_cap());
            break;
        case 3:
            (void)p.vm.reset<FrontRoot>();
            (void)p.slot.reset<FrontRoot>();
            break;
        case 4: { // clone keeps the kind; clone_into crosses kinds
            auto vm2 = p.vm.clone();
            auto slot2 = p.slot.clone();
            auto vm3 = XBuffer::create<FrontRoot>(4096, vm_cap());
            auto slot3 = XBuffer::create<FrontRoot>(4096, slot_cap());
            p.slot.clone_into(vm3); // slot source: no front, every hole in V
            p.vm.clone_into(slot3); // VM source: front entries appended to V
            Pair cross{std::move(vm3), std::move(slot3), {}};
            cross.check_kinds(where + " cross");
            cross.compare(where + " cross clone_into");
            Pair cloned{std::move(vm2), std::move(slot2), {}};
            cloned.check_kinds(where + " clone");
            cloned.compare(where + " clone");
            for (int k = 0; k < 64; ++k) (void)allocate(cross, rng, where + " cross");
            cross.compare(where + " cross after allocations");
            p = rng() % 2 ? std::move(cloned) : std::move(cross);
            drain(p, rng);
            break;
        }
        case 5: { // swap two VM owners (front and table travel together)
            auto other = XBuffer::create<FrontRoot>(kMax, vm_cap());
            auto* q = other.arena()->allocate_aligned(64, 8);
            other.arena()->deallocate(q, 64);
            p.vm.swap(other);
            p.vm.swap(other);
            need(other.free_block_count() == 1, where + ": swap moved a front entry");
            break;
        }
        default: { // move assignment onto a fresh VM owner
            auto moved = std::move(p.vm);
            p.vm = XBuffer::create<FrontRoot>(4096, vm_cap());
            p.vm = std::move(moved);
            break;
        }
        }
        p.check_kinds(where + " after transition");
        p.compare(where + " after transition");
    }
    std::cout << "  seed " << seed << ": " << p.compared << " comparisons of the last pair\n";
}

// A pooled mapping returns with its last owner's front still in the prefix
// page; the next owner must start with no free blocks.
void pooled_mapping_starts_empty() {
    XBufferPool pool(XBufferPool::Limits{2, 1u << 26, 1u << 26});
    const auto cap = XBuffer::maximum_capacity(1024 * 1024).with_pool(pool);
    const void* first = nullptr;
    {
        XBuffer a(64 * 1024, cap);
        need(a.storage_kind() == XStorageKind::Vm, "pooled buffer is not VM");
        first = a.get_address();
        std::vector<void*> blocks;
        for (int i = 0; i < 40; ++i) blocks.push_back(a.arena()->allocate_aligned(8u * std::uint32_t(1 + i % 48), 8));
        for (int i = 0; i < 40; ++i) a.arena()->deallocate(blocks[i], 8u * std::uint32_t(1 + i % 48));
        need(a.free_block_count() == 40, "fixture holes");
    }
    XBuffer b(64 * 1024, cap);
    need(b.get_address() == first, "pool did not hand the mapping back");
    need(b.free_block_count() == 0 && b.free_block_bytes() == 0 && holes_of(b).empty(),
         "pooled mapping kept its last owner's front");
    XBuffer fresh(64 * 1024, XBuffer::maximum_capacity(1024 * 1024).with_storage(XStorage::Pinned));
    for (std::uint32_t bytes : {8u, 64u, 384u}) {
        auto* x = b.arena()->allocate_aligned(bytes, 8);
        auto* y = fresh.arena()->allocate_aligned(bytes, 8);
        need(static_cast<char*>(x) - b.data() == static_cast<char*>(y) - fresh.data(),
             "pooled mapping allocates differently from a fresh one");
    }
}

// The first 14 retires of a front size need no host allocation; the 15th
// moves the oldest entry to the table, whose allocation may fail. The lost
// entry is recorded and save_verified recomputes the hole table.
void spill_needs_the_table() {
    XBufferCore::trim_free_table_cache();
    auto b = XBuffer::create<FrontRoot>(1024 * 1024, XBuffer::maximum_capacity(1024 * 1024));
    need(b.storage_kind() == XStorageKind::Vm, "1 MiB buffer is not VM");
    std::array<void*, 15> blocks{};
    for (auto& q : blocks) q = b.arena()->allocate_aligned(64, 8);
    auto* guard = b.arena()->allocate_aligned(64, 8);
    std::memset(guard, 0x5d, 64);
    attempts = hits = 0;
    watch = true;
    for (int i = 0; i < 14; ++i) fixed_detail::retire_dynamic_bytes(fixed_detail::allocator_ref{b.arena()}, blocks[i], 64);
    watch = false;
    need(attempts == 0, "front retires allocated host memory");
    need(b.free_block_count() == 14 && b.free_table_complete(), "front holds 14 entries");
    attempts = hits = 0;
    fail_after = 0;
    watch = true;
    fixed_detail::retire_dynamic_bytes(fixed_detail::allocator_ref{b.arena()}, blocks[14], 64);
    watch = false;
    fail_after = -1;
    need(hits == 1 && !b.free_table_complete(), "spill OOM was not recorded as an incomplete table");
    need(b.free_block_count() == 14, "spill OOM lost more than the evicted entry");
    for (int j = 0; j < 64; ++j) need(static_cast<unsigned char*>(guard)[j] == 0x5d, "spill OOM changed live data");
    b.arena()->deallocate(guard, 64);
    const auto wire = b.save_verified<FrontRoot>();
    need(b.free_table_complete() && b.free_block_bytes() == 16 * 64, "save_verified did not recompute the holes");
    auto loaded = XBuffer::load_verified<FrontRoot>(wire);
    need(loaded.save_verified<FrontRoot>() == wire, "recomputed wire is not byte stable");
}
} // namespace

int main() {
    try {
        std::cout << "=== D5 front ring ===\n";
        if (XBuffer(4096, slot_cap()).storage_kind() == XStorageKind::Vm) {
            std::cout << "slots disabled: no slot owner to compare against, differential skipped\n";
        } else {
            for (std::uint64_t seed : {1u, 2u, 3u, 4u, 5u, 6u}) differential(seed);
        }
        pooled_mapping_starts_empty();
        spill_needs_the_table();
        std::cout << "All D5 front ring tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAILED: " << e.what() << "\n";
        return 1;
    }
}
