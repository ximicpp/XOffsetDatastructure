// Cross-compiler wire corpus.
//
// Builds a fixed set of V2 wires from deterministic inputs: every container
// kind, inline and heap strings, holes left by mutation, a zero-run-elided
// (sparse) wire, enum keys ordered only by operator<=>, nested records,
// arrays, bool and floating point fields. The same source must produce byte-identical wires with every
// supported compiler (P2996 Clang with libc++, GCC 16 with libstdc++), and each
// compiler must accept the other's wires through load_verified.
//
// usage:
//   test_cross_compiler_wire                  write and read in memory
//   test_cross_compiler_wire --write <dir>    write <dir>/<case>.xwire
//   test_cross_compiler_wire --read <dir>     load each case from <dir>, then
//                                             require it to equal this build's
//                                             wire byte for byte
//
// Inputs come from std::mt19937_64 raw output only: its sequence is fixed by
// the standard, while the distributions differ between standard libraries.
// Each draw is a separate statement, never two arguments of one call.
#include "xoffsetdatastructure.hpp"

#include <array>
#include <compare>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xo = XOffsetDatastructure;

namespace cw {
enum class Kind : std::uint8_t { none = 0, sword = 1, bow = 2, staff = 3 };
// Order defined only by an ADL operator<=> (reverse of the underlying value).
enum class Rank : std::uint32_t { r1 = 1, r2, r3, r4, r5, r6, r7, r8 };
std::strong_ordering operator<=>(Rank a, Rank b) noexcept {
    return std::uint32_t(b) <=> std::uint32_t(a);
}

struct Vec3 { float x{}, y{}, z{}; };
struct Item {
    std::uint32_t id{};
    bool equipped{};
    Kind kind{};
    double weight{};
    xo::XString name;
    xo::XVector<std::uint16_t> stats;
};
struct Unit {
    std::int64_t hp{};
    Vec3 pos;
    std::array<std::int16_t, 5> slots{};
    xo::XString tag;
};
struct Root {
    std::uint64_t epoch{};
    xo::XString title;
    xo::XVector<Item> items;
    xo::XVector<Unit> units;
    xo::XVector<xo::XString> labels;
    xo::XSet<std::int32_t> keys;
    xo::XMap<xo::XString, std::uint64_t> attrs;
    xo::XMap<Rank, xo::XString> ranks;
    xo::XSet<Rank> rank_set;
};
} // namespace cw
XOFFSET_RECORD_DERIVED(cw::Vec3, (), x, y, z)
XOFFSET_RECORD_DERIVED(cw::Item, (), id, equipped, kind, weight, name, stats)
XOFFSET_RECORD_DERIVED(cw::Unit, (), hp, pos, slots, tag)
XOFFSET_RECORD_DERIVED(cw::Root, (), epoch, title, items, units, labels, keys, attrs, ranks, rank_set)

XOFFSET_REGISTER_SCHEMA_NAME(cw::Root, "cross_compiler.v2.root")

namespace {
using Buffer = xo::TypedXBuffer<cw::Root>;
constexpr std::size_t kCapacity = 32u << 20;
xo::XBufferCore::MaximumCapacity cap() {
    return xo::XBuffer::maximum_capacity(kCapacity).with_growth(xo::XGrowPolicy::Automatic);
}

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_cross_compiler_wire FAILED: " << message << "\n";
    std::exit(1);
}

struct Source {
    std::mt19937_64 engine;
    explicit Source(std::uint64_t seed) : engine(seed) {}
    std::uint64_t next() { return engine(); }
    std::uint64_t below(std::uint64_t n) { return next() % n; }
    std::string text(std::size_t max_len) {
        std::string s(static_cast<std::size_t>(below(max_len + 1)), ' ');
        for (auto& c : s) c = static_cast<char>('a' + below(26));
        return s;
    }
};

void fill(cw::Root& r, Source& in, std::size_t scale) {
    r.epoch = in.next();
    r.title = in.text(40);
    for (std::size_t i = 0; i < scale; ++i) {
        auto& item = r.items.emplace_back();
        item.id = static_cast<std::uint32_t>(in.next());
        item.equipped = in.below(2) == 1;
        item.kind = static_cast<cw::Kind>(in.below(4));
        item.weight = static_cast<double>(in.below(1u << 20)) / 64.0;
        item.name = in.text(i % 2 ? 8 : 30); // both inline and heap strings
        for (std::size_t k = 0, n = in.below(6); k < n; ++k)
            item.stats.push_back(static_cast<std::uint16_t>(in.next()));
    }
    for (std::size_t i = 0; i < scale / 2 + 1; ++i) {
        auto& unit = r.units.emplace_back();
        unit.hp = static_cast<std::int64_t>(in.next());
        unit.pos = {static_cast<float>(in.below(4096)) / 8.0f, -1.5f, static_cast<float>(i)};
        for (auto& s : unit.slots) s = static_cast<std::int16_t>(in.next());
        unit.tag = in.text(12);
    }
    for (std::size_t i = 0; i < scale; ++i) {
        r.labels.emplace_back(in.text(20));
        r.keys.insert(static_cast<std::int32_t>(in.next()));
        // Two draws in one call would run in an unspecified order (GCC and
        // Clang differ), so every draw is its own statement.
        auto key = in.text(16);
        const auto value = in.next();
        r.attrs.insert_or_assign(std::move(key), value);
    }
    for (std::uint32_t v = 1; v <= 8; ++v) {
        if (in.below(3) == 0) continue;
        r.ranks.insert_or_assign(static_cast<cw::Rank>(v), in.text(14));
        r.rank_set.insert(static_cast<cw::Rank>(v));
    }
}

// Mutations that leave holes: shrink, erase, relocate, string replace.
void churn(cw::Root& r, Source& in) {
    if (r.items.size() > 3) r.items.erase(r.items.begin() + 1);
    for (auto& item : r.items) if (in.below(2)) item.stats.clear();
    if (!r.labels.empty()) r.labels[0] = "a label long enough to need a heap block";
    r.title = "t";
    for (std::size_t i = 0; i < 3 && !r.keys.empty(); ++i) r.keys.erase(*r.keys.begin());
    r.units.reserve(r.units.capacity() * 2 + 1); // relocates: old block becomes a hole
}

struct Case {
    const char* name;
    std::function<std::string()> make;
};

std::vector<Case> cases() {
    return {
        {"empty", [] {
            auto b = Buffer::create(4096, cap());
            return b.save_verified();
        }},
        {"small", [] {
            Source in(0x5eed0001);
            auto b = Buffer::create(65536, cap());
            fill(b.root(), in, 4);
            return b.save_verified();
        }},
        {"medium_holes", [] {
            Source in(0x5eed0002);
            auto b = Buffer::create(1u << 20, cap());
            fill(b.root(), in, 200);
            churn(b.root(), in);
            return b.save_verified();
        }},
        {"reloaded_mutated", [] {
            Source in(0x5eed0003);
            auto b = Buffer::create(1u << 20, cap());
            fill(b.root(), in, 60);
            auto again = Buffer::load(b.save_verified(), cap());
            churn(again.root(), in);
            fill(again.root(), in, 10); // reuses holes from the hole table
            return again.save_verified();
        }},
        {"sparse", [] {
            Source in(0x5eed0004);
            auto b = Buffer::create(1u << 20, cap());
            auto& r = b.root();
            fill(r, in, 8);
            r.labels.emplace_back(std::string(64 * 1024, 'x'));
            r.items.emplace_back().name = "after the large block";
            r.labels.pop_back(); // frees a 64 KiB run: the default emission skips it
            return b.save_verified();
        }},
        {"elided", [] {
            Source in(0x5eed0005);
            auto b = Buffer::create(1u << 20, cap());
            auto& r = b.root();
            fill(r, in, 8);
            r.labels.emplace_back(std::string(64 * 1024, 'x'));
            r.items.emplace_back().name = "after the large block";
            r.labels.pop_back();
            r.items.reserve(r.items.size() * 4); // zero slack outside holes too
            return b.save_verified(xo::XSaveOptions{.emission = xo::XWireEmission::zero_runs});
        }},
    };
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) fail("cannot read " + path.string());
    return std::string((std::istreambuf_iterator<char>(f)), {});
}

void check_wire(const char* name, const std::string& wire, const std::string& expected) {
    try {
        xo::XWireHeaderV2 header{};
        std::memcpy(&header, wire.data(), sizeof(header));
        auto loaded = Buffer::load(wire, cap());
        // The default emission, or zero-run elision for a wire it cannot be.
        if (loaded.save_verified() != wire &&
            (header.extent_count == 0 ||
             loaded.save_verified(xo::XSaveOptions{.emission = xo::XWireEmission::zero_runs}) != wire))
            fail(std::string(name) + ": load_verified -> save_verified is not byte-identical");
    } catch (const std::exception& e) {
        fail(std::string(name) + ": load_verified rejected the wire: " + e.what());
    }
    if (wire != expected) {
        std::size_t at = 0;
        while (at < wire.size() && at < expected.size() && wire[at] == expected[at]) ++at;
        fail(std::string(name) + ": accepted, but differs from this build's wire at byte " +
             std::to_string(at) + " (sizes " + std::to_string(wire.size()) + " vs " +
             std::to_string(expected.size()) + ")");
    }
}
} // namespace

int main(int argc, char** argv) {
    const auto all = cases();
    if (argc == 3 && std::string_view(argv[1]) == "--write") {
        const std::filesystem::path dir = argv[2];
        std::filesystem::create_directories(dir);
        for (const auto& c : all) {
            const auto wire = c.make();
            std::ofstream f(dir / (std::string(c.name) + ".xwire"), std::ios::binary);
            f.write(wire.data(), static_cast<std::streamsize>(wire.size()));
            if (!f) fail(std::string("cannot write ") + c.name);
            std::cout << c.name << " " << wire.size() << " bytes\n";
        }
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--read") {
        const std::filesystem::path dir = argv[2];
        for (const auto& c : all) {
            check_wire(c.name, read_file(dir / (std::string(c.name) + ".xwire")), c.make());
            std::cout << c.name << " accepted and byte-identical\n";
        }
        return 0;
    }
    if (argc != 1) fail("usage: test_cross_compiler_wire [--write <dir> | --read <dir>]");
    int sparse_seen = 0;
    for (const auto& c : all) {
        const auto wire = c.make();
        if (wire != c.make()) fail(std::string(c.name) + ": generation is not deterministic");
        check_wire(c.name, wire, wire);
        xo::XWireHeaderV2 header{};
        std::memcpy(&header, wire.data(), sizeof(header));
        const std::string_view name(c.name);
        sparse_seen += (name == "sparse" || name == "elided") && header.extent_count != 0;
    }
    if (sparse_seen != 2) fail("the sparse or elided case produced a dense wire");
    std::cout << "cross-compiler wire corpus: " << all.size() << " cases deterministic and self-consistent\n";
    return 0;
}
