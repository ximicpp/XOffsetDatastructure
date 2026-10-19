// Tail growth must preserve allocations and wire metadata without committing
// additional logical capacity. The arena primitive requires a live allocation.
#include <array>
#include <cassert>
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

using Header = XBufferCore::AllocationHeader;

Header allocation_header(const void* ptr) {
    Header header{};
    std::memcpy(&header, static_cast<const char*>(ptr) - sizeof(Header), sizeof(Header));
    return header;
}

std::size_t offset_of(const XBufferCore& buffer, const void* ptr) {
    return reinterpret_cast<std::uintptr_t>(ptr) -
           reinterpret_cast<std::uintptr_t>(buffer.get_address());
}

void check_allocator_end(XBufferCore& buffer, const void* ptr) {
    const auto header = allocation_header(ptr);
    assert(header.total_bytes == sizeof(Header) + header.user_bytes);
    assert(header.user_offset == offset_of(buffer, ptr));
    assert(buffer.arena()->used_end == header.user_offset + header.user_bytes);
    auto* state = buffer.find_allocator_storage<XAllocatorStateV1>();
    assert(state);
    const auto from_state = reinterpret_cast<const char*>(ptr) -
                            reinterpret_cast<const char*>(state);
    assert(from_state >= 0);
    assert(state->arena_end >= static_cast<std::size_t>(from_state) + header.user_bytes);
}

template <typename Operation>
void reject_without_change(XBufferCore& buffer, Operation operation) {
    const auto bytes = buffer.copy_bytes();
    const auto free = buffer.get_free_memory();
    const auto logical = buffer.get_size();
    const auto accessible = buffer.accessible_bytes();
    const auto epoch = buffer.epoch();
    const auto* base = buffer.get_address();
    assert(!operation());
    assert(buffer.copy_bytes() == bytes);
    assert(buffer.get_free_memory() == free);
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
    const auto old_header = allocation_header(data);
    const auto old_used = buffer.arena()->used_end;
    const auto old_free = buffer.get_free_memory();
    auto* state = root.values.allocator_state();
    const auto cached_free = state->free_bytes;
    root.values.reserve(13);
    assert(root.values.data() == data && &root.values[0] == first);
    assert(root.values.size() == 4 && root.values.capacity() == 13);
    for (std::int32_t i = 0; i < 4; ++i) assert(root.values[i] == i * 19);
    const auto new_header = allocation_header(data);
    const auto increase = new_header.user_bytes - old_header.user_bytes;
    assert(increase > 0 && buffer.arena()->used_end == old_used + increase);
    assert(buffer.get_free_memory() + increase == old_free);
    assert(state->free_bytes == cached_free);
    check_allocator_end(buffer, data);

    root.label.reserve(7);
    root.label = "seed";
    auto* text = root.label.data();
    root.label.reserve(127);
    assert(root.label.data() == text && root.label == "seed");
    assert(root.label.c_str()[root.label.size()] == '\0');
    const std::string longer(101, 'q');
    root.label = longer;
    assert(root.label.data() == text && root.label == longer);
    check_allocator_end(buffer, text);
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

void physical_size_survives_core_reuse() {
    XBufferCore buffer(4096, XBufferCore::maximum_capacity(8192));
    auto* large = static_cast<unsigned char*>(buffer.arena()->allocate_aligned(256, 64));
    std::memset(large, 0x6d, 256);
    const auto used = buffer.arena()->used_end;
    buffer.arena()->deallocate(large);
    auto* reused = static_cast<unsigned char*>(buffer.arena()->allocate_aligned(32, 64));
    assert(reused == large);
    assert(allocation_header(reused).user_bytes == 256);
    assert(buffer.arena()->try_expand(reused, 96, 64));
    assert(allocation_header(reused).user_bytes == 256 && buffer.arena()->used_end == used);
    assert(buffer.arena()->try_expand(reused, 384, 64));
    assert(allocation_header(reused).user_bytes == 384 && buffer.arena()->used_end == used + 128);
    for (std::size_t i = 0; i < 256; ++i) assert(reused[i] == 0x6d);
    buffer.arena()->deallocate(reused);
    auto* again = buffer.arena()->allocate_aligned(320, 64);
    assert(again == reused && allocation_header(again).user_bytes == 384);
    auto* following = buffer.arena()->allocate_aligned(64, 64);
    assert(offset_of(buffer, following) >= offset_of(buffer, again) + 384 + sizeof(Header));
}

void physical_size_survives_cached_reuse() {
    auto buffer = XBuffer::create<TailExpansionRoot>(8192);
    auto& root = buffer.root<TailExpansionRoot>();
    root.other.reserve(64);
    root.other.push_back(73);
    auto* donor = root.other.data();
    const auto binding = root.other.binding();
    const auto used = buffer.arena()->used_end;
    std::destroy_at(&root.other);
    std::construct_at(&root.other, binding);
    assert(binding.state->free_bytes == 256);
    root.values.reserve(4);
    assert(root.values.data() == donor && binding.state->free_bytes == 0);
    root.values.push_back(89);
    root.values.reserve(32);
    assert(root.values.data() == donor && root.values[0] == 89);
    assert(allocation_header(donor).user_bytes == 256 && buffer.arena()->used_end == used);
    root.values.reserve(96);
    assert(root.values.data() == donor && root.values[0] == 89);
    assert(allocation_header(donor).user_bytes == 384 && buffer.arena()->used_end == used + 128);
    check_allocator_end(buffer, donor);
    detail::verify_structural_wire_graph(root, buffer);
    const auto wire = buffer.save_verified<TailExpansionRoot>();
    auto loaded = XBuffer::load_verified<TailExpansionRoot>(wire);
    assert(loaded.root<TailExpansionRoot>().values[0] == 89);

    assert(loaded.grow(8192));
    auto& values = loaded.root<TailExpansionRoot>().values;
    auto* loaded_data = values.data();
    values.reserve(192);
    assert(values.data() == loaded_data && values[0] == 89);
    check_allocator_end(loaded, loaded_data);
    const auto live_binding = values.binding();
    std::destroy_at(&values);
    std::construct_at(&values, live_binding);
    assert(live_binding.state->free_bytes == 768);
    fixed_detail::release_cached_blocks(live_binding.state);
    assert(live_binding.state->free_bytes == 0);
    auto& other = loaded.root<TailExpansionRoot>().other;
    other.reserve(160);
    assert(other.data() == loaded_data && allocation_header(other.data()).user_bytes == 768);
    other.push_back(101);
    other.reserve(256);
    assert(other.data() == loaded_data && other[0] == 101);
    detail::verify_structural_wire_graph(loaded.root<TailExpansionRoot>(), loaded);
}

void bounds_alignment_and_failure_atomicity() {
    XBufferCore buffer(4096, XBufferCore::maximum_capacity(8192));
    auto* first = buffer.arena()->allocate_aligned(64, 64);
    auto* tail = buffer.arena()->allocate_aligned(64, 64);
    std::memset(tail, 0x42, 64);
    reject_without_change(buffer, [&] { return buffer.arena()->try_expand(first, 128, 64); });
    for (const auto alignment : {std::size_t(0), std::size_t(3), std::size_t(1) << 32})
        reject_without_change(buffer, [&] { return buffer.arena()->try_expand(tail, 128, alignment); });
    reject_without_change(buffer, [&] {
        return buffer.arena()->try_expand(tail, std::numeric_limits<std::uint32_t>::max(), 64);
    });
    const auto remaining = buffer.get_size() - offset_of(buffer, tail);
    reject_without_change(buffer, [&] {
        return buffer.arena()->try_expand(tail, static_cast<std::uint32_t>(remaining + 1), 64);
    });
    const auto accessible = buffer.accessible_bytes();
    assert(buffer.arena()->try_expand(tail, static_cast<std::uint32_t>(remaining), 64));
    assert(buffer.arena()->used_end == buffer.get_size());
    assert(buffer.get_size() == 4096 && buffer.accessible_bytes() == accessible);
    for (std::size_t i = 0; i < 64; ++i) assert(static_cast<unsigned char*>(tail)[i] == 0x42);
    assert(buffer.grow(4096));
    assert(buffer.arena()->try_expand(tail, static_cast<std::uint32_t>(remaining + 4096), 64));
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
    text = "seed";
    auto* text_data = text.data();
    const auto before_string = string_buffer.copy_bytes();
    rejected = false;
    try { text.reserve(5000); } catch (const XBadAlloc&) { rejected = true; }
    assert(rejected && string_buffer.copy_bytes() == before_string);
    assert(text.data() == text_data && text == "seed" && text.c_str()[4] == '\0');
    assert(string_buffer.grow(4096));
    text.reserve(5000);
    assert(text.data() == text_data && text == "seed" && text.c_str()[4] == '\0');
    check_allocator_end(string_buffer, text_data);
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
    assert(allocation_header(values.data()).alignment >= alignof(TailAlignedValue));
    check_allocator_end(buffer, values.data());
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
        check_allocator_end(loaded, old_data);
        detail::verify_structural_wire_graph(current, loaded);
        wire = loaded.save_verified<TailExpansionRoot>();
    }
    auto valid = XBuffer::load_verified<TailExpansionRoot>(wire);
    const auto data_offset = offset_of(valid, valid.root<TailExpansionRoot>().values.data());
    Header broken = allocation_header(valid.root<TailExpansionRoot>().values.data());
    broken.user_bytes = 8;
    broken.total_bytes = sizeof(Header) + 8;
    std::memcpy(wire.data() + sizeof(XWireHeaderV1) + data_offset - sizeof(Header), &broken, sizeof(broken));
    bool rejected = false;
    try { (void)XBuffer::load_verified<TailExpansionRoot>(wire); }
    catch (const XException&) { rejected = true; }
    assert(rejected);
}

} // namespace

int main() {
    tail_vector_and_string();
    interleaved_and_nested_fallback();
    physical_size_survives_core_reuse();
    physical_size_survives_cached_reuse();
    bounds_alignment_and_failure_atomicity();
    large_alignment_is_preserved();
    transport_and_graph_admission();
    std::cout << "Tail expansion tests passed\n";
}
