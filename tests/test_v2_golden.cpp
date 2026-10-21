// V2 golden wire gate (N4): a deterministic model must serialize to the
// pinned bytes (size + FNV-1a fingerprint), and write-read-write must be
// byte-stable for dense and sparse emissions. A V1 wire header is rejected
// with an explicit "format version 1" error.
#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

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
// format change (and say so in the change description).
#define GOLDEN_DENSE_SIZE 1800u
#define GOLDEN_DENSE_FNV 0xa2dd300693cbc32full
#define GOLDEN_SPARSE_SIZE 1856u
#define GOLDEN_SPARSE_FNV 0x5465a3facca00e8full

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

// Deterministic content; `sparse` frees a >= 4 KiB block before saving.
std::string build(bool sparse) {
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
    if (sparse) {
        auto& big = m.entries.emplace_back();
        big.label = text(5, 30);
        for (std::uint32_t j = 0; j < 2000; ++j) big.values.push_back(j);
        auto& tail = m.entries.emplace_back();
        tail.label = text(6, 30);
        m.entries.erase(m.entries.begin() + std::ptrdiff_t(m.entries.size() - 2));
    }
    return buf.save_verified();
}

void check(const std::string& wire, bool expect_sparse, std::size_t pinned_size, std::uint64_t pinned_hash) {
    xo::XWireHeaderV2 h;
    std::memcpy(&h, wire.data(), sizeof(h));
    if ((h.extent_count != 0) != expect_sparse) fail("unexpected dense/sparse emission");
    if (h.hole_count == 0) fail("expected holes in the golden model");
    if (wire.size() != pinned_size || fnv1a(wire) != pinned_hash) {
        std::cerr << "size=" << wire.size() << " fnv=0x" << std::hex << fnv1a(wire) << std::dec << "\n";
        fail("golden bytes changed");
    }
    auto loaded = Buffer(xo::XBuffer::load_verified<golden::Model>(wire));
    if (loaded.save_verified() != wire) fail("write-read-write not byte stable");
    // Again through a growable reload and a no-op mutation cycle.
    auto again = Buffer(xo::XBuffer::load_verified<golden::Model>(
        wire, xo::XBuffer::maximum_capacity(1u << 20).with_growth(xo::XGrowPolicy::Automatic)));
    if (again.root().entries.size() == 0 || again.save_verified() != wire) fail("second rewrite differs");
}
} // namespace

int main() {
    const auto dense = build(false);
    const auto sparse = build(true);
    if (build(false) != dense || build(true) != sparse) fail("emission is not deterministic");
    check(dense, false, GOLDEN_DENSE_SIZE, GOLDEN_DENSE_FNV);
    check(sparse, true, GOLDEN_SPARSE_SIZE, GOLDEN_SPARSE_FNV);

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
    std::cout << "test_v2_golden: dense=" << dense.size() << " sparse=" << sparse.size() << " ok\n";
    return 0;
}
