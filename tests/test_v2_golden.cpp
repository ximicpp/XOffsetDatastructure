// Golden wire gate (N4): a deterministic model must serialize to the pinned
// bytes (size + FNV-1a fingerprint), and write-read-write must be
// byte-stable for dense and zero-run-elided (sparse) emissions. The default
// (free-run) emission must equal an independent build of its rule and load
// to the same image. V1 and V2.0 wire headers are rejected with an explicit
// format version error.
//
// The pinned bytes are those of a build that zeroes what containers vacate
// (M4, XOFFSET_HARDENED): the model erases elements, and without the scrub
// switches their old bytes stay in spare capacity. The format is the same in
// both builds; this single-file program always builds the scrubbed one.
#ifndef XOFFSET_HARDENED
#define XOFFSET_HARDENED 1
#endif
#include "xoffsetdatastructure.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace golden {
struct Entry {
    std::uint32_t id{};
    bool active{};
    xo::XString label;
    xo::XVector<std::uint32_t> values;
};
struct Model {
    std::uint64_t version{};
    xo::XString name;
    xo::XVector<Entry> entries;
    xo::XSet<std::uint32_t> ids;
    xo::XMap<xo::XString, std::uint64_t> props;
};
} // namespace golden
XOFFSET_RECORD_DERIVED(golden::Entry, (), id, active, label, values)
XOFFSET_RECORD_DERIVED(golden::Model, (), version, name, entries, ids, props)

XOFFSET_REGISTER_SCHEMA_NAME(golden::Model, "golden.v2.model")

// Pinned bytes of build(false) / build(true). Update only for an intended
// format change (and say so in the change description). 2026-10-21: hole
// bytes and spare container capacity are emitted as zero (threat model M3/M4);
// retired blocks are cleared, so bytes a reused block does not overwrite are
// zero too. 2026-10-22 (G1): appending to a full vector of records builds the
// new element in the grown block instead of a staged slot, so allocation
// placement changes (dense 1800 -> 1792, 7 -> 5 holes; sparse 1856 -> 1800,
// 8 -> 5 holes, 2 -> 1 extents). The format itself is unchanged.
// 2026-10-22: the default emission skips runs of at least 4 KiB of adjacent
// free blocks of at least 256 B (free_runs, which replaced N10); on this
// model its bytes equal the N10 pin of build(true). XWireEmission::dense and
// ::zero_runs have their own pins (after G1: large dense 10032 -> 9984,
// zero-run elided 1688 -> 1632).
// 2026-10-27 (V2.1 R4): 12-byte container heads without arena_delta: dense
// 1792 -> 1648, large dense 9984 -> 9840, zero-run elided 1632 -> 1520,
// free runs 1800 -> 1656.
// 2026-10-27 (V2.1 R7): 40-byte wire header and the 48-byte payload header is
// no longer on the wire (104 bytes less): dense 1648 -> 1544, large dense
// 9840 -> 9736, zero-run elided 1520 -> 1416, free runs 1656 -> 1552.
// 2026-11-17: an empty vector's first block holds at least 32 bytes
// (fixed_detail::first_capacity), so the model's push_back-grown uint32
// vectors start with room for 8 values instead of 4 and their blocks are
// placed differently (16 bytes more in each wire: dense 1544 -> 1560, large
// dense 9736 -> 9752, zero-run elided 1416 -> 1432, free runs 1552 -> 1568).
// The format itself is unchanged.
// 2026-11-19 (format 3.0): the wire is the image: the 40-byte header, the
// root at 40, and the free directory and free blocks in place (no hole table
// after the payload); free runs skip each free block's body but not its link
// word. Dense 1560 -> 1632, large dense 9752 -> 9824, zero-run elided
// 1432 -> 1504, free runs 1568 -> 1648.
#define GOLDEN_DENSE_SIZE 1632u
#define GOLDEN_DENSE_FNV 0xe82fdc351fa701d3ull
#define GOLDEN_LARGE_DENSE_SIZE 9824u
#define GOLDEN_LARGE_DENSE_FNV 0xc34915d89008998dull
#define GOLDEN_ELIDED_SIZE 1504u
#define GOLDEN_ELIDED_FNV 0x5a339c9fa227ac2bull
#define GOLDEN_N10_SIZE 1648u
#define GOLDEN_N10_FNV 0x2b5fed45acb19bf6ull

namespace {
using Buffer = xo::TypedXBuffer<golden::Model>;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_v2_golden FAILED: " << message << "\n";
    std::exit(1);
}

std::uint64_t fnv1a(std::string_view bytes) {
    std::uint64_t h = 14695981039346656037ull;
    for (unsigned char c : bytes) { h ^= c; h *= 1099511628211ull; }
    return h;
}

std::string text(std::uint32_t seed, std::size_t n) {
    std::string s(n, 'a');
    for (std::size_t i = 0; i < n; ++i) s[i] = char('a' + (seed * 31u + i * 7u) % 26u);
    return s;
}

// Deterministic content; `large` frees a >= 4 KiB block before saving.
std::string build(bool large, xo::XSaveOptions options = {}) {
    auto buf = Buffer::create(4096, xo::XBuffer::maximum_capacity(1u << 20).with_growth(xo::XGrowPolicy::Automatic));
    auto& m = buf.plain_root();
    m.version = 7;
    m.name = "golden-model";               // heap (> 10 bytes)
    for (std::uint32_t i = 0; i < 12; ++i) {
        auto& e = m.entries.emplace_back();
        e.id = 1000 + i;
        e.active = (i % 3) == 0;
        e.label = text(i, i % 2 ? 6 : 24); // inline and heap labels
        for (std::uint32_t j = 0; j < i; ++j) e.values.push_back(i * 100 + j);
        m.ids.insert(i * 17 % 41);
    }
    m.props.insert_or_assign(std::string_view("alpha"), std::uint64_t(1));
    m.props.insert_or_assign(std::string_view("beta-property-name"), std::uint64_t(2));
    // Mutations leave free blocks.
    m.entries[3].label = text(99, 40);
    m.entries[5].values.push_back(5);
    m.entries.erase(m.entries.begin() + 7);
    if (large) {
        auto& big = m.entries.emplace_back();
        big.label = text(5, 30);
        for (std::uint32_t j = 0; j < 2000; ++j) big.values.push_back(j);
        auto& tail = m.entries.emplace_back();
        tail.label = text(6, 30);
        m.entries.erase(m.entries.begin() + std::ptrdiff_t(m.entries.size() - 2));
    }
    return buf.save_verified(options);
}

constexpr xo::XSaveOptions kElide{.emission = xo::XWireEmission::zero_runs};
constexpr xo::XSaveOptions kDense{.emission = xo::XWireEmission::dense};

void check(const std::string& wire, bool expect_sparse, std::size_t pinned_size, std::uint64_t pinned_hash,
           xo::XSaveOptions options) {
    xo::XWireHeader h;
    std::memcpy(&h, wire.data(), sizeof(h));
    if ((h.extent_count != 0) != expect_sparse) fail("unexpected dense/sparse emission");
    if (h.free_dir == 0 || h.free_bytes == 0) fail("expected free blocks in the golden model");
    if (wire.size() != pinned_size || fnv1a(wire) != pinned_hash) {
        std::cerr << "size=" << wire.size() << " fnv=0x" << std::hex << fnv1a(wire) << std::dec << "\n";
        fail("golden bytes changed");
    }
    auto loaded = Buffer(xo::XBuffer::load_verified<golden::Model>(wire));
    if (loaded.save_verified(options) != wire) fail("write-read-write not byte stable");
    // Again through a growable reload and a no-op mutation cycle.
    auto again = Buffer(xo::XBuffer::load_verified<golden::Model>(
        wire, xo::XBuffer::maximum_capacity(1u << 20).with_growth(xo::XGrowPolicy::Automatic)));
    if (again.plain_root().entries.size() == 0 || again.save_verified(options) != wire) fail("second rewrite differs");
}

struct Hole { std::uint32_t offset, bytes; };

std::uint32_t word_at(const std::string& image, std::size_t at) {
    std::uint32_t v;
    std::memcpy(&v, image.data() + at, sizeof(v));
    return v;
}

// The free-run emission, built independently of the library from a dense
// wire (the image): free blocks of at least 1 KiB (from the image's free
// directory) sorted by offset, adjacent ones merged; in every merged run of
// at least 4 KiB each block's body (all but its link word) is skipped
// through the extent table.
std::string free_run_wire(const std::string& dense) {
    xo::XWireHeader h;
    std::memcpy(&h, dense.data(), sizeof(h));
    const std::size_t used = std::size_t(h.used_bytes);
    std::vector<Hole> blocks;
    for (std::uint32_t k = 0, n = h.free_dir ? word_at(dense, h.free_dir) : 0; k < n; ++k) {
        const std::uint32_t size = word_at(dense, h.free_dir + 8 + 8 * k);
        for (std::uint32_t at = word_at(dense, h.free_dir + 12 + 8 * k); at != 0; at = word_at(dense, at))
            if (size >= 1024) blocks.push_back({at, size});
    }
    std::sort(blocks.begin(), blocks.end(), [](Hole a, Hole b) { return a.offset < b.offset; });
    std::vector<Hole> gaps;
    for (std::size_t i = 0; h.free_bytes >= 4096 && i < blocks.size();) {
        std::size_t end = blocks[i].offset + std::size_t(blocks[i].bytes), j = i + 1;
        while (j < blocks.size() && blocks[j].offset == end) end += blocks[j++].bytes;
        if (end - blocks[i].offset >= 4096)
            for (std::size_t k = i; k < j; ++k) gaps.push_back({blocks[k].offset + 8, blocks[k].bytes - 8});
        i = j;
    }
    if (gaps.empty()) return dense;
    std::string table, data;
    std::size_t begin = xo::XWIRE_PAYLOAD_BEGIN;
    auto extent = [&](std::size_t from, std::size_t to) {
        const Hole e{std::uint32_t(from), std::uint32_t(to - from)};
        table.append(reinterpret_cast<const char*>(&e), sizeof(e));
        data.append(dense, from, to - from);
    };
    for (auto g : gaps) { extent(begin, g.offset); begin = g.offset + std::size_t(g.bytes); }
    if (begin < used) extent(begin, used);
    h.extent_count = std::uint32_t(table.size() / 8);
    std::string out(reinterpret_cast<const char*>(&h), sizeof(h));
    return out + table + data;
}
} // namespace

int main() {
    const auto dense = build(false);
    const auto large = build(true);              // default: free runs
    const auto large_dense = build(true, kDense);
    const auto sparse = build(true, kElide);
    if (build(false) != dense || build(true) != large || build(true, kDense) != large_dense ||
        build(true, kElide) != sparse)
        fail("emission is not deterministic");
    if (build(false, kElide) != dense || build(false, kDense) != dense)
        fail("a payload without a free run, or below 4 KiB, must stay dense");
    check(dense, false, GOLDEN_DENSE_SIZE, GOLDEN_DENSE_FNV, {});
    check(large, true, GOLDEN_N10_SIZE, GOLDEN_N10_FNV, {});
    check(large_dense, false, GOLDEN_LARGE_DENSE_SIZE, GOLDEN_LARGE_DENSE_FNV, kDense);
    check(sparse, true, GOLDEN_ELIDED_SIZE, GOLDEN_ELIDED_FNV, kElide);
    {   // The free-run emission (built here, independently) is the same
        // wire, and every form loads the dense image.
        if (free_run_wire(large_dense) != large) fail("free-run emission differs from its rule on the golden model");
        for (const auto* wire : {&large, &sparse}) {
            auto loaded = Buffer(xo::XBuffer::load_verified<golden::Model>(*wire));
            if (loaded.save_verified(kDense) != large_dense) fail("sparse wire does not load the dense payload");
        }
    }

    // V1 and V2.0 rejection: their magic in front of the same bytes.
    for (const char* magic : {"XOFFV1\0\0", "XOFFV2\0\0"}) {
        auto old = dense;
        std::memcpy(old.data(), magic, 8);
        const char* expected = magic[5] == '1' ? "wire format 1 is not supported" : "wire format 2.0 is not supported";
        try {
            (void)xo::XBuffer::load_verified<golden::Model>(old);
            fail("V1 / V2.0 wire accepted");
        } catch (const xo::XException& e) {
            if (std::string(e.what()).find(expected) == std::string::npos)
                fail(std::string("V1 / V2.0 wire rejected with an unclear error: ") + e.what());
        }
    }
    std::cout << "test_v2_golden: dense=" << dense.size() << " large=" << large.size()
              << " large dense=" << large_dense.size() << " elided=" << sparse.size() << " ok\n";
    return 0;
}
