// V2.1 owner lookup (detail::OwnerMap): a head names no owner; its binding
// is the payload it lies in, read from a byte table with one entry per 4 MiB
// granule. Covers VM payloads (2^k aligned, entries only for the mapped
// span), pinned and movable slots, pool reuse, unmapping, and heads outside
// every payload.
#include "xoffsetdatastructure.hpp"

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
        xo::XBuffer::maximum_capacity((24u << 20) - xo::detail::SlotGeometry::header_bytes).with_storage(xo::XStorage::Pinned));
    need(b.storage_kind() == xo::XStorageKind::Vm, "fixture is not VM storage");
    auto* base = base_of(b);
    // The region (64-byte host header, then the payload) spans the
    // reservation plus the header rounded up to 4 MiB; it is aligned to the
    // maximum's next power of two (here 32 MiB), never less than 4 MiB, and
    // the payload starts 64 bytes past that boundary, as in a slot.
    constexpr std::size_t header = xo::detail::SlotGeometry::header_bytes;
    const auto span = (b.reserved_bytes() + header + OwnerMap::granule_bytes - 1) & ~(OwnerMap::granule_bytes - 1);
    const auto align = std::bit_ceil(std::size_t((24u << 20) - header));
    need(span == (24u << 20) && align == (32u << 20), "VM fixture span is not 24 MiB");
    need(reinterpret_cast<std::uintptr_t>(base) % align == header, "VM region not aligned to 2^k");
    require_maps(base, span - header, "VM payload granules");
    // The granules past the span, inside the alignment, are not this payload's.
    need(OwnerMap::payload_of(base - header + span) == nullptr, "granule past the VM span is mapped");
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

// A maximum of exactly 2^k (here 32 MiB): the region keeps the 2^k
// alignment (not 64 MiB). The header pushes the last 64 payload bytes into
// one more granule, which carries the same entry; every payload byte
// resolves, and nothing past that granule is mapped.
void vm_power_of_two_maximum() {
    constexpr std::size_t header = xo::detail::SlotGeometry::header_bytes;
    constexpr std::size_t maximum = 32u << 20;
    auto b = xo::XBuffer::create<OwnerRoot>(1u << 20,
        xo::XBuffer::maximum_capacity(maximum).with_storage(xo::XStorage::Pinned));
    need(b.storage_kind() == xo::XStorageKind::Vm, "2^k fixture is not VM storage");
    auto* base = base_of(b);
    const auto region = reinterpret_cast<std::uintptr_t>(base) - header;
    need(region % maximum == 0, "2^k region not aligned to 2^k");
    need(b.reserved_bytes() >= maximum, "2^k fixture reserves less than its maximum");
    require_maps(base, maximum, "2^k payload granules");
    const auto* past = reinterpret_cast<const char*>(region + maximum);  // last 64 payload bytes
    need(OwnerMap::payload_of(past) == base && OwnerMap::payload_of(past + header - 1) == base,
         "payload bytes past 2^k do not resolve");
    need(OwnerMap::same_payload(past, past + 32) == base && OwnerMap::same_payload(past - 8, past + 8) == base,
         "heads past 2^k do not share their payload");
    need(OwnerMap::payload_of(past + OwnerMap::granule_bytes) == nullptr, "granule past the mapping is mapped");
    // Grow to the very end: the last payload bytes bind to the payload.
    need(b.grow(maximum - b.get_size()), "2^k fixture did not grow to its maximum");
    auto& r = b.root<OwnerRoot>();
    r.values.resize(1000, 9u);
    need(OwnerMap::payload_of(base + maximum - 8) == base, "last payload byte does not resolve");
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
    vm_power_of_two_maximum();
    slot_payloads();
    reuse_and_unmap();
    heads_outside();
    std::cout << "test_owner_map: " << cases << " cases passed\n";
    return 0;
}
