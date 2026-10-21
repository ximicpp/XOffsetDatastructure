#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>
namespace allocator_model {
inline void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
struct Interval { std::uint64_t begin, end, id; unsigned owner; };
// This oracle knows only ranges and owners; it has no donor-selection or free-list code.
// V2 owners: 0 = live block (extent derived from its owner), 1 = free hole
// (host-side free table / wire hole table), 2 = payload header [0, 48).
inline void validate(std::vector<Interval>& ranges, std::uint64_t limit) {
    std::sort(ranges.begin(), ranges.end(), [](auto a, auto b) { return a.begin < b.begin; });
    std::uint64_t previous = 0;
    for (const auto& r : ranges) {
        require(r.begin < r.end && r.end <= limit, "invalid interval bounds");
        require(r.begin >= previous, "physical ownership overlap");
        require(r.owner <= 2, "invalid owner/index reference");
        previous = r.end;
    }
}
// V2 exact coverage: header + live blocks + holes tile [0, limit) with no gap.
// `ranges` must already be physically ordered and overlap-free (validate or
// OrderedRanges::validate). An unreachable/unrecorded byte is a gap.
inline void require_exact_cover(const std::vector<Interval>& ranges, std::uint64_t limit) {
    std::uint64_t next = 0;
    for (const auto& r : ranges) {
        require(r.begin == next, "payload is not exactly covered (gap before interval)");
        next = r.end;
    }
    require(next == limit, "payload is not exactly covered (gap before used_end)");
}
// A persistent physical-address ordering avoids sorting every complete snapshot.
// This is oracle metadata only: it does not know free-list order, donor rules,
// allocator slot identities, or size classes. All references are stable indices.
class OrderedRanges {
    struct Slot { Interval range; std::uint64_t generation; };
    std::vector<Slot> slots_;
    std::vector<std::size_t> ordered_;
    std::unordered_map<std::uint64_t, std::size_t> slot_for_begin_;
    std::uint64_t generation_ = 0;
public:
    void validate(std::vector<Interval>& ranges, std::uint64_t limit) {
        if (generation_ == (std::numeric_limits<std::uint64_t>::max)()) {
            for (auto& slot : slots_) slot.generation = 0;
            generation_ = 1;
        } else ++generation_;
        const auto count = ranges.size();
        for (const auto range : ranges) {
            require(range.begin < range.end && range.end <= limit, "invalid interval bounds");
            require(range.owner <= 2, "invalid owner/index reference");
            const auto found = slot_for_begin_.find(range.begin);
            std::size_t index;
            if (found == slot_for_begin_.end()) {
                index = slots_.size();
                slots_.push_back({range, 0});
                // Physical bump allocation can append. A newly split free
                // residual inserts into the retained address order, moving indices only.
                if (ordered_.empty() || slots_[ordered_.back()].range.begin < range.begin)
                    ordered_.push_back(index);
                else {
                    const auto place = std::lower_bound(ordered_.begin(), ordered_.end(), range.begin,
                        [&](std::size_t slot, std::uint64_t begin) { return slots_[slot].range.begin < begin; });
                    ordered_.insert(place, index);
                }
                slot_for_begin_.emplace(range.begin, index);
            } else index = found->second;
            require(slots_[index].generation != generation_, "duplicate physical interval begin");
            slots_[index].range = range;
            slots_[index].generation = generation_;
        }
        std::uint64_t previous = 0;
        std::size_t output = 0;
        for (const auto index : ordered_) {
            const auto& slot = slots_[index];
            if (slot.generation != generation_) continue;
            require(slot.range.begin >= previous, "physical ownership overlap");
            previous = slot.range.end;
            ranges[output++] = slot.range;
        }
        require(output == count, "ordered snapshot lost a current interval");
    }
    void reset() noexcept {
        slots_.clear(); ordered_.clear(); slot_for_begin_.clear(); generation_ = 0;
    }
};
// Conservation is independent of the allocator's counters and slot/index state.
// Remember only observed physical unions, never the hull across unknown gaps.
class Coverage {
    struct Span { std::uint64_t begin, end; };
    std::vector<Span> known_, scratch_;
    std::uint64_t declared_loss_bytes_ = 0;
public:
    void observe(const std::vector<Interval>& ranges) {
        scratch_.clear();
        scratch_.reserve(ranges.size());
        for (const auto& range : ranges) {
            require(range.begin < range.end, "invalid coverage interval");
            scratch_.push_back({range.begin, range.end});
        }
        const auto order = [](const Span& a, const Span& b) { return a.begin < b.begin; };
        if (!std::is_sorted(scratch_.begin(), scratch_.end(), order))
            std::sort(scratch_.begin(), scratch_.end(), order);
        std::size_t output = 0;
        for (const auto span : scratch_) {
            if (output && span.begin <= scratch_[output - 1].end)
                scratch_[output - 1].end = std::max(scratch_[output - 1].end, span.end);
            else scratch_[output++] = span;
        }
        scratch_.resize(output);
        std::size_t current = 0;
        for (const auto span : known_) {
            while (current < scratch_.size() && scratch_[current].end <= span.begin) ++current;
            require(current < scratch_.size() && scratch_[current].begin <= span.begin &&
                        scratch_[current].end >= span.end,
                    "previously owned physical bytes disappeared");
        }
        // Swap only after every prior byte is accounted for. On failure known_
        // still contains the preceding successful observation for reproduction.
        known_.swap(scratch_);
    }
    // Only callers handling an explicitly allowed loss event (e.g. best-effort
    // host OOM reclamation) may remove known ownership. Normal traces never do.
    void declare_loss(std::uint64_t begin, std::uint64_t end) {
        require(begin < end, "invalid declared loss");
        auto found = std::find_if(known_.begin(), known_.end(), [&](const Span& span) {
            return span.begin <= begin && span.end >= end;
        });
        require(found != known_.end(), "declared loss includes unknown physical bytes");
        const auto bytes = end - begin;
        require(bytes <= (std::numeric_limits<std::uint64_t>::max)() - declared_loss_bytes_,
                "declared loss count overflow");
        scratch_.clear();
        scratch_.reserve(known_.size() + 1);
        for (const auto span : known_) {
            if (span.end <= begin || span.begin >= end) scratch_.push_back(span);
            else {
                if (span.begin < begin) scratch_.push_back({span.begin, begin});
                if (end < span.end) scratch_.push_back({end, span.end});
            }
        }
        known_.swap(scratch_);
        declared_loss_bytes_ += bytes;
    }
    // A load/new-owner lifecycle may intentionally discard the old host map;
    // the caller must mark that boundary explicitly rather than silently reset.
    void reset() noexcept { known_.clear(); scratch_.clear(); declared_loss_bytes_ = 0; }
    std::uint64_t declared_loss_bytes() const noexcept { return declared_loss_bytes_; }
};
inline constexpr unsigned negative_control_count = 13;
inline void negative_controls() {
    unsigned caught=0;
    for (auto data : {std::vector<Interval>{{16,48,1,0},{32,64,2,1}},
                      std::vector<Interval>{{16,48,1,3}},
                      std::vector<Interval>{{16,128,1,0}}}) {
        try { validate(data,64); } catch(const std::runtime_error&) { ++caught; }
    }
    try {
        Coverage coverage;
        // A 512-byte free donor is known. Handing out only a 64-byte front
        // block loses the 448-byte split residual even if both the allocator's
        // free record and free-byte count lose it together.
        coverage.observe({{48,560,1,1}});
        coverage.observe({{48,112,1,0}});
    } catch (const std::runtime_error&) { ++caught; }
    // Exact coverage: a gap after the header, an interior gap and a gap
    // before used_end must each be rejected even when nothing overlaps.
    for (auto data : {std::vector<Interval>{{8,48,1,2},{48,64,2,0}},
                      std::vector<Interval>{{0,48,1,2},{56,64,2,0}},
                      std::vector<Interval>{{0,48,1,2},{48,56,2,1}}}) {
        try { validate(data,64); require_exact_cover(data,64); }
        catch (const std::runtime_error&) { ++caught; }
    }
    for (auto data : {std::vector<Interval>{{16,48,1,0},{16,48,2,1}},
                      std::vector<Interval>{{16,48,1,0},{16,64,2,1}},
                      std::vector<Interval>{{16,48,1,0},{32,64,2,1}},
                      std::vector<Interval>{{16,128,1,0}},
                      std::vector<Interval>{{16,48,1,3}}}) {
        try { OrderedRanges ordered; ordered.validate(data,64); }
        catch (const std::runtime_error&) { ++caught; }
    }
    try {
        OrderedRanges ordered;
        Coverage coverage;
        std::vector<Interval> ranges{{16,96,1,0},{96,544,2,1}};
        ordered.validate(ranges,544); coverage.observe(ranges);
        ranges={{16,96,1,0}};
        ordered.validate(ranges,544); coverage.observe(ranges);
    } catch (const std::runtime_error&) { ++caught; }
    require(caught==negative_control_count,"oracle negative control escaped");
    // Old slots must never resurrect missing ownership, and a returning begin
    // must receive its current end/id/owner rather than retaining stale fields.
    OrderedRanges ordered;
    std::vector<Interval> current{{64,80,2,1},{16,32,1,0}};
    ordered.validate(current,128);
    require(current[0].begin==16 && current[1].begin==64,"ordered snapshot is not physical order");
    current={{64,96,3,2}};
    ordered.validate(current,128);
    require(current.size()==1 && current[0].begin==64 && current[0].end==96 &&
                current[0].id==3 && current[0].owner==2,"stale physical entry remained current");
    current={{64,96,3,2},{16,40,4,1}};
    ordered.validate(current,128);
    require(current[0].begin==16 && current[0].end==40 && current[0].id==4,
            "returning physical begin retained stale bounds");
    ordered.reset(); current={{16,48,5,0}}; ordered.validate(current,64);
    // A complete V2 tiling passes.
    current={{48,64,6,1},{0,48,7,2},{64,128,8,0}};
    ordered.reset(); ordered.validate(current,128); require_exact_cover(current,128);
}
inline std::uint64_t random(std::uint64_t& state) {
    state ^= state << 13; state ^= state >> 7; state ^= state << 17; return state;
}
inline constexpr std::array<std::uint64_t,8> seeds={0x13579bdf,0x2468ace1,0xdeadbeef,0x12345678,
    0x9e3779b9,0xcafebabe,0x31415926,0x27182818};
}
