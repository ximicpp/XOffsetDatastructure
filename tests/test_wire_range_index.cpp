// Differential tests for the online registration step, not a complete wire audit.
// Oracle: unsorted vectors and a simple scan of every previously accepted range.
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <vector>
#include "../xoffsetdatastructure.hpp"
#include "bitmap_test_support.hpp"

using namespace XOffsetDatastructure;
namespace wd = XOffsetDatastructure::detail;
namespace {
void need(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
enum class Result : unsigned { accepted, bounds, owned_owned, owned_free, free_owned, free_free };
constexpr const char* bounds_message = "range-index-test: invalid storage range";
struct Interval { std::size_t begin, end; bool operator==(const Interval&) const = default; };
struct Operation { bool free; std::uintptr_t address; std::size_t bytes; };
struct Counters {
    std::uint64_t operations{}, helper_checks{}, valid_permutations{}, random_streams{}, first_rejections{};
    std::array<std::uint64_t, 6> results{};
} counts;
// This oracle neither orders ranges nor calls the candidate overlap helper.
struct PairwiseOracle {
    std::uintptr_t base;
    std::size_t extent;
    std::vector<Interval> owned, free;
    static bool overlap(const std::vector<Interval>& ranges, std::size_t begin, std::size_t end) {
        if (begin == end) return false;
        for (const auto& old : ranges) if (begin < old.end && old.begin < end) return true;
        return false;
    }
    Result apply(const Operation& op) {
        // Preserve alpha.1's existing no-op behavior, including nullptr/nonzero.
        if (op.bytes == 0 || op.address == 0) return Result::accepted;
        if (op.address < base || op.address - base > extent) return Result::bounds;
        const auto begin = static_cast<std::size_t>(op.address - base);
        if (op.bytes > extent - begin) return Result::bounds;
        const auto end = begin + op.bytes;
        if (overlap(owned, begin, end)) return op.free ? Result::free_owned : Result::owned_owned;
        if (overlap(free, begin, end)) return op.free ? Result::free_free : Result::owned_free;
        (op.free ? free : owned).push_back({begin, end});
        return Result::accepted;
    }
};
Result classify(std::string_view what) {
    if (what == bounds_message) return Result::bounds;
    if (what == "wire validation detected overlapping dynamic ranges") return Result::owned_owned;
    if (what == "wire validation detected live/free overlap") return Result::owned_free;
    if (what == "wire validation detected free-list overlap with live ranges") return Result::free_owned;
    if (what == "wire validation detected overlapping free-list ranges") return Result::free_free;
    throw std::runtime_error("unexpected registration error category");
}
struct Fixture {
    alignas(64) std::array<char, 128> arena{};
    wd::WireValidationContext actual;
    PairwiseOracle oracle;
    explicit Fixture(std::size_t extent)
        : actual{.base = arena.data(), .end = arena.data() + extent},
          oracle{reinterpret_cast<std::uintptr_t>(arena.data()), extent, {}, {}} {
        need(extent <= arena.size(), "test extent exceeds real storage");
    }
    Operation op(bool free, std::size_t begin, std::size_t bytes) const {
        return {free, oracle.base + begin, bytes};
    }
    static void same(const wd::WireValidationByteIndex& index, const std::vector<Interval>& expected) {
        bitmap_test::same(index, expected, 128);
    }
    void check_state() const { same(actual.owned_ranges, oracle.owned); same(actual.free_ranges, oracle.free); }
    Result step(const Operation& operation) {
        auto previous_owned = bitmap_test::snapshot(actual.owned_ranges);
        auto previous_free = bitmap_test::snapshot(actual.free_ranges);
        const auto expected = oracle.apply(operation);
        auto observed = Result::accepted;
        try {
            if (operation.free) wd::register_free_range(reinterpret_cast<const void*>(operation.address), operation.bytes, actual, bounds_message);
            else wd::register_owned_range(reinterpret_cast<const void*>(operation.address), operation.bytes, actual, bounds_message);
        } catch (const XException& error) { observed = classify(error.what()); }
        need(observed == expected, "first rejection/category differs from pairwise oracle");
        if (expected != Result::accepted)
            need(previous_owned == bitmap_test::snapshot(actual.owned_ranges) && previous_free == bitmap_test::snapshot(actual.free_ranges), "rejected registration changed prior state");
        check_state(); ++counts.operations; ++counts.results[static_cast<unsigned>(observed)];
        return observed;
    }
    void helper(std::size_t begin, std::size_t end) const {
        need(begin <= end, "test does not send reversed ranges to internal helper");
        need(wd::wire_range_overlaps(actual.owned_ranges, begin, end) == PairwiseOracle::overlap(oracle.owned, begin, end), "owned predecessor/successor decision differs");
        need(wd::wire_range_overlaps(actual.free_ranges, begin, end) == PairwiseOracle::overlap(oracle.free, begin, end), "free predecessor/successor decision differs");
        counts.helper_checks += 2;
    }
};
void explicit_boundaries() {
    Fixture f(64);
    f.step(f.op(false, 8, 8)); f.step(f.op(true, 24, 8));
    // New ranges spanning both classes must report owned overlap first.
    need(f.step(f.op(false, 0, 40)) == Result::owned_owned, "owned-first priority lost");
    need(f.step(f.op(true, 0, 40)) == Result::free_owned, "free insertion owned-first priority lost");
    need(f.step(f.op(false, 24, 8)) == Result::owned_free, "live/free category lost");
    need(f.step(f.op(true, 24, 8)) == Result::free_free, "free/free category lost");
    f.step(f.op(false, 0, 8)); f.step(f.op(true, 16, 8)); // Exact adjacency.
    f.step(f.op(false, 56, 8));
    f.step(f.op(false, 64, 0)); f.step(f.op(true, 100, 0));
    f.step({false, 0, 0}); f.step({true, 0, (std::numeric_limits<std::size_t>::max)()});
    f.step({false, f.oracle.base - 1, 1}); f.step(f.op(true, 64, 1));
    f.step(f.op(false, 63, 2)); f.step(f.op(true, 1, (std::numeric_limits<std::size_t>::max)()));
    f.step({false, (std::numeric_limits<std::uintptr_t>::max)(), 1});
    f.step({true, 1, 1});
    for (std::size_t begin = 0; begin <= 64; ++begin)
        for (std::size_t end = begin; end <= 64; ++end) f.helper(begin, end);
    // Bounds and alignment rejection must precede any possible insertion.
    for (const auto alignment : {std::size_t(1), std::size_t(2), std::size_t(4), std::size_t(8), std::size_t(16)}) {
        for (std::size_t begin = 0; begin <= 65; ++begin) {
            const auto address = f.oracle.base + begin;
            const bool expected = begin < 64 && address % alignment == 0;
            bool accepted = true;
            try { wd::validate_storage_range(reinterpret_cast<const void*>(address), 1, alignment, f.actual, bounds_message); }
            catch (const XException& e) { need(std::string_view(e.what()) == bounds_message, "storage-range error text changed"); accepted = false; }
            need(accepted == expected, "storage bounds/alignment decision differs");
        }
    }
    wd::validate_storage_range(nullptr, 0, 1, f.actual, bounds_message);
    bool rejected = false;
    try { wd::validate_storage_range(nullptr, 1, 1, f.actual, bounds_message); } catch (const XException&) { rejected = true; }
    need(rejected, "standalone storage validator accepted nonempty nullptr");
    wd::WireValidationRangeIndex high;
    const auto max = (std::numeric_limits<std::size_t>::max)();
    high.emplace(max - 8, max - 4); high.emplace(max - 2, max);
    need(!wd::wire_range_overlaps(high, max - 4, max - 2), "high-offset adjacency rejected");
    need(wd::wire_range_overlaps(high, max - 5, max - 2), "high-offset overlap missed");
    need(!wd::wire_range_overlaps(high, max, max), "empty high-offset query overlaps");
    // A zero-extent arena also preserves null/zero no-ops and rejects nonempty.
    Fixture empty(0); empty.step(empty.op(false, 0, 0)); empty.step(empty.op(false, 0, 1)); empty.step({true, 0, 1});
}
void exhaustive_small_domain() {
    constexpr std::size_t domain = 6;
    std::vector<Interval> all;
    for (std::size_t begin = 0; begin < domain; ++begin)
        for (std::size_t end = begin + 1; end <= domain; ++end) all.push_back({begin, end});
    std::vector<Interval> selected;
    auto exercise = [&] {
        const auto n = selected.size(); std::vector<std::size_t> order(n); std::iota(order.begin(), order.end(), 0);
        do {
            for (unsigned mask = 0; mask < (1u << n); ++mask) {
                Fixture f(domain);
                for (auto i : order) need(f.step(f.op((mask >> i) & 1u, selected[i].begin, selected[i].end - selected[i].begin)) == Result::accepted, "valid insertion permutation rejected");
                ++counts.valid_permutations;
                for (std::size_t begin = 0; begin <= domain; ++begin)
                    for (std::size_t end = begin; end <= domain; ++end) f.helper(begin, end);
                for (const auto candidate : all) for (bool owner : {false, true}) {
                    // Recreate the exact accepted history, rather than copying
                    // a resource-owning index or adding a production shadow map.
                    Fixture probe(domain);
                    for (auto i : order) probe.step(probe.op((mask >> i) & 1u, selected[i].begin, selected[i].end-selected[i].begin));
                    const auto result = probe.step(probe.op(owner,candidate.begin,candidate.end-candidate.begin));
                    if (result != Result::accepted) ++counts.first_rejections;
                }
            }
        } while (std::next_permutation(order.begin(), order.end()));
    };
    auto visit = [&](auto&& self, std::size_t start) -> void {
        exercise(); if (selected.size() == 3) return;
        for (std::size_t i = start; i < all.size(); ++i) {
            if (PairwiseOracle::overlap(selected, all[i].begin, all[i].end)) continue;
            selected.push_back(all[i]); self(self, i + 1); selected.pop_back();
        }
    };
    visit(visit, 0);
}
struct Random {
    std::uint64_t state = 0x6a09e667f3bcc909ULL;
    std::uint64_t next() { state ^= state >> 12; state ^= state << 25; state ^= state >> 27; return state * 2685821657736338717ULL; }
};
void random_streams() {
    Random random;
    for (unsigned stream = 0; stream < 512; ++stream) {
        Fixture f(64); bool first = true;
        for (unsigned position = 0; position < 96; ++position) {
            const bool owner = (random.next() & 1u) != 0;
            auto address = f.oracle.base + random.next() % 68;
            auto bytes = static_cast<std::size_t>(random.next() % 21);
            switch (random.next() % 31) {
                case 0: address = 0; break;
                case 1: address = f.oracle.base - 1; break;
                case 2: address = (std::numeric_limits<std::uintptr_t>::max)(); break;
                case 3: bytes = (std::numeric_limits<std::size_t>::max)(); break;
                default: break;
            }
            const auto result = f.step({owner, address, bytes});
            if (first && result != Result::accepted) { ++counts.first_rejections; first = false; }
            auto a = static_cast<std::size_t>(random.next() % 65), b = static_cast<std::size_t>(random.next() % 65);
            if (a > b) std::swap(a, b); f.helper(a, b);
        }
        ++counts.random_streams;
    }
}
}
int main() {
    try {
        explicit_boundaries(); exhaustive_small_domain(); random_streams();
        for (auto n : counts.results) need(n > 0, "a required registration result category was never exercised");
        std::cout << "{\"schema\":\"wire-range-index-differential-v1\",\"status\":\"passed\",\"operations\":" << counts.operations
                  << ",\"helper_checks\":" << counts.helper_checks << ",\"valid_insertion_permutations\":" << counts.valid_permutations
                  << ",\"random_streams\":" << counts.random_streams << ",\"first_rejections\":" << counts.first_rejections
                  << ",\"result_counts_accepted_bounds_ownown_ownfree_freeown_freefree\":[" << counts.results[0] << ',' << counts.results[1] << ',' << counts.results[2] << ',' << counts.results[3] << ',' << counts.results[4] << ',' << counts.results[5] << ']'
                  << ",\"seed\":\"0x6a09e667f3bcc909\",\"oracle\":\"unsorted-pairwise\",\"compares_rejected_state\":true,\"complete_wire_audit_each_operation\":false,\"performance_samples\":0}\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
