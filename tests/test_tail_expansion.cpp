// Tail growth must preserve allocations and wire metadata without committing
// additional logical capacity. The arena primitive requires a live allocation.
// V2: blocks carry no header; a block's extent is round8 of the size its owner
// derives, only the block ending at used_end can grow in place, and free
// blocks live in the host-side free table (split exactly on reuse).
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct TailExpansionRoot {
    std::int32_t id{};
    XVector<std::int32_t> values;
    XVector<std::int32_t> other;
    XString label;
    XVector<XString> nested;
};
XOFFSET_REGISTER_SCHEMA_NAME(TailExpansionRoot, "test.TailExpansionRoot")

struct alignas(65536) TailAlignedValue {
    std::int64_t value{};
};
struct TailAlignedRoot { XVector<TailAlignedValue> values; };

namespace {

std::size_t extent(std::size_t bytes) { return (bytes + 7u) & ~std::size_t(7); }

std::size_t offset_of(const XBufferCore& buffer, const void* ptr) {
    return reinterpret_cast<std::uintptr_t>(ptr) -
           reinterpret_cast<std::uintptr_t>(buffer.get_address());
}

const char* at(const void* ptr, std::size_t bytes) { return static_cast<const char*>(ptr) + bytes; }

bool has_hole(const XBufferCore& buffer, const void* ptr, std::uint32_t bytes) {
    const auto offset = offset_of(buffer, ptr);
    bool found = false;
    buffer.for_each_free_block([&](XBufferCore::WireHole hole) {
        found |= hole.offset == offset && hole.bytes == bytes;
    });
    return found;
}

std::vector<std::pair<std::uint32_t, std::uint32_t>> holes(const XBufferCore& buffer) {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> out;
    buffer.for_each_free_block([&](XBufferCore::WireHole hole) { out.emplace_back(hole.offset, hole.bytes); });
    return out;
}

// V2 replacement for the V1 header/allocator-state envelope check: the block of
// `bytes` (as its owner derives them) is 8-aligned, ends exactly at used_end and
// lies inside the logical extent of the buffer that owns its arena.
void check_allocator_end(XBufferCore& buffer, const void* ptr, std::size_t bytes) {
    const auto offset = offset_of(buffer, ptr);
    assert(offset % 8 == 0 && offset >= sizeof(XArena));
    assert(buffer.arena()->used_end == offset + extent(bytes));
    assert(buffer.arena()->used_end <= buffer.get_size());
    assert(&XBufferCore::owner_from_arena(buffer.arena()) == &buffer);
}

template <typename Operation>
void reject_without_change(XBufferCore& buffer, Operation operation) {
    const auto bytes = buffer.copy_bytes();
    const auto free = buffer.get_free_memory();
    const auto free_table = holes(buffer);
    const auto logical = buffer.get_size();
    const auto accessible = buffer.accessible_bytes();
    const auto epoch = buffer.epoch();
    const auto* base = buffer.get_address();
    assert(!operation());
    assert(buffer.copy_bytes() == bytes);
    assert(buffer.get_free_memory() == free && holes(buffer) == free_table);
    assert(buffer.get_size() == logical && buffer.accessible_bytes() == accessible);
    assert(buffer.get_address() == base && buffer.epoch() == epoch);
}

void tail_vector_and_string() {
    auto buffer = XBuffer::create<TailExpansionRoot>(8192, XBuffer::maximum_capacity(65536));
    auto& root = buffer.root<TailExpansionRoot>();
    const auto* base = buffer.get_address();
    const auto logical = buffer.get_size();
    const auto accessible = buffer.accessible_bytes();
    const auto epoch = buffer.epoch();
    root.values.reserve(4);
    for (std::int32_t i = 0; i < 4; ++i) root.values.push_back(i * 19);
    auto* data = root.values.data();
    auto* first = &root.values[0];
    const auto old_used = buffer.arena()->used_end;
    const auto old_free = buffer.get_free_memory();
    const auto cached = holes(buffer);
    root.values.reserve(13);
    assert(root.values.data() == data && &root.values[0] == first);
    assert(root.values.size() == 4 && root.values.capacity() == 13);
    for (std::int32_t i = 0; i < 4; ++i) assert(root.values[i] == i * 19);
    const auto increase = extent(13 * sizeof(std::int32_t)) - extent(4 * sizeof(std::int32_t));
    assert(increase > 0 && buffer.arena()->used_end == old_used + increase);
    assert(buffer.get_free_memory() + increase == old_free);
    assert(holes(buffer) == cached);
    check_allocator_end(buffer, data, 13 * sizeof(std::int32_t));

    // reserve() on an empty string creates an out-of-line block even for a
    // value that would fit inline (I1), so this exercises string tail growth.
    root.label.reserve(7);
    root.label = "seed";
    assert(!root.label.is_inline());
    auto* text = root.label.data();
    root.label.reserve(127);
    assert(root.label.data() == text && root.label == "seed");
    assert(root.label.c_str()[root.label.size()] == '\0');
    const std::string longer(101, 'q');
    root.label = longer;
    assert(root.label.data() == text && root.label == longer);
    check_allocator_end(buffer, text, 128);
    assert(buffer.get_address() == base && buffer.epoch() == epoch);
    assert(buffer.get_size() == logical && buffer.accessible_bytes() == accessible);
    detail::verify_structural_wire_graph(root, buffer);
}

void interleaved_and_nested_fallback() {
    auto buffer = XBuffer::create<TailExpansionRoot>(32768);
    auto& root = buffer.root<TailExpansionRoot>();
    root.values.reserve(4);
    root.values.push_back(11);
    auto* first = root.values.data();
    root.other.reserve(4);
    root.other.push_back(22);
    auto* second = root.other.data();
    root.values.reserve(64);
    assert(root.values.data() != first && root.values[0] == 11);
    assert(root.other.data() == second && root.other[0] == 22);
    root.other.reserve(128);
    assert(root.other.data() != second && root.other[0] == 22);
    second = root.other.data();
    root.other.reserve(256);
    assert(root.other.data() == second && root.other[0] == 22);

    root.nested.reserve(2);
    root.nested.emplace_back() = "nested allocation prevents outer tail growth";
    auto* outer = root.nested.data();
    auto* nested_text = root.nested[0].data();
    root.nested.reserve(16);
    assert(root.nested.data() != outer);
    assert(root.nested[0].data() == nested_text);
    assert(root.nested[0] == "nested allocation prevents outer tail growth");
    outer = root.nested.data();
    root.nested.reserve(32);
    assert(root.nested.data() == outer && root.nested[0].data() == nested_text);
    root.nested.emplace_back() = "second nested object";
    detail::verify_structural_wire_graph(root, buffer);
    const auto wire = buffer.save_verified<TailExpansionRoot>();
    auto loaded = XBuffer::load_verified<TailExpansionRoot>(wire);
    assert(loaded.root<TailExpansionRoot>().nested[1] == "second nested object");
}

// V1 kept a reused block's physical (header) size and grew into that slack.
// V2 never hands out a block larger than requested: reuse splits the free
// block at its front, the residual stays an exact free block, a non-tail block
// cannot grow in place, and the allocator writes no metadata into the arena.
void split_reuse_keeps_exact_extents() {
    XBufferCore buffer(4096, XBufferCore::maximum_capacity(8192));
    auto* large = static_cast<unsigned char*>(buffer.arena()->allocate_aligned(256, 8));
    std::memset(large, 0x6d, 256);
    const auto used = buffer.arena()->used_end;
    buffer.arena()->deallocate(large, 256);
    assert(buffer.free_block_count() == 1 && has_hole(buffer, large, 256));
    auto* reused = static_cast<unsigned char*>(buffer.arena()->allocate_aligned(32, 8));
    assert(reused == large && buffer.arena()->used_end == used);
    assert(buffer.free_block_count() == 1 && has_hole(buffer, large + 32, 224));
    reject_without_change(buffer, [&] { return buffer.arena()->try_expand(reused, 32, 96); });
    auto* rest = static_cast<unsigned char*>(buffer.arena()->allocate_aligned(224, 8));
    assert(rest == large + 32 && buffer.free_block_count() == 0 && buffer.arena()->used_end == used);
    // Retire/reuse kept every byte: there are no in-arena free-list headers.
    for (std::size_t i = 0; i < 256; ++i) assert(large[i] == 0x6d);

    auto* tail = static_cast<unsigned char*>(buffer.arena()->allocate_aligned(64, 8));
    assert(offset_of(buffer, tail) == used);
    std::memset(tail, 0x2a, 64);
    assert(buffer.arena()->try_expand(tail, 64, 384));
    assert(buffer.arena()->used_end == used + 384);
    for (std::size_t i = 0; i < 64; ++i) assert(tail[i] == 0x2a);
    buffer.arena()->deallocate(tail, 384);
    auto* again = buffer.arena()->allocate_aligned(320, 8);
    assert(again == tail && has_hole(buffer, tail + 320, 64));
    auto* following = buffer.arena()->allocate_aligned(64, 8);
    assert(offset_of(buffer, following) == offset_of(buffer, again) + 320);
    assert(buffer.free_block_count() == 0 && buffer.arena()->used_end == used + 384);
}

// V1 relied on a container reusing an in-arena cached block and growing into
// its header slack, then release_cached_blocks after a reload. V2: a cached
// block is split exactly for a container, growth of a non-tail block moves
// (served by the best-fit residual before the tail grows), the hole table
// carries cached blocks across save/load, and a tail block grows in place.
void cached_reuse_and_reload() {
    auto buffer = XBuffer::create<TailExpansionRoot>(8192);
    auto& root = buffer.root<TailExpansionRoot>();
    root.other.reserve(64);
    root.other.push_back(73);
    auto* donor = root.other.data();
    const auto binding = root.other.binding();
    const auto used = buffer.arena()->used_end;
    assert(offset_of(buffer, donor) + 256 == used);
    std::destroy_at(&root.other);
    std::construct_at(&root.other, binding);
    assert(buffer.free_block_count() == 1 && has_hole(buffer, donor, 256));
    root.values.reserve(4);
    assert(root.values.data() == donor && has_hole(buffer, at(donor, 16), 240));
    root.values.push_back(89);
    root.values.reserve(32);
    assert(root.values[0] == 89 && root.values.data() == reinterpret_cast<const std::int32_t*>(at(donor, 16)));
    assert(buffer.arena()->used_end == used);
    assert(has_hole(buffer, donor, 16) && has_hole(buffer, at(donor, 16 + 128), 112));
    root.values.reserve(96);
    assert(root.values[0] == 89 && offset_of(buffer, root.values.data()) == used);
    check_allocator_end(buffer, root.values.data(), 384);
    auto* tail_data = root.values.data();
    root.values.reserve(128);
    assert(root.values.data() == tail_data && root.values[0] == 89);
    check_allocator_end(buffer, tail_data, 512);
    detail::verify_structural_wire_graph(root, buffer);
    const auto saved_holes = holes(buffer);
    const auto wire = buffer.save_verified<TailExpansionRoot>();
    auto loaded = XBuffer::load_verified<TailExpansionRoot>(wire);
    assert(loaded.root<TailExpansionRoot>().values[0] == 89);
    assert(holes(loaded) == saved_holes);

    assert(loaded.grow(8192));
    auto& values = loaded.root<TailExpansionRoot>().values;
    auto* loaded_data = values.data();
    values.reserve(192);
    assert(values.data() == loaded_data && values[0] == 89);
    check_allocator_end(loaded, loaded_data, 768);
    // A block cached before the save is reused after the reload.
    auto& label = loaded.root<TailExpansionRoot>().label;
    label.reserve(15);
    assert(offset_of(loaded, label.data()) == offset_of(buffer, donor));
    const auto live_binding = values.binding();
    std::destroy_at(&values);
    std::construct_at(&values, live_binding);
    assert(has_hole(loaded, loaded_data, 768));
    auto& other = loaded.root<TailExpansionRoot>().other;
    other.reserve(160);
    assert(other.data() == loaded_data && has_hole(loaded, at(loaded_data, 640), 128));
    other.push_back(101);
    assert(other[0] == 101);
    detail::verify_structural_wire_graph(loaded.root<TailExpansionRoot>(), loaded);
    auto again = XBuffer::load_verified<TailExpansionRoot>(loaded.save_verified<TailExpansionRoot>());
    assert(again.root<TailExpansionRoot>().other[0] == 101);
}

void bounds_alignment_and_failure_atomicity() {
    XBufferCore buffer(4096, XBufferCore::maximum_capacity(8192));
    auto* first = buffer.arena()->allocate_aligned(64, 64);
    auto* tail = buffer.arena()->allocate_aligned(64, 64);
    std::memset(tail, 0x42, 64);
    reject_without_change(buffer, [&] { return buffer.arena()->try_expand(first, 64, 128); });
    // The alignment-checked entry point rejects invalid alignments; V2 blocks
    // are 8-aligned, so a stricter in-place expansion request is refused too.
    const auto ref = fixed_detail::allocator_ref_of(buffer.arena());
    for (const auto alignment : {std::size_t(0), std::size_t(3), std::size_t(1) << 32})
        reject_without_change(buffer, [&] { return fixed_detail::try_expand_dynamic_bytes(ref, tail, 64, 128, alignment); });
    reject_without_change(buffer, [&] {
        return buffer.arena()->try_expand(tail, 64, std::numeric_limits<std::uint32_t>::max());
    });
    const auto remaining = buffer.get_size() - offset_of(buffer, tail);
    reject_without_change(buffer, [&] {
        return buffer.arena()->try_expand(tail, 64, static_cast<std::uint32_t>(remaining + 1));
    });
    const auto accessible = buffer.accessible_bytes();
    assert(buffer.arena()->try_expand(tail, 64, static_cast<std::uint32_t>(remaining)));
    assert(buffer.arena()->used_end == buffer.get_size());
    assert(buffer.get_size() == 4096 && buffer.accessible_bytes() == accessible);
    for (std::size_t i = 0; i < 64; ++i) assert(static_cast<unsigned char*>(tail)[i] == 0x42);
    assert(buffer.grow(4096));
    assert(buffer.arena()->try_expand(tail, static_cast<std::uint32_t>(remaining),
                                      static_cast<std::uint32_t>(remaining + 4096)));
    assert(buffer.arena()->used_end == 8192);

    auto limited = XBuffer::create<TailExpansionRoot>(4096, XBuffer::maximum_capacity(4096));
    auto& values = limited.root<TailExpansionRoot>().values;
    values.reserve(4);
    values.push_back(131);
    const auto before = limited.copy_bytes();
    const auto used = limited.arena()->used_end;
    bool rejected = false;
    try { values.reserve(4096); } catch (const XBadAlloc&) { rejected = true; }
    assert(rejected && limited.copy_bytes() == before && limited.arena()->used_end == used);
    values.push_back(137);
    assert(values[0] == 131 && values[1] == 137);

    auto string_buffer = XBuffer::create<TailExpansionRoot>(4096, XBuffer::maximum_capacity(8192));
    auto& text = string_buffer.root<TailExpansionRoot>().label;
    // I1: a short value is inline; a failed move to the heap leaves it inline.
    text = "seed";
    assert(text.is_inline());
    const auto inline_before = string_buffer.copy_bytes();
    rejected = false;
    try { text.reserve(5000); } catch (const XBadAlloc&) { rejected = true; }
    assert(rejected && string_buffer.copy_bytes() == inline_before && text.is_inline() && text == "seed");
    // An out-of-line value at the tail keeps its address across failed and
    // successful tail growth. A heap string keeps its buffer for short values.
    text = std::string(11, 'x');
    text = "seed";
    assert(!text.is_inline() && text.capacity() == 11);
    auto* text_data = text.data();
    const auto before_string = string_buffer.copy_bytes();
    rejected = false;
    try { text.reserve(5000); } catch (const XBadAlloc&) { rejected = true; }
    assert(rejected && string_buffer.copy_bytes() == before_string);
    assert(text.data() == text_data && text == "seed" && text.c_str()[4] == '\0');
    assert(string_buffer.grow(4096));
    text.reserve(5000);
    assert(text.data() == text_data && text == "seed" && text.c_str()[4] == '\0');
    check_allocator_end(string_buffer, text_data, 5001);
}

void large_alignment_is_preserved() {
    auto buffer = XBuffer::create<TailAlignedRoot>(1024 * 1024);
    auto& values = buffer.root<TailAlignedRoot>().values;
    values.reserve(1);
    values.emplace_back().value = 211;
    const auto ptr_address = reinterpret_cast<std::uintptr_t>(values.data());
    assert(ptr_address % alignof(TailAlignedValue) == 0);
    values.reserve(3);
    assert(reinterpret_cast<std::uintptr_t>(values.data()) == ptr_address && values[0].value == 211);
    values.emplace_back().value = 223;
    assert(reinterpret_cast<std::uintptr_t>(&values[1]) == ptr_address + sizeof(TailAlignedValue) && values[1].value == 223);
    check_allocator_end(buffer, values.data(), 3 * sizeof(TailAlignedValue));
}

void transport_and_graph_admission() {
    auto buffer = XBuffer::create<TailExpansionRoot>(8192);
    auto& root = buffer.root<TailExpansionRoot>();
    root.id = 307;
    root.values.reserve(4);
    auto* data = root.values.data();
    for (std::int32_t i = 0; i < 4; ++i) root.values.push_back(i * 7);
    root.values.reserve(128);
    assert(root.values.data() == data);
    for (std::int32_t i = 4; i < 128; ++i) root.values.push_back(i * 7);
    std::string wire = buffer.save_verified<TailExpansionRoot>();
    for (std::uint32_t round = 0; round < 3; ++round) {
        auto loaded = XBuffer::load_verified<TailExpansionRoot>(wire);
        auto& current = loaded.root<TailExpansionRoot>();
        assert(current.id == 307);
        assert(loaded.grow(8192));
        auto* old_data = current.values.data();
        const auto count = current.values.size();
        current.values.reserve(current.values.capacity() + 128);
        assert(current.values.data() == old_data);
        for (std::uint32_t i = 0; i < count; ++i) assert(current.values[i] == static_cast<std::int32_t>(i * 7));
        for (std::uint32_t i = count; i < count + 128; ++i) current.values.push_back(static_cast<std::int32_t>(i * 7));
        check_allocator_end(loaded, old_data, current.values.capacity() * sizeof(std::int32_t));
        detail::verify_structural_wire_graph(current, loaded);
        wire = loaded.save_verified<TailExpansionRoot>();
    }
    auto valid = XBuffer::load_verified<TailExpansionRoot>(wire);
    // V1 corrupted the block header's user_bytes. V2 derives the extent from
    // the head: shrinking or growing capacity_ must break exact coverage.
    XWireHeaderV2 header{};
    std::memcpy(&header, wire.data(), sizeof(header));
    assert(header.extent_count == 0 && header.hole_count == 0); // dense: payload = bytes [96, 96 + used)
    auto& live = valid.root<TailExpansionRoot>();
    const auto head = sizeof(XWireHeaderV2) + offset_of(valid, &live.values);
    const auto size_at = head + offsetof(XFixedVector<std::int32_t>, size_);
    const auto capacity_at = head + offsetof(XFixedVector<std::int32_t>, capacity_);
    std::uint32_t capacity = 0;
    std::memcpy(&capacity, wire.data() + capacity_at, sizeof(capacity));
    assert(capacity == live.values.capacity() && capacity >= 4);
    for (const std::uint32_t forged : {std::uint32_t(2), capacity + 2}) {
        auto broken = wire;
        const std::uint32_t forged_size = std::min(forged, capacity);
        std::memcpy(broken.data() + size_at, &forged_size, sizeof(forged_size));
        std::memcpy(broken.data() + capacity_at, &forged, sizeof(forged));
        bool rejected = false;
        try { (void)XBuffer::load_verified<TailExpansionRoot>(broken); }
        catch (const XException&) { rejected = true; }
        assert(rejected);
    }
}

} // namespace

int main() {
    tail_vector_and_string();
    interleaved_and_nested_fallback();
    split_reuse_keeps_exact_extents();
    cached_reuse_and_reload();
    bounds_alignment_and_failure_atomicity();
    transport_and_graph_admission();
    // Last: currently aborts on a V2 library defect (over-aligned blocks are
    // aligned relative to the payload offset, not the address; see report).
    large_alignment_is_preserved();
    std::cout << "Tail expansion tests passed\n";
}
