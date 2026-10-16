// Stage3 isolated wire boundary checks. Compile against F or a generated
// snapshot and pass its name: F, A-wire, B-wire. These checks deliberately
// distinguish policy-specific addresses from common safety requirements.
#include "../xoffsetdatastructure.hpp"
#include "../tests/allocator_model_support.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif

using namespace XOffsetDatastructure;

struct WireAllocatorModelRoot {
    std::uint64_t marker{};
    XVector<std::uint32_t> values;
    XVector<std::uint32_t> scratch;
    XString label;
    XVector<XString> nested;
};
XOFFSET_REGISTER_SCHEMA_NAME(WireAllocatorModelRoot, "test.WireAllocatorModelRoot")

namespace {
using Header = XBufferCore::AllocationHeader;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Header header(const void* pointer) {
    Header value{};
    std::memcpy(&value, static_cast<const char*>(pointer) - sizeof(Header), sizeof(Header));
    return value;
}

struct Range { std::uintptr_t begin, end; };

struct Fixture {
    XBufferCore buffer{16384, XBufferCore::maximum_capacity(32768)};
    XAllocatorStateV1* state = buffer.construct_allocator_storage<XAllocatorStateV1>();
    fixed_detail::allocator_ref alloc{state, buffer.arena()};

    Fixture() { fixed_detail::initialize_allocator_state(*state, buffer.arena()); }

    void* cached(std::uint32_t physical, std::uint32_t published, bool tail) {
        auto* donor = buffer.arena()->allocate_aligned(physical, 16);
        buffer.arena()->deallocate(donor);
        auto* reused = fixed_detail::allocate_dynamic_bytes(alloc, published, 8);
        require(reused == donor && header(reused).user_bytes == physical,
                "fixture must create smaller advertised capacity through F Core reuse");
        if (!tail) buffer.arena()->allocate_aligned(64, 16);
        fixed_detail::retire_dynamic_bytes(alloc, reused, published);
        require(state->free_bytes == published, "fixture advertisement mismatch");
        return reused;
    }

    Range physical_range(const void* pointer) const {
        const auto value = header(pointer);
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        require(value.total_bytes == std::uint64_t(value.user_bytes) + sizeof(Header), "invalid total");
        require(value.user_offset == address - reinterpret_cast<std::uintptr_t>(buffer.data()), "invalid offset");
        require(value.alignment != 0 && (value.alignment & (value.alignment - 1)) == 0 &&
                address % value.alignment == 0, "invalid alignment");
        require(std::uint64_t(value.user_offset) + value.user_bytes <= buffer.arena()->used_end,
                "allocation extends beyond used end");
        return {address - sizeof(Header), address + value.user_bytes};
    }

    std::vector<void*> cached_blocks(std::vector<Range> live = {}) const {
        std::vector<void*> blocks;
        std::uint64_t bytes = 0;
        for (std::uint32_t cls = 0; cls < fixed_detail::freelist_class_count; ++cls) {
            auto offset = state->freelist_head[cls];
            while (offset != 0) {
                require(blocks.size() < 64, "cache cycle");
                auto* block = fixed_detail::freelist_block_from_offset(state, offset);
                require(block->block_bytes >= 8 && block->block_bytes % 8 == 0 &&
                        fixed_detail::freelist_size_class(block->block_bytes) == cls,
                        "invalid residual advertisement");
                require(block->block_bytes <= header(block).user_bytes, "advertised capacity exceeds physical");
                const auto range = physical_range(block);
                for (const auto& occupied : live)
                    require(range.end <= occupied.begin || range.begin >= occupied.end,
                            "physical ownership overlap");
                live.push_back(range);
                blocks.push_back(block);
                bytes += block->block_bytes;
                offset = block->next_offset;
            }
        }
        require(bytes == state->free_bytes, "wire free count differs from reachable advertisements");
        return blocks;
    }
};

void non_tail_hidden(const std::string& variant) {
    Fixture fixture;
    auto* donor = fixture.cached(1024, 256, false);
    const auto original = fixture.physical_range(donor);
    auto* result = fixed_detail::allocate_dynamic_bytes(fixture.alloc, 32, 16);
    require(result == donor, "non-tail front policy changed");
    auto blocks = fixture.cached_blocks({fixture.physical_range(result)});
    if (variant == "F") {
        require(blocks.empty() && header(result).user_bytes == 1024, "F whole policy mismatch");
    } else {
        require(blocks.size() == 1 && fixture.state->free_bytes == 208, "non-tail split budget mismatch");
        require(header(result).user_bytes == 32 && header(blocks[0]).user_bytes == 976,
                "hidden suffix must belong to residual physical block");
        require(fixture.physical_range(blocks[0]).end == original.end, "hidden physical suffix lost");
        auto* residual = static_cast<fixed_detail::FreeBlockHeader*>(blocks[0]);
        require(reinterpret_cast<std::uintptr_t>(residual) + residual->block_bytes <=
                reinterpret_cast<std::uintptr_t>(donor) + 256, "residual crossed old advertised boundary");
    }
    fixed_detail::retire_dynamic_bytes(fixture.alloc, result, 32);
    fixture.cached_blocks();
    fixed_detail::release_cached_blocks(fixture.state);
    require(fixture.state->free_bytes == 0 && fixture.cached_blocks().empty(), "flush left cached blocks");
    // Each independent header must survive transfer into F Core's free vector.
    auto* first = fixture.buffer.arena()->allocate_aligned(32, 16);
    auto* second = fixture.buffer.arena()->allocate_aligned(208, 16);
    const auto a = fixture.physical_range(first), b = fixture.physical_range(second);
    require(a.end <= b.begin || b.end <= a.begin, "flush produced overlapping Core allocations");
}

void tail_hidden_and_growth(const std::string& variant, bool consume_prefix) {
    Fixture fixture;
    auto* donor = fixture.cached(1024, 256, true);
    const auto old_end = fixture.buffer.arena()->used_end;
    auto* result = fixed_detail::allocate_dynamic_bytes(fixture.alloc, 32, 16);
    auto blocks = fixture.cached_blocks({fixture.physical_range(result)});
    if (variant != "B-wire") {
        require(result == donor && header(result).user_bytes == 1024 && blocks.empty(),
                "F/A tail donor must remain whole");
        require(fixture.buffer.arena()->try_expand(result, 1104, 16), "whole tail growth failed");
        require(fixture.buffer.arena()->used_end == old_end + 80, "whole tail growth amount differs");
        return;
    }
    require(result != donor && reinterpret_cast<std::uintptr_t>(result) >
            reinterpret_cast<std::uintptr_t>(donor) + 256, "hidden-tail counterexample was not exercised");
    require(blocks.size() == 1 && blocks[0] == donor && fixture.state->free_bytes == 208,
            "B hidden tail did not charge request/header to original W budget");
    require(header(result).user_bytes == 32 && header(donor).user_bytes == 976,
            "B independent physical partition mismatch");
    require(fixture.state->arena_end >= reinterpret_cast<std::uintptr_t>(result) -
            reinterpret_cast<std::uintptr_t>(fixture.state) + 32, "B state did not cover returned location");
    std::memset(result, 0x37, 32);
    void* prefix = nullptr;
    if (consume_prefix) {
        prefix = fixed_detail::allocate_dynamic_bytes(fixture.alloc, 128, 16);
        require(prefix == donor, "B prefix should retain donor selection order");
        std::memset(prefix, 0x6a, 128);
    }
    require(fixed_detail::try_expand_dynamic_bytes(fixture.alloc, result, 80, 16), "B tail growth failed");
    require(fixture.buffer.arena()->used_end == old_end + 48, "B growth lost physical tail identity");
    for (int i = 0; i < 32; ++i) require(static_cast<unsigned char*>(result)[i] == 0x37, "tail data lost");
    std::vector<Range> live{fixture.physical_range(result)};
    if (prefix) {
        for (int i = 0; i < 128; ++i) require(static_cast<unsigned char*>(prefix)[i] == 0x6a, "prefix overwritten");
        live.push_back(fixture.physical_range(prefix));
    }
    fixture.cached_blocks(live);
    fixed_detail::retire_dynamic_bytes(fixture.alloc, result, 80);
    if (prefix) fixed_detail::retire_dynamic_bytes(fixture.alloc, prefix, 128);
    fixture.cached_blocks();
    fixed_detail::release_cached_blocks(fixture.state);
}

void minimum_and_header_shortfall(const std::string& variant) {
    for (bool tail : {false, true}) {
        Fixture fixture;
        // A non-tail 56-byte published block leaves exactly an 8-byte cache
        // record after R=32 plus one 16-byte physical header. B's tail case
        // additionally proves that hidden physical bytes do not grow W.
        auto* donor = fixture.cached(tail ? 64 : 56, 56, tail);
        auto* result = fixed_detail::allocate_dynamic_bytes(fixture.alloc, 32, 8);
        fixture.cached_blocks({fixture.physical_range(result)});
        const bool split = variant != "F" && (!tail || variant == "B-wire");
        require(fixture.state->free_bytes == (split ? 8u : 0u), "minimum residual boundary wrong");
        if (split) require(tail ? result != donor : result == donor, "minimum split position wrong");
    }
    for (bool tail : {false, true}) {
        Fixture fixture;
        auto* donor = fixture.cached(64, 48, tail);
        auto* result = fixed_detail::allocate_dynamic_bytes(fixture.alloc, 32, 8);
        require(result == donor && fixture.state->free_bytes == 0 && header(result).user_bytes == 64,
                "one-header/no-residual shortfall must preserve whole donor");
        fixture.cached_blocks({fixture.physical_range(result)});
    }
}

void tail_alignment_padding(const std::string& variant) {
    Fixture fixture;
    // A 513-byte Core donor has an odd physical endpoint. The residual W
    // budget must pay the tail's P=1 byte and round its advertisement down.
    auto* donor = fixture.cached(513, 256, true);
    const auto physical_end = reinterpret_cast<std::uintptr_t>(donor) + 513;
    auto* result = fixed_detail::allocate_dynamic_bytes(fixture.alloc, 32, 16);
    auto blocks = fixture.cached_blocks({fixture.physical_range(result)});
    if (variant == "B-wire") {
        require(header(result).user_bytes == 33 && fixture.state->free_bytes == 200 && blocks.size() == 1,
                "tail padding was not charged/rounded within W budget");
        require(reinterpret_cast<std::uintptr_t>(result) + header(result).user_bytes == physical_end,
                "aligned tail lost endpoint");
    } else require(result == donor && blocks.empty(), "whole tail alignment policy changed");
}

struct WireLive {
    void* pointer{};
    std::uint32_t bytes{};
    std::size_t alignment{};
    unsigned char value{};
};

struct WireTraceRecord {
    std::uint64_t random{};
    std::uint64_t used_end{};
    std::uint32_t free_bytes{};
    std::uint32_t bytes{};
    std::uint32_t alignment{};
    std::uint32_t offset{};
    unsigned char slot{};
    char operation{};
    signed char outcome{-1};
};

struct WireTraceStats {
    std::uint64_t allocations{}, retires{}, expands{}, flushes{}, exhausted{};
    std::uint64_t lower_stored_alignment_reuses{};
};

// A per-observation epoch table detects repeated cache offsets independently of
// the allocator's links/classes. It allocates only before the operation loop.
class WireVisitedOffsets {
    struct Entry { std::uint32_t offset{}, epoch{}; };
    std::vector<Entry> entries_;
    std::uint32_t epoch_{};
    std::size_t count_{};
public:
    explicit WireVisitedOffsets(std::size_t maximum_items) {
        std::size_t capacity = 8;
        while (capacity < maximum_items * 2) capacity <<= 1;
        entries_.resize(capacity);
    }
    void begin_observation() noexcept {
        count_ = 0;
        if (++epoch_ == 0) {
            for (auto& entry : entries_) entry.epoch = 0;
            epoch_ = 1;
        }
    }
    bool insert(std::uint32_t offset) {
        require(offset != 0 && epoch_ != 0, "invalid wire reachability-table observation");
        auto hash = std::uint64_t(offset >> 3) * 0x9e3779b97f4a7c15ULL;
        hash ^= hash >> 33;
        const auto mask = entries_.size() - 1;
        auto slot = static_cast<std::size_t>(hash) & mask;
        for (std::size_t probe = 0; probe < entries_.size(); ++probe, slot = (slot + 1) & mask) {
            auto& entry = entries_[slot];
            if (entry.epoch != epoch_) {
                require(count_ < entries_.size() / 2, "wire reachability-table capacity exceeded");
                entry = {offset, epoch_};
                ++count_;
                return true;
            }
            if (entry.offset == offset) return false;
        }
        throw std::runtime_error("wire reachability-table has no empty slot");
    }
};

// The oracle validates independently observed physical ownership ranges. It
// neither predicts a donor nor repeats the allocator's class/list selection.
// Range/reachability buffers and replay records precede the operation loop.
struct WireRangeOracle {
    std::vector<allocator_model::Interval> ranges;
    WireVisitedOffsets wire_offsets;
    allocator_model::OrderedRanges ordered_ranges;
    allocator_model::Coverage coverage;

    explicit WireRangeOracle(std::size_t operations) : wire_offsets(operations * 2 + 256) {
        ranges.reserve(operations * 2 + 256);
    }

    void add(XBufferCore& buffer, const void* pointer, std::size_t required,
             std::uint64_t id, unsigned owner) {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        const auto base = reinterpret_cast<std::uintptr_t>(buffer.data());
        require(address >= base + sizeof(Header), "model pointer precedes physical header");
        const auto offset = address - base;
        require(offset <= buffer.arena()->used_end && required <= buffer.arena()->used_end - offset,
                "model user bytes beyond used end");
        const auto value = header(pointer);
        require(value.user_offset == offset && value.user_bytes > 0 && value.user_bytes >= required &&
                value.total_bytes == sizeof(Header) + std::uint64_t(value.user_bytes),
                "model physical header mismatch");
        require(value.alignment && value.alignment <= XBufferCore::MAX_PAYLOAD &&
                !(value.alignment & (value.alignment - 1)) &&
                address % value.alignment == 0 &&
                (address - sizeof(Header)) % alignof(Header) == 0,
                "model physical alignment mismatch");
        ranges.push_back({offset - sizeof(Header), offset + std::uint64_t(value.user_bytes), id, owner});
    }

    void verify(XBufferCore& buffer, XAllocatorStateV1* state,
                const std::array<WireLive, 64>& live) {
        ranges.clear();
        wire_offsets.begin_observation();
        add(buffer, state, sizeof(*state), 64, 0);
        // Byte comparison uses a prepared repeated-byte slab, so the oracle
        // cannot introduce host allocation failure points into allocator work.
        static thread_local const auto expected = [] {
            std::array<std::array<unsigned char, 4096>, 256> bytes{};
            for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i].fill(static_cast<unsigned char>(i));
            return bytes;
        }();
        for (std::size_t id = 0; id < live.size(); ++id) if (const auto& item = live[id]; item.pointer) {
            require(item.bytes <= expected[0].size(), "model live-byte bound exceeded");
            require(reinterpret_cast<std::uintptr_t>(item.pointer) % item.alignment == 0,
                    "wire returned a misaligned actual address");
            add(buffer, item.pointer, item.bytes, id, 0);
            require(std::memcmp(item.pointer, expected[item.value].data(), item.bytes) == 0,
                    "wire live data changed");
        }
        const auto base = reinterpret_cast<std::uintptr_t>(buffer.data());
        const auto origin = reinterpret_cast<std::uintptr_t>(state);
        const auto state_offset = origin - base;
        require(fixed_detail::allocator_arena(state) == buffer.arena(), "wire allocator binding is stale");
        require(state->arena_begin >= sizeof(*state) && state->arena_begin <= state->arena_end &&
                std::uint64_t(state_offset) + state->arena_end <= buffer.arena()->used_end,
                "wire state advertised extent invalid");
        std::uint64_t advertised_total = 0;
        for (std::uint32_t cls = 0; cls < fixed_detail::freelist_class_count; ++cls) {
            for (auto offset = state->freelist_head[cls]; offset != 0;) {
                require(wire_offsets.insert(offset),
                        "wire cache has duplicate/cyclic index reachability");
                require(std::uint64_t(state_offset) + offset >= sizeof(Header) &&
                        std::uint64_t(state_offset) + offset <= buffer.arena()->used_end &&
                        sizeof(fixed_detail::FreeBlockHeader) <=
                            buffer.arena()->used_end - (std::uint64_t(state_offset) + offset),
                        "wire cache offset outside arena");
                auto* block = reinterpret_cast<const fixed_detail::FreeBlockHeader*>(origin + offset);
                require(reinterpret_cast<std::uintptr_t>(block) % 8 == 0, "wire cache record misaligned");
                // Independent class bounds instead of using the implementation's
                // size-class function as its own validation oracle.
                const std::uint32_t upper = std::uint32_t(8) << cls;
                require(block->block_bytes >= 8 && block->block_bytes % 8 == 0 &&
                        block->block_bytes <= upper && (cls == 0 || block->block_bytes > upper / 2),
                        "wire cache record belongs to wrong class");
                require(std::uint64_t(offset) + block->block_bytes <= state->arena_end,
                        "wire cache exceeds advertised arena boundary");
                add(buffer, block, block->block_bytes, offset, 2);
                advertised_total += block->block_bytes;
                offset = block->next_offset;
            }
        }
        require(advertised_total == state->free_bytes, "wire reachable advertisement/count mismatch");
#ifdef S3_PROBE
        require(buffer.s3_validate_index(), "Core slot/index reachability invalid during wire operations");
        std::uint64_t core_total = 0;
        std::uint64_t core_count = 0;
        buffer.s3_visit_free([&](auto offset, auto size, auto alignment) {
            require(offset >= sizeof(Header) && std::uint64_t(offset) + size <= buffer.arena()->used_end,
                    "Core free/index offset invalid during wire operations");
            const auto physical = header(buffer.data() + offset);
            require(physical.user_bytes == size && physical.alignment == alignment,
                    "Core metadata disagrees with referenced physical header");
            add(buffer, buffer.data() + offset, size, core_count++, 1);
            core_total += size + sizeof(Header);
        });
        require(core_total + buffer.get_size() - buffer.arena()->used_end == buffer.get_free_memory(),
                "Core free-byte accounting mismatch during wire operations");
#endif
        ordered_ranges.validate(ranges, buffer.arena()->used_end);
        // Keep all previously known physical bytes owned across operations.
        // Current-range validation alone would miss a residual whose record
        // and free_bytes contribution were both silently dropped.
        coverage.observe(ranges);
    }
};

void dump_wire_replay(std::uint64_t initial_seed, std::size_t failing_step,
                      bool automatic, const std::vector<WireTraceRecord>& log) {
    const auto records = std::min(failing_step + 1, log.size());
    std::cerr << "wire_failure seed=" << initial_seed << " step=" << failing_step
              << " automatic=" << automatic << " cleanup=" << (failing_step == log.size())
              << " replay_records=" << records << '\n';
    for (std::size_t i = 0; i < records; ++i) {
        const auto& entry = log[i];
        std::cerr << "wire_replay step=" << i << " random=" << entry.random
                  << " op=" << entry.operation << " slot=" << unsigned(entry.slot)
                  << " bytes=" << entry.bytes << " alignment=" << entry.alignment
                  << " outcome=" << int(entry.outcome) << " offset=" << entry.offset
                  << " used_end=" << entry.used_end << " free_bytes=" << entry.free_bytes << '\n';
    }
}

WireTraceStats randomized_wire(std::uint64_t initial_seed, std::size_t operations, bool automatic) {
    constexpr std::size_t maximum = 16 * 1024 * 1024;
    XBufferCore buffer(automatic ? 4096 : maximum,
        XBufferCore::maximum_capacity(maximum).with_growth(
            automatic ? XGrowPolicy::Automatic : XGrowPolicy::Explicit));
    auto* state = buffer.construct_allocator_storage<XAllocatorStateV1>();
    fixed_detail::initialize_allocator_state(*state, buffer.arena());
    std::array<WireLive, 64> live{};
    std::vector<WireTraceRecord> log(operations);
    WireRangeOracle oracle(operations);
    WireTraceStats stats;
    const auto* original_base = buffer.get_address();
    auto seed = initial_seed;
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const auto page = static_cast<std::size_t>(info.dwPageSize);
#else
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
    require(page && !(page & (page - 1)), "unexpected runtime page size");
    const std::array<std::size_t, 7> alignments = {1, 2, 8, 16, 64, page, page * 2};
    oracle.verify(buffer, state, live);
    std::size_t step = 0;
    try {
        for (; step < operations; ++step) {
            const auto random = allocator_model::random(seed);
            const auto slot = (random >> 8) % live.size();
            auto& item = live[slot];
            auto& entry = log[step];
            entry.random = random;
            entry.slot = static_cast<unsigned char>(slot);
            const auto alloc = fixed_detail::allocator_ref{state, buffer.arena()};
            if (random % 19 == 0) {
                entry.operation = 'F';
                fixed_detail::release_cached_blocks(state);
                ++stats.flushes;
                entry.outcome = 1;
            } else if (!item.pointer) {
                entry.operation = 'A';
                entry.bytes = static_cast<std::uint32_t>(1 + (random >> 16) % (random % 4 ? 256 : 2048));
                entry.alignment = static_cast<std::uint32_t>(alignments[(random >> 32) % alignments.size()]);
                try {
                    auto* pointer = fixed_detail::allocate_dynamic_bytes(alloc, entry.bytes, entry.alignment);
                    item = {pointer, entry.bytes, entry.alignment,
                            static_cast<unsigned char>((slot + step) % 251 + 1)};
                    entry.offset = header(pointer).user_offset;
                    if (header(pointer).alignment < entry.alignment) ++stats.lower_stored_alignment_reuses;
                    std::memset(pointer, item.value, item.bytes);
                    ++stats.allocations;
                    entry.outcome = 1;
                } catch (const XBadAlloc&) {
                    ++stats.exhausted;
                    entry.outcome = 0;
                }
            } else if (random % 5 < 3) {
                entry.operation = 'R';
                entry.bytes = item.bytes;
                entry.alignment = static_cast<std::uint32_t>(item.alignment);
                entry.offset = header(item.pointer).user_offset;
                fixed_detail::retire_dynamic_bytes(alloc, item.pointer, item.bytes);
                item = {};
                ++stats.retires;
                entry.outcome = 1;
            } else {
                entry.operation = 'E';
                entry.bytes = std::min<std::uint32_t>(4096, item.bytes + 1 + (random >> 24) % 512);
                entry.alignment = static_cast<std::uint32_t>(item.alignment);
                entry.offset = header(item.pointer).user_offset;
                try {
                    const bool expanded = fixed_detail::try_expand_dynamic_bytes(
                        alloc, item.pointer, entry.bytes, item.alignment);
                    entry.outcome = expanded ? 1 : 0;
                    if (expanded) {
                        std::memset(static_cast<char*>(item.pointer) + item.bytes,
                                    item.value, entry.bytes - item.bytes);
                        item.bytes = entry.bytes;
                        ++stats.expands;
                    }
                } catch (const XBadAlloc&) {
                    ++stats.exhausted;
                    entry.outcome = 0;
                }
            }
            entry.used_end = buffer.arena()->used_end;
            entry.free_bytes = state->free_bytes;
            require(buffer.get_size() <= maximum, "wire operation escaped hard maximum");
            require(buffer.get_address() == original_base, "wire owner base moved");
            oracle.verify(buffer, state, live);
            if (step % 997 == 996) {
                XBufferCore moved(std::move(buffer));
                buffer = std::move(moved);
                state = buffer.find_allocator_storage<XAllocatorStateV1>();
                require(state && fixed_detail::allocator_arena(state) == buffer.arena(),
                        "wire binding did not survive Core owner move");
                oracle.verify(buffer, state, live);
            }
        }
        const auto alloc = fixed_detail::allocator_ref{state, buffer.arena()};
        for (auto& item : live) if (item.pointer) {
            fixed_detail::retire_dynamic_bytes(alloc, item.pointer, item.bytes);
            item = {};
        }
        oracle.verify(buffer, state, live);
        fixed_detail::release_cached_blocks(state);
        oracle.verify(buffer, state, live);
    } catch (...) {
        dump_wire_replay(initial_seed, step, automatic, log);
        throw;
    }
    return stats;
}

bool lower_stored_alignment_corner() {
    Fixture fixture;
    constexpr std::size_t stronger = 64;
    fixture.buffer.arena()->allocate_aligned(stronger - sizeof(Header), stronger);
    auto* donor = fixture.buffer.arena()->allocate_aligned(256, 16);
    require(reinterpret_cast<std::uintptr_t>(donor) % stronger == 0 && header(donor).alignment == 16,
            "wire stored-alignment corner setup failed");
    fixture.buffer.arena()->allocate_aligned(64, 16);
    fixed_detail::retire_dynamic_bytes(fixture.alloc, donor, 256);
    auto* result = fixed_detail::allocate_dynamic_bytes(fixture.alloc, 32, stronger);
    require(reinterpret_cast<std::uintptr_t>(result) % stronger == 0, "wire actual-address predicate failed");
    fixture.cached_blocks({fixture.physical_range(result)});
    // F's whole wire reuse does not promote stored alignment. This corner is
    // reported separately, not mistaken for a new physical-ownership failure.
    return header(result).alignment < stronger;
}

void check_root_values(XBuffer& buffer, unsigned cycle, const std::string& label,
                       std::uint32_t count, bool post_mutation) {
    auto& root = buffer.root<WireAllocatorModelRoot>();
    require(root.marker == 0x735731abcULL, "root marker changed over owner/wire lifecycle");
    require(root.values.size() == count + (post_mutation ? 1u : 0u), "root vector count changed");
    for (std::uint32_t i = 0; i < count; ++i)
        require(root.values[i] == i * 31 + cycle, "root vector value changed");
    if (post_mutation) require(root.values.back() == 0x8193u + cycle, "post-load vector mutation changed");
    require(root.label == label, "root string changed");
    require(root.nested.size() == 3, "nested string count changed");
    for (unsigned i = 0; i < 3; ++i)
        require(root.nested[i] == std::string(19 + cycle + i * 11, 'a' + (cycle + i) % 23),
                "nested string changed");
    require(root.values.allocator_state() == buffer.allocator_state() &&
            root.label.allocator_state() == buffer.allocator_state(), "root binding belongs to stale owner");
    detail::verify_structural_wire_graph(root, buffer);
#ifdef S3_PROBE
    require(buffer.s3_validate_index(), "root Core index invalid after save/load/move/swap");
#endif
}

void root_wire_lifecycle() {
    for (const auto policy : {XGrowPolicy::Explicit, XGrowPolicy::Automatic}) {
        constexpr std::size_t maximum = 4 * 1024 * 1024;
        const auto cap = XBuffer::maximum_capacity(maximum).with_growth(policy);
        auto buffer = XBuffer::create<WireAllocatorModelRoot>(65536, cap);
        buffer.root<WireAllocatorModelRoot>().marker = 0x735731abcULL;
        for (unsigned cycle = 0; cycle < 24; ++cycle) {
            // Reacquire root and bindings for every new owner. Deliberately retire
            // a large scratch allocation before a smaller following wave.
            auto& root = buffer.root<WireAllocatorModelRoot>();
            const auto scratch_binding = root.scratch.binding();
            std::destroy_at(&root.scratch);
            std::construct_at(&root.scratch, scratch_binding);
            root.scratch.reserve(cycle % 2 ? 16 : 1024);
            root.scratch.push_back(0x881u + cycle);
            root.values.clear();
            const auto count = std::uint32_t(47 + cycle * 13);
            root.values.reserve(count);
            for (std::uint32_t i = 0; i < count; ++i) root.values.push_back(i * 31 + cycle);
            const auto text_binding = root.label.binding();
            std::destroy_at(&root.label);
            std::construct_at(&root.label, text_binding);
            const std::string label(cycle % 2 ? 31 : 517 + cycle * 3, 'A' + cycle % 23);
            root.label = label;
            root.nested.clear();
            for (unsigned i = 0; i < 3; ++i)
                root.nested.emplace_back() = std::string(19 + cycle + i * 11, 'a' + (cycle + i) % 23);
            check_root_values(buffer, cycle, label, count, false);
            const auto frame = buffer.save_verified<WireAllocatorModelRoot>();
            auto loaded = XBuffer::load_verified<WireAllocatorModelRoot>(frame, cap);
            check_root_values(loaded, cycle, label, count, false);
            require(loaded.grow(65536), "loaded root could not restore mutation headroom");
            XBuffer moved(std::move(loaded));
            check_root_values(moved, cycle, label, count, false);
            auto peer = XBuffer::create<WireAllocatorModelRoot>(65536, cap);
            peer.root<WireAllocatorModelRoot>().marker = 0xdeadbeef;
            swap(moved, peer);
            require(moved.root<WireAllocatorModelRoot>().marker == 0xdeadbeef, "peer root lost during swap");
            check_root_values(peer, cycle, label, count, false);
            auto& again = peer.root<WireAllocatorModelRoot>();
            again.values.push_back(0x8193u + cycle);
            again.label = label + "-post";
            fixed_detail::release_cached_blocks(again.values.allocator_state());
            check_root_values(peer, cycle, label + "-post", count, true);
            buffer = std::move(peer);
            check_root_values(buffer, cycle, label + "-post", count, true);
        }
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        const std::string variant = argc > 1 ? argv[1] : "F";
        require(variant == "F" || variant == "A-wire" || variant == "B-wire", "unknown variant");
        const auto operations = argc > 2 ? std::stoull(argv[2]) : 10000;
        const auto seed_count = argc > 3 ? std::stoull(argv[3]) : 8;
        require(operations > 0 && operations <= 1000000 && seed_count > 0 && seed_count <= 1024,
                "wire trace count outside audit resource limits");
        allocator_model::negative_controls();
        non_tail_hidden(variant);
        tail_hidden_and_growth(variant, false);
        tail_hidden_and_growth(variant, true);
        minimum_and_header_shortfall(variant);
        tail_alignment_padding(variant);
        const bool lower_stored_corner = lower_stored_alignment_corner();
        if (variant == "F") require(lower_stored_corner, "F stored-alignment corner unexpectedly changed");
        WireTraceStats total;
        for (std::size_t index = 0; index < seed_count; ++index) {
            const auto seed = index < allocator_model::seeds.size() ? allocator_model::seeds[index] :
                allocator_model::seeds[index % allocator_model::seeds.size()] +
                    (index + 1) * 0x9e3779b97f4a7c15ULL;
            std::cerr << "wire_seed=" << seed << " operations=" << operations
                      << " automatic=" << index % 2 << '\n';
            const auto stats = randomized_wire(seed, operations, index % 2);
            total.allocations += stats.allocations;
            total.retires += stats.retires;
            total.expands += stats.expands;
            total.flushes += stats.flushes;
            total.exhausted += stats.exhausted;
            total.lower_stored_alignment_reuses += stats.lower_stored_alignment_reuses;
        }
        root_wire_lifecycle();
        std::cout << "{\"status\":\"passed\",\"variant\":\"" << variant
                  << "\",\"seeds\":" << seed_count << ",\"operations\":" << operations * seed_count
                  << ",\"allocations\":" << total.allocations << ",\"retires\":" << total.retires
                  << ",\"expands\":" << total.expands << ",\"flushes\":" << total.flushes
                  << ",\"exhausted\":" << total.exhausted
                  << ",\"lower_stored_alignment_reuses\":" << total.lower_stored_alignment_reuses
                  << ",\"lower_stored_alignment_corner\":" << (lower_stored_corner ? "true" : "false")
#ifdef S3_PROBE
                  << ",\"core_probe\":true"
#else
                  << ",\"core_probe\":false"
#endif
                  << ",\"root_wire_cycles\":48,\"negative_controls\":" << allocator_model::negative_control_count << "}\n";
    } catch (const std::exception& error) {
        std::cerr << "Stage3 wire validation failure: " << error.what() << '\n';
        return 1;
    }
}
