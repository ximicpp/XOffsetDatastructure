// Root spare (the automatic emission from 16 KiB): from
// detail::wire_spare_scan_min used bytes, the spare capacity of the root
// object's own vector, map and set heads counts as dead bytes with the free
// bytes, (capacity - size - size / 8 - 1) elements each, signed
// (detail::VerifiedPathsAccess::auto_compact, step 3):
//   - a message built by push_back (a Character with every count times k):
//     k = 5 and 17 go compact and arrive with the same contents; k = 3
//     (under 16 KiB) and the counts that fill their vectors (k = 8, 16) go as
//     the image; repeated sends pick the same wire;
//   - random shapes follow the rule as this test computes it from the
//     root's heads (a vector filled to capacity counts minus the slack the
//     compact wire adds to it, so it can keep the image);
//   - an over-reserved message goes compact from 16 KiB and as the image
//     below;
//   - heads of records held by value (two levels deep too) count, heads
//     inside container elements do not; a root with more than 64 heads is not
//     read (fixed arrays of containers are not admitted wire types: only
//     their head count is checked);
//   - with a spare watch, a root head both watched and read counts once, and
//     the watched heads outside the root count with the root's spare;
//   - save_verified, save_verified_into (every output shape) and the general
//     path after the free state was lost pick the same wire.
#include "xoffsetdatastructure.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <string>

namespace xo = XOffsetDatastructure;

namespace rs {
struct Skill {
    std::uint32_t id{}, level{};
};
struct Item {
    std::uint32_t id{};
    std::uint32_t number{};
    std::uint64_t uuid{};
    std::uint64_t timestamp{};
};
struct Position {
    float x{}, y{}, z{};
};
struct Equip {
    std::uint32_t id{};
    std::uint32_t number{};
    std::uint64_t uuid{};
    std::uint64_t timestamp{};
    std::uint32_t level{};
    xo::XVector<float> attributes;
};
// The CppCon 2024 Character: five root heads and a name.
struct Character {
    std::uint64_t id{};
    xo::XString name;
    std::uint32_t level{};
    float healthpoint{}, manapoint{}, speed{};
    Position pos;
    xo::XMap<std::uint64_t, Item> items;
    xo::XVector<Position> path;
    xo::XVector<float> attributes;
    xo::XVector<Equip> equips;
    xo::XVector<Skill> skills;
};
struct Lane {
    std::uint64_t tag{};
    xo::XVector<std::uint64_t> values;
};
struct Deep {
    std::uint32_t depth{};
    Lane inner;
};
// Root heads in records held by value (lane, deep.inner) and a set; rows
// holds heads inside its elements.
struct Nested {
    std::uint64_t id{};
    Lane lane;
    Deep deep;
    xo::XSet<std::uint64_t> keys;
    xo::XVector<Lane> rows;
};
// 4, 16, 64 and 65 heads, by value.
struct Quad {
    xo::XVector<std::uint64_t> a, b, c, d;
};
struct Hex {
    Quad q0, q1, q2, q3;
};
struct Wide64 {
    std::uint64_t id{};
    Hex h0, h1, h2, h3;
};
struct Wide65 {
    std::uint64_t id{};
    Hex h0, h1, h2, h3;
    xo::XVector<std::uint64_t> extra;
};
// Not a wire type (an array of containers): only counted.
struct ArrayHeads {
    xo::XVector<std::uint64_t> lanes[3];
    Lane pair[2];
};
} // namespace rs
XOFFSET_RECORD(rs::Skill, id, level)
XOFFSET_RECORD(rs::Item, id, number, uuid, timestamp)
XOFFSET_RECORD(rs::Position, x, y, z)
XOFFSET_RECORD(rs::Equip, id, number, uuid, timestamp, level, attributes)
XOFFSET_RECORD(rs::Character, id, name, level, healthpoint, manapoint, speed, pos, items, path, attributes, equips,
               skills)
XOFFSET_RECORD(rs::Lane, tag, values)
XOFFSET_RECORD(rs::Deep, depth, inner)
XOFFSET_RECORD(rs::Nested, id, lane, deep, keys, rows)
XOFFSET_RECORD(rs::Quad, a, b, c, d)
XOFFSET_RECORD(rs::Hex, q0, q1, q2, q3)
XOFFSET_RECORD(rs::Wide64, id, h0, h1, h2, h3)
XOFFSET_RECORD(rs::Wide65, id, h0, h1, h2, h3, extra)
XOFFSET_RECORD(rs::ArrayHeads, lanes, pair)
XOFFSET_REGISTER_SCHEMA_NAME(rs::Character, "test.root_spare.character")
XOFFSET_REGISTER_SCHEMA_NAME(rs::Nested, "test.root_spare.nested")
XOFFSET_REGISTER_SCHEMA_NAME(rs::Wide64, "test.root_spare.wide64")
XOFFSET_REGISTER_SCHEMA_NAME(rs::Wide65, "test.root_spare.wide65")

namespace {
using xo::detail::wire_prefers_compact;
using xo::detail::wire_spare_scan_min;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_root_spare FAILED: " << message << "\n";
    std::exit(1);
}
void need(bool ok, const std::string& what) { if (!ok) fail(what); }

constexpr xo::XSaveOptions kRuns{.emission = xo::XWireEmission::free_runs};
constexpr xo::XSaveOptions kDense{.emission = xo::XWireEmission::dense};

const auto kCap = xo::XBuffer::maximum_capacity(std::size_t(1) << 24).with_growth(xo::XGrowPolicy::Automatic);

template <class T>
xo::TypedXBuffer<T> fresh() {
    auto b = xo::TypedXBuffer<T>::create(4096, kCap);
    b.plain_root().id = 42;
    return b;
}

xo::XWireHeader header_of(const std::string& wire) {
    need(wire.size() >= sizeof(xo::XWireHeader), "wire too small");
    xo::XWireHeader h{};
    std::memcpy(&h, wire.data(), sizeof(h));
    return h;
}

// What the compact wire does not send of one head: (capacity - size -
// size / 8 - 1) elements, signed.
template <class V>
std::int64_t head_spare(const V& v) {
    const auto n = std::int64_t(v.size());
    return (std::int64_t(v.capacity()) - n - n / 8 - 1) * std::int64_t(sizeof(typename V::value_type));
}
std::int64_t root_spare(const rs::Character& r) {
    return head_spare(r.items) + head_spare(r.path) + head_spare(r.attributes) + head_spare(r.equips) +
           head_spare(r.skills);
}
std::int64_t root_spare(const rs::Nested& r) {
    return head_spare(r.lane.values) + head_spare(r.deep.inner.values) + head_spare(r.keys) + head_spare(r.rows);
}

struct Image {
    std::uint64_t used = 0;
    std::uint64_t free_bytes = 0;
};
template <class B>
Image image_of(B& b) {
    const auto h = header_of(b.save_verified(kDense));
    return {h.used_bytes, h.free_bytes};
}

// The rule without a spare watch: the free bytes alone, or from 16 KiB the
// free bytes with the root's own spare.
bool rule(const Image& i, std::int64_t spare) {
    if (wire_prefers_compact(i.used, i.free_bytes)) return true;
    if (i.used < wire_spare_scan_min) return false;
    const std::int64_t dead = std::int64_t(i.free_bytes) + spare;
    return dead > 0 && wire_prefers_compact(i.used, std::uint64_t(dead));
}

// Every entry and output shape sends `expected`, twice. After the free
// state was lost (the general path recomputes it from the live graph, which
// may rewrite the free blocks' link words) the choice is the same: the
// compact wire, or the free-run wire of the recomputed image.
template <class T>
void check_entries(xo::TypedXBuffer<T>& b, const std::string& expected, bool compact, const std::string& where) {
    for (int round = 0; round < 2; ++round) {
        need(b.save_verified() == expected, where + ": save_verified");
        need(b.xo::XBuffer::template save_verified<T>() == expected, where + ": XBuffer::save_verified");
        std::string out;
        b.save_verified_into(out);
        need(out == expected, where + ": save_verified_into (empty output)");
        b.save_verified_into(out);
        need(out == expected, where + ": save_verified_into (reused output)");
        std::string short_out = "previous";
        short_out.reserve(expected.size() - 1);
        short_out.assign(short_out.capacity(), char(0x5a));
        b.save_verified_into(short_out);
        need(short_out == expected, where + ": save_verified_into (output one byte short)");
        std::string exact(expected.size(), char(0x5a));
        exact.shrink_to_fit();
        b.save_verified_into(exact);
        need(exact == expected, where + ": save_verified_into (exactly sized output)");
    }
    auto chosen = [&] { return compact ? b.save_compact() : b.save_verified(kRuns); };
    std::string out;
    b.inband_lose();
    need(!b.free_table_complete(), where + ": the free state is still complete");
    b.save_verified_into(out);
    need(b.free_table_complete(), where + ": the free state was not recomputed");
    need(out == chosen(), where + ": save_verified_into (free state lost)");
    const auto again = out;
    b.inband_lose();
    b.save_verified_into(out);
    need(out == again, where + ": save_verified_into (free state lost, reused output)");
    b.inband_lose();
    need(b.save_verified() == again, where + ": save_verified (free state lost)");
    b.inband_lose();
    need(b.xo::XBuffer::template save_verified<T>() == again, where + ": XBuffer::save_verified (free state lost)");
}

// The default sends the compact wire, which loads with the same contents.
template <class T, class Same>
void expect_compact(xo::TypedXBuffer<T>& b, Same same, const std::string& where) {
    using B = xo::TypedXBuffer<T>;
    const auto compact = b.save_compact();
    const auto h = header_of(compact);
    need(h.extent_count == 0 && h.free_dir == 0 && h.free_bytes == 0 && compact.size() == h.used_bytes,
         where + ": not a compact wire");
    need(compact.size() < b.save_verified(kDense).size(), where + ": the compact wire is not smaller");
    check_entries(b, compact, true, where);
    auto loaded = B::load(compact);
    need(same(loaded.plain_root(), b.plain_root()), where + ": the compact wire loads different contents");
    // The explicit forms still send the image.
    const auto dense = b.save_verified(kDense);
    need(header_of(dense).used_bytes == dense.size(), where + ": dense emission is not the image");
    need(B::load(b.save_verified(kRuns)).save_verified(kDense) == dense, where + ": free-run wire");
}

// The default sends the image, as free_runs does.
template <class T, class Same>
void expect_image(xo::TypedXBuffer<T>& b, Same same, const std::string& where) {
    using B = xo::TypedXBuffer<T>;
    const auto runs = b.save_verified(kRuns);
    check_entries(b, runs, false, where);
    auto loaded = B::load(runs);
    need(same(loaded.plain_root(), b.plain_root()), where + ": the image loads different contents");
}

// ---- Character -------------------------------------------------------------
std::size_t count(double base, double k) { return std::size_t(base * k + 0.5); }

// The 2024 make_character's counts times k (items, path, attributes, equips
// of 8 attributes, skills), every container filled by push_back.
xo::TypedXBuffer<rs::Character> character(double k, std::uint64_t seed = 0) {
    auto b = fresh<rs::Character>();
    auto& r = b.plain_root();
    std::mt19937_64 rng(seed);
    r.name = "abcdefgh";
    r.level = 3;
    for (std::size_t i = 0; i < count(64, k); ++i) r.items[rng()] = rs::Item{std::uint32_t(i), 1, rng(), rng()};
    for (std::size_t i = 0; i < count(16, k); ++i) r.path.push_back(rs::Position{float(i), 1, 2});
    for (std::size_t i = 0; i < count(16, k); ++i) r.attributes.push_back(float(i));
    for (std::size_t i = 0; i < count(8, k); ++i) {
        auto& e = r.equips.emplace_back();
        e.id = std::uint32_t(i);
        e.uuid = rng();
        for (int a = 0; a < 8; ++a) e.attributes.push_back(float(a));
    }
    for (std::size_t i = 0; i < count(4, k); ++i) r.skills.push_back(rs::Skill{std::uint32_t(i), 7});
    return b;
}

bool same_character(const rs::Character& x, const rs::Character& y) {
    if (x.id != y.id || x.name.view() != y.name.view() || x.level != y.level || x.items.size() != y.items.size() ||
        x.path.size() != y.path.size() || x.attributes.size() != y.attributes.size() ||
        x.equips.size() != y.equips.size() || x.skills.size() != y.skills.size())
        return false;
    for (auto i = x.items.begin(), j = y.items.begin(); i != x.items.end(); ++i, ++j)
        if (i->first != j->first || i->second.uuid != j->second.uuid || i->second.timestamp != j->second.timestamp)
            return false;
    for (std::size_t i = 0; i < x.path.size(); ++i)
        if (x.path[i].x != y.path[i].x) return false;
    for (std::size_t i = 0; i < x.attributes.size(); ++i)
        if (x.attributes[i] != y.attributes[i]) return false;
    for (std::size_t i = 0; i < x.equips.size(); ++i) {
        if (x.equips[i].uuid != y.equips[i].uuid || x.equips[i].attributes.size() != y.equips[i].attributes.size())
            return false;
        for (std::size_t a = 0; a < x.equips[i].attributes.size(); ++a)
            if (x.equips[i].attributes[a] != y.equips[i].attributes[a]) return false;
    }
    for (std::size_t i = 0; i < x.skills.size(); ++i)
        if (x.skills[i].id != y.skills[i].id) return false;
    return true;
}

void check_character() {
    struct Case {
        double k;
        bool compact;
    };
    // k = 5 and 17: half-empty vectors after their last doubling; 3: under
    // 16 KiB; 8 and 16: every vector filled to its capacity.
    for (const Case c : {Case{5, true}, Case{17, true}, Case{3, false}, Case{8, false}, Case{16, false}}) {
        auto b = character(c.k);
        const auto where = "Character k = " + std::to_string(int(c.k));
        const auto i = image_of(b);
        need(!wire_prefers_compact(i.used, i.free_bytes), where + ": the free bytes alone prefer compact");
        need(!b.spare_watched(), where + ": a head is watched");
        need((i.used >= wire_spare_scan_min) == (c.k > 4), where + ": fixture on the wrong side of 16 KiB");
        need(rule(i, root_spare(b.plain_root())) == c.compact, where + ": the rule picks the other wire");
        if (c.compact) expect_compact(b, same_character, where);
        else expect_image(b, same_character, where);
    }
    {   // Edits that keep every count send the same wire again.
        auto b = character(5);
        const auto first = b.save_verified();
        b.plain_root().level = 9;
        b.plain_root().path[3].x = 5;
        const auto second = b.save_verified();
        need(header_of(first).used_bytes == header_of(second).used_bytes && second == b.save_compact(),
             "Character k = 5 after an edit: not the compact wire");
    }
}

// Random counts (log-uniform per container, 1 KB to about 300 KB), filled by
// push_back or reserved: every choice follows the rule, both wires occur on
// both sides of 16 KiB.
void check_random_shapes() {
    std::mt19937_64 rng(0x5eed5a7e);
    std::uniform_real_distribution<double> unit(0, 1);
    int compacts = 0, images = 0, small_images = 0;
    for (int round = 0; round < 60; ++round) {
        auto b = fresh<rs::Character>();
        auto& r = b.plain_root();
        auto n = [&](double top) { return std::size_t(std::exp(unit(rng) * std::log(top))); };
        const bool reserve = round % 4 == 3;
        const std::size_t items = n(4000), path = n(3000), attrs = n(6000), equips = n(400), skills = n(1500);
        if (reserve) {
            r.path.reserve(path + n(path + 1));
            r.attributes.reserve(attrs);
        }
        for (std::size_t i = 0; i < items; ++i) r.items[rng()] = rs::Item{std::uint32_t(i), 2, i, i};
        for (std::size_t i = 0; i < path; ++i) r.path.push_back(rs::Position{1, 2, 3});
        for (std::size_t i = 0; i < attrs; ++i) r.attributes.push_back(float(i));
        for (std::size_t i = 0; i < equips; ++i) {
            auto& e = r.equips.emplace_back();
            for (int a = 0; a < 8; ++a) e.attributes.push_back(float(a));
        }
        for (std::size_t i = 0; i < skills; ++i) r.skills.push_back(rs::Skill{1, 2});
        const auto i = image_of(b);
        const bool compact = rule(i, root_spare(r));
        const auto where = "random shape " + std::to_string(round);
        const auto wire = b.save_verified();
        need(wire == (compact ? b.save_compact() : b.save_verified(kRuns)), where + ": not the rule's wire");
        std::string into;
        b.save_verified_into(into);
        need(into == wire, where + ": save_verified_into differs");
        auto loaded = xo::TypedXBuffer<rs::Character>::load(wire);
        need(same_character(loaded.plain_root(), r), where + ": the wire loads different contents");
        compacts += compact;
        images += !compact;
        small_images += !compact && i.used < wire_spare_scan_min && root_spare(r) > std::int64_t(i.used / 8);
    }
    need(compacts > 10 && images > 10 && small_images > 0,
         "random shapes: " + std::to_string(compacts) + " compact, " + std::to_string(images) + " image (" +
             std::to_string(small_images) + " under 16 KiB with spare over 1/8)");
}

// ---- Nested, wide roots ------------------------------------------------------
bool same_nested(const rs::Nested& x, const rs::Nested& y) {
    auto same_vec = [](const xo::XVector<std::uint64_t>& a, const xo::XVector<std::uint64_t>& b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (a[i] != b[i]) return false;
        return true;
    };
    if (x.id != y.id || !same_vec(x.lane.values, y.lane.values) || !same_vec(x.deep.inner.values, y.deep.inner.values) ||
        x.deep.depth != y.deep.depth || x.keys.size() != y.keys.size() || x.rows.size() != y.rows.size())
        return false;
    for (auto i = x.keys.begin(), j = y.keys.begin(); i != x.keys.end(); ++i, ++j)
        if (*i != *j) return false;
    for (std::size_t i = 0; i < x.rows.size(); ++i)
        if (x.rows[i].tag != y.rows[i].tag || !same_vec(x.rows[i].values, y.rows[i].values)) return false;
    return true;
}

void fill(xo::XVector<std::uint64_t>& v, std::size_t reserve, std::size_t n) {
    v.reserve(reserve);
    for (std::size_t i = 0; i < n; ++i) v.push_back(i * 7 + 1);
}

// rows: `count` elements (the vector itself filled to capacity), each with
// `each` values filled to capacity.
void tight_rows(rs::Nested& r, std::size_t count, std::size_t each) {
    r.rows.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto& row = r.rows.emplace_back();
        row.tag = i;
        fill(row.values, each, each);
    }
}

void check_nested() {
    using Buffer = xo::TypedXBuffer<rs::Nested>;
    static_assert(xo::detail::root_head_count<rs::Nested>() == 4);
    static_assert(xo::detail::root_head_count<rs::Character>() == 5);
    static_assert(xo::detail::root_head_count<rs::Wide64>() == 64 && xo::detail::root_head_count<rs::Wide65>() == 65);
    static_assert(xo::detail::root_head_count<rs::ArrayHeads>() == 5);
    static_assert(xo::detail::root_spare_scanned<rs::Nested>() && xo::detail::root_spare_scanned<rs::Wide64>() &&
                  !xo::detail::root_spare_scanned<rs::Wide65>());
    struct Case {
        const char* name;
        std::size_t lane, deep, keys;
        bool compact;
    };
    // Over-reserved root heads (4096 elements, 100 used) next to 24 KiB of
    // filled rows: compact from each kind of head; the same reserve over
    // 2 KiB of rows (under 16 KiB): the image.
    for (const Case c : {Case{"record held by value", 4096, 0, 0, true},
                         Case{"record two levels deep", 0, 4096, 0, true}, Case{"set", 0, 0, 4096, true}}) {
        auto b = fresh<rs::Nested>();
        auto& r = b.plain_root();
        tight_rows(r, 3, 1024);
        if (c.lane) fill(r.lane.values, c.lane, 100);
        if (c.deep) fill(r.deep.inner.values, c.deep, 100);
        if (c.keys) {
            r.keys.reserve(c.keys);
            for (std::uint64_t k = 0; k < 100; ++k) r.keys.insert(k * 3);
        }
        const auto i = image_of(b);
        const auto where = std::string("over-reserved ") + c.name;
        need(i.used >= wire_spare_scan_min && !wire_prefers_compact(i.used, i.free_bytes), where + ": fixture");
        need(rule(i, root_spare(r)) == c.compact, where + ": the rule picks the other wire");
        expect_compact(b, same_nested, where);
    }
    {
        auto b = fresh<rs::Nested>();
        auto& r = b.plain_root();
        tight_rows(r, 1, 256);
        fill(r.lane.values, 1400, 100);
        const auto i = image_of(b);
        need(i.used < wire_spare_scan_min && std::int64_t(i.used) < 8 * root_spare(r), "over-reserved, small: fixture");
        expect_image(b, same_nested, "over-reserved under 16 KiB");
    }
    {   // The spare inside the rows' elements does not count.
        auto b = fresh<rs::Nested>();
        auto& r = b.plain_root();
        r.rows.reserve(4);
        for (std::size_t i = 0; i < 4; ++i) {
            auto& row = r.rows.emplace_back();
            fill(row.values, 2048, 10);
        }
        const auto i = image_of(b);
        need(i.used >= wire_spare_scan_min && !rule(i, root_spare(r)), "element heads: fixture");
        need(b.save_compact().size() * 4 < i.used, "element heads: the compact wire is not much smaller");
        expect_image(b, same_nested, "spare inside container elements");
    }
    {   // Free blocks under the share from 16 KiB: the image as free_runs
        // sends it, by the small path's copy below one run (a 2 KiB block)
        // and by the free-run path from one run (an 8 KiB block).
        struct Free {
            const char* name;
            bool run;
        };
        for (const Free f : {Free{"2 KiB free block", false}, Free{"8 KiB free block", true}}) {
            auto b = fresh<rs::Nested>();
            auto& r = b.plain_root();
            if (!f.run) fill(r.lane.values, 256, 256);
            tight_rows(r, 8, 1024);
            if (f.run) r.rows[0].values.push_back(1); // relocates: its 8 KiB block is freed
            else r.lane.values.push_back(1);          // relocates: its 2 KiB block is freed
            const auto i = image_of(b);
            const auto where = std::string("free blocks, ") + f.name;
            need(i.used >= wire_spare_scan_min && i.free_bytes >= xo::detail::wire_auto_compact_min_free &&
                     (i.free_bytes >= xo::detail::wire_free_run_min_gap) == f.run && !rule(i, root_spare(r)),
                 where + ": fixture");
            expect_image(b, same_nested, where);
        }
    }
    {   // A vector filled to capacity counts minus the compact wire's slack:
        // lane's 5 KB of spare would reach an eighth alone, the 1 KB the
        // compact wire adds to deep.inner's 1024 elements keeps it below.
        auto b = fresh<rs::Nested>();
        auto& r = b.plain_root();
        tight_rows(r, 2, 900);
        fill(r.deep.inner.values, 1024, 1024);
        fill(r.lane.values, 1770, 1000);
        const auto i = image_of(b);
        const std::int64_t lane_only = head_spare(r.lane.values);
        need(i.used >= wire_spare_scan_min && wire_prefers_compact(i.used, i.free_bytes + std::uint64_t(lane_only)) &&
                 !rule(i, root_spare(r)),
             "negative share: fixture (lane alone would go compact)");
        expect_image(b, same_nested, "filled vector against spare");
    }
    {   // 64 heads are read, 65 are not.
        auto w64 = fresh<rs::Wide64>();
        fill(w64.plain_root().h3.q3.d, 8192, 10);
        need(w64.save_verified() == w64.save_compact(), "64 heads: not the compact wire");
        auto w65 = fresh<rs::Wide65>();
        fill(w65.plain_root().extra, 8192, 10);
        const auto i = image_of(w65);
        need(i.used >= wire_spare_scan_min && !wire_prefers_compact(i.used, i.free_bytes), "65 heads: fixture");
        need(w65.save_verified() == w65.save_verified(kRuns), "65 heads: not the image");
        std::string out;
        w65.save_verified_into(out);
        need(out == w65.save_verified(kRuns), "65 heads: save_verified_into not the image");
    }
}

// A watched root head counts once (as the root's head); a watched head
// outside the root counts with the root's spare.
void check_spare_watch() {
    {
        auto b = fresh<rs::Nested>();
        auto& r = b.plain_root();
        tight_rows(r, 4, 1500);
        fill(r.lane.values, 1024, 600);
        r.lane.values.resize(200);
        need(b.spare_watched(), "watched root head: not registered");
        const auto i = image_of(b);
        const std::int64_t watched = (1024 - 200) * 8;
        const std::int64_t spare = root_spare(r);
        need(i.used >= wire_spare_scan_min && !wire_prefers_compact(i.used, i.free_bytes + std::uint64_t(watched)) &&
                 !rule(i, spare) && rule(i, spare + watched),
             "watched root head: fixture (counted twice it would go compact)");
        expect_image(b, same_nested, "watched root head");
        need(b.spare_watched(), "watched root head: dropped");
    }
    {
        auto b = fresh<rs::Nested>();
        auto& r = b.plain_root();
        r.rows.reserve(4);
        fill(r.rows.emplace_back().values, 1024, 600);
        for (std::size_t k = 1; k < 4; ++k) fill(r.rows.emplace_back().values, 1600, 1600);
        r.rows[0].values.resize(200);
        need(b.spare_watched(), "watched element head: not registered");
        fill(r.lane.values, 1024, 200);
        const auto i = image_of(b);
        const std::int64_t watched = (1024 - 200) * 8;
        const std::int64_t spare = root_spare(r);
        need(i.used >= wire_spare_scan_min && !wire_prefers_compact(i.used, i.free_bytes + std::uint64_t(watched)) &&
                 !rule(i, spare) && rule(i, spare + watched),
             "watched element head: fixture (the root's spare alone keeps the image)");
        expect_compact(b, same_nested, "watched element head with the root's spare");
    }
}
} // namespace

int main() {
    static_assert(wire_spare_scan_min == 16384 && xo::detail::wire_spare_scan_max_heads == 64);
    check_character();
    check_random_shapes();
    check_nested();
    check_spare_watch();
    std::cout << "{\"status\":\"passed\",\"root_spare\":true}\n";
    return 0;
}
