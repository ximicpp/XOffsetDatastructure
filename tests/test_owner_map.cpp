// V2.1 owner lookup (detail::OwnerMap): a head names no owner; its binding
// is the payload it lies in, read from a byte table with one entry per 4 MiB
// granule. Covers VM payloads (2^k aligned, entries only for the mapped
// span), pinned and movable slots, pool reuse, unmapping, and heads outside
// every payload.
#include "xoffsetdatastructure.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xo = XOffsetDatastructure;
using xo::detail::OwnerMap;

struct OwnerRoot {
    xo::XString name;
    xo::XVector<std::uint32_t> values;
};
XOFFSET_RECORD_DERIVED(OwnerRoot, (), name, values)
XOFFSET_REGISTER_SCHEMA_NAME(OwnerRoot, "test.v21.owner-map")

namespace {
int cases = 0;

void need(bool ok, const char* what) {
    if (!ok) {
        std::cerr << "test_owner_map FAILED: " << what << "\n";
        std::exit(1);
    }
}

char* base_of(const xo::XBuffer& b) { return const_cast<char*>(reinterpret_cast<const char*>(b.get_address())); }

// Every byte of [base, base + bytes) resolves to base; spot-check both ends
// and each granule boundary in between.
void require_maps(const char* base, std::size_t bytes, const char* what) {
    need(OwnerMap::payload_of(base) == base, what);
    need(OwnerMap::payload_of(base + bytes - 1) == base, what);
    const auto first = reinterpret_cast<std::uintptr_t>(base) >> OwnerMap::granule_shift;
    const auto last = (reinterpret_cast<std::uintptr_t>(base) + bytes - 1) >> OwnerMap::granule_shift;
    for (auto g = first + 1; g <= last; ++g) {
        const auto* at = reinterpret_cast<const char*>(g << OwnerMap::granule_shift);
        need(OwnerMap::payload_of(at) == base && OwnerMap::payload_of(at - 1) == base, what);
    }
}

void vm_payload() {
    auto b = xo::XBuffer::create<OwnerRoot>(1u << 20,
        xo::XBuffer::maximum_capacity(24u << 20).with_storage(xo::XStorage::Pinned));
    need(b.storage_kind() == xo::XStorageKind::Vm, "fixture is not VM storage");
    auto* base = base_of(b);
    // The region is the reservation plus the payload's color offset, rounded
    // up to 4 MiB; it is aligned to the span's next power of two (here 32
    // MiB), never less than 4 MiB, and the payload starts a page-aligned
    // color offset (below 64 KiB) in.
    const auto offset = reinterpret_cast<std::uintptr_t>(base) % (std::size_t(4) << 20);
    const auto span = (b.reserved_bytes() + offset + OwnerMap::granule_bytes - 1) & ~(OwnerMap::granule_bytes - 1);
    const auto align = std::bit_ceil(span);
    need(span == (28u << 20) || (offset == 0 && span == (24u << 20)), "VM fixture span is not 24 or 28 MiB");
    need(align == (32u << 20), "VM fixture alignment is not 32 MiB");
    need(offset % 4096 == 0 && offset < OwnerMap::color_span, "VM payload offset is not a color");
    auto* region = base - offset;
    need(reinterpret_cast<std::uintptr_t>(region) % align == 0, "VM region not aligned to 2^k");
    require_maps(base, b.reserved_bytes(), "VM payload granules");
    // The granules past the span, inside the alignment, are not this payload's.
    need(OwnerMap::payload_of(region + span) == nullptr, "granule past the VM span is mapped");
    // A power-of-two reservation keeps its alignment: the color does not raise
    // k, and the payload's last byte, in the granule past 2^k, maps back.
    {
        auto p = xo::XBuffer::create<OwnerRoot>(4096,
            xo::XBuffer::maximum_capacity(32u << 20).with_storage(xo::XStorage::Pinned));
        auto* pb = base_of(p);
        const auto po = reinterpret_cast<std::uintptr_t>(pb) % (std::size_t(4) << 20);
        need(p.reserved_bytes() == (32u << 20), "power-of-two fixture is not 32 MiB");
        need((reinterpret_cast<std::uintptr_t>(pb) - po) % (32u << 20) == 0, "color raised the VM alignment");
        need(OwnerMap::payload_of(pb + p.reserved_bytes() - 1) == pb, "last byte past 2^k maps elsewhere");
    }
    auto& r = b.root<OwnerRoot>();
    need(r.name.binding().segment == b.arena() && r.values.binding().segment == b.arena(),
         "root heads do not bind to their payload");
    r.name = "a value longer than the inline storage";
    r.values.resize(1000, 7u);
    need(OwnerMap::payload_of(r.name.data()) == base && OwnerMap::payload_of(r.values.data()) == base,
         "blocks do not map to their payload");
    need(OwnerMap::same_payload(&r.name, &r.values) == base, "two heads in one payload differ");
    ++cases;
}

// Consecutive VM payloads start at different page offsets in their regions,
// so their first pages do not share the low page-number bits.
void vm_colors() {
    std::vector<xo::XBuffer> bufs;
    std::vector<std::uintptr_t> offsets;
    for (int i = 0; i < 8; ++i) {
        bufs.push_back(xo::XBuffer::create<OwnerRoot>(4096,
            xo::XBuffer::maximum_capacity(1u << 20).with_storage(xo::XStorage::Pinned)));
        auto* base = base_of(bufs.back());
        const auto offset = reinterpret_cast<std::uintptr_t>(base) % (std::size_t(4) << 20);
        need(offset % xo::detail::SlotSlab::page_size() == 0, "colored VM payload is not page-aligned");
        need(OwnerMap::payload_of(base) == base && OwnerMap::payload_of(base + (1u << 20) - 1) == base,
             "colored VM payload does not map to itself");
        bufs.back().root<OwnerRoot>().name = "a value longer than the inline storage";
        need(OwnerMap::payload_of(bufs.back().root<OwnerRoot>().name.data()) == base, "colored block maps elsewhere");
        offsets.push_back(offset);
    }
    // OwnerMap::color_span / page colors: 16 on 4 KiB pages, 4 on 16 KiB
    // pages (Apple Silicon), so 8 consecutive buffers take min(8, colors).
    const std::size_t page = xo::detail::SlotSlab::page_size();
    const std::size_t colors = (OwnerMap::color_span + page - 1) / page;
    const std::ptrdiff_t expect = static_cast<std::ptrdiff_t>(colors < 8 ? colors : 8);
    std::sort(offsets.begin(), offsets.end());
    need(std::unique(offsets.begin(), offsets.end()) - offsets.begin() == expect,
         "8 consecutive VM payloads do not take min(8, colors) colors");
    ++cases;
}

// Slots of the largest class in different chunks start at different page
// offsets (each chunk carves from its color); every slot maps to itself.
void slot_colors() {
    if constexpr (!xo::detail::SlotGeometry::enabled) return;
    constexpr std::size_t slot = xo::detail::SlotGeometry::max_slot_bytes;
    if (slot <= 2 * 4096) return;
    std::vector<xo::XBuffer> bufs;
    std::vector<std::uintptr_t> offsets;
    for (int i = 0; i < 48; ++i) {
        bufs.push_back(xo::XBuffer::create<OwnerRoot>(4096,
            xo::XBuffer::maximum_capacity(xo::detail::SlotGeometry::max_payload)));
        need(bufs.back().storage_kind() == xo::XStorageKind::PinnedSlot, "slot color fixture is not a slot");
        auto* base = base_of(bufs.back());
        need(OwnerMap::payload_of(base) == base &&
             OwnerMap::payload_of(base + bufs.back().accessible_bytes() - 1) == base &&
             OwnerMap::payload_of(base + bufs.back().accessible_bytes() + 64) != base,
             "colored slot does not map to itself");
        bufs.back().root<OwnerRoot>().name = "a slot value longer than the inline storage";
        need(OwnerMap::payload_of(bufs.back().root<OwnerRoot>().name.data()) == base, "colored slot block maps elsewhere");
        const auto offset = (reinterpret_cast<std::uintptr_t>(base) - 64) % slot;
        need(offset % 4096 == 0, "colored slot is not page-aligned");
        offsets.push_back(offset);
    }
    for (std::size_t i = 1; i < bufs.size(); ++i)
        need(OwnerMap::same_payload(&bufs[i - 1].root<OwnerRoot>().name, &bufs[i].root<OwnerRoot>().name) == nullptr,
             "heads in two colored slots report one payload");
    std::sort(offsets.begin(), offsets.end());
    need(std::unique(offsets.begin(), offsets.end()) - offsets.begin() >= 2, "48 slots of 3+ chunks share one color");
    ++cases;
}

void slot_payloads() {
    if constexpr (!xo::detail::SlotGeometry::enabled) return;
    auto movable = xo::XBuffer::create<OwnerRoot>(4096,
        xo::XBuffer::maximum_capacity(8u << 20).with_growth(xo::XGrowPolicy::Explicit));
    auto pinned = xo::XBuffer::create<OwnerRoot>(4096, xo::XBuffer::maximum_capacity(1u << 15));
    need(movable.storage_kind() == xo::XStorageKind::MovableSlot &&
         pinned.storage_kind() == xo::XStorageKind::PinnedSlot, "slot fixtures have the wrong kind");
    for (auto* b : {&movable, &pinned}) {
        auto* base = base_of(*b);
        need(OwnerMap::payload_of(base) == base &&
             OwnerMap::payload_of(base + b->accessible_bytes() - 1) == base, "slot payload does not map to itself");
        auto& r = b->root<OwnerRoot>();
        need(r.name.binding().segment == b->arena(), "slot root head unbound");
        r.name = "a slot value longer than the inline storage";
        need(OwnerMap::payload_of(r.name.data()) == base, "slot block maps elsewhere");
    }
    // Two slots in one chunk share a granule but not a payload.
    need(OwnerMap::same_payload(&movable.root<OwnerRoot>().name, &pinned.root<OwnerRoot>().name) == nullptr,
         "heads in two slots report one payload");
    // A movable slot that grows moves to a larger slot or to VM; the moved
    // heads resolve to the new payload.
    need(movable.grow(1u << 20), "movable slot did not grow");
    movable.root<OwnerRoot>().values.resize(100000, 3u);
    auto* moved = base_of(movable);
    need(movable.root<OwnerRoot>().name.binding().segment == movable.arena() &&
         OwnerMap::payload_of(movable.root<OwnerRoot>().values.data()) == moved, "moved slot heads unbound");
    ++cases;
}

void reuse_and_unmap() {
    // A pooled mapping keeps its entries; whoever reuses it is bound at once.
    constexpr std::size_t maximum = 8u << 20;
    xo::XBufferPool pool(xo::XBufferPool::Limits{2, 2 * maximum, 2 * maximum});
    const auto pooled = xo::XBuffer::maximum_capacity(maximum).with_storage(xo::XStorage::Pinned).with_pool(pool);
    char* first = nullptr;
    { auto b = xo::XBuffer::create<OwnerRoot>(1u << 20, pooled); first = base_of(b); }
    need(pool.stats().cached_blocks == 1 && OwnerMap::payload_of(first) == first, "pooled mapping lost its entries");
    auto again = xo::XBuffer::create<OwnerRoot>(1u << 20, pooled);
    need(base_of(again) == first && pool.stats().hits == 1, "pool did not reuse the mapping");
    require_maps(first, maximum, "reused mapping granules");
    need(again.root<OwnerRoot>().name.binding().segment == again.arena(), "reused mapping head unbound");
    // An unpooled mapping is unmapped and its entries cleared.
    char* gone = nullptr;
    {
        auto b = xo::XBuffer::create<OwnerRoot>(1u << 20,
            xo::XBuffer::maximum_capacity(std::size_t(1) << 30).with_storage(xo::XStorage::Pinned));
        gone = base_of(b);
        require_maps(gone, std::size_t(1) << 30, "1 GiB payload granules");
    }
    need(OwnerMap::payload_of(gone) == nullptr && OwnerMap::payload_of(gone + (std::size_t(1) << 30) - 1) == nullptr,
         "an unmapped payload keeps its entries");
    ++cases;
}

void heads_outside() {
    xo::XString local;
    need(!local.is_bound() && OwnerMap::payload_of(&local) == nullptr, "a stack head is bound");
    auto heap = std::make_unique<xo::XVector<std::uint32_t>>();
    need(!heap->is_bound(), "a heap head is bound");
    local = "short";
    need(local.view() == "short", "an inline value outside a payload");
    bool threw = false;
    try { local = "a value longer than the inline storage"; } catch (const xo::XException&) { threw = true; }
    need(threw && local.view() == "short", "a head outside every payload allocated a block");
    threw = false;
    try { heap->push_back(1); } catch (const xo::XException&) { threw = true; }
    need(threw && heap->empty(), "a vector outside every payload allocated a block");
    // Copying an inline or empty value out of a payload needs no block.
    auto b = xo::XBuffer::create<OwnerRoot>(4096, xo::XBuffer::maximum_capacity(1u << 20));
    b.root<OwnerRoot>().name = "tiny";
    xo::XString copy(b.root<OwnerRoot>().name);
    xo::XVector<std::uint32_t> empty_copy(b.root<OwnerRoot>().values);
    need(copy.view() == "tiny" && empty_copy.empty(), "inline/empty copies outside a payload");
    b.root<OwnerRoot>().values.push_back(5);
    threw = false;
    try { xo::XVector<std::uint32_t> block_copy(b.root<OwnerRoot>().values); } catch (const xo::XException&) { threw = true; }
    need(threw, "a block copy outside every payload did not throw");
    ++cases;
}
} // namespace

int main() {
    vm_payload();
    vm_colors();
    slot_colors();
    slot_payloads();
    reuse_and_unmap();
    heads_outside();
    std::cout << "test_owner_map: " << cases << " cases passed\n";
    return 0;
}
