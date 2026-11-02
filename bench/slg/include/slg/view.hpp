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

    // One server tick: every unit moves; some lose or regain hp or change state.
    void tick() {
        ++tick_;
        for (auto& u : units_) {
            u.x += u.vx; u.y += u.vy;
            if (u.x < 0 || u.x >= kMapSize) { u.vx = -u.vx; u.x = u.x < 0 ? -u.x : 2 * kMapSize - 1 - u.x; }
            if (u.y < 0 || u.y >= kMapSize) { u.vy = -u.vy; u.y = u.y < 0 ? -u.y : 2 * kMapSize - 1 - u.y; }
            const int32_t r = tick_rng_.below(16);
            if (r == 0) u.hp = u.hp > 10 ? u.hp - tick_rng_.range(1, 10) : u.hp + 500;
            else if (r == 1) u.state = tick_rng_.below(4);
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
