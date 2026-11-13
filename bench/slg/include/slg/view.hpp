// SLG benchmark, demo case set (SLG_CASES=demo): server -> client map data.
//
// A client sees one screen of the map, a 512 x 384 viewport. What it holds of
// the map is exactly what lies in that rectangle: the tiles under it (32 x 32
// px each, about 17 x 13 of them), the units standing in it (cities, stationed
// troops, resource points; 40 fields each, see WorldUnit) and the marches whose
// line crosses it. A march is a
// line record (start, end, start tick, arrival tick); the client interpolates,
// so a march changes only when it starts, is recalled or arrives.
//
// Two cases (ViewCases):
//   browse (class 2, key EB) - every client keeps dragging its viewport (or
//            jumps elsewhere now and then) and every tick pulls everything in
//            the new rectangle in full. The world changes at the peace rate, so
//            this is almost all full snapshots.
//   static (class 3, keys UP / UC / US) - clients do not move. After one full
//            pull (untimed), each tick the server sends each client the changes
//            inside its viewport only: units that changed, entered or left,
//            marches that started, changed or left, tiles whose owner changed.
//            Three change rates (Rate):
//     peace  - a few unit events per tick (captured: owner, names, alliance;
//              state and its timer; a skirmish: hp, troops, wounded, power; a
//              buff gained, lost or refreshed; a level-up), a few tile owner
//              changes and march turnover (the main case);
//     combat - peace plus two battle zones where every engaged unit changes
//              every tick (a 1 s battle round) and 1/16 of the zone's tiles
//              change owner (reference);
//     stress - a quarter of all units move every tick, 1/16 change hp, 1/16
//              change state, 1/64 of the tiles change owner (upper bound, report
//              only).
//
// The world, the clients' movement and the server's bookkeeping of what each
// client holds are plain game-server code, the same in every runner, and not
// timed. Timed, per format (run_view_case):
//   EB.send   fill one client's viewport snapshot from the world and encode it
//             (the spatial query of what lies in the viewport is bookkeeping,
//             untimed, as the tracker's work is for U*)
//   EB.recv   the client receives it (the format's checked receive) and reads
//             every tile, unit and march
//   EBW.recv  the same, into one view reused for every pull (one real device
//             holds one view, warm in cache; EB.recv's views, one per
//             simulated client, are cold when their client pulls again)
//   U*.send   fill one client's viewport delta from the bookkeeping and encode it
//   U*.recv   the client receives the delta
//   U*.apply  it applies the delta to its held view (through its id index)
//   U*W.recv  (2026-11-04, scoring V3) one device that holds one view, warm: the
//             deliveries of clients 0..kWarmDevices-1 in this cycle, replayed after
//             the cycle, each client's into its own copy of its view (receive +
//             apply, as U*.recv + U*.apply, timed per delivery like them). Before
//             each delivery the device reads its whole view (untimed: the client
//             draws it every frame), so the view is warm as on a real device;
//             count U*W.deliveries. N*W.recv the same for the native deltas.
//   EBZ.send, U*Z.send, N*Z.send  (scoring V3) zstd level 3 of each wire the server
//             sends (one frame per message, context reused, no dictionary), timed
//             apart from *.send, after the round, so it does not evict the timed
//             stages' working set; send + compress = *.send + *Z.send. Own prefix so
//             per-case totals (pool_v4, the perf gate) are unchanged. Sizes in the
//             *_zstd byte keys.
// XOffset native (with XOFFSET_DELTA_SYNC_API), keys N*: the server keeps each
// client's view as an XBuffer, mutates it in place (N*.send) and writes the
// delta against that client's baseline (N*.diff); the client patches and
// verifies (N*.recv) and reads in place.
// XOffset native browse (2026-11-05, report only), keys NB: the same for the
// browse case. The server keeps each client's view, moves it with the client's
// rectangle every tick (units and marches that entered, changed or left; tiles
// that entered or left, tile owners that changed; tiles sit in a ring of slots
// so a tile keeps its slot while the view pans) and writes the delta: NB.send
// (slots + mutation) + NB.diff, NB.recv (patch + verify), NBW.recv (warm single
// devices, as N*W), NBZ.send / NB.delta_zstd (zstd 3 per message), NBZS
// (streaming), NB.deliveries. Run in a pass of its own after EB's, so EB's
// stages keep their range; SLG_NATIVE_OFF=1 drops it.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "generic.hpp"
#include "flat_held_index.hpp"
#if SLG_WITH_COMPRESSION
#include <zstd.h>
#endif

namespace slg {

struct ViewScale {
    const char* name;
    int units;          // units on the map
    int clients;        // client views on the map
    int rounds;         // server ticks (1 s) per cycle
    int marches;        // marches under way, on average
};

inline const ViewScale* find_view_scale(std::string_view n) {
    static const ViewScale scales[] = {
        {"S", 2000, 64, 4, 200},
        {"M", 10000, 1500, 4, 1000},
        {"L", 40000, 6000, 4, 4000},
    };
    for (const auto& s : scales) if (n == s.name) return &s;
    return nullptr;
}

constexpr int32_t kMapSize = 4096;      // map [0, 4096) x [0, 4096)
constexpr int32_t kViewW = 512;         // one screen
constexpr int32_t kViewH = 384;
constexpr int32_t kTile = 32;           // tile edge, px
constexpr int32_t kTiles = kMapSize / kTile;    // 128 x 128 tiles
constexpr int32_t kCell = 128;          // spatial index cell (server bookkeeping)
constexpr int32_t kCells = kMapSize / kCell;    // 32 x 32 cells
constexpr int32_t kPanStep = 48;        // browse: view pan per tick, each axis, at most
constexpr int32_t kJumpOneIn = 16;      // browse: the view jumps elsewhere with p = 1/16 per tick
constexpr int32_t kZoneR = 384;         // combat zone radius

enum class Rate { Peace, Combat, Stress };

struct ViewRect { int32_t x0{}, y0{}, x1{}, y1{}; };   // [x0, x1) x [y0, y1), px

inline int32_t clamp_i(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

// The viewport centred on (cx, cy), kept inside the map.
inline ViewRect view_rect(int32_t cx, int32_t cy) {
    ViewRect r;
    r.x0 = clamp_i(cx - kViewW / 2, 0, kMapSize - kViewW); r.x1 = r.x0 + kViewW;
    r.y0 = clamp_i(cy - kViewH / 2, 0, kMapSize - kViewH); r.y1 = r.y0 + kViewH;
    return r;
}
inline bool in_rect(const ViewRect& r, int32_t x, int32_t y) { return x >= r.x0 && x < r.x1 && y >= r.y0 && y < r.y1; }

// Tiles a viewport covers (partly covered ones included), in tile coordinates.
struct TileSpan { int32_t tx0, ty0, cols, rows; };
inline TileSpan tile_span(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    const int32_t tx0 = x0 / kTile, ty0 = y0 / kTile;
    return {tx0, ty0, (x1 - 1) / kTile - tx0 + 1, (y1 - 1) / kTile - ty0 + 1};
}
// Position of tile (tx, ty) in a view's tile list (row-major over its span).
template <class V> std::size_t tile_pos(const V& v, int32_t tx, int32_t ty) {
    const TileSpan s = tile_span(v.x0, v.y0, v.x1, v.y1);
    return std::size_t((ty - s.ty0) * s.cols + (tx - s.tx0));
}

// A map unit (city, army, resource point) with the fields a real SLG map entity
// carries: 33 scalars (int32, int64 timers, bools), three strings (the owner's
// name passes 10 bytes, so it is not inline in XOffset), a nested commander and
// three lists. Every model's ViewUnit has these fields under these names.
#define SLG_VUNIT_SCALARS(X) X(id) X(kind) X(owner) X(alliance) X(x) X(y) X(hp) X(max_hp) X(state) X(level) \
    X(faction) X(flags) X(speed) X(attack) X(defense) X(morale) X(load) X(icon) X(skin) X(banner) X(title) X(vip) \
    X(rank) X(slots) X(power) X(state_end) X(shield_end) X(cooldown_end) X(last_update) X(kills) \
    X(in_battle) X(shielded) X(hidden)

// Dirty flag bit of each ViewUnit field: SLG_VUNIT_SCALARS order, then the rest.
namespace dbit {
enum : int {
#define SLG_X(f) f,
    SLG_VUNIT_SCALARS(SLG_X)
#undef SLG_X
    name, alliance_tag, owner_name, commander, heroes, troops, buffs
};
}
#define SLG_D(f) (uint64_t(1) << dbit::f)

struct WorldHero { int32_t id{}; int32_t level{}; int32_t star{}; int32_t skin{}; };
struct WorldTroop { int32_t type{}; int32_t count{}; int32_t wounded{}; };
struct WorldBuff { int32_t id{}; int32_t stacks{}; int64_t expire_ms{}; };
struct WorldUnit {
    int32_t id{}; int32_t kind{}; int32_t owner{}; int32_t alliance{}; int32_t x{}; int32_t y{}; int32_t hp{};
    int32_t max_hp{}; int32_t state{}; int32_t level{}; int32_t faction{}; int32_t flags{}; int32_t speed{};
    int32_t attack{}; int32_t defense{}; int32_t morale{}; int32_t load{}; int32_t icon{}; int32_t skin{};
    int32_t banner{}; int32_t title{}; int32_t vip{}; int32_t rank{}; int32_t slots{};
    int64_t power{}; int64_t state_end{}; int64_t shield_end{}; int64_t cooldown_end{}; int64_t last_update{}; int64_t kills{};
    bool in_battle{}; bool shielded{}; bool hidden{};
    std::string name, alliance_tag, owner_name;
    WorldHero commander;
    std::vector<WorldHero> heroes;
    std::vector<WorldTroop> troops;
    std::vector<WorldBuff> buffs;
    int32_t vx{}; int32_t vy{};     // world only: velocity (stress)
    uint32_t ver = 0;               // world only: bumped on every change (server bookkeeping)
    uint64_t dirty = 0;             // world only: SLG_D bits of the fields changed in tick dirty_tick
    int64_t dirty_tick = -1;
};

// Copies of a unit between any two models (std, cista, XOffset, the world).
// A string is assigned only when it differs, through a copy (the source may
// live in the same buffer); a list of the same length is updated in place.
template <class D, class S> void set_str(D& d, const S& s) {
    const std::string_view v = sv(s);
    if (sv(d) == v) return;
    const std::string t(v);
    d = std::string_view(t);
}
template <class D, class S, class F> void copy_list(D& d, const S& s, F&& f) {
    if (d.size() == s.size()) { for (std::size_t i = 0; i < s.size(); ++i) f(d[i], s[i]); return; }
    d.clear();
    d.reserve(s.size());
    for (const auto& e : s) f(d.emplace_back(), e);
}
template <class D, class S> void copy_uhero(D& d, const S& s) { d.id = s.id; d.level = s.level; d.star = s.star; d.skin = s.skin; }
template <class D, class S> void copy_unit(D& d, const S& s) {
#define SLG_X(f) d.f = s.f;
    SLG_VUNIT_SCALARS(SLG_X)
#undef SLG_X
    set_str(d.name, s.name); set_str(d.alliance_tag, s.alliance_tag); set_str(d.owner_name, s.owner_name);
    copy_uhero(d.commander, s.commander);
    copy_list(d.heroes, s.heroes, [](auto& a, const auto& b) { copy_uhero(a, b); });
    copy_list(d.troops, s.troops, [](auto& a, const auto& b) { a.type = b.type; a.count = b.count; a.wounded = b.wounded; });
    copy_list(d.buffs, s.buffs, [](auto& a, const auto& b) { a.id = b.id; a.stacks = b.stacks; a.expire_ms = b.expire_ms; });
}
// A unit read field by field (what a renderer / client logic touches). A
// zeroed (free) slot reads as 0.
template <class U> uint64_t query_unit(const U& u) {
    if (u.id == 0) return 0;
    uint64_t h = 0;
#define SLG_X(f) h = h * 31u + uint64_t(u.f);
    SLG_VUNIT_SCALARS(SLG_X)
#undef SLG_X
    h += sv(u.name).size() * 7u + sv(u.alliance_tag).size() * 11u + sv(u.owner_name).size() * 13u;
    h += uint64_t(u.commander.id) * 3u + uint64_t(u.commander.level + u.commander.star + u.commander.skin);
    for (const auto& e : u.heroes) h = h * 17u + uint64_t(e.id) + uint64_t(e.level) * 5u + uint64_t(e.star + e.skin);
    for (const auto& e : u.troops) h = h * 19u + uint64_t(e.type) + uint64_t(e.count) * 3u + uint64_t(e.wounded);
    for (const auto& e : u.buffs) h = h * 23u + uint64_t(e.id) + uint64_t(e.stacks) + uint64_t(e.expire_ms);
    return h;
}

struct WorldMarch {
    int32_t id{}; int32_t owner{}; int32_t x0{}; int32_t y0{}; int32_t x1{}; int32_t y1{};
    int64_t t0{}; int64_t t1{};
    std::vector<int32_t> pts;     // its line sampled every 32 px: x, y, x, y, ...
    std::vector<int32_t> cells;   // index cells those points fall in
};

struct WorldTile { int32_t terrain{}; int32_t level{}; int64_t owner{}; int64_t changed = -1; };

struct ViewClient { int32_t cx{}; int32_t cy{}; };

class MapWorld {
public:
    void setup(uint64_t seed, const ViewScale& vs, Rate rate) {
        vs_ = &vs; rate_ = rate; tick_ = 0;
        Rng rng(mix_seed(seed, 0x7e3));
        units_.clear();
        units_.reserve(std::size_t(vs.units));
        for (int i = 0; i < vs.units; ++i) {
            WorldUnit u;
            u.id = i + 1; u.owner = 1 + rng.below(5000);
            u.x = rng.below(kMapSize); u.y = rng.below(kMapSize);
            u.vx = rng.range(-8, 8); u.vy = rng.range(-8, 8);
            u.hp = rng.range(100, 1000); u.state = rng.below(4);
            u.name = std::string(rng.below(2) ? "u_" : "sq_") + std::to_string(u.id);   // at most 8 bytes
            dress(rng, u);
            units_.push_back(std::move(u));
        }
        for (auto& z : zones_) { z[0] = rng.range(kZoneR, kMapSize - kZoneR); z[1] = rng.range(kZoneR, kMapSize - kZoneR); }
        engaged_.clear();
        for (int32_t i = 0; i < int32_t(units_.size()); ++i)
            if (in_zone(units_[std::size_t(i)].x, units_[std::size_t(i)].y)) engaged_.push_back(i);
        marches_.clear();
        next_march_ = 1;
        for (int i = 0; i < vs.marches; ++i) {
            add_march(rng);
            auto& m = marches_.back();
            const int64_t back = rng.below(int32_t(m.t1 - m.t0));   // already under way
            m.t0 -= back; m.t1 -= back;
            if (m.t1 <= 0) m.t1 = 1;
        }
        clients_.assign(std::size_t(vs.clients), {});
        for (auto& c : clients_) jump(rng, c);
        Rng tr(mix_seed(seed, 0x7e6));
        tiles_.assign(std::size_t(kTiles) * kTiles, {});
        for (auto& t : tiles_) { t.terrain = tr.below(6); t.level = 1 + tr.below(10); t.owner = tr.below(3) ? 0 : 1 + tr.below(5000); }
        zone_tiles_.clear();
        for (int32_t ty = 0; ty < kTiles; ++ty)
            for (int32_t tx = 0; tx < kTiles; ++tx)
                if (in_zone(tx * kTile + kTile / 2, ty * kTile + kTile / 2)) zone_tiles_.push_back(ty * kTiles + tx);
        tick_rng_ = Rng(mix_seed(seed, 0x7e4));
        view_rng_ = Rng(mix_seed(seed, 0x7e5));
        rebuild();
    }

    void tick() {
        ++tick_;
        changed_.clear();
        changed_tiles_.clear();
        Rng& r = tick_rng_;
        if (rate_ == Rate::Stress) {
            for (auto& u : units_) {
                const int32_t k = r.below(16);
                if (k == 0) { u.hp = u.hp > 10 ? u.hp - r.range(1, 10) : u.hp + 500; hit_troop(r, u); touch(u, SLG_D(hp) | SLG_D(troops) | SLG_D(power)); }
                else if (k == 1) { set_state(r, u); touch(u, SLG_D(state) | SLG_D(state_end)); }
                if (u.state != 0) continue;   // state 0 = moving: about a quarter of the units
                u.x += u.vx; u.y += u.vy; touch(u, SLG_D(x) | SLG_D(y));
                if (u.x < 0 || u.x >= kMapSize) { u.vx = -u.vx; u.x = u.x < 0 ? -u.x : 2 * kMapSize - 1 - u.x; }
                if (u.y < 0 || u.y >= kMapSize) { u.vy = -u.vy; u.y = u.y < 0 ? -u.y : 2 * kMapSize - 1 - u.y; }
            }
            for (int e = 0; e < int(tiles_.size()) / 64; ++e) take_tile(r.below(int32_t(tiles_.size())), r);
        } else {
            // A few events per tick across the map: 1 unit in 500 changes owner, state or hp.
            const int events = std::max(1, int(units_.size()) / 500);
            for (int e = 0; e < events; ++e) {
                auto& u = units_[std::size_t(r.below(int32_t(units_.size())))];
                uint64_t d = 0;
                switch (r.below(5)) {
                case 0:                                                 // captured
                    set_owner(r, u);
                    d = SLG_D(owner) | SLG_D(owner_name) | SLG_D(alliance) | SLG_D(alliance_tag);
                    break;
                case 1: set_state(r, u); d = SLG_D(state) | SLG_D(state_end); break;   // starts gathering, garrisons, ...
                case 2:                                                 // skirmish
                    u.hp = u.hp > 50 ? u.hp - r.range(1, 50) : u.hp + 500;
                    hit_troop(r, u);
                    d = SLG_D(hp) | SLG_D(troops) | SLG_D(power);
                    break;
                case 3: change_buff(r, u); d = SLG_D(buffs); break;    // buff gained, lost or refreshed
                default:                                                // levels up
                    ++u.level; u.power += r.range(100, 5000); u.commander.level = std::min(u.commander.level + 1, 60);
                    d = SLG_D(level) | SLG_D(power) | SLG_D(commander);
                    break;
                }
                touch(u, d);
            }
            // 1 tile in 1024 changes owner (occupied, abandoned).
            for (int e = 0; e < int(tiles_.size()) / 1024; ++e) take_tile(r.below(int32_t(tiles_.size())), r);
            if (rate_ == Rate::Combat) {
                // One battle round per tick: every engaged unit loses (or regains) hp, 1 in 8 changes state.
                for (int32_t i : engaged_) {
                    auto& u = units_[std::size_t(i)];
                    u.hp = u.hp > 30 ? u.hp - r.range(1, 30) : u.hp + 800;
                    u.in_battle = true;
                    hit_troop(r, u);
                    u.morale = std::max(0, u.morale - r.below(3));
                    u.kills += r.below(20);
                    uint64_t d = SLG_D(hp) | SLG_D(in_battle) | SLG_D(troops) | SLG_D(power) | SLG_D(morale) | SLG_D(kills);
                    if (r.below(8) == 0) { set_state(r, u); d |= SLG_D(state) | SLG_D(state_end); }
                    if (r.below(16) == 0) { change_buff(r, u); d |= SLG_D(buffs); }
                    touch(u, d);
                }
                for (int32_t t : zone_tiles_) if (r.below(16) == 0) take_tile(t, r);
            }
        }
        // Marches: arrivals leave, a few are recalled (new end and arrival), new ones start.
        for (std::size_t i = 0; i < marches_.size();) {
            if (marches_[i].t1 <= tick_) { marches_[i] = std::move(marches_.back()); marches_.pop_back(); }
            else ++i;
        }
        for (auto& m : marches_)
            if (r.below(100) == 0) {
                m.x1 = (m.x0 + m.x1) / 2; m.y1 = (m.y0 + m.y1) / 2; m.t1 = tick_ + r.range(2, 8);
                trace(m);
            }
        const int born = vs_->marches / duration();
        for (int i = 0; i < born; ++i) add_march(r);
        rebuild();
    }

    // Browse: client c drags its view (or jumps elsewhere).
    void move(int c) {
        ViewClient& v = clients_[std::size_t(c)];
        if (view_rng_.below(kJumpOneIn) == 0) jump(view_rng_, v);
        else {
            v.cx = clamp_i(v.cx + view_rng_.range(-kPanStep, kPanStep), 0, kMapSize - 1);
            v.cy = clamp_i(v.cy + view_rng_.range(-kPanStep, kPanStep), 0, kMapSize - 1);
        }
    }
    ViewRect rect(int c) const { return view_rect(clients_[std::size_t(c)].cx, clients_[std::size_t(c)].cy); }

    // World unit indices standing in r (index cells row-major, then index order).
    void units_in(const ViewRect& r, std::vector<int32_t>& out) const {
        out.clear();
        for (int32_t cy = r.y0 / kCell; cy <= (r.y1 - 1) / kCell; ++cy)
            for (int32_t cx = r.x0 / kCell; cx <= (r.x1 - 1) / kCell; ++cx)
                for (int32_t i : ucell_[std::size_t(cy * kCells + cx)])
                    if (in_rect(r, units_[std::size_t(i)].x, units_[std::size_t(i)].y)) out.push_back(i);
    }
    // World march indices whose line crosses r (a sample point inside it).
    void marches_in(const ViewRect& r, std::vector<int32_t>& out) const {
        out.clear();
        ++mgen_;
        for (int32_t cy = r.y0 / kCell; cy <= (r.y1 - 1) / kCell; ++cy)
            for (int32_t cx = r.x0 / kCell; cx <= (r.x1 - 1) / kCell; ++cx)
                for (int32_t i : mcell_[std::size_t(cy * kCells + cx)]) {
                    if (mstamp_[std::size_t(i)] == mgen_) continue;
                    mstamp_[std::size_t(i)] = mgen_;
                    const auto& p = marches_[std::size_t(i)].pts;
                    for (std::size_t k = 0; k < p.size(); k += 2)
                        if (in_rect(r, p[k], p[k + 1])) { out.push_back(i); break; }
                }
    }

    const WorldUnit& unit(int32_t i) const { return units_[std::size_t(i)]; }
    // Fields of unit i changed in the current tick (SLG_D bits): server
    // bookkeeping, as a game server's dirty flags, set where the world
    // changes the unit.
    uint64_t dirty(int32_t i) const { const WorldUnit& u = units_[std::size_t(i)]; return u.dirty_tick == tick_ ? u.dirty : 0; }
    const WorldMarch& march(int32_t i) const { return marches_[std::size_t(i)]; }
    int32_t march_count() const { return int32_t(marches_.size()); }
    const WorldTile& tile(int32_t tx, int32_t ty) const { return tiles_[std::size_t(ty * kTiles + tx)]; }
    int64_t tick_no() const { return tick_; }
    const ViewScale& scale() const { return *vs_; }
    // World unit indices changed in the current tick, first change order
    // (server bookkeeping, as dirty(): a game server's dirty set). Untimed.
    const std::vector<int32_t>& changed_units() const { return changed_; }
    // Tiles whose owner changed in the current tick (ty * kTiles + tx), the
    // same kind of bookkeeping. Untimed.
    const std::vector<int32_t>& changed_tiles() const { return changed_tiles_; }

private:
    // A change to u in this tick (the fields `bits`; last_update always).
    void touch(WorldUnit& u, uint64_t bits) const {
        ++u.ver; u.last_update = tick_;
        if (u.dirty_tick != tick_) { u.dirty = 0; u.dirty_tick = tick_; changed_.push_back(int32_t(&u - units_.data())); }
        u.dirty |= bits | SLG_D(last_update);
    }
    static std::string owner_name_of(int32_t owner) {
        std::string d = std::to_string(owner);
        return "lord_" + std::string(7 - std::min<std::size_t>(7, d.size()), '0') + d;   // 12 bytes
    }
    static std::string tag_of(int32_t alliance) { return alliance ? "[A" + std::to_string(alliance % 1000) + "]" : std::string(); }
    void set_owner(Rng& r, WorldUnit& u) const {
        u.owner = 1 + r.below(5000);
        u.owner_name = owner_name_of(u.owner);
        u.alliance = r.below(3) ? 1 + r.below(800) : 0;
        u.alliance_tag = tag_of(u.alliance);
    }
    void set_state(Rng& r, WorldUnit& u) const { u.state = r.below(4); u.state_end = tick_ + r.range(5, 600); }
    static void hit_troop(Rng& r, WorldUnit& u) {
        if (u.troops.empty()) return;
        auto& t = u.troops[std::size_t(r.below(int32_t(u.troops.size())))];
        const int32_t loss = std::min(t.count, r.range(1, 200));
        t.count = t.count > loss ? t.count - loss : t.count + 5000;
        t.wounded += loss / 2;
        u.power = std::max<int64_t>(0, u.power - loss);
    }
    void change_buff(Rng& r, WorldUnit& u) const {
        const int32_t k = r.below(3);
        if (k == 0 && u.buffs.size() < 5) u.buffs.push_back({1 + r.below(200), 1 + r.below(5), (tick_ + r.range(30, 3600)) * 1000});
        else if (k == 1 && !u.buffs.empty()) u.buffs.erase(u.buffs.begin() + r.below(int32_t(u.buffs.size())));
        else if (!u.buffs.empty()) u.buffs[std::size_t(r.below(int32_t(u.buffs.size())))].expire_ms = (tick_ + r.range(30, 3600)) * 1000;
    }
    static void dress(Rng& r, WorldUnit& u) {
        u.kind = r.below(4); u.max_hp = u.hp + r.range(0, 500); u.level = 1 + r.below(30); u.faction = r.below(6);
        u.flags = r.below(1 << 16); u.speed = r.range(10, 100); u.attack = r.range(100, 5000); u.defense = r.range(100, 5000);
        u.morale = r.range(50, 100); u.load = r.below(100000); u.icon = r.below(200); u.skin = r.below(50);
        u.banner = r.below(100); u.title = r.below(20); u.vip = r.below(16); u.rank = r.below(5); u.slots = 1 + r.below(5);
        u.power = int64_t(r.range(1000, 1000000)) * 3; u.state_end = r.range(0, 600); u.shield_end = r.below(4) ? 0 : r.range(1, 86400);
        u.cooldown_end = r.range(0, 300); u.last_update = 0; u.kills = r.below(100000);
        u.in_battle = false; u.shielded = r.below(8) == 0; u.hidden = r.below(32) == 0;
        u.owner_name = owner_name_of(u.owner);
        u.alliance = r.below(3) ? 1 + r.below(800) : 0;
        u.alliance_tag = tag_of(u.alliance);
        u.commander = {1 + r.below(300), 1 + r.below(60), 1 + r.below(6), r.below(10)};
        u.heroes.clear();
        for (int32_t i = 0, n = 1 + r.below(3); i < n; ++i) u.heroes.push_back({1 + r.below(300), 1 + r.below(60), 1 + r.below(6), r.below(10)});
        u.troops.clear();
        for (int32_t i = 0, n = 3 + r.below(4); i < n; ++i) u.troops.push_back({i + 1, r.range(100, 20000), r.below(500)});
        u.buffs.clear();
        for (int32_t i = 0, n = r.below(6); i < n; ++i) u.buffs.push_back({1 + r.below(200), 1 + r.below(5), int64_t(r.range(30, 3600)) * 1000});
    }
    static void jump(Rng& rng, ViewClient& v) { v.cx = rng.below(kMapSize); v.cy = rng.below(kMapSize); }
    bool in_zone(int32_t x, int32_t y) const {
        for (const auto& z : zones_) {
            const int64_t dx = x - z[0], dy = y - z[1];
            if (dx * dx + dy * dy <= int64_t(kZoneR) * kZoneR) return true;
        }
        return false;
    }
    void take_tile(int32_t t, Rng& r) {
        auto& x = tiles_[std::size_t(t)];
        x.owner = r.below(4) ? 1 + r.below(5000) : 0;
        if (x.changed != tick_) changed_tiles_.push_back(t);
        x.changed = tick_;
    }
    int duration() const { return rate_ == Rate::Peace ? 30 : rate_ == Rate::Combat ? 15 : 10; }
    static void trace(WorldMarch& m) {
        m.pts.clear(); m.cells.clear();
        const int32_t dx = m.x1 - m.x0, dy = m.y1 - m.y0;
        const int32_t steps = std::max(1, (std::max(std::abs(dx), std::abs(dy)) + 31) / 32);
        for (int32_t s = 0; s <= steps; ++s) {
            const int32_t x = m.x0 + int32_t(int64_t(dx) * s / steps), y = m.y0 + int32_t(int64_t(dy) * s / steps);
            m.pts.push_back(x); m.pts.push_back(y);
            const int32_t c = (y / kCell) * kCells + x / kCell;
            if (std::find(m.cells.begin(), m.cells.end(), c) == m.cells.end()) m.cells.push_back(c);
        }
    }
    void add_march(Rng& r) {
        WorldMarch m;
        m.id = next_march_++; m.owner = 1 + r.below(5000);
        m.x0 = r.below(kMapSize); m.y0 = r.below(kMapSize);
        m.x1 = clamp_i(m.x0 + r.range(-1500, 1500), 0, kMapSize - 1);
        m.y1 = clamp_i(m.y0 + r.range(-1500, 1500), 0, kMapSize - 1);
        const int d = duration();
        m.t0 = tick_; m.t1 = tick_ + r.range(d / 2, d + d / 2);
        trace(m);
        marches_.push_back(std::move(m));
    }
    void rebuild() {
        ucell_.resize(std::size_t(kCells) * kCells);
        mcell_.resize(std::size_t(kCells) * kCells);
        for (auto& c : ucell_) c.clear();
        for (auto& c : mcell_) c.clear();
        for (std::size_t i = 0; i < units_.size(); ++i)
            ucell_[std::size_t((units_[i].y / kCell) * kCells + units_[i].x / kCell)].push_back(int32_t(i));
        for (std::size_t i = 0; i < marches_.size(); ++i)
            for (int32_t c : marches_[i].cells) mcell_[std::size_t(c)].push_back(int32_t(i));
        mstamp_.assign(marches_.size(), 0);
        mgen_ = 0;
    }

    const ViewScale* vs_ = nullptr;
    Rate rate_ = Rate::Stress;
    std::vector<WorldUnit> units_;
    std::vector<WorldMarch> marches_;
    std::vector<WorldTile> tiles_;
    std::vector<int32_t> engaged_, zone_tiles_;
    int32_t zones_[2][2]{};
    int32_t next_march_ = 1;
    std::vector<ViewClient> clients_;
    mutable std::vector<int32_t> changed_;
    std::vector<int32_t> changed_tiles_;
    std::vector<std::vector<int32_t>> ucell_, mcell_;
    mutable std::vector<uint32_t> mstamp_;
    mutable uint32_t mgen_ = 0;
    int64_t tick_ = 0;
    Rng tick_rng_{0}, view_rng_{0};
};

template <class M, class W> void copy_vmarch(M& d, const W& s) {
    d.id = s.id; d.owner = s.owner; d.x0 = s.x0; d.y0 = s.y0; d.x1 = s.x1; d.y1 = s.y1; d.t0 = s.t0; d.t1 = s.t1;
}
template <class T> void copy_vtile(T& d, const MapWorld& w, int32_t tx, int32_t ty) {
    const WorldTile& s = w.tile(tx, ty);
    d.x = tx; d.y = ty; d.terrain = s.terrain; d.level = s.level; d.owner = s.owner;
}

// What lies in a viewport (server bookkeeping, untimed like the tracker):
// world unit and march indices from the spatial index.
struct ViewQuery {
    std::vector<int32_t> units, marches;
    void run(const MapWorld& w, const ViewRect& r) { w.units_in(r, units); w.marches_in(r, marches); }
};

// Fill a viewport snapshot of any model (std, XOffset, cista) from the world:
// every tile under r (row-major), the units and marches q found in it.
template <class V> void fill_view(V& v, const MapWorld& w, const ViewRect& r, const ViewQuery& q, int64_t request_id, int64_t tick) {
    const auto& us = q.units;
    const auto& ms = q.marches;
    v.request_id = request_id; v.tick = tick;
    v.x0 = r.x0; v.y0 = r.y0; v.x1 = r.x1; v.y1 = r.y1;
    v.units.clear();
    v.units.reserve(us.size());
    for (int32_t i : us) {
        copy_unit(v.units.emplace_back(), w.unit(i));
    }
    v.marches.clear();
    v.marches.reserve(ms.size());
    for (int32_t i : ms) copy_vmarch(v.marches.emplace_back(), w.march(i));
    const TileSpan sp = tile_span(r.x0, r.y0, r.x1, r.y1);
    v.tiles.clear();
    v.tiles.reserve(std::size_t(sp.cols * sp.rows));
    for (int32_t ty = sp.ty0; ty < sp.ty0 + sp.rows; ++ty)
        for (int32_t tx = sp.tx0; tx < sp.tx0 + sp.cols; ++tx) copy_vtile(v.tiles.emplace_back(), w, tx, ty);
}

// The client reads every tile, unit and march of the view it holds (what a
// renderer touches); order-independent, and the result must match across
// formats. Zeroed (free) unit and march slots add nothing.
template <class V> uint64_t query_view(const V& v) {
    uint64_t h = uint64_t(v.request_id) * 31u + uint64_t(v.tick) + uint64_t(v.x0) * 7u + uint64_t(v.y1);
    for (const auto& u : v.units) h += query_unit(u);
    for (const auto& m : v.marches)
        h += uint64_t(m.id) * 2654435761u + uint64_t(m.x0 ^ m.y1) + uint64_t(m.x1 ^ m.y0) + uint64_t(m.t0) * 5u + uint64_t(m.t1) + uint64_t(m.owner);
    for (const auto& t : v.tiles)
        h += uint64_t(t.owner) * 40503u + uint64_t(t.level) * 3u + uint64_t(t.terrain) + uint64_t(t.x * 131 + t.y) * 17u;
    return h;
}

// ---- deltas -----------------------------------------------------------------
//
// Server bookkeeping (not timed): for every static client, what it holds.
// Every client is planned every tick, so its record is always the state the
// client holds after that tick's delta.

struct DeltaPlan {
    std::vector<int32_t> units;            // world unit indices: entered or changed
    std::vector<int32_t> removed;          // unit ids that left
    std::vector<int32_t> marches;          // world march indices: new or changed
    std::vector<int32_t> removed_marches;  // march ids that left
    std::vector<int32_t> tiles;            // tiles changed this tick: ty * kTiles + tx
    std::vector<uint8_t> unew, mnew;       // units[k] / marches[k] is new to the client's view (bookkeeping the
                                           // server has anyway; only the D3 prototype uses it)
    // D3 only (XS 2.0 slots): the slot the client keeps units[k] / marches[k] /
    // removed[k] / removed_marches[k] in, as the tracker's record of what the
    // client holds stores it (-1 for one that is new: the D3 sender gives it a
    // slot, timed, and stores it through uref / mref).
    std::vector<int32_t> uslot, mslot, rslot, rmslot;
    std::vector<int32_t*> uref, mref;
    // D3 only: the same units and marches as two lists each, changed and
    // entering (the tracker knows which when it finds one): world index and
    // the client's slot, or where the tracker keeps the slot the sender gives.
    std::vector<int32_t> uci, ucs, uei, mci, mcs, mei;
    std::vector<int32_t*> uer, mer;
    bool empty() const { return units.empty() && removed.empty() && marches.empty() && removed_marches.empty() && tiles.empty(); }
};

class ViewTracker {
public:
    void setup(int clients) { u_.assign(std::size_t(clients), {}); m_.assign(std::size_t(clients), {}); }
    void plan(const MapWorld& w, int c, const ViewRect& r, DeltaPlan& p) {
        p.units.clear(); p.removed.clear(); p.marches.clear(); p.removed_marches.clear(); p.tiles.clear();
        p.unew.clear(); p.mnew.clear();
        p.uslot.clear(); p.mslot.clear(); p.rslot.clear(); p.rmslot.clear(); p.uref.clear(); p.mref.clear();
        p.uci.clear(); p.ucs.clear(); p.uei.clear(); p.uer.clear(); p.mci.clear(); p.mcs.clear(); p.mei.clear(); p.mer.clear();
        ++gen_;
        auto& lu = u_[std::size_t(c)];
        w.units_in(r, tmp_);
        for (int32_t i : tmp_) {
            const WorldUnit& u = w.unit(i);
            auto [it, fresh] = lu.try_emplace(u.id);
            SentU& s = it->second;
            if (fresh || s.ver != u.ver) { s.ver = u.ver; p.units.push_back(i); p.unew.push_back(fresh ? 1 : 0); p.uslot.push_back(s.slot); p.uref.push_back(&s.slot);
                if (fresh) { p.uei.push_back(i); p.uer.push_back(&s.slot); } else { p.uci.push_back(i); p.ucs.push_back(s.slot); } }
            s.gen = gen_;
        }
        pairs_.clear();
        for (auto it = lu.begin(); it != lu.end();) {
            if (it->second.gen != gen_) { pairs_.push_back({it->first, it->second.slot}); it = lu.erase(it); }
            else ++it;
        }
        // Deterministic order (unordered_map iteration order is the same code in
        // every runner, but sorting keeps the messages independent of it).
        std::sort(pairs_.begin(), pairs_.end());
        for (auto [id, sl] : pairs_) { p.removed.push_back(id); p.rslot.push_back(sl); }
        auto& lm = m_[std::size_t(c)];
        w.marches_in(r, tmp_);
        for (int32_t i : tmp_) {
            const WorldMarch& m = w.march(i);
            auto [it, fresh] = lm.try_emplace(m.id);
            SentM& s = it->second;
            if (fresh || s.x1 != m.x1 || s.y1 != m.y1 || s.t1 != m.t1) { s.x1 = m.x1; s.y1 = m.y1; s.t1 = m.t1; p.marches.push_back(i); p.mnew.push_back(fresh ? 1 : 0); p.mslot.push_back(s.slot); p.mref.push_back(&s.slot);
                if (fresh) { p.mei.push_back(i); p.mer.push_back(&s.slot); } else { p.mci.push_back(i); p.mcs.push_back(s.slot); } }
            s.gen = gen_;
        }
        pairs_.clear();
        for (auto it = lm.begin(); it != lm.end();) {
            if (it->second.gen != gen_) { pairs_.push_back({it->first, it->second.slot}); it = lm.erase(it); }
            else ++it;
        }
        std::sort(pairs_.begin(), pairs_.end());
        for (auto [id, sl] : pairs_) { p.removed_marches.push_back(id); p.rmslot.push_back(sl); }
        // The client's tiles never change place; a tile is sent when its owner changed this tick.
        const TileSpan sp = tile_span(r.x0, r.y0, r.x1, r.y1);
        for (int32_t ty = sp.ty0; ty < sp.ty0 + sp.rows; ++ty)
            for (int32_t tx = sp.tx0; tx < sp.tx0 + sp.cols; ++tx)
                if (w.tile(tx, ty).changed == w.tick_no()) p.tiles.push_back(ty * kTiles + tx);
    }
private:
    // slot: where the client keeps the record (D3 only; -1 until its sender gives one).
    struct SentU { uint32_t ver{}; int32_t slot = -1; uint64_t gen{}; };
    struct SentM { int32_t x1{}, y1{}; int64_t t1{}; int32_t slot = -1; uint64_t gen{}; };
    std::vector<std::pair<int32_t, int32_t>> pairs_;
    std::vector<std::unordered_map<int32_t, SentU>> u_;
    std::vector<std::unordered_map<int32_t, SentM>> m_;
    std::vector<int32_t> tmp_;
    uint64_t gen_ = 0;
};

template <class D> void fill_delta(D& d, const MapWorld& w, const ViewRect& r, const DeltaPlan& p, int64_t request_id) {
    d.request_id = request_id; d.tick = w.tick_no();
    d.x0 = r.x0; d.y0 = r.y0; d.x1 = r.x1; d.y1 = r.y1;
    d.upserts.clear();
    d.upserts.reserve(p.units.size());
    for (int32_t i : p.units) {
        copy_unit(d.upserts.emplace_back(), w.unit(i));
    }
    d.removed.clear();
    d.removed.reserve(p.removed.size());
    for (int32_t id : p.removed) d.removed.emplace_back(id);
    d.marches.clear();
    d.marches.reserve(p.marches.size());
    for (int32_t i : p.marches) copy_vmarch(d.marches.emplace_back(), w.march(i));
    d.removed_marches.clear();
    d.removed_marches.reserve(p.removed_marches.size());
    for (int32_t id : p.removed_marches) d.removed_marches.emplace_back(id);
    d.tiles.clear();
    d.tiles.reserve(p.tiles.size());
    for (int32_t t : p.tiles) copy_vtile(d.tiles.emplace_back(), w, t % kTiles, t / kTiles);
}

// Client side: id -> position of a held view's units and marches.
struct HeldIndex { FlatHeldIndexMap units, marches; };

template <class V> void build_index(const V& v, HeldIndex& at) {
    at.units.clear(); at.marches.clear();
    at.units.reserve(v.units.size()); at.marches.reserve(v.marches.size());
    for (std::size_t i = 0; i < v.units.size(); ++i) at.units.emplace(v.units[i].id, int32_t(i));
    for (std::size_t i = 0; i < v.marches.size(); ++i) at.marches.emplace(v.marches[i].id, int32_t(i));
}

// Apply a received delta to a held view (any model) through its index. A record
// that left is replaced by the last one (order is not kept); a changed one is
// updated in place; a new one is appended. Tiles are updated at their place.
template <class V, class D> void apply_delta(V& v, HeldIndex& at, const D& d) {
    v.request_id = d.request_id; v.tick = d.tick; v.x0 = d.x0; v.y0 = d.y0; v.x1 = d.x1; v.y1 = d.y1;
    for (const auto& id : d.removed) {
        const auto it = at.units.find(id);
        if (it == at.units.end()) continue;
        const int32_t pos = it->second, last = int32_t(v.units.size()) - 1;
        if (pos != last) {
            // Through a plain copy: the source lives in the same container.
            static thread_local WorldUnit tmp;
            copy_unit(tmp, v.units[std::size_t(last)]);
            auto& dst = v.units[std::size_t(pos)];
            copy_unit(dst, tmp);
            at.units[dst.id] = pos;
        }
        v.units.pop_back();
        at.units.erase(it);
    }
    for (const auto& u : d.upserts) {
        const auto it = at.units.find(u.id);
        if (it != at.units.end()) {
            copy_unit(v.units[std::size_t(it->second)], u);
        } else {
            at.units.emplace(u.id, int32_t(v.units.size()));
            copy_unit(v.units.emplace_back(), u);
        }
    }
    for (const auto& id : d.removed_marches) {
        const auto it = at.marches.find(id);
        if (it == at.marches.end()) continue;
        const int32_t pos = it->second, last = int32_t(v.marches.size()) - 1;
        if (pos != last) {
            copy_vmarch(v.marches[std::size_t(pos)], v.marches[std::size_t(last)]);
            at.marches[v.marches[std::size_t(pos)].id] = pos;
        }
        v.marches.pop_back();
        at.marches.erase(it);
    }
    for (const auto& m : d.marches) {
        const auto it = at.marches.find(m.id);
        if (it != at.marches.end()) copy_vmarch(v.marches[std::size_t(it->second)], m);
        else {
            at.marches.emplace(m.id, int32_t(v.marches.size()));
            copy_vmarch(v.marches.emplace_back(), m);
        }
    }
    for (const auto& t : d.tiles) {
        auto& dst = v.tiles[tile_pos(v, t.x, t.y)];
        dst.terrain = t.terrain; dst.level = t.level; dst.owner = t.owner;
    }
}

// ---- the driver ---------------------------------------------------------------
//
// Ops (one per case, provided by each runner):
//   void setup(int clients)                    held views: one per client
//   std::size_t snap_encode(int b, auto fill)  fill(model&) then encode into send slot b (always 0)
//   std::string_view snap_wire(int b)
//   void snap_recv(int c, int k, int b)        receive slot b's wire into client c's held view (k = 0);
//                                              c = clients is the warm view (EBW), so held views are clients + 1
//   auto& held(int c, int k)                   the held view (model object)
//   std::size_t delta_encode(auto fill)        fill(model&) then encode the delta
//   std::string_view delta_wire()
//   void delta_recv()                          receive the delta into the reused receive object
//   void delta_recv_from(const std::string&)  the same from a stored wire (U*W replay)
//   const auto& delta()                        that object
// Optional: std::string_view snap_compact(int b) (XOffset: the compact wire, for bytes only).
// Optional native delta (XOffset with XOFFSET_DELTA_SYNC_API), keys N*; every
// client c has its own server-side view and baseline:
//   void n_view(int c, const MapWorld&, const ViewRect&)   lay out the view's rect and tiles (untimed)
//   void n_plan(int c, const MapWorld&, const DeltaPlan&)  which slot each change uses (timed in N*.send)
//   void n_update(int c, const MapWorld&, const DeltaPlan&, int64_t id)   mutate the server's view
//   std::size_t n_diff(int c)                  save_delta_into against the client's baseline
//   std::size_t n_key(int c)                   key delta for the first pull (untimed here)
//   void n_key_recv(int c, int k, int b)       the client applies the key delta (b = c)
//   void n_apply(int c, int k)                 the client applies its delta (patch + verify)
//   auto& n_held(int c, int k)                 its view; must read the same as held(c, k)
//   std::string_view n_wire()                  the delta n_diff wrote (bytes, N*W replay)
//   void n_apply_from(int c, const std::string&)   n_apply of a stored delta (N*W; c = clients)
//   void n_bview(int c)                        browse (NB): lay out the view, tiles in a ring of slots (untimed)
//   int n_check(int c)                         optional: how client c's last apply was checked (NB.check_*, report only)
//   void n_move(int c, const MapWorld&, const ViewRect&, const std::vector<int32_t>& changed)   browse: the new
//                                              rectangle, the tiles that entered or left, owner changes; timed in NB.send

struct ViewKeys {
    std::string esend, erecv, ewrecv, usend, urecv, uapply;
    std::string ewin, ereq, ezstd, ecomp, ecompz, udelta, umsg, uzstd, uunits, umarch, utiles;
    std::string nsend, ndiff, nrecv, ndelta;
    std::string esendz, usendz, uwrecv, uwmsg, nsendz, nzstd, nwrecv;   // scoring V3 (2026-11-04)
    std::string uname, nname, eb_zs, u_zs, n_zs;   // streaming compression prefixes (EBZS, UPZS, NPZS)
    std::string nmsg, nwmsg;   // NB.deliveries, NBW.deliveries (browse native, 2026-11-05)
    // D3 shared deltas (EXPERIMENTAL, 2026-11-04): DP.pub once per tick; DP.send,
    // DP.recv (full check), DPT.recv (trust tier), DP.delta; DPW.recv / DPTW.recv warm
    // devices; DPZ.send + DP.delta_zstd; DPZS streaming.
    std::string dname, dpub, dsend, drecv, dtrecv, ddelta, dsendz, dzstd, dwrecv, dtwrecv, d_zs;
    ViewKeys(const char* e, const char* u, const char* n, const char* d) : uname(u), nname(n), dname(d) {
        const std::string E(e), U(u), N(n), D(d);
        dpub = D + ".pub"; dsend = D + ".send"; drecv = D + ".recv"; dtrecv = D + "T.recv"; ddelta = D + ".delta";
        dsendz = D + "Z.send"; dzstd = D + ".delta_zstd"; dwrecv = D + "W.recv"; dtwrecv = D + "TW.recv"; d_zs = D + "ZS";
        esend = E + ".send"; erecv = E + ".recv"; ewrecv = E + "W.recv";
        ewin = E + ".window"; ereq = E + ".requests"; ezstd = E + ".window_zstd";
        ecomp = E + ".window_compact"; ecompz = E + ".window_compact_zstd";
        usend = U + ".send"; urecv = U + ".recv"; uapply = U + ".apply";
        udelta = U + ".delta"; umsg = U + ".deliveries"; uzstd = U + ".delta_zstd";
        uunits = U + ".units"; umarch = U + ".marches"; utiles = U + ".tiles";
        nsend = N + ".send"; ndiff = N + ".diff"; nrecv = N + ".recv"; ndelta = N + ".delta";
        // The compression keys take their own prefix (EBZ, UPZ, NPZ, ...): tools that sum a
        // case's stages by prefix (pool_v4, the perf gate) keep their totals.
        esendz = E + "Z.send"; usendz = U + "Z.send"; uwrecv = U + "W.recv"; uwmsg = U + "W.deliveries";
        nsendz = N + "Z.send"; nzstd = N + ".delta_zstd"; nwrecv = N + "W.recv";
        eb_zs = E + "ZS"; u_zs = U + "ZS"; n_zs = N + "ZS";
        nmsg = N + ".deliveries"; nwmsg = N + "W.deliveries";
    }
};

inline std::size_t zstd_size(std::string_view w) {
#if SLG_WITH_COMPRESSION
    static thread_local ZSTD_CCtx* cx = ZSTD_createCCtx();
    static thread_local std::string z;
    z.resize(ZSTD_compressBound(w.size()));
    return ZSTD_compressCCtx(cx, z.data(), z.size(), w.data(), w.size(), 3);
#else
    return w.size();
#endif
}

// Streaming compression (scoring V3, report only): one zstd level 3 context per
// connection, kept across messages, one ZSTD_e_flush per message (every format the
// same). Run for the warm devices' connections only (a context is ~1 MB);
// *ZS.send timed, *_zs bytes, *ZS.ctx = ZSTD_sizeof_CCtx per connection.
struct ZStream {
#if SLG_WITH_COMPRESSION
    ZSTD_CCtx* cx = ZSTD_createCCtx();
    std::string out;
    ZStream() { ZSTD_CCtx_setParameter(cx, ZSTD_c_compressionLevel, 3); }
    ~ZStream() { ZSTD_freeCCtx(cx); }
    ZStream(const ZStream&) = delete;
    ZStream& operator=(const ZStream&) = delete;
    std::size_t flush(std::string_view w) {
        out.resize(ZSTD_compressBound(w.size()) + 64);
        ZSTD_inBuffer in{w.data(), w.size(), 0};
        ZSTD_outBuffer o{out.data(), out.size(), 0};
        std::size_t r = 0;
        do {
            r = ZSTD_compressStream2(cx, &o, &in, ZSTD_e_flush);
            if (ZSTD_isError(r)) throw std::runtime_error("zstd stream error");
        } while (r != 0);
        return o.pos;
    }
    std::size_t ctx_bytes() const { return ZSTD_sizeof_CCtx(cx); }
#else
    std::size_t flush(std::string_view w) { return w.size(); }
    std::size_t ctx_bytes() const { return 0; }
#endif
};
// The connections' streams of one case, for one format (one per Ops type and key).
struct ZStreams {
    std::vector<std::unique_ptr<ZStream>> s;
    ZStream& at(std::size_t i) { while (s.size() <= i) s.push_back(std::make_unique<ZStream>()); return *s[i]; }
    std::size_t ctx_bytes() const { std::size_t b = 0; for (const auto& z : s) b += z->ctx_bytes(); return s.empty() ? 0 : b / s.size(); }
};
template <class Ops> ZStreams& zstreams(const std::string& key) {
    static thread_local std::map<std::string, ZStreams> m;
    return m[key];
}
inline void zstream_timed(CycleStats& st, ZStream& z, const std::string& tkey, const std::string& bkey, std::string_view w) {
    std::size_t n = 0;
    timed(st, tkey.c_str(), [&] { n = z.flush(w); });
    st.bytes[bkey] += n;
    st.bytes[tkey.substr(0, tkey.find('.')) + ".msgs"] += 1;   // e.g. UPZS.msgs
}

// The demo cases: 0 browse (class 2), 1..3 static at peace / combat / stress (class 3).
struct ViewCases {
    static constexpr int count = 4;
    static constexpr Rate rates[count] = {Rate::Peace, Rate::Peace, Rate::Combat, Rate::Stress};
    static bool browse(int i) { return i == 0; }
    static const ViewKeys& keys(int i) {
        static const ViewKeys k[count] = {{"EB", "UB", "NB", "DB"}, {"EP", "UP", "NP", "DP"}, {"EC", "UC", "NC", "DC"}, {"ES", "US", "NS", "DS"}};
        return k[i];
    }
    static uint64_t seed(uint64_t s, int i) { return mix_seed(s, 0xb10c + uint64_t(i)); }
};

struct ViewSession {
    MapWorld world;
    ViewTracker tracker;
    DeltaPlan plan;
    ViewQuery query;
    bool browse = false, primed = false;
    std::vector<HeldIndex> index;   // per client
    std::vector<std::size_t> wire_size;   // browse: this round's pull per client (bytes pass check)
    std::vector<HeldIndex> windex;        // static: the warm devices' indexes (U*W)
    std::vector<std::string> dkey;        // D3: the warm devices' key messages (their client's first pull)
    void setup(uint64_t seed, const ViewScale& vs, int which) {
        browse = ViewCases::browse(which); primed = false;
        wire_size.assign(std::size_t(vs.clients), 0);
        world.setup(ViewCases::seed(seed, which), vs, ViewCases::rates[which]);
        tracker.setup(vs.clients);
        index.assign(std::size_t(vs.clients), {});
        windex.assign(std::size_t(warm_devices(vs.clients)), {});
    }
};

template <class Ops> void run_view_case(ViewSession& s, Ops& ops, const ViewKeys& k, int cycle, CycleStats& st, std::string* keep_view, std::string* keep_delta) {
    MapWorld& w = s.world;
    const ViewScale& vs = w.scale();
    constexpr bool native = requires { ops.n_diff(0); };
    // D3 (SLG_D3): 0/unset off, 1 D3 only (no N*), 2 both. Off with SLG_NATIVE_OFF.
    constexpr bool d3 = requires { ops.d_publish(w); };
    const bool run_n = native_on() && d3_mode() != 1;
    const bool run_d = native_on() && d3_mode() != 0;
    (void)run_n; (void)run_d;
    if (s.browse) {
        // NB (XOffset native, report only): before the first round, every client's
        // server-side view is laid out at its current rectangle and sent once as a
        // key delta (untimed, as N*'s first pull), and the warm devices take theirs.
        const int nb_dev = warm_devices(vs.clients);
        static thread_local std::vector<int32_t> nbtiles;   // NB: the tiles in view whose owner changed this tick
        if constexpr (native) if (nb_on() && !s.primed) {
            for (int c = 0; c < vs.clients; ++c) {
                const ViewRect rect = w.rect(c);
                s.tracker.plan(w, c, rect, s.plan);
                s.plan.tiles.swap(nbtiles); s.plan.tiles.clear();   // the tiles are n_move's (n_update's sit in place)
                ops.n_bview(c);
                ops.n_plan(c, w, s.plan);
                ops.n_update(c, w, s.plan, 0);
                ops.n_move(c, w, rect, nbtiles);
                ops.n_key(c);
                ops.n_key_recv(c, 0, c);
            }
            for (int c = 0; c < nb_dev; ++c) ops.n_key_recv(vs.clients + c, 0, c);
            s.primed = true;
        }
        static thread_local std::string nbzbuf;   // this round's NB wires, for the untimed zstd sizes
        static thread_local std::vector<std::size_t> nbzoff;
        static thread_local std::vector<std::vector<std::string>> nbwq;   // warm devices' NB wires this cycle (NBW)
        nbwq.assign(std::size_t(nb_dev), {});
        // Class 2: every client moves its view and pulls the whole rectangle.
        for (int r = 0; r < vs.rounds; ++r) {
            w.tick();
            const int64_t base = (int64_t(cycle) * vs.rounds + r) * (vs.clients + 1);
            for (int c = 0; c < vs.clients; ++c) {
                w.move(c);
                const ViewRect rect = w.rect(c);
                s.query.run(w, rect);   // the spatial query: server bookkeeping, untimed
                timed(st, k.esend.c_str(), [&] {
                    ops.snap_encode(0, [&](auto& v) { fill_view(v, w, rect, s.query, base + c, w.tick_no()); });
                });
                uint64_t q = 0;
                timed(st, k.erecv.c_str(), [&] {
                    ops.snap_recv(c, 0, 0);
                    q = query_view(ops.held(c, 0));
                });
                st.report_sum += q;
                // The same pull received by one device that holds one view (EBW):
                // the client's view is reused pull after pull, so it is warm in
                // cache, as on a real client. EB.recv above keeps every simulated
                // client's view (vs.clients of them), which is cold by the time
                // that client pulls again.
                uint64_t qw = 0;
                timed(st, k.ewrecv.c_str(), [&] {
                    ops.snap_recv(vs.clients, 0, 0);
                    qw = query_view(ops.held(vs.clients, 0));
                });
                if (qw != q) throw std::runtime_error("warm view differs from the client's view");
                st.bytes[k.ereq] += 1;
                st.bytes[k.ewin] += ops.snap_wire(0).size();
                s.wire_size[std::size_t(c)] = ops.snap_wire(0).size();
                if (keep_view && c == 0) *keep_view = std::string(ops.snap_wire(0));
            }
            // Bytes only, in a second untimed pass after the timed one: the same
            // pulls are built again (same world tick, rectangles and request ids,
            // so the same wires) and compressed. Compressing (zstd's tables) and
            // the compact save between two timed pulls would evict the server's
            // working set and charge that to the next pull's EB.send.
            for (int c = 0; c < vs.clients; ++c) {
                const ViewRect rect = w.rect(c);
                s.query.run(w, rect);
                ops.snap_encode(0, [&](auto& v) { fill_view(v, w, rect, s.query, base + c, w.tick_no()); });
                if (ops.snap_wire(0).size() != s.wire_size[std::size_t(c)]) throw std::runtime_error("bytes pass: pull differs from the timed one");
                std::size_t z = 0;
                timed(st, k.esendz.c_str(), [&] { z = zstd_size(ops.snap_wire(0)); });
                st.bytes[k.ezstd] += z;
                if (c < warm_devices(vs.clients)) zstream_timed(st, zstreams<Ops>(k.eb_zs).at(std::size_t(c)), k.eb_zs + ".send", "EB.window_zs", ops.snap_wire(0));
                if constexpr (requires { ops.snap_compact(0); }) {
                    const std::string_view cw = ops.snap_compact(0);
                    st.bytes[k.ecomp] += cw.size();
                    st.bytes[k.ecompz] += zstd_size(cw);
                }
            }
            // NB (XOffset native, report only), in a third pass after EB's two, so
            // EB's stages keep their range and working set: the server's view of
            // each client follows its new rectangle (units and marches that entered,
            // changed or left, from the same tracker as U*; tiles that entered or
            // left, tile owners that changed) and writes the delta against the
            // client's baseline; the client patches and verifies. The view must read
            // as the client's EB pull of this round (same request id and tick).
            if constexpr (native) if (nb_on()) {
                for (int c = 0; c < vs.clients; ++c) {
                    const ViewRect rect = w.rect(c);
                    s.tracker.plan(w, c, rect, s.plan);
                    s.plan.tiles.swap(nbtiles); s.plan.tiles.clear();
                    std::size_t nd = 0;
                    timed(st, k.nsend.c_str(), [&] { ops.n_plan(c, w, s.plan); ops.n_update(c, w, s.plan, base + c); ops.n_move(c, w, rect, nbtiles); });
                    timed(st, k.ndiff.c_str(), [&] { nd = ops.n_diff(c); });
                    timed(st, k.nrecv.c_str(), [&] { ops.n_apply(c, 0); });
                    if (query_view(ops.n_held(c, 0)) != query_view(ops.held(c, 0))) throw std::runtime_error("native browse delta: view differs from the client's pull");
                    if constexpr (requires { ops.n_check(0); }) {
                        // How the apply was checked (report only): a delta that moves records'
                        // blocks takes the whole verification.
                        static const char* const kCheck[] = {".check_key", ".check_none", ".check_in_place", ".check_full"};
                        const int ck = ops.n_check(c);
                        if (ck >= 0 && ck < 4) st.bytes[k.nname + kCheck[ck]] += 1;
                    }
                    st.bytes[k.ndelta] += nd;
                    st.bytes[k.nmsg] += 1;
                    const auto nw = ops.n_wire();
                    nbzoff.push_back(nbzbuf.size()); nbzbuf.append(nw.data(), nw.size());
                    if (c < nb_dev) nbwq[std::size_t(c)].emplace_back(nw);
                }
                nbzoff.push_back(nbzbuf.size());
                for (std::size_t i = 0; i + 1 < nbzoff.size(); ++i) {
                    std::size_t z = 0;
                    timed(st, k.nsendz.c_str(), [&] { z = zstd_size(std::string_view(nbzbuf).substr(nbzoff[i], nbzoff[i + 1] - nbzoff[i])); });
                    st.bytes[k.nzstd] += z;
                }
                nbzbuf.clear(); nbzoff.clear();
            }
        }
        st.bytes[k.eb_zs + ".ctx"] = zstreams<Ops>(k.eb_zs).ctx_bytes();
        // NBW: each warm device's NB deliveries of this cycle, replayed into its own
        // view; the device reads its view before each one (untimed), as N*W.
        if constexpr (native) if (nb_on()) {
            for (int d = 0; d < nb_dev; ++d) {
                for (const std::string& nw : nbwq[std::size_t(d)]) {
                    warm_sink = query_view(ops.n_held(vs.clients + d, 0));
                    timed(st, k.nwrecv.c_str(), [&] { ops.n_apply_from(vs.clients + d, nw); });
                    st.bytes[k.nwmsg] += 1;
                }
                if (query_view(ops.n_held(vs.clients + d, 0)) != query_view(ops.held(d, 0))) throw std::runtime_error("warm native browse view differs from the client's pull");
                ZStream& nzs = zstreams<Ops>(k.n_zs).at(std::size_t(d));
                for (const std::string& nw : nbwq[std::size_t(d)]) zstream_timed(st, nzs, k.n_zs + ".send", k.nname + ".delta_zs", nw);
            }
            st.bytes[k.n_zs + ".ctx"] = zstreams<Ops>(k.n_zs).ctx_bytes();
        }
        return;
    }
    if (!s.primed) {
        // Class 3, before the first timed tick: every client pulls its view once
        // (untimed: that is a class 2 pull) and the server records what it holds.
        if constexpr (d3) if (run_d) { ops.d_init(w); s.dkey.assign(std::size_t(warm_devices(vs.clients)), {}); }
        for (int c = 0; c < vs.clients; ++c) {
            const ViewRect rect = w.rect(c);
            s.tracker.plan(w, c, rect, s.plan);
            s.query.run(w, rect);
            ops.snap_encode(0, [&](auto& v) { fill_view(v, w, rect, s.query, 0, w.tick_no()); });
            ops.snap_recv(c, 0, 0);
            // The client keeps this view and edits it: room for the records
            // that come and go (every format the same).
            auto& h = ops.held(c, 0);
            h.units.reserve(h.units.size() * 3 / 2 + 16);
            h.marches.reserve(h.marches.size() * 3 / 2 + 16);
            build_index(h, s.index[std::size_t(c)]);
            if constexpr (d3) if (run_d) {
                ops.d_view(c, rect);
                ops.d_key(c, w, s.plan);
                ops.d_recv(c);
                if (query_view(ops.d_held(c)) != query_view(ops.held(c, 0))) throw std::runtime_error("D3 key: view differs from the snapshot");
                ops.d_recv_trusted(c);
                if (query_view(ops.d_held_trusted(c)) != query_view(ops.held(c, 0))) throw std::runtime_error("D3 key (trust tier): view differs from the snapshot");
                if (c < warm_devices(vs.clients)) s.dkey[std::size_t(c)] = std::string(ops.d_wire());
            }
            if constexpr (native) if (run_n) {
                ops.n_view(c, w, rect);
                ops.n_plan(c, w, s.plan);
                ops.n_update(c, w, s.plan, 0);
                ops.n_key(c);
                ops.n_key_recv(c, 0, c);
                if (query_view(ops.n_held(c, 0)) != query_view(ops.held(c, 0))) throw std::runtime_error("native key delta: view differs from the snapshot");
            }
        }
        // Warm single devices (U*W, N*W): client c's view again (the same tick, so the
        // same pull) into slot clients + c, after every client's own view is laid out,
        // so the clients' views keep the addresses they had without the warm copies.
        for (int c = 0; c < warm_devices(vs.clients); ++c) {
            const ViewRect rect = w.rect(c);
            s.query.run(w, rect);
            ops.snap_encode(0, [&](auto& v) { fill_view(v, w, rect, s.query, 0, w.tick_no()); });
            ops.snap_recv(vs.clients + c, 0, 0);
            auto& hw = ops.held(vs.clients + c, 0);
            hw.units.reserve(hw.units.size() * 3 / 2 + 16);
            hw.marches.reserve(hw.marches.size() * 3 / 2 + 16);
            build_index(hw, s.windex[std::size_t(c)]);
            if (query_view(hw) != query_view(ops.held(c, 0))) throw std::runtime_error("warm view differs from the client's first pull");
            if constexpr (native) if (run_n) ops.n_key_recv(vs.clients + c, 0, c);
            if constexpr (d3) if (run_d) {
                ops.d_view(vs.clients + c, rect);
                ops.d_recv_from(vs.clients + c, s.dkey[std::size_t(c)]);
                ops.d_recv_trusted_from(vs.clients + c, s.dkey[std::size_t(c)]);
                if (query_view(ops.d_held(vs.clients + c)) != query_view(ops.held(c, 0)) ||
                    query_view(ops.d_held_trusted(vs.clients + c)) != query_view(ops.held(c, 0))) throw std::runtime_error("D3 warm view differs from the client's first pull");
            }
        }
        s.primed = true;
    }
    static thread_local std::string zbuf;          // this round's delta wires, for the untimed zstd sizes
    static thread_local std::vector<std::size_t> zoff;
    static thread_local std::string nzbuf;         // the same for the native deltas
    static thread_local std::vector<std::size_t> nzoff;
    const int nw_dev = warm_devices(vs.clients);
    static thread_local std::vector<std::vector<std::string>> wq, nwq, dwq;   // warm devices' wires this cycle (U*W, N*W, D*W)
    wq.assign(std::size_t(nw_dev), {}); nwq.assign(std::size_t(nw_dev), {}); dwq.assign(std::size_t(nw_dev), {});
    static thread_local std::string dzbuf;         // the same for D3
    static thread_local std::vector<std::size_t> dzoff;
    for (int r = 0; r < vs.rounds; ++r) {
        w.tick();
        // D3: every entity changed this tick is compared and encoded once, for
        // every connection (shared work, charged to the server per tick).
        if constexpr (d3) if (run_d) timed(st, k.dpub.c_str(), [&] { ops.d_publish(w); });
        const int64_t base = (int64_t(cycle) * vs.rounds + r) * (vs.clients + 1) + 1;
        for (int c = 0; c < vs.clients; ++c) {
            const ViewRect rect = w.rect(c);
            s.tracker.plan(w, c, rect, s.plan);
            if (s.plan.empty()) continue;   // nothing changed in this view: nothing sent
            std::size_t nd = 0, dd = 0;
            if constexpr (native) if (run_n) {
                // Slot bookkeeping is timed with the update, as the application-level
                // client's id index is timed in U*.apply (fairness review 2026-11-03).
                timed(st, k.nsend.c_str(), [&] { ops.n_plan(c, w, s.plan); ops.n_update(c, w, s.plan, base + c); });
                timed(st, k.ndiff.c_str(), [&] { nd = ops.n_diff(c); });
            }
            if constexpr (d3) if (run_d) timed(st, k.dsend.c_str(), [&] { dd = ops.d_send(c, w, s.plan, base + c); });
            std::size_t n = 0;
            timed(st, k.usend.c_str(), [&] { n = ops.delta_encode([&](auto& d) { fill_delta(d, w, rect, s.plan, base + c); }); });
            if (keep_delta && c == 0) *keep_delta = std::string(ops.delta_wire());
            timed(st, k.urecv.c_str(), [&] { ops.delta_recv(); });
            HeldIndex& at = s.index[std::size_t(c)];
            timed(st, k.uapply.c_str(), [&] { apply_delta(ops.held(c, 0), at, ops.delta()); });
            const uint64_t q = query_view(ops.held(c, 0));
            st.report_sum += q;
            if constexpr (d3) if (run_d) {
                // Trust tier (the main score column, susu 2026-11-04) and full
                // checking, each into its own copy of the client's view. The
                // first of the two pays for bringing the message and shared
                // state back into cache, so the order alternates by delivery.
                const auto trusted = [&] {
                    timed(st, k.dtrecv.c_str(), [&] { ops.d_recv_trusted(c); });
                    if (query_view(ops.d_held_trusted(c)) != q) throw std::runtime_error("D3 delta (trust tier): view differs from the application-level one");
                };
                const auto full = [&] {
                    timed(st, k.drecv.c_str(), [&] { ops.d_recv(c); });
                    if (query_view(ops.d_held(c)) != q) throw std::runtime_error("D3 delta: view differs from the application-level one");
                };
                if ((c + r) & 1) { full(); trusted(); } else { trusted(); full(); }
                st.bytes[k.ddelta] += dd;
                const auto dw = ops.d_wire();
                dzoff.push_back(dzbuf.size()); dzbuf.append(dw.data(), dw.size());
                if (c < nw_dev) dwq[std::size_t(c)].emplace_back(dw);
            }
            if constexpr (native) if (run_n) {
                timed(st, k.nrecv.c_str(), [&] { ops.n_apply(c, 0); });
                if (query_view(ops.n_held(c, 0)) != q) throw std::runtime_error("native delta: view differs from the application-level one");
                st.bytes[k.ndelta] += nd;
                const auto nw = ops.n_wire();
                nzoff.push_back(nzbuf.size()); nzbuf.append(nw.data(), nw.size());
                if (c < nw_dev) nwq[std::size_t(c)].emplace_back(nw);
            }
            st.bytes[k.udelta] += n;
            // zstd size after the round (below): compressing here would evict
            // the server's working set and charge it to the next timed delivery.
            { const auto dw = ops.delta_wire(); zoff.push_back(zbuf.size()); zbuf.append(dw.data(), dw.size()); if (c < nw_dev) wq[std::size_t(c)].emplace_back(dw); }
            st.bytes[k.umsg] += 1;
            st.bytes[k.uunits] += s.plan.units.size() + s.plan.removed.size();
            st.bytes[k.umarch] += s.plan.marches.size() + s.plan.removed_marches.size();
            st.bytes[k.utiles] += s.plan.tiles.size();
        }
        zoff.push_back(zbuf.size());
        for (std::size_t i = 0; i + 1 < zoff.size(); ++i) {
            std::size_t z = 0;
            timed(st, k.usendz.c_str(), [&] { z = zstd_size(std::string_view(zbuf).substr(zoff[i], zoff[i + 1] - zoff[i])); });
            st.bytes[k.uzstd] += z;
        }
        zbuf.clear(); zoff.clear();
        if (!nzoff.empty()) {
            nzoff.push_back(nzbuf.size());
            for (std::size_t i = 0; i + 1 < nzoff.size(); ++i) {
                std::size_t z = 0;
                timed(st, k.nsendz.c_str(), [&] { z = zstd_size(std::string_view(nzbuf).substr(nzoff[i], nzoff[i + 1] - nzoff[i])); });
                st.bytes[k.nzstd] += z;
            }
        }
        nzbuf.clear(); nzoff.clear();
        if (!dzoff.empty()) {
            dzoff.push_back(dzbuf.size());
            for (std::size_t i = 0; i + 1 < dzoff.size(); ++i) {
                std::size_t z = 0;
                timed(st, k.dsendz.c_str(), [&] { z = zstd_size(std::string_view(dzbuf).substr(dzoff[i], dzoff[i + 1] - dzoff[i])); });
                st.bytes[k.dzstd] += z;
            }
        }
        dzbuf.clear(); dzoff.clear();
    }
    // Warm single devices (U*W, N*W): each one's deliveries of this cycle, replayed
    // into its own view; the device reads its view before each one (untimed).
    for (int d = 0; d < nw_dev; ++d) {
        auto& hw = ops.held(vs.clients + d, 0);
        for (const std::string& dw : wq[std::size_t(d)]) {
            warm_sink = query_view(hw);
            timed(st, k.uwrecv.c_str(), [&] { ops.delta_recv_from(dw); apply_delta(hw, s.windex[std::size_t(d)], ops.delta()); });
            st.bytes[k.uwmsg] += 1;
        }
        if (query_view(ops.held(vs.clients + d, 0)) != query_view(ops.held(d, 0))) throw std::runtime_error("warm view differs from the client's view");
        // The same connection's deliveries through its streaming compressor (report only).
        ZStream& zs = zstreams<Ops>(k.u_zs).at(std::size_t(d));
        for (const std::string& dw : wq[std::size_t(d)]) zstream_timed(st, zs, k.u_zs + ".send", k.uname + ".delta_zs", dw);
        if constexpr (d3) if (run_d) {
            // D3 warm device: the same deliveries into its full-check view
            // (DPW.recv) and its trust-tier view (DPTW.recv), each read first;
            // the order of the two alternates, as for D*.recv / D*T.recv.
            const int wv = vs.clients + d;
            int m = 0;
            for (const std::string& dw : dwq[std::size_t(d)]) {
                const auto full = [&] {
                    warm_sink = query_view(ops.d_held(wv));
                    timed(st, k.dwrecv.c_str(), [&] { ops.d_recv_from(wv, dw); });
                };
                const auto trusted = [&] {
                    warm_sink = query_view(ops.d_held_trusted(wv));
                    timed(st, k.dtwrecv.c_str(), [&] { ops.d_recv_trusted_from(wv, dw); });
                };
                if (m++ & 1) { full(); trusted(); } else { trusted(); full(); }
            }
            if (query_view(ops.d_held(wv)) != query_view(ops.held(d, 0)) || query_view(ops.d_held_trusted(wv)) != query_view(ops.held(d, 0)))
                throw std::runtime_error("D3 warm view differs from the client's view");
            ZStream& dzs = zstreams<Ops>(k.d_zs).at(std::size_t(d));
            for (const std::string& dw : dwq[std::size_t(d)]) zstream_timed(st, dzs, k.d_zs + ".send", k.dname + ".delta_zs", dw);
        }
        if constexpr (native) if (run_n) {
            for (const std::string& nw : nwq[std::size_t(d)]) {
                warm_sink = query_view(ops.n_held(vs.clients + d, 0));
                timed(st, k.nwrecv.c_str(), [&] { ops.n_apply_from(vs.clients + d, nw); });
            }
            if (query_view(ops.n_held(vs.clients + d, 0)) != query_view(ops.held(d, 0))) throw std::runtime_error("warm native view differs from the client's view");
            ZStream& nzs = zstreams<Ops>(k.n_zs).at(std::size_t(d));
            for (const std::string& nw : nwq[std::size_t(d)]) zstream_timed(st, nzs, k.n_zs + ".send", k.nname + ".delta_zs", nw);
        }
    }
    st.bytes[k.u_zs + ".ctx"] = zstreams<Ops>(k.u_zs).ctx_bytes();
    if constexpr (native) if (run_n) st.bytes[k.n_zs + ".ctx"] = zstreams<Ops>(k.n_zs).ctx_bytes();
    if constexpr (d3) if (run_d) st.bytes[k.d_zs + ".ctx"] = zstreams<Ops>(k.d_zs).ctx_bytes();
}

// Memory (--mode mem, report only): the server's per-client state of the
// application-level delta, the same for every format: what it records each
// client holds (the tracker, its interest set) and the client's id index
// (HeldIndex). The shared world and send buffers are not per client.
inline void view_memory(const ViewSession& s, const ViewKeys& k, CycleStats& st) {
    if (s.browse || s.index.empty()) return;
    const auto n = std::int64_t(s.index.size());
    st.bytes["MEM." + k.uname + ".interest"] = std::size_t(heap_bytes(s.tracker) / n);
    st.bytes["MEM." + k.uname + ".held_index"] = std::size_t(heap_bytes(s.index) / n);
}

// Demo case set, scene <-> storage: players saved to the storage process this cycle
// (log-outs), then loaded back into the scene (log-ins).
inline std::vector<int> pick_storage(uint64_t seed, int cycle, int players, int n) {
    Rng rng(mix_seed(seed, 0x5703 + uint64_t(cycle)));
    std::vector<int> out;
    for (int i = 0; i < n; ++i) out.push_back(rng.below(players));
    return out;
}

// Demo case set (README "Demo case set"). Needs SLG_REUSE=1.
inline bool demo_cases() {
    static const bool on = [] { const char* e = std::getenv("SLG_CASES"); return e && std::string_view(e) == "demo"; }();
    return on;
}

} // namespace slg
