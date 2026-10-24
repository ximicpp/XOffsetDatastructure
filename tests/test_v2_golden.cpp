// V2 golden wire gate (N4): a deterministic model must serialize to the
// pinned bytes (size + FNV-1a fingerprint), and write-read-write must be
// byte-stable for dense and zero-run-elided (sparse) emissions. The pinned
// N10 sparse wire of earlier releases (a >= 4 KiB free run skipped) must
// still load to the same payload. A V1 wire header is rejected with an
// explicit "format version 1" error.
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

XOFFSET_REGISTER_SCHEMA_NAME(golden::Model, "golden.v2.model")

// Pinned V2 bytes of build(false) / build(true). Update only for an intended
// format change (and say so in the change description). 2026-10-21: hole
// bytes and spare container capacity are emitted as zero (threat model M3/M4);
// retired blocks are cleared, so bytes a reused block does not overwrite are
// zero too.
// 2026-10-22: the default emission skips runs of at least 4 KiB of adjacent
// free blocks of at least 256 B (free_runs, which replaced N10); on this
// model its bytes equal the earlier N10 pin of build(true). XWireEmission::
// dense and ::zero_runs have their own pins.
#define GOLDEN_DENSE_SIZE 1800u
#define GOLDEN_DENSE_FNV 0x6457288a053030adull
#define GOLDEN_LARGE_DENSE_SIZE 10032u
#define GOLDEN_LARGE_DENSE_FNV 0xfbe17178398d9717ull
#define GOLDEN_ELIDED_SIZE 1688u
#define GOLDEN_ELIDED_FNV 0x5202003544116936ull
#define GOLDEN_N10_SIZE 1856u
#define GOLDEN_N10_FNV 0x03194efaf8964445ull

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
    auto& m = buf.root();
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
    // Mutations leave holes in the hole table.
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
    xo::XWireHeaderV2 h;
    std::memcpy(&h, wire.data(), sizeof(h));
    if ((h.extent_count != 0) != expect_sparse) fail("unexpected dense/sparse emission");
    if (h.hole_count == 0) fail("expected holes in the golden model");
    if (wire.size() != pinned_size || fnv1a(wire) != pinned_hash) {
        std::cerr << "size=" << wire.size() << " fnv=0x" << std::hex << fnv1a(wire) << std::dec << "\n";
        fail("golden bytes changed");
    }
    auto loaded = Buffer(xo::XBuffer::load_verified<golden::Model>(wire));
    if (loaded.save_verified(options) != wire) fail("write-read-write not byte stable");
    // Again through a growable reload and a no-op mutation cycle.
    auto again = Buffer(xo::XBuffer::load_verified<golden::Model>(
        wire, xo::XBuffer::maximum_capacity(1u << 20).with_growth(xo::XGrowPolicy::Automatic)));
    if (again.root().entries.size() == 0 || again.save_verified(options) != wire) fail("second rewrite differs");
}

struct Hole { std::uint32_t offset, bytes; };

// The N10 emission of earlier releases, built independently of the library:
// maximal runs of adjacent holes of at least 4 KiB are skipped through the
// extent table.
std::string n10_wire(const std::string& dense) {
    xo::XWireHeaderV2 h;
    std::memcpy(&h, dense.data(), sizeof(h));
    const std::size_t used = std::size_t(h.used_bytes), at_holes = sizeof(h) + used;
    std::vector<Hole> holes(std::size_t(h.hole_count));
    std::memcpy(holes.data(), dense.data() + at_holes, holes.size() * sizeof(Hole));
    auto sorted = holes;
    std::sort(sorted.begin(), sorted.end(), [](Hole a, Hole b) { return a.offset < b.offset; });
    std::vector<Hole> gaps;
    for (std::size_t i = 0; i < sorted.size();) {
        std::size_t end = sorted[i].offset + std::size_t(sorted[i].bytes), j = i + 1;
        while (j < sorted.size() && sorted[j].offset == end) end += sorted[j++].bytes;
        if (end - sorted[i].offset >= 4096) gaps.push_back({sorted[i].offset, std::uint32_t(end - sorted[i].offset)});
        i = j;
    }
    std::string table, data;
    std::size_t begin = 0;
    auto extent = [&](std::size_t from, std::size_t to) {
        const Hole e{std::uint32_t(from), std::uint32_t(to - from)};
        table.append(reinterpret_cast<const char*>(&e), sizeof(e));
        data.append(dense, sizeof(h) + from, to - from);
    };
    for (auto g : gaps) { extent(begin, g.offset); begin = g.offset + std::size_t(g.bytes); }
    if (begin < used) extent(begin, used);
    h.extent_count = table.size() / 8;
    std::string out(reinterpret_cast<const char*>(&h), sizeof(h));
    return out + table + data + dense.substr(at_holes);
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
    {   // The earlier N10 emission (built here, independently) is the same
        // wire, and every form loads the dense payload.
        if (n10_wire(large_dense) != large) fail("free-run emission differs from N10 on the golden model");
        for (const auto* wire : {&large, &sparse}) {
            auto loaded = Buffer(xo::XBuffer::load_verified<golden::Model>(*wire));
            if (loaded.save_verified(kDense) != large_dense) fail("sparse wire does not load the dense payload");
        }
    }

    // V1 rejection: a V1 header in front of the same bytes.
    auto v1 = dense;
    std::memcpy(v1.data(), xo::XWIRE_MAGIC_V1, 8);
    v1[8] = 1; v1[9] = 0; // format_major = 1
    try {
        (void)xo::XBuffer::load_verified<golden::Model>(v1);
        fail("V1 wire accepted");
    } catch (const xo::XException& e) {
        if (std::string(e.what()).find("format version 1") == std::string::npos)
            fail(std::string("V1 wire rejected with an unclear error: ") + e.what());
    }
    std::cout << "test_v2_golden: dense=" << dense.size() << " large=" << large.size()
              << " large dense=" << large_dense.size() << " elided=" << sparse.size() << " ok\n";
    return 0;
}
