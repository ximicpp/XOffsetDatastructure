// The ascending live run in verify_structural_wire_graph (WireValidationContext
// ::deferred_owned) must accept and reject exactly what direct registration
// does, with the same message, including live ranges that arrive below the run.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <xoffsetdatastructure.hpp>
#include "bitmap_test_support.hpp"

namespace wd = XOffsetDatastructure::detail;
using XOffsetDatastructure::XException;
using bitmap_test::need;

namespace {
struct Op { bool free; std::size_t begin, bytes; };

struct Outcome {
    std::vector<std::string> results; // "ok" or the error message, up to the first error
    std::vector<std::uint64_t> owned_words, free_words;
    std::size_t owned_count{}, free_count{};
    std::uint64_t owned_bytes{}, free_bytes{};
    bool operator==(const Outcome&) const = default;
};

std::vector<std::uint64_t> words(const wd::WireValidationByteIndex& index) {
    return bitmap_test::snapshot(index).words;
}

// Mirrors verify_structural_wire_graph: the header, the leading free ranges
// (the hole table), then the run switched on for the live ranges.
Outcome run(std::vector<char>& arena, const std::vector<Op>& ops, bool defer) {
    wd::WireValidationContext ctx{.base = arena.data(), .end = arena.data() + arena.size()};
    wd::register_owned_range(ctx.base, 48, ctx, "header");
    Outcome out;
    bool started = false;
    for (const auto& op : ops) {
        if (!started && !op.free) {
            started = true;
            ctx.deferred_owned.enabled = defer;
            ctx.deferred_owned.run_end = ctx.owned_ranges.coverage_bounds().second;
        }
        try {
            if (op.free) wd::register_free_range(ctx.base + op.begin, op.bytes, ctx, "free bounds");
            else wd::register_owned_range(ctx.base + op.begin, op.bytes, ctx, "live bounds");
            out.results.push_back("ok");
        } catch (const XException& e) {
            out.results.push_back(e.what());
            break; // validation stops at the first rejection
        }
    }
    if (out.results.empty() || out.results.back() == "ok") {
        wd::stop_deferring_owned(ctx);
        out.owned_words = words(ctx.owned_ranges);
        out.free_words = words(ctx.free_ranges);
        out.owned_count = ctx.owned_ranges.size();
        out.free_count = ctx.free_ranges.size();
        out.owned_bytes = ctx.owned_bytes;
        out.free_bytes = ctx.free_bytes;
    }
    return out;
}

std::size_t checked = 0, lower = 0, rejected = 0;

Outcome compare(std::vector<char>& arena, const std::vector<Op>& ops, const char* what) {
    const auto fast = run(arena, ops, true);
    const auto direct = run(arena, ops, false);
    if (!(fast == direct)) {
        std::cerr << what << ": ascending run differs from direct registration\n";
        for (std::size_t i = 0; i < std::max(fast.results.size(), direct.results.size()); ++i)
            std::cerr << "  op " << i << ": run="
                      << (i < fast.results.size() ? fast.results[i] : "-") << " direct="
                      << (i < direct.results.size() ? direct.results[i] : "-") << "\n";
        throw std::runtime_error("ascending run mismatch");
    }
    ++checked;
    if (fast.results.back() != "ok") ++rejected;
    return fast;
}

void directed(std::vector<char>& arena) {
    // (1) A lower range that meets a recorded range of the run.
    auto r = compare(arena, {{false, 100, 40}, {false, 200, 40}, {false, 120, 8}}, "lower meets run");
    need(r.results.back() == "wire validation detected overlapping dynamic ranges", "lower overlap message");
    // (2) A lower range whose end crosses run_end.
    r = compare(arena, {{false, 100, 40}, {false, 200, 40}, {false, 150, 110}}, "lower crosses run_end");
    need(r.results.back() == "wire validation detected overlapping dynamic ranges", "crossing overlap message");
    // A lower range in a gap is accepted, and the run keeps going after it.
    r = compare(arena, {{false, 100, 40}, {false, 200, 40}, {false, 150, 40}, {false, 240, 16},
                        {false, 190, 10}, {false, 300, 8}, {false, 194, 2}}, "lower in gap");
    need(r.results.back() == "wire validation detected overlapping dynamic ranges", "gap then overlap");
    r = compare(arena, {{false, 100, 40}, {false, 200, 40}, {false, 150, 40}, {false, 240, 16},
                        {false, 190, 10}, {false, 300, 8}}, "lower in gap accepted");
    need(r.results.back() == "ok", "gap ranges accepted");
    // A lower range that meets only a free range keeps the live/free message.
    r = compare(arena, {{true, 160, 16}, {false, 100, 40}, {false, 200, 40}, {false, 150, 20}}, "lower meets free");
    need(r.results.back() == "wire validation detected live/free overlap", "lower free message");
    // A lower range that meets a flushed range (run full after 128 entries).
    std::vector<Op> full;
    for (std::size_t i = 0; i < 130; ++i) full.push_back({false, 64 + i * 16, 8});
    full.push_back({false, 64 + 3 * 16 + 4, 8});
    r = compare(arena, full, "lower meets flushed");
    need(r.results.back() == "wire validation detected overlapping dynamic ranges", "flushed overlap message");
}

void randomized(std::vector<char>& arena, std::uint64_t seed, int rounds) {
    std::mt19937_64 rng(seed);
    const auto extent = arena.size();
    for (int round = 0; round < rounds; ++round) {
        std::vector<Op> ops;
        // Most cases start with a hole table, as a received wire does.
        for (int h = int(rng() % 4); h > 0; --h)
            ops.push_back({true, 48 + 8 * (rng() % ((extent - 120) / 8)), 8 * (1 + rng() % 4)});
        std::size_t cursor = 48;
        const int n = 1 + int(rng() % 300);
        for (int i = 0; i < n && cursor < extent - 64; ++i) {
            const auto kind = rng() % 16;
            const std::size_t bytes = 8 * (1 + rng() % 6);
            if (kind == 15 && extent > 8192) { // a jump to a later page
                cursor += 8 * (rng() % 4096);
                continue;
            }
            if (kind == 0) { // a free range somewhere
                ops.push_back({true, 48 + 8 * (rng() % ((extent - 120) / 8)), bytes});
            } else if (kind < 4) { // a lower range (may or may not overlap)
                const auto begin = 48 + 8 * (rng() % ((cursor - 40) / 8));
                if (begin + bytes <= extent) { ops.push_back({false, begin, bytes}); ++lower; }
            } else { // the next ascending range, with an occasional gap
                cursor += 8 * (rng() % 3);
                if (cursor + bytes > extent) break;
                ops.push_back({false, cursor, bytes});
                cursor += bytes;
            }
        }
        compare(arena, ops, "random");
    }
}
} // namespace

int main() {
    try {
        std::vector<char> small(4096), large(3 * 65536 + 4096);
        directed(small);
        directed(large);
        randomized(small, 20261021, 3000);
        randomized(large, 20261022, 1000);
        need(rejected > 0 && rejected < checked, "random cases cover both outcomes");
        std::cout << "test_validation_ascending_run: " << checked << " sequences (" << rejected
                  << " rejected, " << lower << " lower ranges) agree\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "test_validation_ascending_run failed: " << e.what() << "\n";
        return 1;
    }
}
