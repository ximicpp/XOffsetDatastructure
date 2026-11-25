// Shared deltas (XS) between the versions of an add-only family
// (docs/SCHEMA_EVOLUTION.md). A change body names the members of
// the family's first version, so every version writes and reads the same
// bytes: a later version's new fields travel inside the spare member they
// were taken from. Pinned here, for three versions of a unit record with
// containers, a plain record (diff ops) and the root record:
//
// - whole bodies (enter ops) of any version are byte-identical after a
//   round trip through any other version, which keeps the newer fields in
//   its spare bytes;
// - changed() and every public mask count the version's own members (bit i:
//   member i of T); put_change() writes the first version's member mask, so
//   the same change is the same change body in every version;
// - diff ops of plain records are the same bytes in every version;
// - whole messages (key, then a delta with leave, change, enter and diff
//   sections) from a server of any version apply on clients of every
//   version, XCheck::full and XCheck::admit; the same content gives the same
//   message bytes from every version.
//
// Field lists, so every build (C++20 clients too) runs it. Built with
// XOFFSET_FORCE_FULL_CHECK=0 so admit is the real admit path. No exception
// of its own: also built as test_schema_family_xs_noexc.
#define XOFFSET_FORCE_FULL_CHECK 0
#include "../xoffset_receiver.hpp"
#include "../xoffset_shared_delta.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace xo = XOffsetDatastructure;
namespace xs = XOffsetDatastructure::xsync;

namespace {
int failures = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                            \
        }                                                                          \
    } while (0)
} // namespace

namespace fxs {
namespace v1 {
struct Head { std::int64_t tick{}; std::uint32_t seq{}; xo::XSpare<4> spare{}; };
struct Pos { std::int16_t x{}; std::int16_t y{}; xo::XSpare<4> spare{}; };
struct Unit {
    std::uint32_t hp{};
    bool on{};
    std::uint8_t kind{};
    xo::XSpare<2> more{};
    Pos pos;
    xo::XString name;
    xo::XVector<std::uint16_t> marks;
    xo::XSpare<8> spare{};
};
struct Tile { std::int32_t owner{}; std::uint16_t kind{}; xo::XSpare<2> spare{}; };
struct View { Head head; xo::XVector<Unit> units; xo::XVector<Tile> tiles; };
} // namespace v1

namespace v2 {
struct Head { std::int64_t tick{}; std::uint32_t seq{}; std::uint32_t phase{}; };   // new: phase
struct Pos { std::int16_t x{}; std::int16_t y{}; std::int16_t z{}; xo::XSpare<2> spare{}; };   // new: z
struct Unit {
    std::uint32_t hp{};
    bool on{};
    std::uint8_t kind{};
    std::uint8_t team{};                 // new
    bool elite{};                        // new
    Pos pos;
    xo::XString name;
    xo::XVector<std::uint16_t> marks;
    std::uint32_t shield{};              // new
    xo::XSpare<4> spare{};
};
struct Tile { std::int32_t owner{}; std::uint16_t kind{}; std::uint8_t level{}; std::uint8_t flags{}; };   // new: level, flags
struct View { Head head; xo::XVector<Unit> units; xo::XVector<Tile> tiles; };
} // namespace v2

// A field in v2 that spans two adjacent XSpare members of v1, and a
// zero-padding nested record carved out of XSpare.
namespace span_spare {
struct CleanSub { std::uint8_t tag{}; xo::XSpare<3> pad{}; std::uint32_t val{}; };
struct V1 { std::uint64_t id{}; xo::XSpare<4> s1{}; xo::XSpare<4> s2{}; xo::XSpare<8> sub_spare{}; };
struct V2 { std::uint64_t id{}; std::uint64_t big{}; CleanSub sub{}; };
} // namespace span_spare

namespace v3 {
using v2::Head;
using v2::Tile;
struct Pos { std::int16_t x{}; std::int16_t y{}; std::int16_t z{}; std::int16_t facing{}; };   // new: facing
struct Unit {
    std::uint32_t hp{};
    bool on{};
    std::uint8_t kind{};
    std::uint8_t team{};
    bool elite{};
    Pos pos;
    xo::XString name;
    xo::XVector<std::uint16_t> marks;
    std::uint32_t shield{};
    float speed{};                       // new; nothing spare left
};
struct View { Head head; xo::XVector<Unit> units; xo::XVector<Tile> tiles; };
} // namespace v3
} // namespace fxs

XOFFSET_RECORD(fxs::v1::Head, tick, seq, spare)
XOFFSET_RECORD(fxs::v1::Pos, x, y, spare)
XOFFSET_RECORD(fxs::v1::Unit, hp, on, kind, more, pos, name, marks, spare)
XOFFSET_RECORD(fxs::v1::Tile, owner, kind, spare)
XOFFSET_RECORD(fxs::v1::View, head, units, tiles)
XOFFSET_RECORD(fxs::v2::Head, tick, seq, phase)
XOFFSET_RECORD(fxs::v2::Pos, x, y, z, spare)
XOFFSET_RECORD(fxs::v2::Unit, hp, on, kind, team, elite, pos, name, marks, shield, spare)
XOFFSET_RECORD(fxs::v2::Tile, owner, kind, level, flags)
XOFFSET_RECORD(fxs::v2::View, head, units, tiles)
XOFFSET_RECORD(fxs::v3::Pos, x, y, z, facing)
XOFFSET_RECORD(fxs::v3::Unit, hp, on, kind, team, elite, pos, name, marks, shield, speed)
XOFFSET_RECORD(fxs::v3::View, head, units, tiles)
XOFFSET_RECORD(fxs::span_spare::CleanSub, tag, pad, val)
XOFFSET_RECORD(fxs::span_spare::V1, id, s1, s2, sub_spare)
XOFFSET_RECORD(fxs::span_spare::V2, id, big, sub)
XOFFSET_REGISTER_SCHEMA_NAME(fxs::span_spare::V1, "test.fxs.SpanSpare")
XOFFSET_REGISTER_SCHEMA_NAME(fxs::span_spare::V2, "test.fxs.SpanSpare")
XOFFSET_SCHEMA_FAMILY(fxs::span_spare::V2, fxs::span_spare::V1)

XOFFSET_REGISTER_SCHEMA_NAME(fxs::v1::View, "test.family_xs.View")
XOFFSET_REGISTER_SCHEMA_NAME(fxs::v2::View, "test.family_xs.View")
XOFFSET_REGISTER_SCHEMA_NAME(fxs::v3::View, "test.family_xs.View")

// The records a shared delta sends declare their families too: a change
// body numbers the members of the first version.
XOFFSET_SCHEMA_FAMILY(fxs::v2::Head, fxs::v1::Head)
XOFFSET_SCHEMA_FAMILY(fxs::v2::Unit, fxs::v1::Unit)
XOFFSET_SCHEMA_FAMILY(fxs::v3::Unit, fxs::v2::Unit)
XOFFSET_SCHEMA_FAMILY(fxs::v2::Tile, fxs::v1::Tile)
XOFFSET_SCHEMA_FAMILY(fxs::v2::View, fxs::v1::View)
XOFFSET_SCHEMA_FAMILY(fxs::v3::View, fxs::v2::View)

namespace {
using U1 = fxs::v1::Unit;
using U2 = fxs::v2::Unit;
using U3 = fxs::v3::Unit;
using B1 = xs::XRecordBody<U1>;
using B2 = xs::XRecordBody<U2>;
using B3 = xs::XRecordBody<U3>;

// Numbering: the first version's members on the wire, the version's own in
// every public mask.
static_assert(sizeof(U1) == sizeof(U2) && sizeof(U2) == sizeof(U3));
static_assert(!B1::in_family && B2::in_family && B3::in_family);
static_assert(B1::members::N == 8 && B2::members::N == 8 && B3::members::N == 8);
static_assert(B1::mask_bits == 8 && B2::mask_bits == 10 && B3::mask_bits == 10);
static_assert(B3::mask_all == 0x3ff);
static_assert(std::is_same_v<B3::members::type<3>, xo::XSpare<2>>);       // team and elite: v1's spare bytes
static_assert(std::is_same_v<B3::members::type<4>, fxs::v1::Pos>);        // a plain record: its bytes
static_assert(std::is_same_v<B3::members::type<6>, xo::XVector<std::uint16_t>>);
static_assert(std::is_same_v<B3::members::type<7>, xo::XSpare<8>>);       // shield and speed
static_assert(B3::members::kind<1> == xs::sd_detail::kMemberPlain && B3::members::bools<1>.size() == 1);
static_assert(B3::members::bools<3>.size() == 1 && B3::members::bools<3>[0] == 1);   // v3 checks its elite byte
static_assert(B1::members::bools<3>.size() == 0);                                  // v1: opaque bytes
static_assert(B1::diff_room == B3::diff_room);
static_assert(xs::XRecordBody<fxs::v2::Tile>::members::N == 3 && xs::XRecordBody<fxs::v2::Tile>::mask_bits == 4);

template <class U> constexpr int version_of = 0;
template <> constexpr int version_of<U1> = 1;
template <> constexpr int version_of<U2> = 2;
template <> constexpr int version_of<U3> = 3;

// A unit of version V; the fields of later versions only where V has them.
template <class U>
void fill(U& u, std::uint32_t seed, bool newer) {
    u.hp = 1000 + seed;
    u.on = seed % 2 == 1;
    u.kind = std::uint8_t(seed % 7);
    u.pos.x = std::int16_t(seed * 3);
    u.pos.y = std::int16_t(-int(seed));
    const std::string name = "unit-" + std::to_string(seed);
    u.name = std::string_view(name);
    u.marks.resize(seed % 4);
    for (std::size_t i = 0; i < u.marks.size(); ++i) u.marks[i] = std::uint16_t(seed + i);
    if (!newer) return;
    if constexpr (version_of<U> >= 2) {
        u.team = std::uint8_t(seed % 3 + 1);
        u.elite = seed % 5 == 0;
        u.pos.z = std::int16_t(seed + 9);
        u.shield = 50 + seed;
    }
    if constexpr (version_of<U> >= 3) {
        u.pos.facing = std::int16_t(seed % 360);
        u.speed = 1.5f + float(seed);
    }
}

// What a reader of version R sees from a writer of version W: every field
// both know as written, the fields only R knows at 0 ("unset").
template <class R>
bool holds(const R& r, std::uint32_t seed, bool newer, int writer) {
    const std::string name = "unit-" + std::to_string(seed);
    bool ok = r.hp == 1000 + seed && r.on == (seed % 2 == 1) && r.kind == seed % 7 &&
              r.pos.x == std::int16_t(seed * 3) && r.pos.y == std::int16_t(-int(seed)) && r.name.view() == name &&
              r.marks.size() == seed % 4;
    for (std::size_t i = 0; ok && i < r.marks.size(); ++i) ok = r.marks[i] == std::uint16_t(seed + i);
    if constexpr (version_of<R> >= 2) {
        const bool known = newer && writer >= 2;
        ok = ok && r.team == (known ? seed % 3 + 1 : 0) && r.elite == (known && seed % 5 == 0) &&
             r.pos.z == (known ? std::int16_t(seed + 9) : 0) && r.shield == (known ? 50 + seed : 0u);
    }
    if constexpr (version_of<R> >= 3) {
        const bool known = newer && writer >= 3;
        ok = ok && r.pos.facing == (known ? std::int16_t(seed % 360) : 0) && r.speed == (known ? 1.5f + float(seed) : 0.0f);
    }
    return ok;
}

template <class U>
std::string whole_bytes(const U& u) {
    char body[256];
    const char* e = xs::XRecordBody<U>::put_whole(body, u);
    return std::string(body, std::size_t(e - body));
}

// A unit of version R holding the bytes of w (through w's whole body).
template <class R, class W>
void copy_bytes(R& r, const W& w) {
    const std::string b = whole_bytes(w);
    CHECK(xs::XRecordBody<R>::check_whole(b.data(), b.data() + b.size()) == b.data() + b.size());
    CHECK(xs::XRecordBody<R>::apply_whole(b.data(), r) == b.data() + b.size());
}

// The units live in a buffer: containers need one.
// Slots made up front in a buffer that never grows, so references stay.
struct Units {
    static constexpr std::size_t kSlots = 1024;
    xo::XBuffer buf = xo::XBuffer::create<fxs::v3::View>(std::size_t(1) << 21, xo::XBuffer::maximum_capacity(std::size_t(1) << 21));
    std::size_t used = 0;
    Units() { buf.root<fxs::v3::View>().units.resize(kSlots); }
    template <class U> U& make() {
        auto& v = buf.root<fxs::v3::View>().units;   // the same bytes for every version
        CHECK(used < kSlots);
        return reinterpret_cast<U&>(v[used++ % kSlots]);
    }
};

// ---- whole bodies ----------------------------------------------------------------

template <class W, class R>
void whole_pair(Units& s, bool newer) {
    for (std::uint32_t seed = 1; seed < 6; ++seed) {
        auto& w = s.make<W>();
        fill(w, seed, newer);
        const std::string body = whole_bytes(w);
        auto& r = s.make<R>();
        copy_bytes(r, w);
        CHECK(holds(r, seed, newer, version_of<W>));
        CHECK(whole_bytes(r) == body);                  // the newer bytes stay
        auto& back = s.make<W>();
        copy_bytes(back, r);
        CHECK(holds(back, seed, newer, version_of<W>));
    }
}

template <class W>
void whole_from(Units& s) {
    for (bool newer : {false, true}) {
        whole_pair<W, U1>(s, newer);
        whole_pair<W, U2>(s, newer);
        whole_pair<W, U3>(s, newer);
    }
}

// ---- change bodies ----------------------------------------------------------------

enum class Edit { hp_name, marks, team, shield_speed, facing, marks_elite, all };

template <class U>
void edit(U& u, Edit e) {
    switch (e) {
    case Edit::hp_name: u.hp += 7; u.name = std::string_view("renamed-unit-with-a-long-name"); break;
    case Edit::marks: u.marks.push_back(4242); break;
    case Edit::team: if constexpr (version_of<U> >= 2) u.team = 9; break;
    case Edit::shield_speed:
        if constexpr (version_of<U> >= 2) u.shield = 777;
        if constexpr (version_of<U> >= 3) u.speed = 9.25f;
        break;
    case Edit::facing: if constexpr (version_of<U> >= 3) u.pos.facing = 180; break;
    case Edit::marks_elite:
        u.marks.push_back(4242);
        if constexpr (version_of<U> >= 2) u.elite = !u.elite;
        break;
    case Edit::all:
        edit(u, Edit::hp_name); edit(u, Edit::team); edit(u, Edit::shield_speed);
        edit(u, Edit::facing); edit(u, Edit::marks_elite);
        u.on = !u.on;
        break;
    }
}

template <class U>
std::string change_bytes(const U& now, std::uint64_t mask) {
    char body[256];
    CHECK(xs::XRecordBody<U>::change_room(now, mask) <= sizeof body);
    const char* e = xs::XRecordBody<U>::put_change(body, now, mask);
    return std::string(body, std::size_t(e - body));
}

// W edits a unit; R holds the same bytes before and after. The change body
// W writes for its own mask is the one R writes for its own, and either
// applies on the other side.
template <class W, class R>
void change_pair(Units& s, Edit e, std::uint32_t seed) {
    auto& w_prev = s.make<W>();
    fill(w_prev, seed, true);
    auto& w_now = s.make<W>();
    copy_bytes(w_now, w_prev);
    edit(w_now, e);
    const std::uint64_t wm = xs::XRecordBody<W>::changed(w_now, w_prev);
    CHECK((wm & ~xs::XRecordBody<W>::mask_all) == 0);
    const std::string wb = change_bytes(w_now, wm);

    auto& r_prev = s.make<R>();
    copy_bytes(r_prev, w_prev);
    auto& r_now = s.make<R>();
    copy_bytes(r_now, w_now);
    const std::uint64_t rm = xs::XRecordBody<R>::changed(r_now, r_prev);
    CHECK((rm == 0) == (wm == 0));
    if (wm == 0) return;
    CHECK(change_bytes(r_now, rm) == wb);

    // W's body on R's old unit gives R's new one, and the other way round.
    CHECK(xs::XRecordBody<R>::check_change(wb.data(), wb.data() + wb.size()) == wb.data() + wb.size());
    CHECK(xs::XRecordBody<R>::apply_change(wb.data(), r_prev) == wb.data() + wb.size());
    CHECK(whole_bytes(r_prev) == whole_bytes(w_now));
    const std::string rb = change_bytes(r_now, rm);
    CHECK(xs::XRecordBody<W>::apply_change(rb.data(), w_prev) == rb.data() + rb.size());
    CHECK(whole_bytes(w_prev) == whole_bytes(w_now));
}

template <class W>
void change_from(Units& s) {
    for (Edit e : {Edit::hp_name, Edit::marks, Edit::team, Edit::shield_speed, Edit::facing, Edit::marks_elite, Edit::all}) {
        for (std::uint32_t seed : {1u, 4u}) {
            change_pair<W, U1>(s, e, seed);
            change_pair<W, U2>(s, e, seed);
            change_pair<W, U3>(s, e, seed);
        }
    }
}

// The masks themselves: own numbering in, the first version's out.
void masks(Units& s) {
    auto& prev = s.make<U3>();
    fill(prev, 3, true);
    auto& now = s.make<U3>();
    copy_bytes(now, prev);
    now.elite = !now.elite;   // member 4 of v3, in member 3 of v1 (more)
    now.speed = 2.0f;         // member 9 of v3, in member 7 of v1 (spare)
    CHECK(B3::changed(now, prev) == ((1u << 4) | (1u << 9)));
    const std::string b = change_bytes(now, B3::changed(now, prev));
    CHECK(b.size() == 1 + 2 + 8 && std::uint8_t(b[0]) == ((1u << 3) | (1u << 7)));
    // Members of v3 in one member of v1 go as that member: team alone sends
    // team and elite (the 2 bytes of v1's more).
    const std::string t = change_bytes(now, std::uint64_t(1) << 3);
    CHECK(t.size() == 1 + 2 && std::uint8_t(t[0]) == (1u << 3));
    CHECK(change_bytes(now, (1u << 3) | (1u << 4)) == t);
    // A bool v3 knows is checked as a bool; v1 holds the byte as opaque.
    std::string bad = t;
    bad[2] = 2;   // elite = 2
    CHECK(B3::check_change(bad.data(), bad.data() + bad.size()) == nullptr);
    CHECK(B1::check_change(bad.data(), bad.data() + bad.size()) == bad.data() + bad.size());
    // The writer drops a mask of nothing, and counts the version's own bits.
    xs::XSyncOut out;
    xs::XSyncWriter m(out, fxs::v2::Head{}, 1);
    m.begin(1, xs::change);
    CHECK(!m.change(1, now, 0));
    CHECK(m.change(2, now, std::uint64_t(1) << 9));   // past v1's member count
    m.end();
    m.finish();
}

// ---- diff ops (records without containers) ------------------------------------------

template <class T>
std::string diff_bytes(const T& now, T prev) {
    xs::XSyncOut out;
    const std::size_t n = xs::put_diff_op(out, 5, now, prev);
    CHECK(std::memcmp(&prev, &now, sizeof now) == 0);   // prev takes them
    return std::string(out.view().substr(out.view().size() - n));
}

void diffs() {
    fxs::v2::Tile prev2{11, 2, 3, 4};
    fxs::v1::Tile prev1;
    std::memcpy(static_cast<void*>(&prev1), &prev2, sizeof prev1);
    for (int i = 0; i < 4; ++i) {
        fxs::v2::Tile now2 = prev2;
        if (i == 0) now2.owner = 12;
        if (i == 1) now2.level = 5;
        if (i == 2) { now2.kind = 6; now2.flags = 7; }
        fxs::v1::Tile now1;
        std::memcpy(static_cast<void*>(&now1), &now2, sizeof now1);
        const std::string d2 = diff_bytes(now2, prev2);
        CHECK(d2 == diff_bytes(now1, prev1));
        CHECK(d2.size() == (i == 3 ? 0u : i == 0 ? 4 + 1 + 4 : i == 1 ? 4 + 1 + 2 : 4 + 1 + 2 + 2));
    }
}

// ---- whole messages ----------------------------------------------------------------

template <class V>
using unit_of = std::remove_cvref_t<decltype(std::declval<V&>().units[0])>;
template <class V>
using tile_of = std::remove_cvref_t<decltype(std::declval<V&>().tiles[0])>;
template <class V>
using head_of = decltype(V::head);

xo::XBuffer::MaximumCapacity cap() {
    return xo::XBuffer::maximum_capacity(std::size_t(1) << 22).with_growth(xo::XGrowPolicy::Automatic);
}

// A client of version V: units and tiles keyed by id in slots of its view.
template <class V>
struct Client {
    using tables = std::tuple<head_of<V>, unit_of<V>, tile_of<V>>;
    template <unsigned K> static constexpr bool keyed = K != 0;
    xo::XBuffer buf = xo::XBuffer::create<V>(4096, cap());
    std::map<std::uint32_t, std::int64_t> idx[3];
    V& view() { return buf.root<V>(); }
    template <unsigned K> std::int64_t find(std::uint32_t id) {
        if constexpr (K == 0) return id == 0 ? 0 : -1;
        else {
            const auto it = idx[K].find(id);
            return it == idx[K].end() ? -1 : it->second;
        }
    }
    template <unsigned K> auto& record(std::int64_t h) {
        if constexpr (K == 0) return view().head;
        else if constexpr (K == 1) return view().units[std::size_t(h)];
        else return view().tiles[std::size_t(h)];
    }
    template <unsigned K> std::int64_t enter(std::uint32_t id) {
        std::int64_t s;
        if constexpr (K == 1) { s = std::int64_t(view().units.size()); view().units.emplace_back(); }
        else { s = std::int64_t(view().tiles.size()); view().tiles.emplace_back(); }
        idx[K].emplace(id, s);
        return s;
    }
    template <unsigned K> void leave(std::uint32_t id, std::int64_t) { idx[K].erase(id); }
};

// A server of version V runs one script: a key message (head, units 1 to 4,
// tiles 1 and 2), then a delta (unit 2 leaves, units 1 and 3 change, unit 9
// enters, tile 2 changes). The newer fields are set only when newer.
template <class V>
struct Script {
    std::string key, delta;
};

template <class V>
Script<V> run_server(bool newer) {
    using U = unit_of<V>;
    using T = tile_of<V>;
    xo::XBuffer b = xo::XBuffer::create<V>(std::size_t(1) << 16, cap());
    V& v = b.root<V>();
    v.head.tick = 100;
    v.head.seq = 1;
    if constexpr (requires(V& x) { x.head.phase; }) if (newer) v.head.phase = 3;
    v.units.resize(5);
    for (std::uint32_t i = 1; i < 5; ++i) fill(v.units[i], i, newer);
    v.tiles.resize(3);
    for (std::uint32_t i = 1; i < 3; ++i) {
        v.tiles[i].owner = std::int32_t(i * 10);
        v.tiles[i].kind = std::uint16_t(i);
        if constexpr (requires(T& x) { x.level; }) if (newer) v.tiles[i].level = std::uint8_t(i + 1);
    }
    T tile_prev = v.tiles[2];
    Script<V> out;
    xs::XSyncOut msg;
    {
        xs::XSyncWriter m(msg, v.head, 1, 0x1122334455667788ull);
        m.begin(1, xs::enter);
        for (std::uint32_t i = 1; i < 5; ++i) m.whole(i, v.units[i]);
        m.end();
        m.begin(2, xs::enter);
        for (std::uint32_t i = 1; i < 3; ++i) m.whole(i, v.tiles[i]);
        m.end();
        m.finish();
    }
    out.key = std::string(msg.view());

    xo::XBuffer pb = xo::XBuffer::create<V>(std::size_t(1) << 16, cap());
    V& prev = pb.root<V>();
    prev.units.resize(5);
    for (std::uint32_t i = 1; i < 5; ++i) copy_bytes(prev.units[i], v.units[i]);
    v.head.tick = 101;
    v.head.seq = 2;
    edit(v.units[1], newer ? Edit::all : Edit::hp_name);
    edit(v.units[3], newer ? Edit::shield_speed : Edit::marks);
    if (!newer) v.units[3].pos.x = 77;
    fill(v.units[2], 9, newer);   // the slot unit 2 leaves
    v.tiles[2].owner = 99;
    if constexpr (requires(T& x) { x.flags; }) if (newer) v.tiles[2].flags = 0x5a;
    {
        xs::XSyncWriter m(msg, v.head, 2);
        const std::uint32_t gone = 2;
        m.leaves(1, &gone, 1);
        m.begin(1, xs::change);
        for (std::uint32_t i : {1u, 3u}) m.change(i, v.units[i], xs::XRecordBody<U>::changed(v.units[i], prev.units[i]));
        m.end();
        m.begin(1, xs::enter);
        m.whole(9, v.units[2]);
        m.end();
        m.begin(2, xs::change);
        CHECK(m.diff(2, v.tiles[2], tile_prev));
        m.end();
        m.finish();
    }
    out.delta = std::string(msg.view());
    return out;
}

// Client R applies server W's messages (full, then admit) and holds what W
// sent: the fields it knows, the newer ones kept in its spare bytes.
template <class W, class R>
void messages_pair(bool newer) {
    const Script<W> s = run_server<W>(newer);
    xo::XReceiver<xo::XLink::from_server> rx(cap());
    for (int tier = 0; tier < 2; ++tier) {
        Client<R> c;
        xs::XSyncCursor cur;
        auto apply = [&](std::string_view m) {
            return tier == 0 ? rx.apply_sync<xo::XCheck::full>(m, cur, c) : rx.apply_sync<xo::XCheck::admit>(m, cur, c);
        };
        CHECK(apply(s.key).has_value());
        CHECK(apply(s.delta).has_value());
        CHECK(c.view().head.tick == 101 && c.view().head.seq == 2);
        if constexpr (requires(R& x) { x.head.phase; })
            CHECK(c.view().head.phase == (newer && version_of<unit_of<W>> >= 2 ? 3u : 0u));
        CHECK(c.template find<1>(2) < 0);
        const std::int64_t h4 = c.template find<1>(4), h9 = c.template find<1>(9), h1 = c.template find<1>(1);
        CHECK(h4 >= 0 && h9 >= 0 && h1 >= 0);
        if (h4 < 0 || h9 < 0 || h1 < 0) continue;
        CHECK(holds(c.view().units[std::size_t(h4)], 4, newer, version_of<unit_of<W>>));
        CHECK(holds(c.view().units[std::size_t(h9)], 9, newer, version_of<unit_of<W>>));
        // Unit 1 as the server edited it: compare through a unit of version
        // W filled and edited the same way.
        Units tmp;
        auto& want = tmp.make<unit_of<W>>();
        fill(want, 1, newer);
        edit(want, newer ? Edit::all : Edit::hp_name);
        CHECK(whole_bytes(c.view().units[std::size_t(h1)]) == whole_bytes(want));
        auto& want3 = tmp.make<unit_of<W>>();
        fill(want3, 3, newer);
        edit(want3, newer ? Edit::shield_speed : Edit::marks);
        if (!newer) want3.pos.x = 77;
        const std::int64_t h3 = c.template find<1>(3);
        CHECK(h3 >= 0 && whole_bytes(c.view().units[std::size_t(h3)]) == whole_bytes(want3));
        const std::int64_t t2 = c.template find<2>(2);
        CHECK(t2 >= 0);
        if (t2 >= 0) {
            const auto& t = c.view().tiles[std::size_t(t2)];
            CHECK(t.owner == 99 && t.kind == 2);
            if constexpr (requires(tile_of<R>& x) { x.flags; })
                CHECK(t.flags == (newer && version_of<unit_of<W>> >= 2 ? 0x5a : 0) &&
                      t.level == (newer && version_of<unit_of<W>> >= 2 ? 3 : 0));
        }
    }
}

template <class W>
void messages_from() {
    for (bool newer : {false, true}) {
        messages_pair<W, fxs::v1::View>(newer);
        messages_pair<W, fxs::v2::View>(newer);
        messages_pair<W, fxs::v3::View>(newer);
    }
}

// The same content: the same message bytes from every version.
void same_messages() {
    const auto a = run_server<fxs::v1::View>(false);
    const auto b = run_server<fxs::v2::View>(false);
    const auto c = run_server<fxs::v3::View>(false);
    CHECK(a.key == b.key && b.key == c.key);
    CHECK(a.delta == b.delta && b.delta == c.delta);
}

// A later field spanning adjacent XSpare members of the first version must set
// every overlapped first-version member bit in change bodies; a zero-padding
// nested record taken from XSpare keeps whole bodies identical across versions.
void adjacent_spare_span() {
    using V1 = fxs::span_spare::V1;
    using V2 = fxs::span_spare::V2;
    using B1 = xs::XRecordBody<V1>;
    using B2 = xs::XRecordBody<V2>;

    static_assert(B1::whole_body::I::plain_bytes == B2::whole_body::I::plain_bytes);
    V2 src{7, 0x1122334455667788ULL, {0x5a, {}, 0xdeadbeefU}};
    V2 dst{7, 0, {}};
    const std::uint64_t mask = B2::changed(src, dst);
    CHECK(mask == ((std::uint64_t(1) << 1) | (std::uint64_t(1) << 2)));

    std::string chg(B2::change_room(src, mask), '\0');
    char* cend = B2::put_change(chg.data(), src, mask);
    chg.resize(static_cast<std::size_t>(cend - chg.data()));
    CHECK(B1::check_change(chg.data(), chg.data() + chg.size()) == chg.data() + chg.size());
    CHECK(B2::check_change(chg.data(), chg.data() + chg.size()) == chg.data() + chg.size());

    // Round-trip through V1 (admit / apply_change -> whole_body -> V2) and direct V2 apply.
    V1 mid{7, {}, {}, {}};
    CHECK(B1::apply_change(chg.data(), mid) == chg.data() + chg.size());
    std::string whole(B1::whole_room(mid), '\0');
    char* wend = B1::put_whole(whole.data(), mid);
    whole.resize(static_cast<std::size_t>(wend - whole.data()));
    CHECK(B2::check_whole(whole.data(), whole.data() + whole.size()) == whole.data() + whole.size());

    V2 via_v1{};
    CHECK(B2::apply_whole(whole.data(), via_v1) == whole.data() + whole.size());
    CHECK(via_v1.id == 7 && via_v1.big == 0x1122334455667788ULL &&
          via_v1.sub.tag == 0x5a && via_v1.sub.val == 0xdeadbeefU);

    CHECK(B2::apply_change(chg.data(), dst) == chg.data() + chg.size());
    CHECK(dst.id == 7 && dst.big == 0x1122334455667788ULL &&
          dst.sub.tag == 0x5a && dst.sub.val == 0xdeadbeefU);
}

} // namespace

int main() {
    {
        Units s;
        whole_from<U1>(s);
        whole_from<U2>(s);
        whole_from<U3>(s);
    }
    {
        Units s;
        change_from<U1>(s);
        change_from<U2>(s);
        change_from<U3>(s);
        masks(s);
    }
    diffs();
    messages_from<fxs::v1::View>();
    messages_from<fxs::v2::View>();
    messages_from<fxs::v3::View>();
    same_messages();
    adjacent_spare_span();
    if (failures) {
        std::fprintf(stderr, "test_schema_family_xs: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("test_schema_family_xs: ok\n");
    return 0;
}
