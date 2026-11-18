// An empty vector's first block (assign_range, reserve, push_back) is
// published by fixed_detail::publish_first_block, which takes allocate_block's
// inline outcomes itself. In a buffer whose holes and tail a seeded workload
// keeps changing, every first block must land where allocate_block would
// place the same block in a copy with the same hole table (an exact hole, a
// split of the smallest larger hole, a large block or the tail), and hold
// the elements written into it (test_free_table_policy holds allocate_block
// to its placement oracle). Retired blocks read as zero, and lists that grow
// past their block keep every element.
#include "xoffsetdatastructure.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace fb {
struct Pos {
    float x, y, z;
    bool operator==(const Pos&) const = default;
};
struct Root {
    xo::XVector<xo::XVector<std::uint32_t>> words;
    xo::XVector<xo::XVector<Pos>> points;
    xo::XVector<xo::XVector<std::uint8_t>> bytes;
};
} // namespace fb
XOFFSET_RECORD(fb::Pos, x, y, z)
XOFFSET_RECORD_DERIVED(fb::Root, (), words, points, bytes)

namespace {
using Hole = xo::XBufferCore::WireHole;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_first_block FAILED: " << message << "\n";
    std::exit(1);
}
void check(bool condition, const std::string& message) {
    if (!condition) fail(message);
}

// Where allocate_block places `bytes` (block size) in a copy of `buffer`
// with the same hole table, as an offset.
std::size_t predicted_offset(const xo::XBuffer& buffer, std::uint32_t bytes) {
    std::vector<Hole> holes;
    buffer.for_each_free_block([&](Hole h) { holes.push_back(h); });
    const auto wire = buffer.copy_bytes();
    xo::XBufferCore twin(wire.data(), wire.size(), xo::XBufferCore::maximum_capacity(8u * 1024u * 1024u));
    twin.replace_free_blocks(holes);
    return static_cast<std::size_t>(static_cast<char*>(twin.allocate_block(bytes, 8)) - twin.data());
}

std::uint32_t block_of(std::size_t bytes) { return static_cast<std::uint32_t>((bytes + 7u) & ~std::size_t(7)); }

template <typename T>
std::vector<T> values(std::mt19937_64& rng, std::size_t n) {
    std::vector<T> out(n);
    for (auto& v : out) {
        if constexpr (std::is_same_v<T, fb::Pos>) v = fb::Pos{float(rng() % 1000), float(rng() % 7), 0.5f};
        else v = static_cast<T>(rng());
    }
    return out;
}

struct Counts { std::size_t assign = 0, reserve = 0, push = 0, reused = 0, bumped = 0, regrowths = 0; };

// One first block for the empty list v: checks its placement and contents.
template <typename T>
void first_block(xo::XBuffer& buffer, xo::XVector<T>& v, std::mt19937_64& rng, Counts& counts) {
    const std::size_t n = 1 + rng() % (rng() % 4 == 0 ? 1100 : 40);
    const auto src = values<T>(rng, n);
    const auto kind = rng() % 3;
    // push_back's first block holds first_capacity elements, not n.
    const std::size_t elements = kind == 2 ? xo::fixed_detail::first_capacity<T>() : n;
    const std::uint32_t bytes = block_of(elements * sizeof(T));
    const auto used_before = buffer.arena()->used_end;
    const auto expected = predicted_offset(buffer, bytes);
    if (kind == 0) {
        v.assign_range(std::span<const T>(src));
        ++counts.assign;
    } else if (kind == 1) {
        v.reserve(n);
        for (const auto& x : src) v.push_back(x);
        ++counts.reserve;
    } else {
        v.push_back(src[0]);
        ++counts.push;
    }
    const auto at = static_cast<std::size_t>(reinterpret_cast<const char*>(v.data()) - buffer.data());
    check(at == expected, "first block at " + std::to_string(at) + ", allocate_block places it at " +
                              std::to_string(expected));
    (at == used_before ? counts.bumped : counts.reused) += 1;
    const std::size_t written = kind == 2 ? 1 : n;
    check(v.size() == written, "first block size");
    for (std::size_t i = 0; i < written; ++i) check(v[std::uint32_t(i)] == src[i], "first block element");
}

// Destroys one list (its block becomes a hole) and appends an empty one.
template <typename T>
void release_one(xo::XVector<xo::XVector<T>>& lists, std::mt19937_64& rng) {
    const auto at = rng() % lists.size();
    const auto& v = lists[std::uint32_t(at)];
    if (v.capacity() == 0) return;
    const auto* old = reinterpret_cast<const unsigned char*>(v.data());
    const std::size_t old_bytes = block_of(std::size_t(v.capacity()) * sizeof(T));
    lists.erase(lists.begin() + std::ptrdiff_t(at));
    lists.emplace_back();
    check(std::all_of(old, old + old_bytes, [](unsigned char c) { return c == 0; }), "retired block not zeroed");
}

// Pushes onto a non-empty list; its regrowths relocate the elements (small
// plain elements as one block copy) and every element must survive.
template <typename T>
void grow_one(xo::XVector<xo::XVector<T>>& lists, std::mt19937_64& rng, std::size_t& regrowths) {
    auto& v = lists[std::uint32_t(rng() % lists.size())];
    if (v.empty()) return;
    std::vector<T> model(v.begin(), v.end());
    const auto more = values<T>(rng, 1 + rng() % 30);
    for (const auto& x : more) {
        const auto* before = v.data();
        v.push_back(x);
        model.push_back(x);
        if (v.data() != before) ++regrowths;
    }
    check(v.size() == model.size(), "grown size");
    for (std::size_t i = 0; i < model.size(); ++i) check(v[std::uint32_t(i)] == model[i], "grown element");
}
} // namespace

int main() {
    xo::XBuffer buffer(1 << 22);
    auto* root = buffer.make<fb::Root>();
    root->words.resize(48);
    root->points.resize(48);
    root->bytes.resize(48);
    std::mt19937_64 rng(0x6669727374626cull);
    Counts counts;
    for (int step = 0; step < 4000; ++step) {
        const auto pick = rng() % 10;
        const auto list = rng() % 3;
        if (pick < 5) {
            auto first = [&](auto& lists) {
                auto& v = lists[std::uint32_t(rng() % lists.size())];
                if (v.capacity() == 0) first_block(buffer, v, rng, counts);
            };
            if (list == 0) first(root->words);
            else if (list == 1) first(root->points);
            else first(root->bytes);
        } else if (pick < 8) {
            if (list == 0) release_one(root->words, rng);
            else if (list == 1) release_one(root->points, rng);
            else release_one(root->bytes, rng);
        } else {
            if (list == 0) grow_one(root->words, rng, counts.regrowths);
            else if (list == 1) grow_one(root->points, rng, counts.regrowths);
            else grow_one(root->bytes, rng, counts.regrowths);
        }
        // Now and then a reset: first blocks of a message built anew bump.
        if (step % 997 == 996) {
            check(buffer.XBuffer::build<fb::Root>([](fb::Root& r) {
                r.words.resize(48);
                r.points.resize(48);
                r.bytes.resize(48);
            }), "build");
            root = &buffer.unsafe_root<fb::Root>();
        }
    }
    check(counts.assign > 200 && counts.reserve > 200 && counts.push > 200, "every first-block entry ran");
    check(counts.reused > 100 && counts.bumped > 100, "first blocks both reused holes and bumped");
    check(counts.regrowths > 100, "lists regrew");
    std::cout << "test_first_block: " << counts.assign << " assign_range, " << counts.reserve << " reserve, "
              << counts.push << " push_back first blocks (" << counts.reused << " in holes, " << counts.bumped
              << " at the tail), " << counts.regrowths << " regrowths: OK\n";
    return 0;
}
