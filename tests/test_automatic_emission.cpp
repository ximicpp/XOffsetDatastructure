// Automatic emission (XWireEmission::automatic, the default of XSaveOptions):
// save_verified and save_verified_into send the image, exactly as free_runs
// does, while the image's free blocks are under 512 bytes or under 1/8 of its
// used bytes, and the compact wire (save_compact with the default slack) once
// they are both (detail::wire_prefers_compact of the image header).
//   - a fresh buffer and one with a few small free blocks: the image;
//   - 512 or more free bytes that are under 1/8 of a large image: the image;
//   - under 512 free bytes that are over 1/8 of a small image: the image;
//   - a churned buffer: the compact wire, through both entries, into fresh,
//     reused, too small and exactly sized outputs;
//   - random layouts on both sides of the threshold follow the rule;
//   - a compact wire loads with no free block, its default re-save is the
//     same bytes, and the explicit forms (free_runs, dense, zero_runs) still
//     send the image.
#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace ae {
struct Blob {
    xo::XVector<std::uint64_t> data;
};
struct Root {
    std::uint64_t id;
    xo::XVector<Blob> blobs;
};
} // namespace ae
XOFFSET_RECORD_DERIVED(ae::Blob, (), data)
XOFFSET_RECORD_DERIVED(ae::Root, (), id, blobs)

XOFFSET_REGISTER_SCHEMA_NAME(ae::Root, "test.automatic_emission.root")
namespace XOffsetDatastructure {
template<> struct wire_record_layout<ae::Blob> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<ae::Root> { static constexpr bool ordinary_members = true; };
}

namespace {
using Buffer = xo::TypedXBuffer<ae::Root>;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_automatic_emission FAILED: " << message << "\n";
    std::exit(1);
}
void need(bool ok, const std::string& what) { if (!ok) fail(what); }

constexpr xo::XSaveOptions kRuns{.emission = xo::XWireEmission::free_runs};
constexpr xo::XSaveOptions kDense{.emission = xo::XWireEmission::dense};
constexpr xo::XSaveOptions kElide{.emission = xo::XWireEmission::zero_runs};
static_assert(xo::XSaveOptions{}.emission == xo::XWireEmission::automatic);

// Blobs whose blocks hold `sizes` bytes, allocated back to back, then a live
// barrier blob; erases the blobs whose index is in `freed` (ascending).
Buffer make(const std::vector<std::size_t>& sizes, const std::vector<std::size_t>& freed) {
    auto b = Buffer::create(1u << 16, xo::XBuffer::maximum_capacity(1u << 22).with_growth(xo::XGrowPolicy::Automatic));
    auto& r = b.plain_root();
    r.id = 42;
    r.blobs.reserve(sizes.size() + 1);
    std::uint64_t v = 1;
    for (auto bytes : sizes) {
        auto& blob = r.blobs.emplace_back();
        blob.data.reserve(bytes / 8);
        blob.data.push_back(v++);
    }
    r.blobs.emplace_back().data.assign(8, 0x2222222222222222ull);
    for (auto it = freed.rbegin(); it != freed.rend(); ++it) r.blobs.erase(r.blobs.begin() + std::ptrdiff_t(*it));
    return b;
}

xo::XWireHeader header_of(const std::string& wire) {
    need(wire.size() >= sizeof(xo::XWireHeader), "wire too small");
    xo::XWireHeader h{};
    std::memcpy(&h, wire.data(), sizeof(h));
    return h;
}

// What the rule picks for this buffer, from its image header.
bool prefers_compact(Buffer& b) {
    const auto h = header_of(b.save_verified(kDense));
    return xo::detail::wire_prefers_compact(h.used_bytes, h.free_bytes);
}

// The default emission of `b` through every entry and output shape equals
// `expected`.
void check_entries(Buffer& b, const std::string& expected, const std::string& where) {
    need(b.save_verified() == expected, where + ": save_verified");
    need(b.XBuffer::save_verified<ae::Root>() == expected, where + ": XBuffer::save_verified");
    std::string fresh;
    b.save_verified_into(fresh);
    need(fresh == expected, where + ": save_verified_into (empty output)");
    b.save_verified_into(fresh); // reused output that holds the wire
    need(fresh == expected, where + ": save_verified_into (reused output)");
    std::string small = "previous";
    small.reserve(expected.size() - 1);
    small.assign(small.capacity(), char(0x5a));
    b.save_verified_into(small);
    need(small == expected, where + ": save_verified_into (output one byte short)");
    std::string exact(expected.size(), char(0x5a));
    exact.shrink_to_fit();
    b.save_verified_into(exact);
    need(exact == expected, where + ": save_verified_into (exactly sized output)");
}

// The default sends the image: the free-run wire.
void expect_image(Buffer& b, const std::string& where) {
    need(!prefers_compact(b), where + ": fixture expected below the compact threshold");
    check_entries(b, b.save_verified(kRuns), where);
}

// The default sends the compact wire, which loads clean and re-saves itself.
void expect_compact(Buffer& b, const std::string& where) {
    need(prefers_compact(b), where + ": fixture expected above the compact threshold");
    const auto compact = b.save_compact();
    const auto h = header_of(compact);
    need(h.extent_count == 0 && h.free_dir == 0 && h.free_bytes == 0 && compact.size() == h.used_bytes,
         where + ": not a compact wire");
    check_entries(b, compact, where);
    auto loaded = Buffer::load(compact);
    need(loaded.free_block_bytes() == 0 && loaded.plain_root().id == b.plain_root().id &&
             loaded.plain_root().blobs.size() == b.plain_root().blobs.size(),
         where + ": compact wire loads different contents");
    need(loaded.save_verified() == compact, where + ": a loaded compact wire does not re-save itself");
    // The explicit forms still send the image.
    const auto dense = b.save_verified(kDense);
    need(header_of(dense).used_bytes == dense.size() && header_of(dense).free_bytes != 0,
         where + ": dense emission is not the image");
    need(Buffer::load(b.save_verified(kRuns)).save_verified(kDense) == dense, where + ": free-run wire");
    need(Buffer::load(b.save_verified(kElide)).save_verified(kDense) == dense, where + ": zero-run wire");
}
} // namespace

int main() {
    using xo::detail::wire_auto_compact_min_free;
    using xo::detail::wire_prefers_compact;
    static_assert(wire_auto_compact_min_free == 512 && xo::detail::wire_auto_compact_share == 8);
    static_assert(!wire_prefers_compact(4000, 511) && wire_prefers_compact(4000, 512) &&
                  !wire_prefers_compact(4097, 512) && wire_prefers_compact(4096, 512));
    {
        auto b = Buffer::create(4096);
        b.plain_root().id = 7;
        expect_image(b, "fresh buffer");
    }
    {
        auto b = make({64, 64, 64}, {0, 1});
        need(b.free_block_bytes() != 0, "few small blocks: no free block");
        expect_image(b, "few small free blocks");
    }
    {   // 2 KiB free in an image of about 41 KiB.
        std::vector<std::size_t> sizes(40, 1024);
        auto b = make(sizes, {10, 30});
        const auto h = header_of(b.save_verified(kDense));
        need(h.free_bytes >= wire_auto_compact_min_free && h.free_bytes * 8 < h.used_bytes,
             "large image: fixture is not 512+ free bytes under 1/8");
        expect_image(b, "large image, small share free");
    }
    {   // About 256 free bytes in an image of under 2 KiB.
        auto b = make({256, 200}, {0});
        const auto h = header_of(b.save_verified(kDense));
        need(h.free_bytes != 0 && h.free_bytes < wire_auto_compact_min_free && h.free_bytes * 8 >= h.used_bytes,
             "small image: fixture is not under 512 free bytes over 1/8");
        expect_image(b, "small image, large share free");
    }
    {   // 3 KiB free in an image of about 4 KiB.
        auto b = make({1024, 1024, 1024, 64}, {0, 1, 2});
        expect_compact(b, "churned buffer");
    }
    {   // Read-write rounds: a vector regrown in place leaves its old blocks.
        auto b = make({64}, {});
        for (std::uint64_t i = 0; i < 2000; ++i) b.plain_root().blobs[0].data.push_back(i);
        if (prefers_compact(b)) expect_compact(b, "regrown vector");
        else expect_image(b, "regrown vector");
    }
    {   // Random layouts on both sides of the threshold.
        std::mt19937_64 rng(0x5eedae01);
        int images = 0, compacts = 0, near = 0;
        for (int round = 0; round < 300; ++round) {
            const std::size_t n = 1 + rng() % 24;
            std::vector<std::size_t> sizes, freed;
            for (std::size_t i = 0; i < n; ++i) sizes.push_back(16 + 8 * (rng() % 160));
            for (std::size_t i = 0; i < n; ++i)
                if (rng() % 3 == 0) freed.push_back(i);
            auto b = make(sizes, freed);
            const auto h = header_of(b.save_verified(kDense));
            near += h.free_bytes >= 384 && h.free_bytes < 1024;
            const auto where = "random layout " + std::to_string(round);
            if (wire_prefers_compact(h.used_bytes, h.free_bytes)) {
                ++compacts;
                expect_compact(b, where);
            } else {
                ++images;
                expect_image(b, where);
            }
        }
        need(images > 30 && compacts > 30 && near > 10, "random layouts did not cover both sides of the threshold");
    }
    std::cout << "{\"status\":\"passed\",\"automatic_emission\":true}\n";
    return 0;
}
