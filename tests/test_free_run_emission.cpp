// Default (free-run) emission: in each run of at least 4 KiB of adjacent free
// blocks of at least 1 KiB (kMinBlock), skips every block's body (all of it
// but its first word, the link to the next block of its size), nothing else.
//   - a run next to small free blocks skips only its large blocks' bodies;
//   - a run made only of small blocks is not skipped (dense wire);
//   - a single large free block is skipped;
//   - a block that hosts the free directory at its end is shorter by it;
//   - random layouts with few candidates (sorted in local storage) and with
//     many candidates and many runs (heap storage) skip exactly the bodies a
//     reference computed from the image's free directory finds.
// Every wire loads the dense image, save_verified_into emits the same bytes,
// and every skipped byte is a free block's body. (A run at the image tail is
// covered by test_wire_hole_zeroing's declared-size claim, which is
// re-emitted with its tail skipped.)
#include "xoffsetdatastructure.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace xo = XOffsetDatastructure;

// The smallest free block whose body the default emission skips.
constexpr std::uint32_t kMinBlock = xo::detail::wire_free_run_min_block;
static_assert(kMinBlock == 1024);

namespace fr {
struct Blob {
    xo::XVector<std::uint64_t> data;
};
struct Root {
    xo::XVector<Blob> blobs;
};
} // namespace fr
XOFFSET_RECORD_DERIVED(fr::Blob, (), data)
XOFFSET_RECORD_DERIVED(fr::Root, (), blobs)

XOFFSET_REGISTER_SCHEMA_NAME(fr::Root, "test.free_run_emission.root")
namespace XOffsetDatastructure {
template<> struct wire_record_layout<fr::Blob> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<fr::Root> { static constexpr bool ordinary_members = true; };
}

namespace {
using Buffer = xo::TypedXBuffer<fr::Root>;
struct Hole { std::uint32_t offset, bytes; };

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_free_run_emission FAILED: " << message << "\n";
    std::exit(1);
}
void need(bool ok, const std::string& what) { if (!ok) fail(what); }

constexpr xo::XSaveOptions kDense{.emission = xo::XWireEmission::dense};

// Retires the blocks of blobs `indices` (ascending) by erasing the blobs.
void release(Buffer& b, std::vector<std::size_t> indices) {
    auto& blobs = b.plain_root().blobs;
    for (auto it = indices.rbegin(); it != indices.rend(); ++it)
        blobs.erase(blobs.begin() + std::ptrdiff_t(*it));
}

// Blocks of `sizes` bytes, allocated back to back after the outer vector,
// then a live barrier block; frees the blocks whose index is in `freed`.
Buffer make(const std::vector<std::size_t>& sizes, const std::vector<std::size_t>& freed) {
    auto b = Buffer::create(1u << 16, xo::XBuffer::maximum_capacity(1u << 22).with_growth(xo::XGrowPolicy::Automatic));
    auto& r = b.plain_root();
    r.blobs.reserve(sizes.size() + 1);
    for (auto bytes : sizes) {
        auto& blob = r.blobs.emplace_back();
        blob.data.reserve(bytes / 8);
        blob.data.push_back(0x1111111111111111ull);
    }
    r.blobs.emplace_back().data.assign(8, 0x2222222222222222ull); // barrier: keeps the run off the tail
    release(b, freed);
    return b;
}

struct Parsed {
    xo::XWireHeader h{};
    std::vector<Hole> extents, gaps;
};

Parsed parse(const std::string& w) {
    Parsed p;
    std::memcpy(&p.h, w.data(), sizeof(p.h));
    std::size_t at = sizeof(p.h), stored = 0;
    for (std::uint64_t i = 0; i < p.h.extent_count; ++i, at += 8) {
        Hole e; std::memcpy(&e, w.data() + at, 8);
        p.extents.push_back(e);
        stored += e.bytes;
    }
    // A dense wire is the image: one implicit extent after the header.
    if (p.extents.empty()) {
        p.extents.push_back({std::uint32_t(xo::XWIRE_PAYLOAD_BEGIN), std::uint32_t(p.h.used_bytes - xo::XWIRE_PAYLOAD_BEGIN)});
        stored = p.h.used_bytes - xo::XWIRE_PAYLOAD_BEGIN;
    }
    std::uint64_t prev = xo::XWIRE_PAYLOAD_BEGIN;
    for (auto e : p.extents) {
        if (e.offset > prev) p.gaps.push_back({std::uint32_t(prev), std::uint32_t(e.offset - prev)});
        prev = std::uint64_t(e.offset) + e.bytes;
    }
    if (prev < p.h.used_bytes) p.gaps.push_back({std::uint32_t(prev), std::uint32_t(p.h.used_bytes - prev)});
    at += stored;
    need(at == w.size(), "wire framing does not add up");
    return p;
}

std::uint32_t word_at(const std::string& image, std::size_t at) {
    std::uint32_t v;
    std::memcpy(&v, image.data() + at, sizeof(v));
    return v;
}
// The free blocks of a dense wire (the image): its directory's stacks.
std::vector<Hole> free_blocks(const std::string& image) {
    xo::XWireHeader h;
    std::memcpy(&h, image.data(), sizeof(h));
    std::vector<Hole> out;
    if (h.free_dir == 0) return out;
    for (std::uint32_t k = 0, n = word_at(image, h.free_dir); k < n; ++k) {
        const std::uint32_t size = word_at(image, h.free_dir + 8 + 8 * k);
        for (std::uint32_t at = word_at(image, h.free_dir + 12 + 8 * k); at != 0; at = word_at(image, at))
            out.push_back({at, size});
    }
    return out;
}

// The bodies the emission must skip, from the image's free directory: free
// blocks of at least kMinBlock sorted by offset, adjacent ones merged, and in
// every merged run of at least 4 KiB each block's body; nothing when the free
// blocks hold less than 4 KiB.
std::vector<Hole> reference_gaps(const std::string& dense) {
    auto blocks = free_blocks(dense);
    std::uint64_t total = 0;
    for (auto h : blocks) total += h.bytes;
    std::erase_if(blocks, [](Hole h) { return h.bytes < kMinBlock; });
    std::sort(blocks.begin(), blocks.end(), [](Hole a, Hole c) { return a.offset < c.offset; });
    std::vector<Hole> gaps;
    if (total < 4096) return gaps;
    for (std::size_t first = 0; first < blocks.size();) {
        std::size_t last = first + 1;
        while (last < blocks.size() && blocks[last].offset == blocks[last - 1].offset + blocks[last - 1].bytes) ++last;
        if (std::uint64_t(blocks[last - 1].offset) + blocks[last - 1].bytes - blocks[first].offset >= 4096)
            for (std::size_t k = first; k < last; ++k) gaps.push_back({blocks[k].offset + 8, blocks[k].bytes - 8});
        first = last;
    }
    return gaps;
}

// Checks the common properties and returns the skipped ranges.
std::vector<Hole> emit(Buffer& b, const std::string& where) {
    const auto dense = b.save_verified(kDense);
    const auto wire = b.save_verified();
    std::string into = "reused";
    b.save_verified_into(into);
    need(into == wire, where + ": save_verified_into differs");
    const auto p = parse(wire);
    const auto expected = reference_gaps(dense);
    need(p.gaps.size() == expected.size(), where + ": skipped range count differs from the reference");
    for (std::size_t i = 0; i < p.gaps.size(); ++i)
        need(p.gaps[i].offset == expected[i].offset && p.gaps[i].bytes == expected[i].bytes,
             where + ": a skipped range is not a free block's body");
    auto loaded = Buffer::load(wire);
    need(loaded.save_verified(kDense) == dense, where + ": wire does not load the dense image");
    need(wire.size() == dense.size() - [&] { std::size_t s = 0; for (auto g : p.gaps) s += g.bytes; return s; }() +
                            (p.gaps.empty() ? 0 : p.extents.size() * 8),
         where + ": wire size does not match its runs");
    return p.gaps;
}

// Blocks of random sizes (some below kMinBlock), a random subset freed.
void random_layout(std::uint64_t seed, std::size_t blocks, unsigned free_percent, std::size_t& candidates,
                   std::size_t& runs) {
    std::uint64_t s = seed;
    auto next = [&] { s = s * 6364136223846793005ull + 1442695040888963407ull; return s >> 33; };
    std::vector<std::size_t> sizes, freed;
    for (std::size_t i = 0; i < blocks; ++i) {
        sizes.push_back(next() % 8 == 0 ? 64 + 8 * (next() % 16) : 256 + 8 * (next() % 400));
        if (next() % 100 < free_percent) freed.push_back(i);
    }
    auto b = make(sizes, freed);
    const auto gaps = emit(b, "random layout");
    candidates = 0;
    for (auto h : free_blocks(b.save_verified(kDense))) candidates += h.bytes >= kMinBlock ? 1 : 0;
    // Runs: skipped bodies that do not continue a block right before them.
    runs = 0;
    for (std::size_t i = 0; i < gaps.size(); ++i)
        runs += i == 0 || std::uint64_t(gaps[i - 1].offset) + gaps[i - 1].bytes + 8 != gaps[i].offset;
}

std::uint32_t offset_of_block(Buffer& b, std::size_t i) {
    return std::uint32_t(reinterpret_cast<const char*>(b.plain_root().blobs[i].data.data()) -
                         static_cast<const char*>(b.get_address()));
}
} // namespace

int main() {
    {   // 64 B | 2 KiB | 2 KiB | 64 B, all freed: only the two 2 KiB blocks
        // (at least kMinBlock each) form the run; the small neighbours are sent
        // (the last, freed first, holds the free directory).
        auto b = make({64, 2048, 2048, 64}, {});
        const auto first = offset_of_block(b, 1);
        need(offset_of_block(b, 2) == first + 2048 && offset_of_block(b, 0) + 64 == first &&
             offset_of_block(b, 3) == first + 4096, "fixture: blocks are not adjacent");
        release(b, {0, 1, 2, 3});
        const auto gaps = emit(b, "run next to small blocks");
        need(gaps.size() == 2 && gaps[0].offset == first + 8 && gaps[0].bytes == 2040 &&
             gaps[1].offset == first + 2048 + 8 && gaps[1].bytes == 2040,
             "run next to small blocks: expected exactly the two 2 KiB blocks' bodies");
    }
    {   // 40 adjacent 128 B free blocks (5 KiB): not skipped.
        auto b = make(std::vector<std::size_t>(40, 128), [] { std::vector<std::size_t> v; for (std::size_t i = 0; i < 40; ++i) v.push_back(i); return v; }());
        need(b.free_block_bytes() + b.free_directory().bytes >= 5120, "fixture: small blocks missing");
        const auto gaps = emit(b, "run of small blocks");
        need(gaps.empty(), "run of small blocks: must stay dense");
    }
    {   // One 64 KiB free block; its end holds the directory, which is sent.
        auto b = make({64 * 1024}, {});
        const auto at = offset_of_block(b, 0);
        release(b, {0});
        const auto gaps = emit(b, "large block");
        const std::uint32_t skipped = 64 * 1024 - b.free_directory().bytes - 8;
        need(gaps.size() == 1 && gaps[0].offset == at + 8 && gaps[0].bytes == skipped, "large block: not skipped");
    }
    {   // Two 4 KiB blocks separated by a live block. The directory went into
        // the end of the block freed first (the second), which is then too
        // short for a run: only the first block's body is skipped (two
        // extents).
        auto b = make({4096, 512, 4096}, {});
        const auto a = offset_of_block(b, 0), c = offset_of_block(b, 2);
        release(b, {0, 2});
        const auto gaps = emit(b, "two runs");
        const auto dir = b.free_directory();
        need(dir.offset + dir.bytes == c + 4096, "two runs: the directory is not at the second block's end");
        need(gaps.size() == 1 && gaps[0].offset == a + 8 && gaps[0].bytes == 4088, "two runs: expected the first block");
        need(parse(b.save_verified()).h.extent_count == 2, "two runs: expected two extents");
    }
    {   // Random layouts: up to 64 candidates (local storage), and more
        // candidates with more than 8 runs (heap storage).
        bool local = false, spilled = false;
        for (std::uint64_t seed = 1; seed <= 300; ++seed) {
            std::size_t candidates = 0, runs = 0;
            random_layout(seed, seed % 2 ? 60 : 900, seed % 3 == 0 ? 85 : 60, candidates, runs);
            local |= candidates <= 64 && runs >= 1;
            spilled |= candidates > 64 && runs > 8;
        }
        need(local && spilled, "random layouts: both storage paths must be reached");
    }
    std::cout << "test_free_run_emission: 5 groups passed\n";
    return 0;
}
