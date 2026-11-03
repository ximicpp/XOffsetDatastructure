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
//   U*.send   fill one client's viewport delta from the bookkeeping and encode it
//   U*.recv   the client receives the delta
//   U*.apply  it applies the delta to its held view (through its id index)
// XOffset native (with XOFFSET_DELTA_SYNC_API), keys N*: the server keeps each
// client's view as an XBuffer, mutates it in place (N*.send) and writes the
// delta against that client's baseline (N*.diff); the client patches and
// verifies (N*.recv) and reads in place.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "generic.hpp"
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
        Rng& r = tick_rng_;
        if (rate_ == Rate::Stress) {
            for (auto& u : units_) {
                const int32_t k = r.below(16);
                if (k == 0) { u.hp = u.hp > 10 ? u.hp - r.range(1, 10) : u.hp + 500; hit_troop(r, u); touch(u); }
                else if (k == 1) { set_state(r, u); touch(u); }
                if (u.state != 0) continue;   // state 0 = moving: about a quarter of the units
                u.x += u.vx; u.y += u.vy; touch(u);
                if (u.x < 0 || u.x >= kMapSize) { u.vx = -u.vx; u.x = u.x < 0 ? -u.x : 2 * kMapSize - 1 - u.x; }
                if (u.y < 0 || u.y >= kMapSize) { u.vy = -u.vy; u.y = u.y < 0 ? -u.y : 2 * kMapSize - 1 - u.y; }
            }
            for (int e = 0; e < int(tiles_.size()) / 64; ++e) take_tile(r.below(int32_t(tiles_.size())), r);
        } else {
            // A few events per tick across the map: 1 unit in 500 changes owner, state or hp.
            const int events = std::max(1, int(units_.size()) / 500);
            for (int e = 0; e < events; ++e) {
                auto& u = units_[std::size_t(r.below(int32_t(units_.size())))];
                switch (r.below(5)) {
                case 0: set_owner(r, u); break;                         // captured
                case 1: set_state(r, u); break;                         // starts gathering, garrisons, ...
                case 2:                                                 // skirmish
                    u.hp = u.hp > 50 ? u.hp - r.range(1, 50) : u.hp + 500;
                    hit_troop(r, u);
                    break;
                case 3: change_buff(r, u); break;                       // buff gained, lost or refreshed
                default:                                                // levels up
                    ++u.level; u.power += r.range(100, 5000); u.commander.level = std::min(u.commander.level + 1, 60);
                    break;
                }
                touch(u);
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
                    if (r.below(8) == 0) set_state(r, u);
                    if (r.below(16) == 0) change_buff(r, u);
                    touch(u);
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
    const WorldMarch& march(int32_t i) const { return marches_[std::size_t(i)]; }
    const WorldTile& tile(int32_t tx, int32_t ty) const { return tiles_[std::size_t(ty * kTiles + tx)]; }
    int64_t tick_no() const { return tick_; }
    const ViewScale& scale() const { return *vs_; }

private:
    void touch(WorldUnit& u) const { ++u.ver; u.last_update = tick_; }
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
    bool empty() const { return units.empty() && removed.empty() && marches.empty() && removed_marches.empty() && tiles.empty(); }
};

class ViewTracker {
public:
    void setup(int clients) { u_.assign(std::size_t(clients), {}); m_.assign(std::size_t(clients), {}); }
    void plan(const MapWorld& w, int c, const ViewRect& r, DeltaPlan& p) {
        p.units.clear(); p.removed.clear(); p.marches.clear(); p.removed_marches.clear(); p.tiles.clear();
        ++gen_;
        auto& lu = u_[std::size_t(c)];
        w.units_in(r, tmp_);
        for (int32_t i : tmp_) {
            const WorldUnit& u = w.unit(i);
            auto [it, fresh] = lu.try_emplace(u.id);
            SentU& s = it->second;
            if (fresh || s.ver != u.ver) { s.ver = u.ver; p.units.push_back(i); }
            s.gen = gen_;
        }
        for (auto it = lu.begin(); it != lu.end();) {
            if (it->second.gen != gen_) { p.removed.push_back(it->first); it = lu.erase(it); }
            else ++it;
        }
        auto& lm = m_[std::size_t(c)];
        w.marches_in(r, tmp_);
        for (int32_t i : tmp_) {
            const WorldMarch& m = w.march(i);
            auto [it, fresh] = lm.try_emplace(m.id);
            SentM& s = it->second;
            if (fresh || s.x1 != m.x1 || s.y1 != m.y1 || s.t1 != m.t1) { s.x1 = m.x1; s.y1 = m.y1; s.t1 = m.t1; p.marches.push_back(i); }
            s.gen = gen_;
        }
        for (auto it = lm.begin(); it != lm.end();) {
            if (it->second.gen != gen_) { p.removed_marches.push_back(it->first); it = lm.erase(it); }
            else ++it;
        }
        // The client's tiles never change place; a tile is sent when its owner changed this tick.
        const TileSpan sp = tile_span(r.x0, r.y0, r.x1, r.y1);
        for (int32_t ty = sp.ty0; ty < sp.ty0 + sp.rows; ++ty)
            for (int32_t tx = sp.tx0; tx < sp.tx0 + sp.cols; ++tx)
                if (w.tile(tx, ty).changed == w.tick_no()) p.tiles.push_back(ty * kTiles + tx);
        // Deterministic order (unordered_map iteration order is the same code in
        // every runner, but sorting keeps the messages independent of it).
        std::sort(p.removed.begin(), p.removed.end());
        std::sort(p.removed_marches.begin(), p.removed_marches.end());
    }
private:
    struct SentU { uint32_t ver{}; uint64_t gen{}; };
    struct SentM { int32_t x1{}, y1{}; int64_t t1{}; uint64_t gen{}; };
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
struct HeldIndex { std::unordered_map<int32_t, int32_t> units, marches; };

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
//   void snap_recv(int c, int k, int b)        receive slot b's wire into client c's held view (k = 0)
//   auto& held(int c, int k)                   the held view (model object)
//   std::size_t delta_encode(auto fill)        fill(model&) then encode the delta
//   std::string_view delta_wire()
//   void delta_recv()                          receive the delta into the reused receive object
//   const auto& delta()                        that object
// Optional: std::string_view snap_compact(int b) (XOffset: the compact wire, for bytes only).
// Optional native delta (XOffset with XOFFSET_DELTA_SYNC_API), keys N*; every
// client c has its own server-side view and baseline:
//   void n_view(int c, const MapWorld&, const ViewRect&)   lay out the view's rect and tiles (untimed)
//   void n_plan(int c, const MapWorld&, const DeltaPlan&)  which slot each change uses (untimed)
//   void n_update(int c, const MapWorld&, const DeltaPlan&, int64_t id)   mutate the server's view
//   std::size_t n_diff(int c)                  save_delta_into against the client's baseline
//   std::size_t n_key(int c)                   key delta for the first pull (untimed here)
//   void n_key_recv(int c, int k, int b)       the client applies the key delta (b = c)
//   void n_apply(int c, int k)                 the client applies its delta (patch + verify)
//   auto& n_held(int c, int k)                 its view; must read the same as held(c, k)

struct ViewKeys {
    std::string esend, erecv, usend, urecv, uapply;
    std::string ewin, ereq, ezstd, ecomp, ecompz, udelta, umsg, uzstd, uunits, umarch, utiles;
    std::string nsend, ndiff, nrecv, ndelta;
    ViewKeys(const char* e, const char* u, const char* n) {
        const std::string E(e), U(u), N(n);
        esend = E + ".send"; erecv = E + ".recv";
        ewin = E + ".window"; ereq = E + ".requests"; ezstd = E + ".window_zstd";
        ecomp = E + ".window_compact"; ecompz = E + ".window_compact_zstd";
        usend = U + ".send"; urecv = U + ".recv"; uapply = U + ".apply";
        udelta = U + ".delta"; umsg = U + ".deliveries"; uzstd = U + ".delta_zstd";
        uunits = U + ".units"; umarch = U + ".marches"; utiles = U + ".tiles";
        nsend = N + ".send"; ndiff = N + ".diff"; nrecv = N + ".recv"; ndelta = N + ".delta";
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

// The demo cases: 0 browse (class 2), 1..3 static at peace / combat / stress (class 3).
struct ViewCases {
    static constexpr int count = 4;
    static constexpr Rate rates[count] = {Rate::Peace, Rate::Peace, Rate::Combat, Rate::Stress};
    static bool browse(int i) { return i == 0; }
    static const ViewKeys& keys(int i) {
        static const ViewKeys k[count] = {{"EB", "UB", "NB"}, {"EP", "UP", "NP"}, {"EC", "UC", "NC"}, {"ES", "US", "NS"}};
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
    void setup(uint64_t seed, const ViewScale& vs, int which) {
        browse = ViewCases::browse(which); primed = false;
        world.setup(ViewCases::seed(seed, which), vs, ViewCases::rates[which]);
        tracker.setup(vs.clients);
        index.assign(std::size_t(vs.clients), {});
    }
};

template <class Ops> void run_view_case(ViewSession& s, Ops& ops, const ViewKeys& k, int cycle, CycleStats& st, std::string* keep_view, std::string* keep_delta) {
    MapWorld& w = s.world;
    const ViewScale& vs = w.scale();
    constexpr bool native = requires { ops.n_diff(0); };
    if (s.browse) {
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
                st.bytes[k.ereq] += 1;
                st.bytes[k.ewin] += ops.snap_wire(0).size();
                st.bytes[k.ezstd] += zstd_size(ops.snap_wire(0));
                if constexpr (requires { ops.snap_compact(0); }) {
                    const std::string_view cw = ops.snap_compact(0);
                    st.bytes[k.ecomp] += cw.size();
                    st.bytes[k.ecompz] += zstd_size(cw);
                }
                if (keep_view && c == 0) *keep_view = std::string(ops.snap_wire(0));
            }
        }
        return;
    }
    if (!s.primed) {
        // Class 3, before the first timed tick: every client pulls its view once
        // (untimed: that is a class 2 pull) and the server records what it holds.
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
            if constexpr (native) {
                ops.n_view(c, w, rect);
                ops.n_plan(c, w, s.plan);
                ops.n_update(c, w, s.plan, 0);
                ops.n_key(c);
                ops.n_key_recv(c, 0, c);
                if (query_view(ops.n_held(c, 0)) != query_view(ops.held(c, 0))) throw std::runtime_error("native key delta: view differs from the snapshot");
            }
        }
        s.primed = true;
    }
    for (int r = 0; r < vs.rounds; ++r) {
        w.tick();
        const int64_t base = (int64_t(cycle) * vs.rounds + r) * (vs.clients + 1) + 1;
        for (int c = 0; c < vs.clients; ++c) {
            const ViewRect rect = w.rect(c);
            s.tracker.plan(w, c, rect, s.plan);
            if (s.plan.empty()) continue;   // nothing changed in this view: nothing sent
            std::size_t nd = 0;
            if constexpr (native) {
                ops.n_plan(c, w, s.plan);   // slot bookkeeping, untimed like the tracker
                timed(st, k.nsend.c_str(), [&] { ops.n_update(c, w, s.plan, base + c); });
                timed(st, k.ndiff.c_str(), [&] { nd = ops.n_diff(c); });
            }
            std::size_t n = 0;
            timed(st, k.usend.c_str(), [&] { n = ops.delta_encode([&](auto& d) { fill_delta(d, w, rect, s.plan, base + c); }); });
            if (keep_delta && c == 0) *keep_delta = std::string(ops.delta_wire());
            timed(st, k.urecv.c_str(), [&] { ops.delta_recv(); });
            HeldIndex& at = s.index[std::size_t(c)];
            timed(st, k.uapply.c_str(), [&] { apply_delta(ops.held(c, 0), at, ops.delta()); });
            const uint64_t q = query_view(ops.held(c, 0));
            st.report_sum += q;
            if constexpr (native) {
                timed(st, k.nrecv.c_str(), [&] { ops.n_apply(c, 0); });
                if (query_view(ops.n_held(c, 0)) != q) throw std::runtime_error("native delta: view differs from the application-level one");
                st.bytes[k.ndelta] += nd;
            }
            st.bytes[k.udelta] += n;
            st.bytes[k.uzstd] += zstd_size(ops.delta_wire());
            st.bytes[k.umsg] += 1;
            st.bytes[k.uunits] += s.plan.units.size() + s.plan.removed.size();
            st.bytes[k.umarch] += s.plan.marches.size() + s.plan.removed_marches.size();
            st.bytes[k.utiles] += s.plan.tiles.size();
        }
    }
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
