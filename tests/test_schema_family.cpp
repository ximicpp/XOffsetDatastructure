// Add-only type evolution (xoffset_schema_family.hpp, docs/SCHEMA_EVOLUTION.md
// section 9) over three versions of one type tree
// (schema_family/player_versions.hpp): v2 adds fields from v1's spare bytes
// and gives v1's kept list its record type, v3 adds more from v2's.
//   - compile time: the relation holds for v1 -> v2 -> v3 (and v1 -> v3), one
//     fingerprint for the family, the first version's hash unchanged by the
//     family, distinct signatures and field-name digests, the family digests;
//   - whole wires, every pair of versions both ways, every emission and
//     compact: the full check and admit accept, every field the reader knows
//     reads what the writer wrote, fields the writer lacks read 0;
//   - an older program edits a newer image and sends it on as it is (dense,
//     compact, after relayout): the newer fields stay;
//   - image deltas: the same edits through two versions give the same bytes,
//     and a receiver of either version applies them;
//   - saved images: an older save loads in a newer version; a newer save
//     loads in an older one, is edited and saved again, and the newer version
//     finds its fields;
//   - handshake: earlier and later versions accept each other; two versions
//     that add other fields to one version, and another family, are rejected;
//   - with reflection and exceptions (the server side), the freeze check
//     accepts the family too.
// Field lists, so every build runs it; no try/catch: also built without
// exceptions (then without the server side, which needs them).
#define XOFFSET_FORCE_FULL_CHECK 0
#include "schema_family/player_versions.hpp"
#include "../xoffset_handshake.hpp"
#include "../xoffset_receiver.hpp"
#define TEST_SERVER_SIDE (XOFFSET_SCHEMA_REFLECTION && !XOFFSET_NO_EXCEPTIONS)
#if TEST_SERVER_SIDE
#include "../xoffset_schema_freeze.hpp"
#endif

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

using namespace XOffsetDatastructure;

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

// Another layout under the family's schema name: another family.
namespace other {
struct Player { std::uint64_t uid{}; std::int32_t gold{}; std::int32_t gems{}; XString nick; };
} // namespace other
XOFFSET_RECORD(other::Player, uid, gold, gems, nick)
XOFFSET_REGISTER_SCHEMA_NAME(other::Player, "test.family.Player")

// Nested / array / container-element padding inside XSpare bytes.
namespace spare_pad {
struct InternalPad { std::uint8_t tag{}; std::uint32_t val{}; };
struct TailPad { std::uint32_t val{}; std::uint8_t tag{}; };
struct CleanSub { std::uint8_t tag{}; XSpare<3> pad{}; std::uint32_t val{}; };

struct Base8 { std::uint32_t id{}; XSpare<8> spare{}; };
struct BadInternal { std::uint32_t id{}; InternalPad sub{}; };
struct BadTail { std::uint32_t id{}; TailPad sub{}; };
struct GoodClean { std::uint32_t id{}; CleanSub sub{}; };

struct Base16 { std::uint32_t id{}; XSpare<16> spare{}; };
struct BadArray { std::uint32_t id{}; std::array<InternalPad, 2> subs{}; };
struct GoodArray { std::uint32_t id{}; std::array<CleanSub, 2> subs{}; };

struct BaseList { std::uint32_t id{}; XVector<XSpare<8>> items; };
struct BadList { std::uint32_t id{}; XVector<InternalPad> items; };
struct GoodList { std::uint32_t id{}; XVector<CleanSub> items; };
} // namespace spare_pad
XOFFSET_RECORD(spare_pad::InternalPad, tag, val)
XOFFSET_RECORD(spare_pad::TailPad, val, tag)
XOFFSET_RECORD(spare_pad::CleanSub, tag, pad, val)
XOFFSET_RECORD(spare_pad::Base8, id, spare)
XOFFSET_RECORD(spare_pad::BadInternal, id, sub)
XOFFSET_RECORD(spare_pad::BadTail, id, sub)
XOFFSET_RECORD(spare_pad::GoodClean, id, sub)
XOFFSET_RECORD(spare_pad::Base16, id, spare)
XOFFSET_RECORD(spare_pad::BadArray, id, subs)
XOFFSET_RECORD(spare_pad::GoodArray, id, subs)
XOFFSET_RECORD(spare_pad::BaseList, id, items)
XOFFSET_RECORD(spare_pad::BadList, id, items)
XOFFSET_RECORD(spare_pad::GoodList, id, items)

static_assert(!schema_family_successor_v<spare_pad::BadInternal, spare_pad::Base8>);
static_assert(detail::family::error_v<spare_pad::Base8, spare_pad::BadInternal> == detail::family::Error::spare_to_padding);
static_assert(!schema_family_successor_v<spare_pad::BadTail, spare_pad::Base8>);
static_assert(detail::family::error_v<spare_pad::Base8, spare_pad::BadTail> == detail::family::Error::spare_to_padding);
static_assert(schema_family_successor_v<spare_pad::GoodClean, spare_pad::Base8>);
static_assert(!schema_family_successor_v<spare_pad::BadArray, spare_pad::Base16>);
static_assert(detail::family::error_v<spare_pad::Base16, spare_pad::BadArray> == detail::family::Error::spare_to_padding);
static_assert(schema_family_successor_v<spare_pad::GoodArray, spare_pad::Base16>);
static_assert(!schema_family_successor_v<spare_pad::BadList, spare_pad::BaseList>);
static_assert(detail::family::error_v<spare_pad::BaseList, spare_pad::BadList> == detail::family::Error::spare_to_padding);
static_assert(schema_family_successor_v<spare_pad::GoodList, spare_pad::BaseList>);

namespace {
using V1 = fam::v1::Player;
using V2 = fam::v2::Player;
using V3 = fam::v3::Player;
using V2b = fam::v2b::Player;
using Solo = fam::solo::Player;

// ---- compile time ----------------------------------------------------------

static_assert(schema_family_successor_v<V2, V1> && schema_family_successor_v<V3, V2>);
static_assert(schema_family_successor_v<V3, V1>);           // two additions at once
static_assert(schema_family_successor_v<V2b, V1> && schema_family_successor_v<V1, V1>);
static_assert(!schema_family_successor_v<V1, V2>);          // a version never loses fields
static_assert(!schema_family_successor_v<V2b, V2> && !schema_family_successor_v<V2, V2b>);
static_assert(schema_family_successor_v<fam::v2::Hero, fam::v1::Hero>);
static_assert(schema_family_successor_v<fam::v3::Stats, fam::v1::Stats>);

static_assert(std::is_same_v<schema_family_first_t<V3>, V1> && std::is_same_v<schema_family_first_t<V1>, V1>);
static_assert(in_schema_family_v<V2> && in_schema_family_v<V3> && !in_schema_family_v<V1>);
static_assert(sizeof(V1) == sizeof(V2) && sizeof(V2) == sizeof(V3));
static_assert(wire_schema_hash_v<V2>() == wire_schema_hash_v<V1>());
static_assert(wire_schema_hash_v<V3>() == wire_schema_hash_v<V1>());
static_assert(wire_schema_hash_v<V2b>() == wire_schema_hash_v<V1>());
// The first version keeps its own hash: the same as the same layout outside
// any family.
static_assert(wire_schema_hash_v<Solo>() == wire_schema_hash_v<V1>());
static_assert(wire_schema_hash_v<other::Player>() != wire_schema_hash_v<V1>());
static_assert(wire_root_type_id_v<V3>() == wire_root_type_id_v<V1>());
static_assert(std::string_view(wire_abi_signature<V1>()) != std::string_view(wire_abi_signature<V2>()));
static_assert(std::string_view(wire_abi_signature<V2>()) != std::string_view(wire_abi_signature<V3>()));
static_assert(schema_name_digest_v<V1> != schema_name_digest_v<V2> &&
              schema_name_digest_v<V2> != schema_name_digest_v<V3> &&
              schema_name_digest_v<V2> != schema_name_digest_v<V2b>);
static_assert(schema_name_digest_v<Solo> == schema_name_digest_v<V1>);
static_assert(schema_family_digests_v<V1>.size() == 1 && schema_family_digests_v<V3>.size() == 3);
static_assert(schema_family_digests_v<V3>[0] == schema_name_digest_v<V1> &&
              schema_family_digests_v<V3>[1] == schema_name_digest_v<V2> &&
              schema_family_digests_v<V3>[2] == schema_name_digest_v<V3>);
static_assert(schema_family_has_digest<V3>(schema_name_digest_v<V1>) &&
              !schema_family_has_digest<V1>(schema_name_digest_v<V3>) &&
              !schema_family_has_digest<V3>(schema_name_digest_v<V2b>));

// ---- values ------------------------------------------------------------------

template <class P> constexpr int version_of = 0;
template <> constexpr int version_of<V1> = 1;
template <> constexpr int version_of<V2> = 2;
template <> constexpr int version_of<V3> = 3;

XBuffer::MaximumCapacity cap() { return XBuffer::maximum_capacity(1u << 22).with_growth(XGrowPolicy::Automatic); }

std::string nick_of(std::uint32_t seed) {
    return seed % 2 ? "nick-" + std::to_string(seed) : "a nickname long enough for the heap #" + std::to_string(seed);
}

// Every field P has, from seed.
template <class P>
void fill(P& p, std::uint32_t seed) {
    p.uid = 1000 + seed;
    p.gold = std::int32_t(seed * 3);
    p.gems = 7;
    p.nick.assign(nick_of(seed));
    for (std::uint32_t i = 0; i < 3; ++i) {
        p.heroes.emplace_back();
        auto& h = p.heroes.back();
        h.id = std::int32_t(i + 1);
        h.level = std::int32_t(seed + i);
        h.name.assign("hero-" + std::to_string(seed * 10 + i));
        for (std::uint32_t k = 0; k <= i; ++k) h.skills.push_back(std::int32_t(100 * i + k));
        h.stats.atk = std::int32_t(10 + i);
        h.stats.def = std::int32_t(20 + i);
        if constexpr (requires { h.stats.crit; }) h.stats.crit = std::int32_t(30 + i);
        if constexpr (requires { h.stats.dodge; }) h.stats.dodge = std::int32_t(40 + i);
        if constexpr (requires { h.star; }) { h.star = std::uint8_t(1 + i); h.awake = i % 2 == 0; }
        if constexpr (requires { h.skin; }) { h.skin = std::uint16_t(500 + i); h.cls = fam::Class::mage; }
    }
    for (std::int32_t k = 0; k < 2; ++k) {
        auto& b = p.buffs[k * 5];
        b.atk = k + 1;
        if constexpr (requires { b.crit; }) b.crit = k + 2;
        if constexpr (requires { b.dodge; }) b.dodge = k + 3;
    }
    if constexpr (requires { p.runes[0].id; }) {
        for (std::int32_t k = 0; k < 2; ++k) p.runes.push_back({std::int64_t(seed) * 100 + k, k + 1, k + 7});
    } else {
        p.runes.resize(2);
    }
    if constexpr (requires { p.vip; }) p.vip = std::int32_t(seed % 9 + 1);
    if constexpr (requires { p.guild; }) { p.flags = 0xF00Du; p.guild = std::int64_t(seed) << 33; }
}

// Every field P has reads what a writer of version `writer` wrote with fill(seed),
// and 0 where that writer has no such field.
template <class P>
bool check(const P& p, std::uint32_t seed, int writer) {
    const auto at = [&](int since, auto value) { return writer >= since ? value : decltype(value){}; };
    bool ok = p.uid == 1000 + seed && p.gold == std::int32_t(seed * 3) && p.gems == 7 && p.nick.view() == nick_of(seed);
    ok = ok && p.heroes.size() == 3;
    for (std::uint32_t i = 0; ok && i < 3; ++i) {
        const auto& h = p.heroes[i];
        ok = h.id == std::int32_t(i + 1) && h.level == std::int32_t(seed + i) &&
             h.name.view() == "hero-" + std::to_string(seed * 10 + i) && h.skills.size() == i + 1 &&
             h.stats.atk == std::int32_t(10 + i) && h.stats.def == std::int32_t(20 + i);
        for (std::uint32_t k = 0; ok && k <= i; ++k) ok = h.skills[k] == std::int32_t(100 * i + k);
        if constexpr (requires { h.stats.crit; }) ok = ok && h.stats.crit == at(2, std::int32_t(30 + i));
        if constexpr (requires { h.stats.dodge; }) ok = ok && h.stats.dodge == at(3, std::int32_t(40 + i));
        if constexpr (requires { h.star; })
            ok = ok && h.star == at(2, std::uint8_t(1 + i)) && h.awake == at(2, i % 2 == 0);
        if constexpr (requires { h.skin; })
            ok = ok && h.skin == at(3, std::uint16_t(500 + i)) && h.cls == at(3, fam::Class::mage);
    }
    ok = ok && p.buffs.size() == 2;
    for (std::int32_t k = 0; ok && k < 2; ++k) {
        const auto it = p.buffs.find(k * 5);
        ok = it != p.buffs.end() && it->second.atk == k + 1 && it->second.def == 0;
        if constexpr (requires { it->second.crit; }) ok = ok && it->second.crit == at(2, k + 2);
        if constexpr (requires { it->second.dodge; }) ok = ok && it->second.dodge == at(3, k + 3);
    }
    ok = ok && p.runes.size() == 2;
    if constexpr (requires { p.runes[0].id; }) {
        for (std::int32_t k = 0; ok && k < 2; ++k)
            ok = p.runes[std::size_t(k)].id == at(2, std::int64_t(seed) * 100 + k) &&
                 p.runes[std::size_t(k)].kind == at(2, k + 1) && p.runes[std::size_t(k)].level == at(2, k + 7);
    }
    if constexpr (requires { p.vip; }) ok = ok && p.vip == at(2, std::int32_t(seed % 9 + 1));
    if constexpr (requires { p.guild; })
        ok = ok && p.flags == at(3, 0xF00Du) && p.guild == at(3, std::int64_t(seed) << 33);
    return ok;
}

template <class P>
TypedXBuffer<P> make(std::uint32_t seed) {
    auto b = TypedXBuffer<P>::create(4096, cap());
    fill(b.plain_root(), seed);
    return b;
}

// ---- whole wires, every pair -------------------------------------------------

template <class W, class R>
void whole_wires_pair() {
    XReceiver<XLink::server_peer> peer(cap());
    for (std::uint32_t seed = 1; seed <= 4; ++seed) {
        auto src = make<W>(seed);
        std::vector<std::string> wires;
        for (auto e : {XWireEmission::dense, XWireEmission::free_runs, XWireEmission::zero_runs})
            wires.push_back(src.save_verified(XSaveOptions{e}));
        wires.push_back(src.save_compact());
        for (const auto& wire : wires) {
            auto full = TypedXBuffer<R>::try_load_verified(wire, cap());
            CHECK(full.has_value());
            if (full) CHECK(check(full->plain_root(), seed, version_of<W>));
            auto admitted = peer.load<R>(wire);
            CHECK(admitted.has_value());
            if (admitted) CHECK(check(admitted->template root<R>(), seed, version_of<W>));
            auto typed = TypedXBuffer<R>::create(4096, cap());
            CHECK(peer.reload(typed, wire));
            CHECK(check(typed.plain_root(), seed, version_of<W>));
        }
    }
}

template <class W>
void whole_wires_from() {
    whole_wires_pair<W, V1>();
    whole_wires_pair<W, V2>();
    whole_wires_pair<W, V3>();
}

void whole_wires() {
    whole_wires_from<V1>();
    whole_wires_from<V2>();
    whole_wires_from<V3>();
}

// ---- an older program edits and resends ------------------------------------

// Edits through the fields every version has.
template <class P>
void edit_common(P& p, std::uint32_t round) {
    p.gold += std::int32_t(round);
    p.nick.assign(round % 2 ? "x" : "a renamed player whose name goes to the heap");
    p.heroes[1].level += 1;
    p.heroes[0].name.assign("renamed-" + std::to_string(round));
    p.heroes[2].skills.push_back(std::int32_t(round));
    p.heroes.emplace_back();
    p.heroes.back().id = std::int32_t(10 + round);
    p.buffs[100 + std::int32_t(round)].atk = 9;
    p.runes.emplace_back();
}

// What edit_common did, read by a newer version: its own fields kept on the
// records that were there, 0 on the ones the older program added.
template <class P>
bool edited_keeps_newer(const P& p, std::uint32_t seed, std::uint32_t rounds) {
    bool ok = p.heroes.size() == 3 + rounds && p.runes.size() == 2 + rounds && p.buffs.size() == 2 + rounds;
    for (std::uint32_t i = 0; ok && i < 3; ++i) {
        const auto& h = p.heroes[i];
        if constexpr (requires { h.star; }) ok = h.star == std::uint8_t(1 + i) && h.awake == (i % 2 == 0);
        if constexpr (requires { h.skin; }) ok = ok && h.skin == std::uint16_t(500 + i) && h.cls == fam::Class::mage;
        if constexpr (requires { h.stats.crit; }) ok = ok && h.stats.crit == std::int32_t(30 + i);
        if constexpr (requires { h.stats.dodge; }) ok = ok && h.stats.dodge == std::int32_t(40 + i);
    }
    for (std::uint32_t i = 3; ok && i < p.heroes.size(); ++i) {
        const auto& h = p.heroes[i];
        if constexpr (requires { h.star; }) ok = h.star == 0 && !h.awake;
        if constexpr (requires { h.skin; }) ok = ok && h.skin == 0 && h.cls == fam::Class::none;
    }
    if constexpr (requires { p.runes[0].id; }) {
        ok = ok && p.runes[0].id == std::int64_t(seed) * 100 && p.runes[1].level == 8;
        for (std::size_t k = 2; ok && k < p.runes.size(); ++k) ok = p.runes[k].id == 0 && p.runes[k].level == 0;
    }
    if constexpr (requires { p.vip; }) ok = ok && p.vip == std::int32_t(seed % 9 + 1);
    if constexpr (requires { p.guild; }) ok = ok && p.flags == 0xF00Du && p.guild == std::int64_t(seed) << 33;
    return ok;
}

template <class Old, class New>
void older_edits_and_resends() {
    const std::uint32_t seed = 5;
    auto src = make<New>(seed);
    auto old = TypedXBuffer<Old>::load_verified(src.save_verified(), cap());
    for (std::uint32_t round = 1; round <= 3; ++round) {
        edit_common(old.plain_root(), round);
        std::vector<std::string> sends = {old.save_verified(), old.save_compact()};
        if (round == 3) {
            (void)old.relayout();
            sends.push_back(old.save_verified());
        }
        for (const auto& wire : sends) {
            auto back = TypedXBuffer<New>::try_load_verified(wire, cap());
            CHECK(back.has_value());
            if (!back) continue;
            CHECK(edited_keeps_newer(back->plain_root(), seed, round));
            CHECK(back->plain_root().gold == old.plain_root().gold);
            CHECK(back->plain_root().nick.view() == old.plain_root().nick.view());
        }
    }
}

// ---- image deltas -------------------------------------------------------------

template <class A, class B>
void deltas_pair() {
    const std::uint32_t seed = 9;
    const std::string wire = make<V3>(seed).save_verified();
    auto a = TypedXBuffer<A>::load_verified(wire, cap());
    auto b = TypedXBuffer<B>::load_verified(wire, cap());
    XDeltaBaseline ba(77), bb(77);   // one stream id: the same delta bytes
    TypedXBuffer<B> rx_b(4096, cap());   // receives A's deltas
    TypedXBuffer<A> rx_a(4096, cap());   // receives B's deltas
    XDeltaCursor ca, cb;
    std::string da, db;
    for (std::uint32_t round = 0; round <= 4; ++round) {
        if (round) {
            edit_common(a.plain_root(), round);
            edit_common(b.plain_root(), round);
        }
        (void)a.save_delta_into(da, ba);
        (void)b.save_delta_into(db, bb);
        CHECK(da == db);
        CHECK(bool(rx_b.try_apply_delta_verified(da, ca)));
        CHECK(bool(rx_a.try_apply_delta_verified(db, cb)));
        CHECK(rx_b.save_verified() == a.save_verified());
        CHECK(rx_a.save_verified() == b.save_verified());
    }
    CHECK(edited_keeps_newer(rx_b.plain_root(), seed, 4));
}

// ---- saved images ------------------------------------------------------------

template <class Old, class New>
void saved_images() {
    // An older save, loaded by the newer version: its fields read 0.
    const std::string old_save = make<Old>(11).save_compact();
    auto loaded = TypedXBuffer<New>::try_load_verified(old_save, cap());
    CHECK(loaded.has_value());
    if (loaded) CHECK(check(loaded->plain_root(), 11, version_of<Old>));

    // A newer save, edited and saved again by the older version, then
    // loaded by the newer one: its fields are there.
    const std::string new_save = make<New>(12).save_compact();
    auto by_old = TypedXBuffer<Old>::try_load_verified(new_save, cap());
    CHECK(by_old.has_value());
    if (!by_old) return;
    CHECK(check(by_old->plain_root(), 12, version_of<Old>));
    edit_common(by_old->plain_root(), 1);
    const std::string again = by_old->save_compact();
    auto back = TypedXBuffer<New>::try_load_verified(again, cap());
    CHECK(back.has_value());
    if (back) CHECK(edited_keeps_newer(back->plain_root(), 12, 1));
}

// ---- handshake -----------------------------------------------------------------

template <class Client, class Server>
XSchemaVerdict verdict() {
    const std::string hello = make_schema_hello<Client>("client");
    std::string ack;
    const auto r = accept_schema_hello<Server>(hello, ack, "server");
    CHECK(r.valid && r.entries.size() == 1);
    if (!r.valid || r.entries.size() != 1) return XSchemaVerdict::reject_unknown;
    const auto read = read_schema_ack<Client>(ack);
    CHECK(bool(read) == accepted(r.entries[0].verdict));
    return r.entries[0].verdict;
}

void handshake() {
    // The hello lists the earlier versions of the family as known entries.
    const std::string hello = make_schema_hello<V3>("v3");
    auto h = TypedXBuffer<XSchemaHello>::load_verified(hello, XBuffer::maximum_capacity(XSCHEMA_HELLO_MAX_BYTES));
    const auto& m = h.plain_root();
    CHECK(m.entries.size() == 3);
    if (m.entries.size() == 3) {
        CHECK(m.entries[0].kind == 0 && m.entries[0].name_digest == schema_name_digest_v<V3>);
        CHECK(m.entries[1].kind == 1 && m.entries[1].name_digest == schema_name_digest_v<V1>);
        CHECK(m.entries[2].kind == 1 && m.entries[2].name_digest == schema_name_digest_v<V2>);
        for (const auto& e : m.entries)
            CHECK(e.root_type_id == wire_root_type_id_v<V1>() && e.schema_hash == wire_schema_hash_v<V1>());
    }
    CHECK(TypedXBuffer<XSchemaHello>::load_verified(make_schema_hello<V1>("v1")).plain_root().entries.size() == 1);

    // Earlier and later versions accept each other.
    CHECK((verdict<V1, V3>() == XSchemaVerdict::same));
    CHECK((verdict<V3, V1>() == XSchemaVerdict::same));
    CHECK((verdict<V2, V3>() == XSchemaVerdict::same));
    CHECK((verdict<V3, V2>() == XSchemaVerdict::same));
    CHECK((verdict<V1, V2>() == XSchemaVerdict::same));
    CHECK((verdict<V2, V1>() == XSchemaVerdict::same));
    CHECK((verdict<V2b, V1>() == XSchemaVerdict::same));
    CHECK((verdict<V1, V2b>() == XSchemaVerdict::same));
    CHECK((verdict<V3, V3>() == XSchemaVerdict::same));
    // The same layout and names outside the family: the first version itself.
    CHECK((verdict<Solo, V3>() == XSchemaVerdict::same));
    // Two versions that add other fields to v1 do not read each other's.
    CHECK((verdict<V2b, V2>() == XSchemaVerdict::reject_names));
    CHECK((verdict<V2, V2b>() == XSchemaVerdict::reject_names));
    CHECK((verdict<V2b, V3>() == XSchemaVerdict::reject_names));
    CHECK((verdict<V3, V2b>() == XSchemaVerdict::reject_names));
    // Another family.
    CHECK((verdict<other::Player, V3>() == XSchemaVerdict::reject_unknown));
    CHECK((verdict<V3, other::Player>() == XSchemaVerdict::reject_unknown));
}

#if TEST_SERVER_SIDE
// The server side: the freeze check.
void server_side() {
    namespace fz = detail::freeze;
    auto root = [](auto type, const char* version) {
        using R = typename decltype(type)::type;
        return fz::LockRoot{version, std::string(schema_name<R>::value), "frozen", wire_schema_hash_v<R>(),
                            schema_name_digest_v<R>, std::string(std::string_view(wire_abi_signature<R>()))};
    };
    fz::Lock lock;
    lock.roots.push_back(root(std::type_identity<V1>{}, "1.0"));
    lock.roots.push_back(root(std::type_identity<V2>{}, "1.1"));
    std::string report;
    CHECK(fz::check_root<V3>(lock, report));
    CHECK(report.find("adds fields to 1.1") != std::string::npos);
    report.clear();
    CHECK(fz::check_root<V2>(lock, report));
    CHECK(report.find("equals 1.1") != std::string::npos);
    report.clear();
    // v2b adds other fields to v1: the frozen v2 is not an earlier version of it.
    CHECK(!fz::check_root<V2b>(lock, report));
    CHECK(report.find("other field names") != std::string::npos);
}
#endif

} // namespace

int main() {
    whole_wires();
    older_edits_and_resends<V1, V2>();
    older_edits_and_resends<V1, V3>();
    older_edits_and_resends<V2, V3>();
    deltas_pair<V1, V3>();
    deltas_pair<V3, V1>();
    deltas_pair<V2, V3>();
    deltas_pair<V1, V2>();
    saved_images<V1, V2>();
    saved_images<V1, V3>();
    saved_images<V2, V3>();
    handshake();
#if TEST_SERVER_SIDE
    server_side();
#endif
    if (failures) {
        std::fprintf(stderr, "test_schema_family: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("test_schema_family: ok\n");
    return 0;
}
