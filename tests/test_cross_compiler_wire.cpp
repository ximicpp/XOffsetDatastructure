// Cross-compiler wire corpus.
//
// Builds a fixed set of V2 wires from deterministic inputs: every container
// kind, inline and heap strings, holes left by mutation, a zero-run-elided
// (sparse) wire, compact wires (save_compact, slack 0 and 8), enum keys
// ordered only by operator<=>, nested records, arrays, bool and floating point
// fields. The same source must produce byte-identical wires with every
// supported compiler (P2996 Clang with libc++, GCC 16 with libstdc++, the
// client compilers), and each compiler must accept the other's wires through
// load_verified.
//
// Each case also has two edited wires: the case's wire loaded, changed by the
// fixed steps of edit() (scalars, container inserts and erases, short strings
// made long), then saved with save_verified (<case>.edit.xwire) and with
// save_compact, slack 0 (<case>.edit-compact.xwire). A receiver that edits a
// wire written elsewhere must write the same bytes as the writer would.
//
// usage:
//   test_cross_compiler_wire                  write and read in memory
//   test_cross_compiler_wire --write <dir>    write <dir>/<case>.xwire and the
//                                             two edited wires of each case
//   test_cross_compiler_wire --read <dir>     load each case from <dir>, then
//                                             require it to equal this build's
//                                             wire byte for byte; edit the
//                                             loaded wire and require both
//                                             results to equal <dir>'s
//   test_cross_compiler_wire --mutate-ref <ref> <out>
//                                             load each case from <ref>, edit
//                                             it and write the two edited
//                                             wires to <out> (diff with <ref>)
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

// Fixed edits of a received wire: scalars, inserts and erases in every
// container kind, a short (inline) string made long (heap), a new map key.
void edit(cw::Root& r) {
    Source in(0x5eed00ed);
    fill(r, in, 3); // appends (and sets epoch and title, edited again below)
    r.epoch ^= 0x00ff00ff00ff00ffull;
    r.title = "a title edited by the receiver, long enough for a heap block";
    if (!r.items.empty()) {
        auto& first = r.items.front();
        first.weight = first.weight * 2.0 + 0.25;
        first.equipped = !first.equipped;
        first.name = "renamed by the receiver to need a heap block";
        first.stats.push_back(0xbeef);
    }
    if (r.items.size() > 2) r.items.erase(r.items.begin() + 2);
    if (!r.units.empty()) {
        r.units.back().hp = -1;
        r.units.back().tag = "u";
    }
    if (!r.labels.empty()) r.labels.pop_back();
    r.keys.insert(-7);
    if (r.keys.size() > 1) r.keys.erase(*std::next(r.keys.begin()));
    r.attrs.insert_or_assign(std::string("edited-key"), std::uint64_t{42});
    r.ranks.insert_or_assign(cw::Rank::r3, "rank three, edited");
    r.rank_set.erase(cw::Rank::r1);
}

constexpr xo::XCompactOptions kTight{.slack_div = 0};
constexpr xo::XCompactOptions kSlack8{.slack_div = 8};

struct Edited {
    std::string verified;
    std::string compact;
};

Edited edited(const std::string& wire) {
    auto b = Buffer::load(wire, cap());
    edit(b.root());
    return {b.save_verified(), b.save_compact(kTight)};
}

struct Case {
    const char* name;
    std::function<std::string()> make;
    // Re-save of a loaded wire that must give the same bytes; empty: the
    // default emission, or zero-run elision for a wire it cannot be.
    std::function<std::string(Buffer&)> resave;
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
        {"compact_slack0", [] {
            Source in(0x5eed0006);
            auto b = Buffer::create(1u << 20, cap());
            fill(b.root(), in, 120);
            churn(b.root(), in);
            return b.save_compact(kTight);
        }, [](Buffer& b) { return b.save_compact(kTight); }},
        {"compact_slack8", [] {
            Source in(0x5eed0007);
            auto b = Buffer::create(1u << 20, cap());
            fill(b.root(), in, 120);
            churn(b.root(), in);
            return b.save_compact(kSlack8);
        }, [](Buffer& b) { return b.save_compact(kSlack8); }},
    };
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) fail("cannot read " + path.string());
    return std::string((std::istreambuf_iterator<char>(f)), {});
}

void require_same(const std::string& name, const std::string& wire, const std::string& expected) {
    if (wire != expected) {
        std::size_t at = 0;
        while (at < wire.size() && at < expected.size() && wire[at] == expected[at]) ++at;
        fail(name + ": accepted, but differs from this build's wire at byte " +
             std::to_string(at) + " (sizes " + std::to_string(wire.size()) + " vs " +
             std::to_string(expected.size()) + ")");
    }
}

void check_wire(const Case& c, const std::string& wire, const std::string& expected) {
    const std::string name = c.name;
    try {
        xo::XWireHeaderV2 header{};
        std::memcpy(&header, wire.data(), sizeof(header));
        auto loaded = Buffer::load(wire, cap());
        if (c.resave) {
            if (c.resave(loaded) != wire)
                fail(name + ": load_verified -> save_compact is not byte-identical");
        } else if (loaded.save_verified() != wire &&
                   (header.extent_count == 0 ||
                    loaded.save_verified(xo::XSaveOptions{.emission = xo::XWireEmission::zero_runs}) != wire)) {
            fail(name + ": load_verified -> save_verified is not byte-identical");
        }
    } catch (const std::exception& e) {
        fail(name + ": load_verified rejected the wire: " + e.what());
    }
    require_same(name, wire, expected);
}

// The edited wires of a received case must load and equal `expected`.
void check_edited(const Case& c, const Edited& got, const Edited& expected) {
    const std::string name = c.name;
    try {
        (void)Buffer::load(got.verified, cap());
        (void)Buffer::load(got.compact, cap());
    } catch (const std::exception& e) {
        fail(name + ": an edited wire does not load: " + e.what());
    }
    require_same(name + ".edit", got.verified, expected.verified);
    require_same(name + ".edit-compact", got.compact, expected.compact);
}

void write_file(const std::filesystem::path& path, const std::string& wire) {
    std::ofstream f(path, std::ios::binary);
    f.write(wire.data(), static_cast<std::streamsize>(wire.size()));
    if (!f) fail("cannot write " + path.string());
}

std::filesystem::path edit_path(const std::filesystem::path& dir, const Case& c, bool compact) {
    return dir / (std::string(c.name) + (compact ? ".edit-compact.xwire" : ".edit.xwire"));
}
} // namespace

int main(int argc, char** argv) {
    const auto all = cases();
    if (argc == 3 && std::string_view(argv[1]) == "--write") {
        const std::filesystem::path dir = argv[2];
        std::filesystem::create_directories(dir);
        for (const auto& c : all) {
            const auto wire = c.make();
            const auto e = edited(wire);
            write_file(dir / (std::string(c.name) + ".xwire"), wire);
            write_file(edit_path(dir, c, false), e.verified);
            write_file(edit_path(dir, c, true), e.compact);
            std::cout << c.name << " " << wire.size() << " bytes, edited " << e.verified.size()
                      << " / compact " << e.compact.size() << " bytes\n";
        }
        return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--read") {
        const std::filesystem::path dir = argv[2];
        for (const auto& c : all) {
            const auto wire = read_file(dir / (std::string(c.name) + ".xwire"));
            check_wire(c, wire, c.make());
            check_edited(c, edited(wire),
                         {read_file(edit_path(dir, c, false)), read_file(edit_path(dir, c, true))});
            std::cout << c.name << " accepted and byte-identical, edits byte-identical\n";
        }
        return 0;
    }
    if (argc == 4 && std::string_view(argv[1]) == "--mutate-ref") {
        const std::filesystem::path ref = argv[2], out = argv[3];
        std::filesystem::create_directories(out);
        for (const auto& c : all) {
            const auto e = edited(read_file(ref / (std::string(c.name) + ".xwire")));
            write_file(edit_path(out, c, false), e.verified);
            write_file(edit_path(out, c, true), e.compact);
            std::cout << c.name << " edited " << e.verified.size() << " / compact " << e.compact.size()
                      << " bytes\n";
        }
        return 0;
    }
    if (argc != 1)
        fail("usage: test_cross_compiler_wire [--write <dir> | --read <dir> | --mutate-ref <ref> <out>]");
    int sparse_seen = 0;
    for (const auto& c : all) {
        const auto wire = c.make();
        if (wire != c.make()) fail(std::string(c.name) + ": generation is not deterministic");
        check_wire(c, wire, wire);
        const auto e = edited(wire);
        check_edited(c, edited(wire), e);
        if (e.verified == wire) fail(std::string(c.name) + ": edit() changed nothing");
        xo::XWireHeaderV2 header{};
        std::memcpy(&header, wire.data(), sizeof(header));
        const std::string_view name(c.name);
        sparse_seen += (name == "sparse" || name == "elided") && header.extent_count != 0;
    }
    if (sparse_seen != 2) fail("the sparse or elided case produced a dense wire");
    std::cout << "cross-compiler wire corpus: " << all.size() << " cases deterministic and self-consistent\n";
    return 0;
}
