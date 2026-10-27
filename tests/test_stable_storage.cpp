// Integration coverage for the lifetime of an address-stable XBuffer storage.
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct StableItem {
    std::int32_t code{};
    XString label;
};

struct StableScalar {
    std::int64_t value{};
};

template <std::size_t Alignment>
struct alignas(Alignment) StableOveralignedRoot {
    std::int64_t value{};
};

static_assert(is_byte_copy_safe_v<StableOveralignedRoot<65536>>);
static_assert(is_byte_copy_safe_v<StableOveralignedRoot<131072>>);

struct OtherStableRoot {
    std::int32_t id{};
};

struct StableRoot {
    std::int32_t id{};
    std::int64_t independent_values[8]{};
    XString name;
    XVector<std::int32_t> numbers;
    XVector<StableItem> items;
};

XOFFSET_REGISTER_SCHEMA_NAME(StableRoot, "test.StableRoot")
XOFFSET_REGISTER_SCHEMA_NAME(OtherStableRoot, "test.OtherStableRoot")

static_assert(std::is_same_v<decltype(std::declval<XBuffer&>().get_buffer()),
                             std::span<char>>);
static_assert(std::is_same_v<decltype(std::declval<const XBuffer&>().get_buffer()),
                             std::span<const char>>);

namespace {
// This file checks VM address stability, page permissions and reservations,
// so every bounded buffer asks for Pinned (VM) storage explicitly.
XBufferCore::MaximumCapacity stable_capacity(std::size_t bytes) {
    return XBufferCore::maximum_capacity(bytes).with_storage(XStorage::Pinned);
}

std::size_t system_page_size() {
#if defined(_WIN32)
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwPageSize;
#else
    const auto size = sysconf(_SC_PAGESIZE);
    assert(size > 0);
    return static_cast<std::size_t>(size);
#endif
}

template <typename Operation>
void require_rejection(Operation&& operation) {
    bool rejected = false;
    try {
        operation();
    } catch (const XException&) {
        rejected = true;
    }
    assert(rejected);
}

// V2 keeps allocator state on the host: the arena header plus the host-side
// free table replace the V1 in-arena XAllocatorStateV1 snapshot.
struct AllocatorSnapshot {
    XArena arena;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> holes;
};
AllocatorSnapshot allocator_snapshot(const XBufferCore& buffer) {
    AllocatorSnapshot snapshot{*buffer.arena(), {}};
    buffer.for_each_free_block([&](XBufferCore::WireHole hole) {
        snapshot.holes.emplace_back(hole.offset, hole.bytes);
    });
    return snapshot;
}
bool same_allocator_state(const XBufferCore& buffer, const AllocatorSnapshot& before) {
    const auto now = allocator_snapshot(buffer);
    return std::memcmp(&now.arena, &before.arena, sizeof(XArena)) == 0 && now.holes == before.holes &&
        &XBufferCore::owner_from_arena(buffer.arena()) == &buffer;
}
bool has_hole(const XBufferCore& buffer, const void* ptr, std::uint32_t bytes) {
    const auto offset = static_cast<std::uint32_t>(static_cast<const char*>(ptr) - buffer.data());
    bool found = false;
    buffer.for_each_free_block([&](XBufferCore::WireHole hole) { found |= hole.offset == offset && hole.bytes == bytes; });
    return found;
}
// V2.1 verified frame: header + payload bytes [48, used) + hole table (dense
// unless a free run of at least 4 KiB makes it sparse; these fixtures never
// have one).
XWireHeaderV2 check_verified_frame(const std::string& frame, const XBufferCore& buffer) {
    XWireHeaderV2 header{};
    std::memcpy(&header, frame.data(), sizeof(header));
    assert(header.extent_count == 0);
    assert(header.used_bytes == buffer.segment_size());
    assert(header.hole_count == buffer.free_block_count());
    assert(frame.size() == sizeof(header) + header.used_bytes - XWIRE_PAYLOAD_BEGIN +
                               header.hole_count * sizeof(XBufferCore::WireHole));
    return header;
}

void check_extents(const XBuffer& buffer) {
    assert(buffer.get_size() == buffer.segment_size());
    assert(buffer.maximum_capacity() == buffer.reserved_capacity());
    assert(buffer.get_size() <= buffer.reserved_capacity());
    assert(buffer.get_size() <= buffer.accessible_bytes());
    assert(buffer.accessible_bytes() <= buffer.reserved_bytes());
    // A movable slot reserves only its slot; it moves to reach its maximum.
    assert(!buffer.address_stable() || buffer.reserved_capacity() <= buffer.reserved_bytes());
    assert(buffer.bytes().size_bytes() == buffer.get_size());
    assert(buffer.get_buffer().size() == buffer.get_size());
    assert(buffer.get_buffer().data() == buffer.get_address());
    assert(buffer.data() == buffer.get_address());
}

void fill_root(StableRoot& root, std::int32_t id) {
    root.id = id;
    root.name = "stable root";
    for (std::size_t i = 0; i < 8; ++i) root.independent_values[i] = id + i * 101;
    root.numbers.reserve(32);
    for (std::int32_t i = 0; i < 32; ++i) root.numbers.push_back(id + i * 3);
    root.items.reserve(12);
    for (std::int32_t i = 0; i < 12; ++i) {
        auto& item = root.items.emplace_back();
        item.code = id + i;
        item.label = "nested " + std::to_string(i);
    }
}

void check_root(const StableRoot& root, std::int32_t id) {
    assert(root.id == id);
    assert(root.name == "stable root");
    for (std::size_t i = 0; i < 8; ++i)
        assert(root.independent_values[i] == id + static_cast<std::int64_t>(i * 101));
    assert(root.numbers.size() == 32);
    for (std::int32_t i = 0; i < 32; ++i) assert(root.numbers[i] == id + i * 3);
    assert(root.items.size() == 12);
    for (std::int32_t i = 0; i < 12; ++i) {
        assert(root.items[i].code == id + i);
        assert(root.items[i].label == "nested " + std::to_string(i));
    }
}

void page_boundaries_and_capacity() {
    const auto page = system_page_size();
    const auto maximum = page * 3 + 17;
    XBuffer buffer(512, stable_capacity(maximum));
    auto* root = buffer.make<StableScalar>();
    root->value = 123456789;
    auto handle = buffer.handle<StableScalar>(); // Locally constructed types need no schema name.
    assert(handle.get() == root);
    const auto* base = buffer.get_address();
    const auto epoch = buffer.epoch();
    const auto reservation = buffer.reserved_bytes();
    const auto accessible = buffer.accessible_bytes();
    assert(buffer.reserved_capacity() == maximum);
    assert(reservation >= maximum);
    assert(accessible < reservation);
    assert(accessible % page == 0);

    const auto original_size = buffer.get_size();
    assert(buffer.grow(0));
    assert(buffer.get_size() == original_size);
    assert(buffer.accessible_bytes() == accessible);
    assert(buffer.grow(1));
    assert(buffer.get_size() == original_size + 1);
    assert(buffer.accessible_bytes() == accessible);

    const std::array targets{page - 1, page, page + 1, page * 2, maximum};
    for (const auto target : targets) {
        const auto before = buffer.get_size();
        assert(target >= before);
        assert(buffer.grow(target - before));
        assert(buffer.get_size() == target);
        assert(buffer.get_address() == base);
        assert(buffer.reserved_bytes() == reservation);
        assert(buffer.epoch() == epoch);
        assert(root == &buffer.root<StableScalar>());
        assert(handle.get() == root);
        assert(root->value == 123456789);
        assert(std::all_of(buffer.get_buffer().begin() + before,
                           buffer.get_buffer().end(), [](char byte) { return byte == 0; }));
        check_extents(buffer);
    }

    const auto accessible_at_limit = buffer.accessible_bytes();
    assert(!buffer.grow(1));
    assert(!buffer.grow(std::numeric_limits<std::size_t>::max()));
    assert(buffer.get_size() == maximum);
    assert(buffer.accessible_bytes() == accessible_at_limit);
    assert(buffer.get_address() == base);
    assert(buffer.epoch() == epoch);
    assert(root->value == 123456789);
    // Page rounding grants access to extra bytes, but cannot increase the hard limit.
    assert(buffer.accessible_bytes() > buffer.reserved_capacity());
    check_extents(buffer);
    auto copy = buffer.copy_bytes();
    assert(copy.size() == buffer.get_size());
    assert(std::memcmp(copy.data(), buffer.data(), copy.size()) == 0);
    copy[0] = 'Q';
    assert(buffer.data()[0] == 'X');

    require_rejection([&] {
        XBuffer invalid(maximum + 1, stable_capacity(maximum));
    });
    require_rejection([&] {
        XBuffer invalid(512, stable_capacity(
            std::numeric_limits<std::size_t>::max()));
    });
    require_rejection([&] {
        XBuffer invalid(std::numeric_limits<std::size_t>::max());
    });

    // The historical max_capacity parameter remains a reservation hint. It
    // does not pin: a small hinted buffer starts on a movable slot (R1).
    XBuffer hinted(512, XBuffer::max_capacity(512));
    assert(hinted.reserved_capacity() >= XBuffer::DEFAULT_MAXIMUM_CAPACITY);
    assert(hinted.reserved_capacity() >= 512);
    const auto* hinted_base = hinted.get_address();
    const auto hinted_epoch = hinted.epoch();
    assert(hinted.grow(std::min(page * 2, hinted.reserved_capacity() - hinted.get_size())));
    assert(hinted.address_stable() ? hinted.get_address() == hinted_base
                                   : hinted.epoch() != hinted_epoch);
}

template <std::size_t Alignment>
void check_overaligned_root(XBuffer& buffer, std::int64_t value) {
    using Root = StableOveralignedRoot<Alignment>;
    auto* root = buffer.make<Root>();
    assert(reinterpret_cast<std::uintptr_t>(root) % Alignment == 0);
    root->value = value;
    auto handle = buffer.handle<Root>();
    assert(handle.get() == root);
    assert(&buffer.root<Root>() == root);
    const auto& core = static_cast<const XBufferCore&>(buffer);
    assert(core.find_root_storage<Root>() == root);
    const auto root_offset = buffer.arena()->root_offset;
    buffer.arena()->root_offset += 8;
    assert(buffer.find_root_storage<Root>() == nullptr);
    assert(core.find_root_storage<Root>() == nullptr);
    buffer.arena()->root_offset = root_offset;
    const auto* base = buffer.get_address();
    const auto epoch = buffer.epoch();
    assert(buffer.grow(system_page_size() + 1));
    assert(buffer.get_address() == base && buffer.epoch() == epoch);
    assert(handle.get() == root && &buffer.root<Root>() == root);
    assert(core.find_root_storage<Root>() == root && root->value == value);
    // Local byte-copy-safe roots require no schema name. The verified wire ABI
    // deliberately has a stricter alignment limit and is not exercised here.
}

void actual_addresses_honor_large_alignment() {
    constexpr std::array<std::size_t, 2> alignments{65536, 131072};
    const auto page = system_page_size();
    std::array<XBuffer, 16> buffers;
    unsigned unaligned_bases = 0;
    for (std::size_t instance = 0; instance < buffers.size(); ++instance) {
        auto& buffer = buffers[instance];
        // Keep odd-page reservations alive together so the OS can supply bases
        // with different residues; a single lucky aligned mapping hid this bug.
        buffer = XBuffer(alignments.back() * 5,
                         stable_capacity(page * 257));
        const auto base = reinterpret_cast<std::uintptr_t>(buffer.get_address());
        if (base % alignments.back() != 0) ++unaligned_bases;
        std::array<char*, 2> allocations{};
        for (std::size_t i = 0; i < alignments.size(); ++i) {
            allocations[i] = static_cast<char*>(buffer.arena()->allocate_aligned(32, alignments[i]));
            assert(reinterpret_cast<std::uintptr_t>(allocations[i]) % alignments[i] == 0);
            std::memset(allocations[i], static_cast<int>(instance + i + 1), 32);
        }
        const auto used_end = buffer.arena()->used_end;
        const auto epoch = buffer.epoch();
        assert(buffer.grow(page + 1));
        assert(reinterpret_cast<std::uintptr_t>(buffer.get_address()) == base);
        assert(buffer.arena()->used_end == used_end && buffer.epoch() == epoch);
        for (std::size_t i = 0; i < alignments.size(); ++i) {
            const auto byte = static_cast<char>(instance + i + 1);
            assert(std::all_of(allocations[i], allocations[i] + 32,
                               [byte](char found) { return found == byte; }));
            // V2: a retired block is an exact 32-byte hole in the host free
            // table; free blocks serve only 8-aligned requests, so stricter
            // requests are placed at the tail and must still be aligned.
            buffer.arena()->deallocate(allocations[i], 32);
            assert(has_hole(buffer, allocations[i], 32));
            auto* same = buffer.arena()->allocate_aligned(32, alignments[i]);
            assert(reinterpret_cast<std::uintptr_t>(same) % alignments[i] == 0);
            buffer.arena()->deallocate(same, 32);
            auto* smaller = buffer.arena()->allocate_aligned(32, alignments[i] / 2);
            assert(reinterpret_cast<std::uintptr_t>(smaller) % (alignments[i] / 2) == 0);
            buffer.arena()->deallocate(smaller, 32);
        }
        // The retired over-aligned blocks are reused for an 8-aligned request.
        {
            const auto used_before = buffer.arena()->used_end;
            const auto holes_before = allocator_snapshot(buffer).holes;
            auto* reused = static_cast<char*>(buffer.arena()->allocate_aligned(32, 8));
            const auto offset = static_cast<std::uint32_t>(reused - buffer.data());
            assert(std::find(holes_before.begin(), holes_before.end(),
                             std::pair<std::uint32_t, std::uint32_t>(offset, 32)) != holes_before.end());
            assert(buffer.arena()->used_end == used_before && !has_hole(buffer, reused, 32));
            buffer.arena()->deallocate(reused, 32);
        }
        if (instance % 2 == 0) check_overaligned_root<65536>(buffer, 1234 + instance);
        else check_overaligned_root<131072>(buffer, 1234 + instance);
    }
    // OS placement is not a portable guarantee. Check every pointer regardless,
    // and record whether this run also covered a non-aligned reservation base.
    std::cout << "Large-alignment mappings with non-aligned bases: " << unaligned_bases << '\n';

    XBufferCore exact(512, stable_capacity(alignments.back() * 2));
    const auto base = reinterpret_cast<std::uintptr_t>(exact.get_address());
    const auto first = base + exact.arena()->used_end; // V2: blocks have no header
    const auto address = (first + alignments.back() - 1) & ~(alignments.back() - 1);
    const auto required = address - base + 32;
    assert(required > exact.get_size());
    assert(exact.grow(required - exact.get_size() - 1));
    const auto used_end = exact.arena()->used_end;
    require_rejection([&] { exact.arena()->allocate_aligned(32, alignments.back()); });
    assert(exact.arena()->used_end == used_end && exact.get_size() == required - 1);
    assert(exact.grow(1));
    auto* allocation = exact.arena()->allocate_aligned(32, alignments.back());
    assert(reinterpret_cast<std::uintptr_t>(allocation) == address);
    assert(exact.arena()->used_end == required);

    XBufferCore limited(512, stable_capacity(512));
    const auto before = *limited.arena();
    for (const auto alignment : {std::size_t(0), std::size_t(65537),
                                std::size_t{1} << 31,
                                std::numeric_limits<std::size_t>::max(), alignments.back()}) {
        require_rejection([&] { limited.arena()->allocate_aligned(32, alignment); });
        assert(std::memcmp(limited.arena(), &before, sizeof(before)) == 0);
        assert(limited.get_size() == 512 && !limited.grow(1));
    }
    // sizeof(T) must be checked before conversion to the arena's uint32_t byte
    // count. This type is never instantiated and requires no large reservation.
    struct OversizedRoot { char bytes[std::size_t{1} << 32]; };
    require_rejection([&] { limited.construct_root_storage<OversizedRoot>(); });
    assert(std::memcmp(limited.arena(), &before, sizeof(before)) == 0);
    assert(limited.arena()->root_offset == 0);
}

void objects_and_relative_references_survive_growth() {
    const auto page = system_page_size();
    XBuffer buffer(page * 4, stable_capacity(page * 64));
    auto* root = buffer.make<StableRoot>();
    fill_root(*root, 73);
    auto handle = buffer.handle<StableRoot>();
    auto allocator = buffer.arena_allocator<std::int64_t>();
    auto* independent = allocator.allocate(8);
    for (std::size_t i = 0; i < 8; ++i) std::construct_at(independent + i, 9000 + i);
    const auto* base = buffer.get_address();
    const auto* name = root->name.data();
    const auto* numbers = root->numbers.data();
    const auto* items = root->items.data();
    const auto state_before = allocator_snapshot(buffer);
    const auto epoch = buffer.epoch();

    for (std::size_t extra : {std::size_t(1), page - 1, page + 3, page * 7}) {
        assert(buffer.grow(extra));
        assert(buffer.get_address() == base);
        assert(&buffer.root<StableRoot>() == root);
        assert(handle.get() == root);
        assert(root->name.data() == name);
        assert(root->numbers.data() == numbers);
        assert(root->items.data() == items);
        assert(same_allocator_state(buffer, state_before));
        assert(buffer.epoch() == epoch);
        check_root(*root, 73);
        for (std::size_t i = 0; i < 8; ++i) assert(independent[i] == 9000 + i);
        check_extents(buffer);
    }
    auto* later = allocator.allocate(1);
    std::construct_at(later, 777);
    assert(*later == 777);
    allocator.deallocate(later, 1);
    allocator.deallocate(independent, 8);

    // Arena stability does not prevent a container from allocating a new data block.
    const auto old_numbers = root->numbers.data();
    root->numbers.reserve(512);
    assert(root->numbers.data() != old_numbers);
    check_root(*root, 73);
    const auto old_name = root->name.data();
    root->name.reserve(512);
    assert(root->name.data() != old_name);
    assert(root->name == "stable root");
    assert(buffer.get_address() == base && &buffer.root<StableRoot>() == root);

    require_rejection([&] { root->numbers.reserve(std::numeric_limits<std::size_t>::max()); });
    require_rejection([&] { allocator.allocate(std::numeric_limits<std::size_t>::max()); });
    check_root(*root, 73);
}

void shrink_regrow_and_transport() {
    const auto page = system_page_size();
    XBuffer buffer(page * 4, stable_capacity(page * 32 + 17));
    auto* root = buffer.make<StableRoot>();
    fill_root(*root, 91);
    const auto* base = buffer.get_address();
    const auto epoch = buffer.epoch();
    const auto reservation = buffer.reserved_bytes();
    auto handle = buffer.handle<StableRoot>();

    for (int round = 0; round < 3; ++round) {
        const auto used = static_cast<std::size_t>(buffer.arena()->used_end);
        assert(used < buffer.get_size());
        // Dirty both the retained last page and later pages before hiding the tail.
        std::fill(buffer.get_buffer().begin() + used, buffer.get_buffer().end(), '\x5a');
        const auto accessible = buffer.accessible_bytes();
        buffer.shrink_to_fit();
        assert(buffer.get_size() == used);
        assert(buffer.accessible_bytes() == accessible); // Logical shrink retains enabled pages.
        assert(buffer.get_address() == base);
        assert(buffer.epoch() == epoch);
        assert(handle.get() == root);
        assert(buffer.grow(page * 3 + 7));
        assert(std::all_of(buffer.get_buffer().begin() + used,
                           buffer.get_buffer().end(), [](char byte) { return byte == 0; }));
        assert(buffer.get_address() == base);
        assert(buffer.reserved_bytes() == reservation);
        check_root(*root, 91);
    }

    const auto grow_preserving_storage = [](XBuffer& storage, std::size_t extra) {
        const auto* prior_base = storage.get_address();
        auto* prior_root = &storage.unsafe_root<StableRoot>();
        const auto allocator_before = allocator_snapshot(storage);
        const auto payload_before = storage.copy_bytes();
        const auto prior_epoch = storage.epoch();
        assert(storage.grow(extra));
        assert(storage.get_address() == prior_base);
        assert(&storage.unsafe_root<StableRoot>() == prior_root);
        assert(same_allocator_state(storage, allocator_before));
        assert(std::memcmp(storage.data(), payload_before.data(), payload_before.size()) == 0);
        assert(storage.epoch() == prior_epoch);
        check_extents(storage);
    };

    // Repeat both transport paths: save/grow the original storage, then
    // load/grow/mutate/save/load/grow/mutate without relocating either owner.
    for (int round = 0; round < 3; ++round) {
        for (const bool verified_path : {false, true}) {
            const auto frame = verified_path ? buffer.save_verified<StableRoot>() : buffer.save();
            if (verified_path) (void)check_verified_frame(frame, buffer);
            else assert(frame.size() == buffer.segment_size());
            assert(buffer.get_address() == base && &buffer.root<StableRoot>() == root);
            assert(buffer.epoch() == epoch && handle.get() == root);
            grow_preserving_storage(buffer, page * 2 + round + 1);
            check_root(*root, 91);

            const auto limit = stable_capacity(page * 32 + 17);
            auto loaded = verified_path ? XBuffer::load_verified<StableRoot>(frame, limit)
                : XBuffer::load_unverified(frame, limit);
            const auto* round_base = loaded.get_address();
            auto* round_root = &loaded.unsafe_root<StableRoot>();
            auto round_handle = verified_path ? loaded.handle<StableRoot>()
                : loaded.unsafe_handle<StableRoot>();
            check_root(*round_root, 91);
            grow_preserving_storage(loaded, page * 2 + round + 1);
            assert(round_handle.get() == round_root);
            check_root(*round_root, 91);

            const std::int32_t marker = 1000 + round * 10 + verified_path;
            round_root->numbers.push_back(marker);
            const auto repeated_frame = verified_path ? loaded.save_verified<StableRoot>()
                : loaded.save();
            assert(loaded.get_address() == round_base);
            assert(&loaded.unsafe_root<StableRoot>() == round_root);
            assert(round_handle.get() == round_root);
            if (verified_path) (void)check_verified_frame(repeated_frame, loaded);
            else assert(repeated_frame.size() == loaded.segment_size());

            auto reloaded = verified_path ? XBuffer::load_verified<StableRoot>(repeated_frame, limit)
                : XBuffer::load_unverified(repeated_frame, limit);
            auto* reloaded_root = &reloaded.unsafe_root<StableRoot>();
            assert(reloaded_root->numbers.size() == 33 && reloaded_root->numbers.back() == marker);
            grow_preserving_storage(reloaded, page + round + 1);
            assert(reloaded_root->numbers.back() == marker);
            reloaded_root->numbers.pop_back();
            check_root(*reloaded_root, 91);
            reloaded_root->numbers.push_back(marker + 100);
            assert(reloaded_root->numbers.back() == marker + 100);
            reloaded_root->numbers.pop_back();
            check_root(*reloaded_root, 91);
        }
    }

    const auto raw = buffer.save();
    assert(raw.size() == buffer.segment_size());
    assert(raw.size() == buffer.arena()->used_end);
    assert(raw.size() < buffer.reserved_capacity());
    assert(buffer.get_address() == base && buffer.epoch() == epoch);
    assert(handle.get() == root);
    assert(buffer.grow(page * 2));
    check_root(*root, 91);

    auto raw_loaded = XBuffer::load_unverified(raw, stable_capacity(XBuffer::DEFAULT_MAXIMUM_CAPACITY));
    const auto* loaded_base = raw_loaded.get_address();
    auto* loaded_root = &raw_loaded.unsafe_root<StableRoot>();
    const auto loaded_epoch = raw_loaded.epoch();
    assert(raw_loaded.get_size() == raw.size());
    assert(raw_loaded.grow(page * 2 + 1));
    assert(raw_loaded.get_address() == loaded_base);
    assert(&raw_loaded.unsafe_root<StableRoot>() == loaded_root);
    assert(raw_loaded.epoch() == loaded_epoch);
    check_root(*loaded_root, 91);

    auto bounded_raw = XBuffer::load_unverified(raw, stable_capacity(raw.size() + 19));
    const auto* bounded_raw_base = bounded_raw.get_address();
    assert(bounded_raw.grow(19));
    assert(!bounded_raw.grow(1));
    assert(bounded_raw.get_address() == bounded_raw_base);
    check_root(bounded_raw.unsafe_root<StableRoot>(), 91);
    require_rejection([&] {
        (void)XBuffer::load_unverified(raw, stable_capacity(raw.size() - 1));
    });

    auto verified = buffer.save_verified<StableRoot>();
    const auto header = check_verified_frame(verified, buffer);
    assert(buffer.get_address() == base && buffer.epoch() == epoch);
    assert(handle.get() == root);
    auto verified_loaded = XBuffer::load_verified<StableRoot>(verified,
        stable_capacity(XBuffer::DEFAULT_MAXIMUM_CAPACITY));
    auto verified_handle = verified_loaded.handle<StableRoot>();
    const auto* verified_base = verified_loaded.get_address();
    assert(verified_loaded.grow(page * 2 + 1));
    assert(verified_loaded.get_address() == verified_base);
    check_root(*verified_handle, 91);

    auto bounded_verified = XBuffer::load_verified<StableRoot>(
        verified, stable_capacity(static_cast<std::size_t>(header.used_bytes) + 19));
    const auto* bounded_verified_base = bounded_verified.get_address();
    assert(bounded_verified.grow(19));
    assert(!bounded_verified.grow(1));
    assert(bounded_verified.get_address() == bounded_verified_base);
    check_root(bounded_verified.root<StableRoot>(), 91);
    auto typed = TypedXBuffer<StableRoot>::load(verified,
        stable_capacity(static_cast<std::size_t>(header.used_bytes) + 1));
    assert(typed.grow(1) && !typed.grow(1));
    check_root(typed.root(), 91);
    require_rejection([&] {
        (void)XBuffer::load_verified<StableRoot>(verified,
            stable_capacity(static_cast<std::size_t>(header.used_bytes) - 1));
    });

    // V1 frames carried a historic reservation hint (reserved_bytes). V2.1 has
    // no such field: V1 input is rejected outright and flag bits must be 0.
    auto v1_frame = verified;
    std::memcpy(v1_frame.data(), "XOFFV1\0\0", 8);
    bool v1_rejected = false;
    try { (void)XBuffer::load_verified<StableRoot>(v1_frame); }
    catch (const XException& error) {
        v1_rejected = std::string(error.what()).find("wire format 1 is not supported") != std::string::npos;
    }
    assert(v1_rejected);
    auto reserved_frame = verified;
    auto reserved_header = header;
    reserved_header.flags = static_cast<std::uint32_t>(page * 32);
    std::memcpy(reserved_frame.data(), &reserved_header, sizeof(reserved_header));
    require_rejection([&] { (void)XBuffer::load_verified<StableRoot>(reserved_frame); });
    auto plain_frame = XBuffer::load_verified<StableRoot>(verified);
    check_root(plain_frame.root<StableRoot>(), 91);
    assert(plain_frame.grow(page));
    check_extents(plain_frame);

    auto compacted = XCompactor::compact<StableRoot>(buffer);
    assert(compacted.get_address() != base);
    assert(buffer.get_address() == base);
    assert(handle.get() == root);
    check_root(*root, 91);
    check_root(compacted.root<StableRoot>(), 91);
}

XBuffer make_lifecycle_buffer(std::int32_t id) {
    XBuffer buffer(4096, stable_capacity(XBuffer::DEFAULT_MAXIMUM_CAPACITY));
    buffer.make<StableRoot>()->id = id;
    return buffer;
}

void deterministic_growth_failures() {
    detail::clear_vm_failures();
    detail::fail_next_vm_operation(detail::VmOperation::Reserve);
    require_rejection([] { XBuffer failed_reservation(512); });
    detail::clear_vm_failures();

    const auto page = system_page_size();
    XBuffer buffer(page * 4, stable_capacity(page * 64));
    auto* root = buffer.make<StableRoot>();
    fill_root(*root, 87);
    auto handle = buffer.handle<StableRoot>();
    const auto* base = buffer.get_address();
    const auto epoch = buffer.epoch();

    for (const auto failure : {detail::VmOperation::Enable, detail::VmOperation::Prepare}) {
        const auto logical = buffer.get_size();
        const auto accessible = buffer.accessible_bytes();
        const auto allocator_before = allocator_snapshot(buffer);
        const auto payload_before = std::string(buffer.get_buffer().begin(), buffer.get_buffer().end());
        const auto target = accessible + page + 1;
        detail::fail_next_vm_operation(failure);
        assert(!buffer.grow(target - logical));
        detail::clear_vm_failures();
        assert(buffer.get_size() == logical);
        assert(buffer.get_address() == base);
        assert(buffer.epoch() == epoch);
        assert(buffer.accessible_bytes() >= accessible);
        if (failure == detail::VmOperation::Enable)
            assert(buffer.accessible_bytes() == accessible);
        else
            assert(buffer.accessible_bytes() >= target);
        assert(std::memcmp(buffer.get_buffer().data(), payload_before.data(), logical) == 0);
        assert(same_allocator_state(buffer, allocator_before));
        assert(handle.get() == root);
        check_root(*root, 87);
        check_extents(buffer);

        // A failed grow leaves the old arena available for normal container work.
        root->name = "usable after failure";
        assert(root->name == "usable after failure");
        root->name = "stable root";
        assert(buffer.grow(target - logical));
        assert(buffer.get_size() == target);
        assert(buffer.get_address() == base);
        assert(buffer.epoch() == epoch);
        check_root(*root, 87);
    }
    detail::clear_vm_failures();
}

void handle_storage_identity() {
    auto source = make_lifecycle_buffer(11);
    auto source_handle = source.handle<StableRoot>();
    auto* transferred = source_handle.get();
    const auto* transferred_base = source.get_address();
    XBuffer moved(std::move(source));
    assert(source_handle.get() == nullptr);
    assert(moved.get_address() == transferred_base);
    assert(&moved.root<StableRoot>() == transferred);
    moved.root<StableRoot>().name = "moved ownership";

    auto target = make_lifecycle_buffer(22);
    auto target_handle = target.handle<StableRoot>();
    auto moved_handle = moved.handle<StableRoot>();
    target = std::move(moved);
    assert(moved_handle.get() == nullptr);
    assert(target.get_address() == transferred_base);
    assert(target_handle.get() == &target.root<StableRoot>());
    assert(target_handle->id == 11);
    assert(target_handle->name == "moved ownership");

    // Fresh sources have matching epoch histories; an old destination cache must
    // still resolve to its new storage after every assignment.
    for (std::int32_t id = 30; id < 38; ++id) {
        auto fresh = make_lifecycle_buffer(id);
        auto fresh_handle = fresh.handle<StableRoot>();
        const auto* fresh_base = fresh.get_address();
        target = std::move(fresh);
        assert(fresh_handle.get() == nullptr);
        assert(target.get_address() == fresh_base);
        assert(target_handle.get() == &target.root<StableRoot>());
        assert(target_handle->id == id);
        target_handle->name = "assignment ownership";
    }

    auto other = make_lifecycle_buffer(99);
    auto other_handle = other.handle<StableRoot>();
    const auto* target_base = target.get_address();
    const auto* other_base = other.get_address();
    using std::swap;
    swap(target, other);
    assert(target.get_address() == other_base);
    assert(other.get_address() == target_base);
    assert(target_handle.get() == &target.root<StableRoot>());
    assert(other_handle.get() == &other.root<StableRoot>());
    assert(target_handle->id == 99);
    assert(other_handle->id == 37);
    other_handle->name = "swapped ownership";
    assert(other_handle->name == "swapped ownership");
    assert(target.grow(system_page_size()));
    assert(other.grow(system_page_size()));

    // Self move does not release the reservation or invalidate an existing handle.
    auto* self = &target;
    const auto* before_self_move = target.get_address();
    target = std::move(*self);
    assert(target.get_address() == before_self_move);
    assert(target_handle.get() == &target.root<StableRoot>());
    assert(target_handle->id == 99);

    target = XBuffer{};
    assert(target_handle.get() == nullptr);
    assert(target.get_size() == 0);
    assert(target.bytes().empty());
    assert(target.get_buffer().empty());
    assert(!target.grow(1));
}

void handles_reject_different_root_types() {
    auto first = make_lifecycle_buffer(101);
    auto second = XBuffer::create<OtherStableRoot>(4096);
    second.root<OtherStableRoot>().id = 202;
    auto first_handle = first.handle<StableRoot>();
    auto second_handle = second.handle<OtherStableRoot>();
    assert(first_handle.get() == &first.root<StableRoot>());
    assert(second_handle.get() == &second.root<OtherStableRoot>());

    swap(first, second);
    assert(first.matches_schema<OtherStableRoot>());
    assert(second.matches_schema<StableRoot>());
    assert(first_handle.get() == nullptr);
    assert(second_handle.get() == nullptr);
    assert(first.root<OtherStableRoot>().id == 202);
    assert(second.root<StableRoot>().id == 101);

    // A checked handle becomes usable again when its C++ object owns the
    // corresponding type; it is not permanently tied to the original reservation.
    swap(first, second);
    assert(first_handle.get() == &first.root<StableRoot>());
    assert(second_handle.get() == &second.root<OtherStableRoot>());
    assert(first_handle->id == 101 && second_handle->id == 202);

    first = std::move(second);
    assert(first_handle.get() == nullptr);
    assert(second_handle.get() == nullptr);
    assert(first.root<OtherStableRoot>().id == 202);
    first = make_lifecycle_buffer(303);
    assert(first_handle.get() == &first.root<StableRoot>());
    assert(first_handle->id == 303);

    const auto raw = first.save();
    const auto verified = first.save_verified<StableRoot>();
    first = XBuffer::load_unverified(raw);
    assert(first_handle.get() == nullptr);
    require_rejection([&] { (void)first.handle<StableRoot>(); });
    auto unsafe = first.unsafe_handle<StableRoot>();
    assert(unsafe.get() == &first.unsafe_root<StableRoot>());
    assert(unsafe->id == 303);
    XHandle<StableRoot> trusted_core(static_cast<XBufferCore&>(first));
    assert(trusted_core.get() == unsafe.get());
    first = XBuffer::load_verified<StableRoot>(verified);
    assert(first_handle.get() == &first.root<StableRoot>());
    assert(first_handle->id == 303);

    // Operating through the public base must transfer schema and admission
    // metadata with the storage, just as XBuffer's own swap does.
    second = XBuffer::create<OtherStableRoot>(4096);
    second.root<OtherStableRoot>().id = 404;
    auto base_swap_handle = second.handle<OtherStableRoot>();
    auto& first_core = static_cast<XBufferCore&>(first);
    auto& second_core = static_cast<XBufferCore&>(second);
    first_core.swap(second_core);
    assert(first.matches_schema<OtherStableRoot>());
    assert(second.matches_schema<StableRoot>());
    assert(first_handle.get() == nullptr);
    assert(base_swap_handle.get() == nullptr);
    assert(first.root<OtherStableRoot>().id == 404);
    assert(second.root<StableRoot>().id == 303);

    auto checked_destination = second.handle<StableRoot>();
    auto raw_source = XBuffer::load_unverified(raw);
    second_core = std::move(static_cast<XBufferCore&>(raw_source));
    assert(checked_destination.get() == nullptr);
    require_rejection([&] { (void)second.root<StableRoot>(); });
    require_rejection([&] { (void)second.handle<StableRoot>(); });
    auto trusted_destination = second.unsafe_handle<StableRoot>();
    assert(trusted_destination.get() == &second.unsafe_root<StableRoot>());
    assert(trusted_destination->id == 303);
    assert(!raw_source.unsafe_has_root<StableRoot>());
}

void independent_buffer_threads() {
    const auto page = system_page_size();
    constexpr unsigned worker_count = 4;
    std::atomic<unsigned> ready{0};
    std::array<std::thread, worker_count> workers;
    for (unsigned worker = 0; worker < worker_count; ++worker) {
        workers[worker] = std::thread([&, worker] {
            ready.fetch_add(1, std::memory_order_release);
            while (ready.load(std::memory_order_acquire) != worker_count)
                std::this_thread::yield();
            for (unsigned instance = 0; instance < 16; ++instance) {
                // Each worker owns its own buffer. Only the process-wide owner
                // registry is shared, including concurrent registration and removal.
                XBuffer buffer(512, stable_capacity(page * 64));
                auto* root = buffer.make<StableScalar>();
                root->value = worker * 1000 + instance;
                const auto* base = buffer.get_address();
                auto alloc = buffer.arena_allocator<std::int64_t>();
                for (unsigned round = 0; round < 24; ++round) {
                    assert(buffer.grow(round % 4 == 0 ? page : 1));
                    auto* value = alloc.allocate(1);
                    std::construct_at(value, root->value + round);
                    assert(*value == worker * 1000 + instance + round);
                    alloc.deallocate(value, 1);
                    assert(buffer.get_address() == base);
                    assert(root == &buffer.root<StableScalar>());
                    assert(root->value == worker * 1000 + instance);
                }
                buffer.shrink_to_fit();
                assert(buffer.grow(page));
                assert(buffer.get_address() == base);
                assert(root->value == worker * 1000 + instance);
            }
        });
    }
    for (auto& worker : workers) worker.join();
}

} // namespace

int main() {
    page_boundaries_and_capacity();
    objects_and_relative_references_survive_growth();
    shrink_regrow_and_transport();
    deterministic_growth_failures();
    handle_storage_identity();
    handles_reject_different_root_types();
    independent_buffer_threads();
    // Last: V2 currently aligns over-aligned blocks relative to the payload
    // offset rather than the address (library defect, see report).
    actual_addresses_honor_large_alignment();
    std::cout << "Stable storage integration tests passed\n";
}
