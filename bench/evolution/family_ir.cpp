// In-family send, receive and delta against the same-version ones: the Ir
// rows of the add-only type evolution (docs/SCHEMA_EVOLUTION.md,
// docs/PERF_GATE.md "family.*"). The type is the SLG PlayerState with
// reserved bytes (8 per Hero, 16 at the root), in two versions of one
// family: v2 takes Hero::awaken and PlayerState::vip_exp from v1's XSpare
// bytes. A server on v2 talks to a client on v2 (same) or on v1 (in_family):
//
//   send.same       save_verified_into after the handshake with a v2 client
//   send.in_family  the same after the handshake with a v1 client (verdict same:
//                   one fingerprint, the same bytes)
//   recv.same       the v2 client's reload of the server's wire (trusted link, admit)
//   recv.in_family  the v1 client's reload of the same wire
//   delta.same      one tick of an image delta stream (touches baseline, one
//                   field touched): the server's save_delta_into and the v2
//                   replica's apply_delta
//   delta.in_family the same tick applied by a v1 replica
//
// Every in_family row must equal its same row (perf_gate fails otherwise).
//
//   family_ir --list            the rows
//   family_ir --check           the rows' results agree (the gate runs it first)
//   family_ir --row NAME [N]    N measured calls (default 20) after one warm-up;
//                               with FAMILY_IR_COUNT=1 callgrind collects only
//                               inside them (valgrind --collect-atstart=no)
#include "xoffset_receiver.hpp"
#include "xoffset_handshake.hpp"
#include "slg/model_xoffset.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

#if FAMILY_IR_COUNT
#include <valgrind/callgrind.h>
#define FAMILY_COLLECT(expr) do { CALLGRIND_TOGGLE_COLLECT; expr; CALLGRIND_TOGGLE_COLLECT; } while (0)
#else
#define FAMILY_COLLECT(expr) do { expr; } while (0)
#endif

namespace fbench {
namespace v1 {
using namespace XOffsetDatastructure;
using slg::xm::Buff;
using slg::xm::City;
using slg::xm::Skill;
using slg::xm::Troop;
struct alignas(8) Hero {
    int32_t id{}; int32_t level{}; int32_t star{};
    XString name;
    XVector<Skill> skills;
    XVector<Buff> buffs;
    XSpare<8> spare{};
};
struct alignas(8) Army {
    int64_t army_id{}; int64_t owner_id{}; int32_t x{}; int32_t y{};
    XVector<Hero> heroes;
    XVector<Troop> troops;
    XVector<Buff> buffs;
};
struct alignas(8) PlayerState {
    int64_t player_id{};
    XString name;
    City city;
    XVector<Army> armies;
    XVector<Hero> heroes;
    XMap<int32_t, int32_t> research;
    XMap<int32_t, int64_t> items;
    XSpare<16> spare{};
};
} // namespace v1

namespace v2 {
using namespace XOffsetDatastructure;
using slg::xm::Buff;
using slg::xm::City;
using slg::xm::Skill;
using slg::xm::Troop;
struct alignas(8) Hero {
    int32_t id{}; int32_t level{}; int32_t star{};
    XString name;
    XVector<Skill> skills;
    XVector<Buff> buffs;
    int32_t awaken{};                // new
    XSpare<4> spare{};
};
struct alignas(8) Army {
    int64_t army_id{}; int64_t owner_id{}; int32_t x{}; int32_t y{};
    XVector<Hero> heroes;
    XVector<Troop> troops;
    XVector<Buff> buffs;
};
struct alignas(8) PlayerState {
    int64_t player_id{};
    XString name;
    City city;
    XVector<Army> armies;
    XVector<Hero> heroes;
    XMap<int32_t, int32_t> research;
    XMap<int32_t, int64_t> items;
    int64_t vip_exp{};               // new
    XSpare<8> spare{};
};
} // namespace v2
} // namespace fbench

XOFFSET_RECORD(fbench::v1::Hero, id, level, star, name, skills, buffs, spare)
XOFFSET_RECORD(fbench::v1::Army, army_id, owner_id, x, y, heroes, troops, buffs)
XOFFSET_RECORD(fbench::v1::PlayerState, player_id, name, city, armies, heroes, research, items, spare)
XOFFSET_RECORD(fbench::v2::Hero, id, level, star, name, skills, buffs, awaken, spare)
XOFFSET_RECORD(fbench::v2::Army, army_id, owner_id, x, y, heroes, troops, buffs)
XOFFSET_RECORD(fbench::v2::PlayerState, player_id, name, city, armies, heroes, research, items, vip_exp, spare)
XOFFSET_REGISTER_SCHEMA_NAME(fbench::v1::PlayerState, "bench.family.PlayerState")
XOFFSET_REGISTER_SCHEMA_NAME(fbench::v2::PlayerState, "bench.family.PlayerState")
XOFFSET_SCHEMA_FAMILY(fbench::v2::PlayerState, fbench::v1::PlayerState)

namespace {
using namespace XOffsetDatastructure;
using V1 = fbench::v1::PlayerState;
using V2 = fbench::v2::PlayerState;
static_assert(sizeof(V1) == sizeof(V2) && wire_schema_hash_v<V1>() == wire_schema_hash_v<V2>());

constexpr const char* kRows[] = {"send.same", "send.in_family", "recv.same", "recv.in_family",
                                 "delta.same", "delta.in_family"};

XBuffer::MaximumCapacity cap() { return XBuffer::maximum_capacity(std::size_t(1) << 24); }

[[noreturn]] void die(const char* what) {
    std::fprintf(stderr, "family_ir: %s\n", what);
    std::exit(1);
}

std::uint32_t next(std::uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s >> 8;
}

template <class Hero>
void fill_hero(Hero& h, std::uint32_t& s, int id) {
    h.id = id;
    h.level = int32_t(next(s) % 60 + 1);
    h.star = int32_t(next(s) % 6);
    const std::string name = "hero-" + std::to_string(id) + "-" + std::to_string(next(s) % 1000);
    h.name.assign(name);
    const std::size_t skills = 2 + next(s) % 3, buffs = next(s) % 3;
    for (std::size_t i = 0; i < skills; ++i) {
        h.skills.emplace_back();
        h.skills.back().id = int32_t(next(s) % 500);
        h.skills.back().level = int32_t(next(s) % 10);
    }
    for (std::size_t i = 0; i < buffs; ++i) {
        h.buffs.emplace_back();
        h.buffs.back().id = int32_t(next(s) % 200);
        h.buffs.back().stacks = int32_t(next(s) % 5);
        h.buffs.back().expire_ms = int64_t(next(s)) * 1000;
    }
    h.awaken = int32_t(next(s) % 9);
}

// The server's PlayerState: 4 armies of 9 heroes, 40 heroes of its own, a
// city, research and items (about 10 KB, the SLG S-scale size).
TypedXBuffer<V2> make_state() {
    auto b = TypedXBuffer<V2>::create(std::size_t(64) << 10, cap());
    V2& p = b.plain_root();
    std::uint32_t s = 20261021;
    p.player_id = 900001;
    p.name.assign("player-with-a-plausible-name");
    p.city.city_id = 77;
    p.city.level = 12;
    for (int i = 0; i < 12; ++i) p.city.buildings.emplace(i * 3, int32_t(next(s) % 20));
    for (int i = 0; i < 6; ++i) p.city.resources.emplace(i, int64_t(next(s)) * 10);
    int id = 1;
    for (int a = 0; a < 4; ++a) {
        p.armies.emplace_back();
        auto& army = p.armies.back();
        army.army_id = 5000 + a;
        army.owner_id = p.player_id;
        army.x = int32_t(next(s) % 1000);
        army.y = int32_t(next(s) % 1000);
        for (int h = 0; h < 9; ++h) {
            army.heroes.emplace_back();
            fill_hero(army.heroes.back(), s, id++);
        }
        for (int t = 0; t < 4; ++t) {
            army.troops.emplace_back();
            army.troops.back().type = t;
            army.troops.back().count = int32_t(next(s) % 5000);
        }
    }
    for (int h = 0; h < 40; ++h) {
        p.heroes.emplace_back();
        fill_hero(p.heroes.back(), s, id++);
    }
    for (int i = 0; i < 30; ++i) p.research.emplace(i * 7, int32_t(next(s) % 15));
    for (int i = 0; i < 20; ++i) p.items.emplace(1000 + i * 11, int64_t(next(s) % 99));
    p.vip_exp = 0x1122334455667788;
    return b;
}

std::string hello_of_v1() { return make_schema_hello<V1>("client-v1"); }
std::string hello_of_v2() { return make_schema_hello<V2>("client-v2"); }

// The server's side of the handshake with a client, and its send to that client.
struct Peer {
    explicit Peer(const std::string& hello) {
        std::string ack;
        const XSchemaHelloResult r = accept_schema_hello<V2>(hello, ack, "server");
        if (!r.accepted() || r.entries.size() != 1 || r.entries[0].verdict != XSchemaVerdict::same)
            die("handshake: the client is not served the working version");
    }
    void save_into(TypedXBuffer<V2>& state, std::string& out) const { state.save_verified_into(out); }
};

struct Fixture {
    TypedXBuffer<V2> state = make_state();
    std::string wire;
    Fixture() { state.save_verified_into(wire); }
};

// Not inlined, so both rows of a pair run the same harness code around the call.
template <class R>
[[gnu::noinline]] int recv_row(const Fixture& f, int n) {
    XReceiver<XLink::server_peer> rx(cap());
    XBuffer dst(std::size_t(64) << 10, cap());
    if (!rx.reload<R>(dst, f.wire)) die("reload");
    int ok = 0;
    for (int i = 0; i < n; ++i) FAMILY_COLLECT(ok += bool(rx.reload<R>(dst, f.wire)));
    return ok == n ? 0 : 1;
}

[[gnu::noinline]] int send_row(Fixture& f, const std::string& hello, int n) {
    const Peer peer(hello);
    std::string out;
    peer.save_into(f.state, out);
    std::size_t sink = 0;
    for (int i = 0; i < n; ++i) {
        FAMILY_COLLECT(peer.save_into(f.state, out));
        sink += out.size();
    }
    return sink == std::size_t(n) * f.wire.size() ? 0 : 1;
}

template <class R>
[[gnu::noinline]] int delta_row(Fixture& f, int n) {
    XReceiver<XLink::server_peer> rx(cap());
    XDeltaBaseline base(XDeltaRecord::touches);
    XDeltaCursor cursor;
    XBuffer replica(std::size_t(64) << 10, cap());
    std::string d;
    f.state.save_delta_into(d, base);
    if (!rx.apply_delta<R>(replica, d, cursor)) die("key delta");
    V2& root = f.state.plain_root();
    int ok = 0;
    for (int i = 0; i <= n; ++i) {   // the first tick is the warm-up
        auto& h = root.heroes[std::size_t(i) % root.heroes.size()];
        h.level += 1;
        f.state.touch(h.level);
        if (i == 0) {
            f.state.save_delta_into(d, base);
            if (!rx.apply_delta<R>(replica, d, cursor)) die("delta");
            continue;
        }
        FAMILY_COLLECT((f.state.save_delta_into(d, base), ok += bool(rx.apply_delta<R>(replica, d, cursor))));
    }
    return ok == n ? 0 : 1;
}

// Both rows of each pair give the same result.
int check() {
    Fixture f;
    // Send: both handshakes give the working version, the same bytes as save_verified.
    std::string a, b;
    Peer(hello_of_v2()).save_into(f.state, a);
    Peer(hello_of_v1()).save_into(f.state, b);
    if (a != f.wire || b != f.wire) die("send rows differ from save_verified");
    // Receive: v1 reads the v2 wire with v2's fields kept in its spare bytes.
    XReceiver<XLink::server_peer> rx(cap());
    XBuffer d1(4096, cap()), d2(4096, cap());
    if (!rx.reload<V1>(d1, f.wire) || !rx.reload<V2>(d2, f.wire)) die("reload");
    const V1& o = d1.root<V1>();
    const V2& n = d2.root<V2>();
    if (o.heroes.size() != n.heroes.size() || o.heroes[3].level != n.heroes[3].level ||
        o.armies[2].heroes[4].name.view() != n.armies[2].heroes[4].name.view() || n.vip_exp != 0x1122334455667788)
        die("v1 and v2 read the wire differently");
    std::string again;
    d1.save_verified_into<V1>(again);
    if (again != f.wire) die("v1's image is not the server's");
    // Delta: two replicas, one per version, hold the server's image.
    XDeltaBaseline base(XDeltaRecord::touches);
    XDeltaCursor c1, c2;
    XBuffer r1(4096, cap()), r2(4096, cap());
    std::string d;
    f.state.save_delta_into(d, base);
    if (!rx.apply_delta<V1>(r1, d, c1) || !rx.apply_delta<V2>(r2, d, c2)) die("key delta");
    V2& root = f.state.plain_root();
    for (int i = 0; i < 5; ++i) {
        root.heroes[std::size_t(i)].level += 1;
        f.state.touch(root.heroes[std::size_t(i)].level);
        root.heroes[0].awaken += 1;
        f.state.touch(root.heroes[0].awaken);
        f.state.save_delta_into(d, base);
        if (!rx.apply_delta<V1>(r1, d, c1) || !rx.apply_delta<V2>(r2, d, c2)) die("delta");
    }
    std::string s, x1, x2;
    f.state.save_verified_into(s);
    r1.save_verified_into<V1>(x1);
    r2.save_verified_into<V2>(x2);
    if (x1 != s || x2 != s) die("a replica's image is not the server's");
    std::printf("family_ir: check ok (wire %zu B, %zu heroes)\n", f.wire.size(),
                std::size_t(root.heroes.size() + 4 * root.armies[0].heroes.size()));
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "";
    if (mode == "--list") {
        for (const char* r : kRows) std::printf("%s\n", r);
        return 0;
    }
    if (mode == "--check") return check();
    if (mode != "--row" || argc < 3) {
        std::fprintf(stderr, "usage: family_ir --list | --check | --row NAME [N]\n");
        return 2;
    }
    const std::string_view row = argv[2];
    const int n = argc > 3 ? std::atoi(argv[3]) : 20;
    Fixture f;
    if (row == "send.same") return send_row(f, hello_of_v2(), n);
    if (row == "send.in_family") return send_row(f, hello_of_v1(), n);
    if (row == "recv.same") return recv_row<V2>(f, n);
    if (row == "recv.in_family") return recv_row<V1>(f, n);
    if (row == "delta.same") return delta_row<V2>(f, n);
    if (row == "delta.in_family") return delta_row<V1>(f, n);
    std::fprintf(stderr, "family_ir: unknown row %.*s\n", int(row.size()), row.data());
    return 2;
}
