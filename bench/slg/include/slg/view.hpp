// SLG benchmark, demo case set (SLG_CASES=demo): scenario E, viewport sync.
//
// A scene server keeps a map of units that change every tick (move, lose or
// regain hp, switch state). Clients look at a small part of the map; each
// viewport pans a little between requests and now and then jumps elsewhere.
// For every request the server puts all units inside the viewport into one
// ViewSnapshot message and sends it; the client receives it (each format's
// normal checked receive) and reads every unit, as a renderer would.
//
// The world itself is plain game-server state (not a message), identical in
// every runner, so ticking it and finding the units in a viewport are not
// timed. Timed: E.send (fill the message from the world and encode it) and
// E.recv (receive and read every unit).
#pragma once
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "generic.hpp"

namespace slg {

struct ViewScale {
    const char* name;
    int units;          // units on the map
    int clients;        // clients looking at the map
    int rounds;         // per cycle: world tick, then one request per client
};

inline const ViewScale* find_view_scale(std::string_view n) {
    static const ViewScale scales[] = {
        {"S", 2000, 64, 4},
        {"M", 10000, 256, 4},
        {"L", 40000, 1024, 4},
    };
    for (const auto& s : scales) if (n == s.name) return &s;
    return nullptr;
}

constexpr int32_t kMapSize = 4096;      // map [0, 4096) x [0, 4096)
constexpr int32_t kViewW = 512;         // viewport: 1/8 x 1/10.7 of the map side
constexpr int32_t kViewH = 384;
constexpr int32_t kPanStep = 48;        // pan per request, each axis, at most
constexpr int32_t kJumpOneIn = 16;      // a request jumps elsewhere with p = 1/16
constexpr int32_t kCell = 64;           // spatial grid cell
constexpr int32_t kGrid = kMapSize / kCell;

struct WorldUnit {
    int32_t id{}; int32_t owner{}; int32_t x{}; int32_t y{};
    int32_t vx{}; int32_t vy{}; int32_t hp{}; int32_t state{};
    std::string name;
};

struct Viewport { int32_t x0{}; int32_t y0{}; };

class ViewWorld {
public:
    void setup(uint64_t seed, const ViewScale& vs) {
        vs_ = &vs;
        Rng rng(mix_seed(seed, 0x7e3));
        units_.clear();
        units_.reserve(std::size_t(vs.units));
        for (int i = 0; i < vs.units; ++i) {
            WorldUnit u;
            u.id = i + 1; u.owner = 1 + rng.below(5000);
            u.x = rng.below(kMapSize); u.y = rng.below(kMapSize);
            u.vx = rng.range(-8, 8); u.vy = rng.range(-8, 8);
            u.hp = rng.range(100, 1000); u.state = rng.below(4);
            u.name = std::string(rng.below(2) ? "unit_" : "squad_") + std::to_string(u.id);
            units_.push_back(std::move(u));
        }
        views_.assign(std::size_t(vs.clients), {});
        for (auto& v : views_) jump(rng, v);
        tick_rng_ = Rng(mix_seed(seed, 0x7e4));
        view_rng_ = Rng(mix_seed(seed, 0x7e5));
        rebuild_grid();
    }

    // One server tick: marching units (state 0, about a quarter) move; 1 in 16 units
// loses or regains hp, 1 in 16 changes state.
    void tick() {
        ++tick_;
        for (auto& u : units_) {
            const int32_t r = tick_rng_.below(16);
            if (r == 0) u.hp = u.hp > 10 ? u.hp - tick_rng_.range(1, 10) : u.hp + 500;
            else if (r == 1) u.state = tick_rng_.below(4);
            if (u.state != 0) continue;  // state 0 = marching: about a quarter of the units move
            u.x += u.vx; u.y += u.vy;
            if (u.x < 0 || u.x >= kMapSize) { u.vx = -u.vx; u.x = u.x < 0 ? -u.x : 2 * kMapSize - 1 - u.x; }
            if (u.y < 0 || u.y >= kMapSize) { u.vy = -u.vy; u.y = u.y < 0 ? -u.y : 2 * kMapSize - 1 - u.y; }
        }
        rebuild_grid();
    }

    // Client c pans its viewport (or jumps) and asks for it; returns the units inside.
    const std::vector<int32_t>& request(int c) {
        Viewport& v = views_[std::size_t(c)];
        if (view_rng_.below(kJumpOneIn) == 0) jump(view_rng_, v);
        else {
            v.x0 = clamp(v.x0 + view_rng_.range(-kPanStep, kPanStep), 0, kMapSize - kViewW);
            v.y0 = clamp(v.y0 + view_rng_.range(-kPanStep, kPanStep), 0, kMapSize - kViewH);
        }
        cur_ = v;
        sel_.clear();
        for (int32_t gy = v.y0 / kCell; gy <= (v.y0 + kViewH - 1) / kCell; ++gy)
            for (int32_t gx = v.x0 / kCell; gx <= (v.x0 + kViewW - 1) / kCell; ++gx) {
                const auto& cell = grid_[std::size_t(gy * kGrid + gx)];
                for (int32_t i : cell) {
                    const auto& u = units_[std::size_t(i)];
                    if (u.x >= v.x0 && u.x < v.x0 + kViewW && u.y >= v.y0 && u.y < v.y0 + kViewH) sel_.push_back(i);
                }
            }
        return sel_;
    }

    const WorldUnit& unit(int32_t i) const { return units_[std::size_t(i)]; }
    const Viewport& current() const { return cur_; }
    int64_t tick_no() const { return tick_; }
    const ViewScale& scale() const { return *vs_; }

private:
    static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
    static void jump(Rng& rng, Viewport& v) { v.x0 = rng.below(kMapSize - kViewW + 1); v.y0 = rng.below(kMapSize - kViewH + 1); }
    void rebuild_grid() {
        grid_.resize(std::size_t(kGrid * kGrid));
        for (auto& g : grid_) g.clear();
        for (std::size_t i = 0; i < units_.size(); ++i)
            grid_[std::size_t((units_[i].y / kCell) * kGrid + units_[i].x / kCell)].push_back(int32_t(i));
    }

    const ViewScale* vs_ = nullptr;
    std::vector<WorldUnit> units_;
    std::vector<Viewport> views_;
    std::vector<std::vector<int32_t>> grid_;
    std::vector<int32_t> sel_;
    Viewport cur_;
    int64_t tick_ = 0;
    Rng tick_rng_{0}, view_rng_{0};
};

// Fill a ViewSnapshot of any model (std, XOffset, cista) from the world.
template <class V> void fill_view(V& v, const ViewWorld& w, const std::vector<int32_t>& sel, int64_t request_id) {
    const Viewport& r = w.current();
    v.request_id = request_id; v.tick = w.tick_no();
    v.x0 = r.x0; v.y0 = r.y0; v.x1 = r.x0 + kViewW; v.y1 = r.y0 + kViewH;
    v.units.clear();
    v.units.reserve(sel.size());
    for (int32_t i : sel) {
        const WorldUnit& s = w.unit(i);
        auto& d = v.units.emplace_back();
        d.id = s.id; d.owner = s.owner; d.x = s.x; d.y = s.y; d.hp = s.hp; d.state = s.state;
        d.name = std::string_view(s.name);
    }
}

// The client reads every unit (what a renderer touches); the result must match across formats.
template <class V> uint64_t query_view(const V& v) {
    uint64_t h = uint64_t(v.request_id) * 31u + uint64_t(v.tick) + uint64_t(v.x0) * 7u + uint64_t(v.y1);
    for (const auto& u : v.units) {
        h += uint64_t(u.hp) * 3u + uint64_t(u.x ^ u.y) + uint64_t(u.state) + uint64_t(u.id) * 1315423911u;
        h += uint64_t(u.owner) + sv(u.name).size();
    }
    return h;
}

// ---- scenario U: delta sync ------------------------------------------------
//
// Each client already holds the units of its viewport. After every server tick
// the server sends that client only what changed since its last sync: units
// that entered the viewport or changed (whole record), and the ids of units that
// left. The client applies the delta to the view it holds. This is the usual
// application-level delta: none of the formats has a built-in one.
// Server bookkeeping (what each client was last sent) is the same plain code in
// every runner and is not timed. Timed: U.send (fill the delta message, encode),
// U.recv (receive with the format's checks) and U.apply (apply to the held view;
// the same template code for every format, on that format's own view object).

struct DeltaPlan { std::vector<int32_t> upserts; std::vector<int32_t> removed; };

class DeltaTracker {
public:
    void setup(int clients) { sent_.assign(std::size_t(clients), {}); }
    // sel: world indices in client c's viewport now. Fills plan (world indices to send,
    // ids that left) and records what the client will hold.
    void plan(const ViewWorld& w, int c, const std::vector<int32_t>& sel, DeltaPlan& p) {
        auto& last = sent_[std::size_t(c)];
        p.upserts.clear(); p.removed.clear();
        ++gen_;
        for (int32_t i : sel) {
            const WorldUnit& u = w.unit(i);
            auto [it, fresh] = last.try_emplace(u.id);
            Sent& s = it->second;
            if (fresh || s.x != u.x || s.y != u.y || s.hp != u.hp || s.state != u.state) {
                s.x = u.x; s.y = u.y; s.hp = u.hp; s.state = u.state;
                p.upserts.push_back(i);
            }
            s.gen = gen_;
        }
        for (auto it = last.begin(); it != last.end();) {
            if (it->second.gen != gen_) { p.removed.push_back(it->first); it = last.erase(it); }
            else ++it;
        }
    }
private:
    struct Sent { int32_t x{}, y{}, hp{}, state{}; uint64_t gen{}; };
    std::vector<std::unordered_map<int32_t, Sent>> sent_;
    uint64_t gen_ = 0;
};

template <class D> void fill_delta(D& d, const ViewWorld& w, const DeltaPlan& p, int64_t request_id) {
    const Viewport& r = w.current();
    d.request_id = request_id; d.tick = w.tick_no();
    d.x0 = r.x0; d.y0 = r.y0; d.x1 = r.x0 + kViewW; d.y1 = r.y0 + kViewH;
    d.upserts.clear();
    d.upserts.reserve(p.upserts.size());
    for (int32_t i : p.upserts) {
        const WorldUnit& s = w.unit(i);
        auto& u = d.upserts.emplace_back();
        u.id = s.id; u.owner = s.owner; u.x = s.x; u.y = s.y; u.hp = s.hp; u.state = s.state;
        u.name = std::string_view(s.name);
    }
    d.removed.clear();
    d.removed.reserve(p.removed.size());
    for (int32_t id : p.removed) d.removed.emplace_back(id);
}

// Client side: apply a received delta to the view it holds (any model's
// ViewSnapshot) through an id -> position index. A unit that left is replaced by
// the last unit (order is not kept); a changed unit is updated in place.
template <class V, class D> void apply_delta(V& v, std::unordered_map<int32_t, int32_t>& at, const D& d) {
    v.request_id = d.request_id; v.tick = d.tick; v.x0 = d.x0; v.y0 = d.y0; v.x1 = d.x1; v.y1 = d.y1;
    for (const auto& id : d.removed) {
        const auto it = at.find(id);
        if (it == at.end()) continue;
        const int32_t pos = it->second, last = int32_t(v.units.size()) - 1;
        if (pos != last) {
            auto& dst = v.units[std::size_t(pos)];
            const auto& src = v.units[std::size_t(last)];
            dst.id = src.id; dst.owner = src.owner; dst.x = src.x; dst.y = src.y; dst.hp = src.hp; dst.state = src.state;
            if (sv(dst.name) != sv(src.name)) dst.name = std::string(sv(src.name));
            at[dst.id] = pos;
        }
        v.units.pop_back();
        at.erase(it);
    }
    for (const auto& u : d.upserts) {
        const auto it = at.find(u.id);
        if (it != at.end()) {
            auto& dst = v.units[std::size_t(it->second)];
            dst.owner = u.owner; dst.x = u.x; dst.y = u.y; dst.hp = u.hp; dst.state = u.state;
            if (sv(dst.name) != sv(u.name)) dst.name = sv(u.name);
        } else {
            at.emplace(u.id, int32_t(v.units.size()));
            auto& dst = v.units.emplace_back();
            dst.id = u.id; dst.owner = u.owner; dst.x = u.x; dst.y = u.y; dst.hp = u.hp; dst.state = u.state;
            dst.name = sv(u.name);
        }
    }
}

// Order-independent read of a held view (must match across formats).
template <class V> uint64_t query_held(const V& v) {
    uint64_t h = uint64_t(v.request_id) * 31u + uint64_t(v.tick);
    for (const auto& u : v.units)
        h += uint64_t(u.hp) * 3u + uint64_t(u.x ^ u.y) + uint64_t(u.state) + uint64_t(u.id) * 1315423911u + uint64_t(u.owner) + sv(u.name).size();
    return h;
}

// Demo case set, scene <-> storage: players saved to the storage process this cycle
// (log-outs), then loaded back into the scene (log-ins).
inline std::vector<int> pick_storage(uint64_t seed, int cycle, int players, int n) {
    Rng rng(mix_seed(seed, 0x5703 + uint64_t(cycle)));
    std::vector<int> out;
    for (int i = 0; i < n; ++i) out.push_back(rng.below(players));
    return out;
}

// Demo case set: A unchanged, B = migration between scene servers, C unchanged,
// E = viewport sync (replaces D). Needs SLG_REUSE=1.
inline bool demo_cases() {
    static const bool on = [] { const char* e = std::getenv("SLG_CASES"); return e && std::string_view(e) == "demo"; }();
    return on;
}

} // namespace slg
