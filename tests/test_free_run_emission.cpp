// Default (free-run) emission: skips each run of at least 4 KiB of adjacent
// free blocks of at least 256 B, nothing else.
//   - a run next to small holes skips only its large blocks;
//   - a run made only of small holes is not skipped (dense wire);
//   - a single large free block (the large table) is skipped;
//   - two runs give three extents.
// Every wire loads the dense payload, save_verified_into emits the same
// bytes, and a beta.1-style check holds: every skipped byte is a hole. (A run
// at the payload tail is covered by test_wire_hole_zeroing's declared-size
// claim, which is re-emitted with its tail skipped.)
#include "xoffsetdatastructure.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace fr {
struct Blob {
    xo::XVector<std::uint64_t> data;
};
struct Root {
    xo::XVector<Blob> blobs;
};
} // namespace fr

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
    auto& blobs = b.root().blobs;
    for (auto it = indices.rbegin(); it != indices.rend(); ++it)
        blobs.erase(blobs.begin() + std::ptrdiff_t(*it));
}

// Blocks of `sizes` bytes, allocated back to back after the outer vector,
// then a live barrier block; frees the blocks whose index is in `freed`.
Buffer make(const std::vector<std::size_t>& sizes, const std::vector<std::size_t>& freed) {
    auto b = Buffer::create(1u << 16, xo::XBuffer::maximum_capacity(1u << 22).with_growth(xo::XGrowPolicy::Automatic));
    auto& r = b.root();
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
    xo::XWireHeaderV2 h{};
    std::vector<Hole> extents, gaps, holes;
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
    // The wire carries payload bytes from offset 48 (the payload header is the
    // receiver's); a dense wire is one implicit extent.
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
    for (std::uint64_t i = 0; i < p.h.hole_count; ++i, at += 8) {
        Hole h; std::memcpy(&h, w.data() + at, 8);
        p.holes.push_back(h);
    }
    need(at == w.size(), "wire framing does not add up");
    return p;
}

// Checks the common properties and returns the skipped runs.
std::vector<Hole> emit(Buffer& b, const std::string& where) {
    const auto dense = b.save_verified(kDense);
    const auto wire = b.save_verified();
    std::string into = "reused";
    b.save_verified_into(into);
    need(into == wire, where + ": save_verified_into differs");
    const auto p = parse(wire);
    auto holes = p.holes;
    std::sort(holes.begin(), holes.end(), [](Hole a, Hole c) { return a.offset < c.offset; });
    for (auto g : p.gaps) {
        need(g.bytes >= 4096, where + ": skipped a run below 4 KiB");
        std::uint64_t covered = g.offset;
        for (auto h : holes)
            if (h.offset <= covered && std::uint64_t(h.offset) + h.bytes > covered) covered = std::uint64_t(h.offset) + h.bytes;
        need(covered >= std::uint64_t(g.offset) + g.bytes, where + ": skipped bytes outside holes");
    }
    auto loaded = Buffer::load(wire);
    need(loaded.save_verified(kDense) == dense, where + ": wire does not load the dense payload");
    need(wire.size() == dense.size() - [&] { std::size_t s = 0; for (auto g : p.gaps) s += g.bytes; return s; }() +
                            (p.gaps.empty() ? 0 : p.extents.size() * 8),
         where + ": wire size does not match its runs");
    return p.gaps;
}

std::uint32_t offset_of_block(Buffer& b, std::size_t i) {
    return std::uint32_t(reinterpret_cast<const char*>(b.root().blobs[i].data.data()) -
                         static_cast<const char*>(b.get_address()));
}
} // namespace

int main() {
    {   // 64 B | 2 KiB | 2 KiB | 64 B, all freed: only the two 2 KiB blocks
        // (at least 256 B each) form the run; the small neighbours are sent.
        auto b = make({64, 2048, 2048, 64}, {});
        const auto first = offset_of_block(b, 1);
        need(offset_of_block(b, 2) == first + 2048 && offset_of_block(b, 0) + 64 == first &&
             offset_of_block(b, 3) == first + 4096, "fixture: blocks are not adjacent");
        release(b, {0, 1, 2, 3});
        const auto gaps = emit(b, "run next to small holes");
        need(gaps.size() == 1 && gaps[0].offset == first && gaps[0].bytes == 4096,
             "run next to small holes: expected exactly the two 2 KiB blocks");
    }
    {   // 40 adjacent 128 B holes (5 KiB): not skipped.
        auto b = make(std::vector<std::size_t>(40, 128), [] { std::vector<std::size_t> v; for (std::size_t i = 0; i < 40; ++i) v.push_back(i); return v; }());
        need(b.free_block_bytes() >= 5120, "fixture: small holes missing");
        const auto gaps = emit(b, "run of small holes");
        need(gaps.empty(), "run of small holes: must stay dense");
    }
    {   // One 64 KiB free block (large table).
        auto b = make({64 * 1024}, {});
        const auto at = offset_of_block(b, 0);
        release(b, {0});
        const auto gaps = emit(b, "large block");
        need(gaps.size() == 1 && gaps[0].offset == at && gaps[0].bytes == 64 * 1024, "large block: not skipped");
    }
    {   // Two runs separated by a live block: three extents.
        auto b = make({4096, 512, 4096}, {});
        const auto a = offset_of_block(b, 0), c = offset_of_block(b, 2);
        release(b, {0, 2});
        const auto gaps = emit(b, "two runs");
        need(gaps.size() == 2 && gaps[0].offset == a && gaps[1].offset == c, "two runs: expected both 4 KiB blocks");
        need(parse(b.save_verified()).h.extent_count == 3, "two runs: expected three extents");
    }
    std::cout << "test_free_run_emission: 4 groups passed\n";
    return 0;
}
