// Stage 3 generated-header checks for F, C-linear and C-index.  The generated
// diagnostic probe s3_validate_index() is required (F's probe returns true).
// All checks remain active under NDEBUG.  FIFO expectations are independent
// vector records of successful release events, never the candidate's buckets.
#include "../xoffsetdatastructure.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace XOffsetDatastructure;

namespace {
using Header = XBufferCore::AllocationHeader;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Header header(const void* pointer) {
    Header result{};
    std::memcpy(&result, static_cast<const char*>(pointer) - sizeof(result), sizeof(result));
    return result;
}

struct Record {
    void* pointer{};
    Header physical{};
    unsigned char pattern{};
};

struct Fixture {
    XBufferCore core{65536, XBufferCore::maximum_capacity(1024 * 1024)};
    std::vector<Record> live;
    std::vector<Record> free; // successful retirement order; erasure preserves order
    unsigned next_pattern = 1;

    void verify() const {
        require(core.s3_validate_index(), "candidate index/list invariant failed");
        const auto base = reinterpret_cast<std::uintptr_t>(core.data());
        const auto used = core.arena()->used_end;
        struct Range { std::uint64_t begin, end; };
        std::vector<Range> owned;
        std::uint64_t free_bytes = core.get_size() - used;
        auto inspect = [&](const Record& item, bool active) {
            const auto h = header(item.pointer);
            require(h.total_bytes == sizeof(Header) + std::uint64_t(h.user_bytes), "physical total changed");
            require(h.user_offset == reinterpret_cast<std::uintptr_t>(item.pointer) - base,
                    "physical offset changed");
            require(h.total_bytes == item.physical.total_bytes && h.user_bytes == item.physical.user_bytes &&
                    h.alignment == item.physical.alignment, "whole-reuse physical header changed");
            require(h.alignment && (h.alignment & (h.alignment - 1)) == 0 &&
                    reinterpret_cast<std::uintptr_t>(item.pointer) % h.alignment == 0,
                    "physical alignment invalid");
            const Range range{h.user_offset - sizeof(Header), std::uint64_t(h.user_offset) + h.user_bytes};
            require(range.end <= used && range.begin >= sizeof(XBufferCore::arena_header),
                    "owned interval outside arena");
            for (const auto& prior : owned)
                require(range.end <= prior.begin || range.begin >= prior.end, "duplicate or overlapping ownership");
            owned.push_back(range);
            if (active) {
                const auto* bytes = static_cast<const unsigned char*>(item.pointer);
                for (std::uint32_t i = 0; i != h.user_bytes; ++i)
                    require(bytes[i] == item.pattern, "live data changed during allocator operation");
            } else {
                free_bytes += h.total_bytes;
            }
        };
        for (const auto& item : live) inspect(item, true);
        for (const auto& item : free) inspect(item, false);
        require(core.get_free_memory() == free_bytes, "free byte accounting differs from FIFO oracle");
    }

    void* take(std::uint32_t bytes, std::size_t alignment = 16) {
        verify();
        const auto normalized_bytes = std::max<std::uint32_t>(1, bytes);
        const auto normalized_alignment = std::max<std::size_t>(alignment, alignof(std::max_align_t));
        auto expected = free.end();
        for (auto item = free.begin(); item != free.end(); ++item) {
            if (item->physical.user_bytes >= normalized_bytes && item->physical.alignment >= normalized_alignment &&
                reinterpret_cast<std::uintptr_t>(item->pointer) % normalized_alignment == 0) {
                expected = item;
                break;
            }
        }
        const auto old_used = core.arena()->used_end;
        auto* result = core.arena()->allocate_aligned(bytes, alignment);
        const auto h = header(result);
        if (expected != free.end()) {
            require(result == expected->pointer, "donor differs from independent earliest-insertion oracle");
            require(core.arena()->used_end == old_used, "whole reuse unexpectedly changed used_end");
            free.erase(expected);
        } else {
            require(h.user_offset >= old_used + sizeof(Header), "no-fit search reused an ineligible block");
            require(h.user_bytes == normalized_bytes, "new bump allocation has unexpected capacity");
        }
        require(reinterpret_cast<std::uintptr_t>(result) % normalized_alignment == 0,
                "returned address fails requested alignment");
        const auto pattern = static_cast<unsigned char>((next_pattern++ % 251) + 1);
        std::memset(result, pattern, h.user_bytes);
        live.push_back({result, h, pattern});
        verify();
        return result;
    }

    void release(void* pointer) {
        verify();
        const auto found = std::find_if(live.begin(), live.end(),
                                        [&](const auto& item) { return item.pointer == pointer; });
        require(found != live.end(), "test attempted to release nonlive allocation");
        free.push_back(*found);
        live.erase(found);
        core.arena()->deallocate(pointer);
        verify();
    }

    void swap(Fixture& other) {
        core.swap(other.core);
        live.swap(other.live);
        free.swap(other.free);
        std::swap(next_pattern, other.next_pattern);
        verify();
        other.verify();
    }

    void move_from(Fixture& other) {
        core = std::move(other.core);
        live = std::move(other.live);
        free = std::move(other.free);
        next_pattern = other.next_pattern;
        other.live.clear();
        other.free.clear();
        require(other.core.get_address() == nullptr && other.core.arena() == nullptr,
                "moved-from owner still has storage");
        require(other.core.s3_validate_index(), "moved-from slot state was not emptied");
        verify();
    }
};

void cross_bucket_order() {
    Fixture test;
    auto* early_large = test.take(2048, 64);
    auto* later_small = test.take(96, 64);
    auto* later_medium = test.take(768, 64);
    test.take(17);
    test.release(early_large);
    test.release(later_small);
    test.release(later_medium);
    require(test.take(64, 64) == early_large, "smallest bucket improperly outranked earliest larger donor");
    require(test.take(64, 64) == later_small, "oldest residual donor order changed");
    require(test.take(600, 64) == later_medium, "middle bucket eligibility changed");
}

void same_bucket_alignment_and_no_fit() {
    Fixture test;
    std::vector<void*> low;
    for (unsigned i = 0; i != 24; ++i) low.push_back(test.take(64 + i, 16));
    auto* high = test.take(112, 64);
    test.take(17);
    for (auto* pointer : low) test.release(pointer);
    test.release(high);
    require(test.take(64, 64) == high, "alignment-rejected bucket chain hid later qualified donor");
    const auto used = test.core.arena()->used_end;
    test.take(127, 16); // all free donors share its bucket but are too small
    require(test.core.arena()->used_end > used, "exact first-bucket size filter missing");
    const auto used_again = test.core.arena()->used_end;
    test.take(32, 256); // size fits, stored alignment does not
    require(test.core.arena()->used_end > used_again, "stored alignment filter missing");
    for (auto* pointer : low)
        require(test.take(32, 16) == pointer, "same-bucket FIFO order changed after no-fit queries");
}

void aligned_address_does_not_override_stored_alignment() {
    Fixture test;
    void* low = nullptr;
    for (unsigned i = 0; i != 8 && !low; ++i) {
        auto* candidate = test.take(80, 16);
        if (reinterpret_cast<std::uintptr_t>(candidate) % 64 == 0) low = candidate;
        else test.take(17, 16); // advances residue by 16 across candidate/padding pairs
    }
    require(low != nullptr, "could not construct aligned-address/stored-low fixture");
    auto* high = test.take(80, 64);
    test.release(low);
    test.release(high);
    require(test.take(64, 64) == high, "actual address improperly widened recorded alignment");
    require(test.take(64, 16) == low, "alignment rejection lost earlier free entry");
}

void stable_holes_preserve_fifo() {
    Fixture test;
    std::vector<void*> blocks;
    for (unsigned i = 0; i != 12; ++i) blocks.push_back(test.take(128, 64));
    test.take(17);
    for (auto* pointer : blocks) test.release(pointer);
    std::vector<void*> recycled;
    for (unsigned i = 0; i != 7; ++i) recycled.push_back(test.take(32, 64));
    for (auto pointer = recycled.rbegin(); pointer != recycled.rend(); ++pointer) test.release(*pointer);
    for (unsigned i = 7; i != blocks.size(); ++i)
        require(test.take(32, 64) == blocks[i], "reused low-numbered slot overtook older insertion");
    for (auto pointer = recycled.rbegin(); pointer != recycled.rend(); ++pointer)
        require(test.take(32, 64) == *pointer, "hole recycling did not retain new release order");
}

void sequence_overflow_renumbers_without_reordering() {
    Fixture test;
    auto* large = test.take(2048, 64);
    auto* small = test.take(96, 64);
    auto* medium = test.take(768, 64);
    auto* fourth = test.take(256, 64);
    test.take(17);
    test.release(large);
    test.release(small);
    test.release(medium);
    test.release(fourth);
    require(test.take(1536, 64) == large, "overflow fixture did not consume FIFO head");
    require(test.take(80, 64) == small, "overflow fixture did not leave sequence gaps");
    // Only the diagnostic generated header exposes this counter hook.  The
    // next retirement must renumber [medium,fourth] and then append small.
    require(test.core.s3_bad_index_control(),"invalid slot reference escaped diagnostic oracle");
    test.core.s3_force_sequence_overflow();
    test.release(small);
    require(test.take(64, 64) == medium, "overflow renumber let smaller bucket overtake oldest donor");
    require(test.take(64, 64) == fourth, "overflow renumber changed second FIFO donor");
    require(test.take(64, 64) == small, "overflow renumber lost newly appended donor");
    test.release(large);
    require(test.take(1024, 64) == large, "post-renumber insertion sequence is invalid");

    Fixture empty;
    auto* first = empty.take(128, 64);
    require(empty.core.s3_bad_index_control(),"invalid slot reference escaped diagnostic oracle");
    empty.core.s3_force_sequence_overflow();
    empty.release(first); // empty active list also resets its sequence safely
    require(empty.take(64, 64) == first, "empty-list overflow renumber lost first donor");
}

void move_and_swap() {
    Fixture first;
    auto* one = first.take(128, 64);
    auto* two = first.take(256, 64);
    first.take(17);
    first.release(one);
    first.release(two);
    Fixture moved;
    moved.move_from(first);
    require(moved.take(64, 64) == one, "move assignment lost FIFO head");
    moved.release(one);
    Fixture third;
    auto* three = third.take(512, 64);
    third.take(17);
    third.release(three);
    const auto moved_base = moved.core.get_address();
    const auto third_base = third.core.get_address();
    moved.swap(third);
    require(moved.core.get_address() == third_base && third.core.get_address() == moved_base,
            "swap did not transfer storage owner");
    require(moved.take(32, 64) == three, "swap did not transfer index metadata with arena");
    require(third.take(64, 64) == two, "swap lost former donor insertion order");
    require(third.take(64, 64) == one, "swap lost reused-hole insertion order");

    // Exercise XBufferCore's move constructor independently of assignment.
    auto* queued = moved.take(768, 64);
    moved.release(queued);
    XBufferCore constructed(std::move(moved.core));
    require(moved.core.get_address() == nullptr && moved.core.s3_validate_index(), "move constructor left source index");
    require(constructed.s3_validate_index(), "move constructor invalidated destination index");
    require(constructed.arena()->allocate_aligned(600, 64) == queued, "move constructor lost donor");
}

void prune_known_free_suffix() {
    Fixture test;
    auto* keep = test.take(96, 64);
    auto* last_live = test.take(48, 16);
    const auto new_end = std::uint64_t(header(last_live).user_offset) + header(last_live).user_bytes;
    auto* remove_one = test.take(128, 64);
    auto* remove_two = test.take(256, 64);
    test.release(keep);
    test.release(remove_one);
    test.release(remove_two);
    test.core.shrink_to_fit();
    test.verify(); // ordinary shrink must retain all still-bounded free records

    // Controlled arena-metadata fixture: lower used_end only across a suffix
    // containing no live interval, then exercise prune's stale-record removal.
    for (const auto& item : test.live)
        require(std::uint64_t(item.physical.user_offset) + item.physical.user_bytes <= new_end,
                "prune fixture would remove live storage");
    test.free.erase(std::remove_if(test.free.begin(), test.free.end(), [&](const auto& item) {
        return std::uint64_t(item.physical.user_offset) + item.physical.user_bytes > new_end;
    }), test.free.end());
    test.core.arena()->used_end = new_end;
    test.core.shrink_to_fit();
    test.verify();
    require(test.take(64, 64) == keep, "prune lost surviving FIFO donor");
    require(test.core.grow(4096), "prune fixture could not restore logical extent");
    auto* fresh = test.take(200, 64);
    test.release(fresh);
    require(test.take(160, 64) == fresh, "pruned slot hole was not reusable");
}

void load_drops_host_table_then_rebuilds() {
    Fixture original;
    auto* abandoned = original.take(512, 16);
    auto* reusable_live = original.take(192, 16);
    original.take(17);
    original.release(abandoned);
    const auto wire = original.core.copy_bytes();
    Fixture loaded;
    loaded.core = XBufferCore(wire.data(), wire.size(), XBufferCore::maximum_capacity(1024 * 1024));
    loaded.live.clear();
    loaded.free.clear(); // host Core free records are intentionally not serialized
    for (auto item : original.live) {
        item.pointer = loaded.core.data() + item.physical.user_offset;
        loaded.live.push_back(item);
    }
    loaded.verify();
    const auto used = loaded.core.arena()->used_end;
    loaded.take(64, 16);
    require(loaded.core.arena()->used_end > used, "load guessed an old Core free allocation");
    auto* mapped_live = loaded.core.data() + header(reusable_live).user_offset;
    loaded.release(mapped_live);
    require(loaded.take(128, 16) == mapped_live, "post-load lazy free table failed to rebuild");
    original.verify();
    loaded.verify();
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string variant = argc > 1 ? argv[1] : "F";
        require(variant == "F" || variant == "C-linear" || variant == "C-index", "unknown index variant");
        cross_bucket_order();
        same_bucket_alignment_and_no_fit();
        aligned_address_does_not_override_stored_alignment();
        stable_holes_preserve_fifo();
        sequence_overflow_renumbers_without_reordering();
        move_and_swap();
        prune_known_free_suffix();
        load_drops_host_table_then_rebuilds();
        std::cout << "{\"status\":\"complete\",\"variant\":\"" << variant
                  << "\",\"cases\":8,\"fifo_oracle\":true,\"sequence_overflow\":\"injected_max_counter\"}\n";
    } catch (const std::exception& error) {
        std::cerr << "Stage 3 index donor/lifecycle failure: " << error.what() << '\n';
        return 1;
    }
}
