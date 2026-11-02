// SLG benchmark, demo case set (SLG_CASES=demo): server -> client map data.
//
// The map is split into blocks of one screen (512 x 384). A client subscribes
// to the 3 x 3 block window around the centre of its view; it pulls the whole
// window (one snapshot message per block) only when its view centre crosses
// into another block, and in between it gets every change of its blocks as
// per-block delta messages. A block's snapshot and its delta are each encoded
// once per tick and fanned out to every client that wants them (the usual SLG
// map broadcast: views hang on blocks).
//
// Block content: the units standing in it (cities, stationed troops, resource
// points) and the marches whose line crosses it. A march is a line record
// (start, end, start tick, arrival tick); the client interpolates, so a march
// changes only when it starts, is recalled or arrives.
//
// Three change rates (one world each, see Rate):
//   peace  - a few unit events per tick (owner, state, hp) and march turnover;
//   combat - peace plus two battle zones where every engaged unit changes every
//            tick (a 1 s battle round);
//   stress - the earlier E world: a quarter of all units move every tick, 1/16
//            change hp, 1/16 change state (upper bound).
//
// The world, the clients' movement and the server's bookkeeping of what each
// block's subscribers hold are plain game-server code, the same in every runner,
// and not timed. Timed, per format (run_blocks, keys <E>.* and <U>.*):
//   E.send   fill one block snapshot from the world and encode it (once per block
//            per tick, however many clients pull it)
//   E.recv   one client receives one block snapshot (the format's checked
//            receive), reads every unit and march, and keeps it as its held view
//   E.index  the client builds its id -> position index of that view (the same
//            code for every format; needed to apply deltas by id)
//   U.send   fill one block delta from the bookkeeping and encode it (once)
//   U.recv   one subscriber receives the delta
//   U.apply  that subscriber applies it to its held view of the block
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
constexpr int32_t kBlockW = 512;        // one block = one screen
constexpr int32_t kBlockH = 384;
constexpr int32_t kBX = (kMapSize + kBlockW - 1) / kBlockW;   // 8
constexpr int32_t kBY = (kMapSize + kBlockH - 1) / kBlockH;   // 11
constexpr int32_t kBlocks = kBX * kBY;                         // 88
constexpr int32_t kPanStep = 48;        // view centre pan per tick, each axis, at most
constexpr int32_t kJumpOneIn = 16;      // the view jumps elsewhere with p = 1/16 per tick
constexpr int32_t kZoneR = 384;         // combat zone radius

enum class Rate { Peace, Combat, Stress };

inline int32_t block_of(int32_t x, int32_t y) { return (y / kBlockH) * kBX + x / kBlockW; }

struct WorldUnit {
    int32_t id{}; int32_t owner{}; int32_t x{}; int32_t y{};
    int32_t vx{}; int32_t vy{}; int32_t hp{}; int32_t state{};
    std::string name;
};

struct WorldMarch {
    int32_t id{}; int32_t owner{}; int32_t x0{}; int32_t y0{}; int32_t x1{}; int32_t y1{};
    int64_t t0{}; int64_t t1{};
    std::vector<int32_t> blocks;   // blocks its line crosses
};

struct ViewClient { int32_t cx{}; int32_t cy{}; int32_t block = -1; int32_t n = 0; int32_t win[9]{}; };

class BlockWorld {
public:
    void setup(uint64_t seed, const ViewScale& vs, Rate rate) {
        vs_ = &vs; rate_ = rate;
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
                if (k == 0) u.hp = u.hp > 10 ? u.hp - r.range(1, 10) : u.hp + 500;
                else if (k == 1) u.state = r.below(4);
                if (u.state != 0) continue;   // state 0 = moving: about a quarter of the units
                u.x += u.vx; u.y += u.vy;
                if (u.x < 0 || u.x >= kMapSize) { u.vx = -u.vx; u.x = u.x < 0 ? -u.x : 2 * kMapSize - 1 - u.x; }
                if (u.y < 0 || u.y >= kMapSize) { u.vy = -u.vy; u.y = u.y < 0 ? -u.y : 2 * kMapSize - 1 - u.y; }
            }
        } else {
            // A few events per tick across the map: 1 unit in 500 changes owner, state or hp.
            const int events = std::max(1, int(units_.size()) / 500);
            for (int e = 0; e < events; ++e) {
                auto& u = units_[std::size_t(r.below(int32_t(units_.size())))];
                switch (r.below(3)) {
                case 0: u.owner = 1 + r.below(5000); break;
                case 1: u.state = r.below(4); break;
                default: u.hp = u.hp > 50 ? u.hp - r.range(1, 50) : u.hp + 500; break;
                }
            }
            if (rate_ == Rate::Combat) {
                // One battle round per tick: every engaged unit loses (or regains) hp, 1 in 8 changes state.
                for (int32_t i : engaged_) {
                    auto& u = units_[std::size_t(i)];
                    u.hp = u.hp > 30 ? u.hp - r.range(1, 30) : u.hp + 800;
                    if (r.below(8) == 0) u.state = r.below(4);
                }
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
                m.blocks = blocks_of_line(m.x0, m.y0, m.x1, m.y1);
            }
        const int born = vs_->marches / duration();
        for (int i = 0; i < born; ++i) add_march(r);
        rebuild();
    }

    // Client c moves its view centre; true when it entered another block (it then
    // pulls window(c), the 3 x 3 blocks around it).
    bool move(int c) {
        ViewClient& v = clients_[std::size_t(c)];
        if (view_rng_.below(kJumpOneIn) == 0) jump(view_rng_, v);
        else {
            v.cx = clamp(v.cx + view_rng_.range(-kPanStep, kPanStep), 0, kMapSize - 1);
            v.cy = clamp(v.cy + view_rng_.range(-kPanStep, kPanStep), 0, kMapSize - 1);
        }
        const int32_t b = block_of(v.cx, v.cy);
        if (b == v.block) return false;
        v.block = b;
        v.n = 0;
        const int32_t bx = b % kBX, by = b / kBX;
        for (int32_t y = by - 1; y <= by + 1; ++y)
            for (int32_t x = bx - 1; x <= bx + 1; ++x)
                if (x >= 0 && x < kBX && y >= 0 && y < kBY) v.win[v.n++] = y * kBX + x;
        return true;
    }

    const ViewClient& client(int c) const { return clients_[std::size_t(c)]; }
    const WorldUnit& unit(int32_t i) const { return units_[std::size_t(i)]; }
    const WorldMarch& march(int32_t i) const { return marches_[std::size_t(i)]; }
    const std::vector<int32_t>& block_units(int32_t b) const { return bunits_[std::size_t(b)]; }
    const std::vector<int32_t>& block_marches(int32_t b) const { return bmarches_[std::size_t(b)]; }
    int64_t tick_no() const { return tick_; }
    const ViewScale& scale() const { return *vs_; }

private:
    static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
    static void jump(Rng& rng, ViewClient& v) { v.cx = rng.below(kMapSize); v.cy = rng.below(kMapSize); }
    bool in_zone(int32_t x, int32_t y) const {
        for (const auto& z : zones_) {
            const int64_t dx = x - z[0], dy = y - z[1];
            if (dx * dx + dy * dy <= int64_t(kZoneR) * kZoneR) return true;
        }
        return false;
    }
    int duration() const { return rate_ == Rate::Peace ? 30 : rate_ == Rate::Combat ? 15 : 10; }
    // Blocks a line crosses: sampled every 64 units of length (blocks are at least 384).
    static std::vector<int32_t> blocks_of_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
        std::vector<int32_t> out;
        const int32_t dx = x1 - x0, dy = y1 - y0;
        const int32_t steps = std::max(1, (std::max(std::abs(dx), std::abs(dy)) + 63) / 64);
        for (int32_t s = 0; s <= steps; ++s) {
            const int32_t b = block_of(x0 + int32_t(int64_t(dx) * s / steps), y0 + int32_t(int64_t(dy) * s / steps));
            if (std::find(out.begin(), out.end(), b) == out.end()) out.push_back(b);
        }
        return out;
    }
    void add_march(Rng& r) {
        WorldMarch m;
        m.id = next_march_++; m.owner = 1 + r.below(5000);
        m.x0 = r.below(kMapSize); m.y0 = r.below(kMapSize);
        m.x1 = clamp(m.x0 + r.range(-1500, 1500), 0, kMapSize - 1);
        m.y1 = clamp(m.y0 + r.range(-1500, 1500), 0, kMapSize - 1);
        const int d = duration();
        m.t0 = tick_; m.t1 = tick_ + r.range(d / 2, d + d / 2);
        m.blocks = blocks_of_line(m.x0, m.y0, m.x1, m.y1);
        marches_.push_back(std::move(m));
    }
    void rebuild() {
        bunits_.resize(std::size_t(kBlocks));
        bmarches_.resize(std::size_t(kBlocks));
        for (auto& b : bunits_) b.clear();
        for (auto& b : bmarches_) b.clear();
        for (std::size_t i = 0; i < units_.size(); ++i) bunits_[std::size_t(block_of(units_[i].x, units_[i].y))].push_back(int32_t(i));
        for (std::size_t i = 0; i < marches_.size(); ++i)
            for (int32_t b : marches_[i].blocks) bmarches_[std::size_t(b)].push_back(int32_t(i));
    }

    const ViewScale* vs_ = nullptr;
    Rate rate_ = Rate::Stress;
    std::vector<WorldUnit> units_;
    std::vector<WorldMarch> marches_;
    std::vector<int32_t> engaged_;
    int32_t zones_[2][2]{};
    int32_t next_march_ = 1;
    std::vector<ViewClient> clients_;
    std::vector<std::vector<int32_t>> bunits_, bmarches_;
    int64_t tick_ = 0;
    Rng tick_rng_{0}, view_rng_{0};
};

template <class M, class W> void copy_vmarch(M& d, const W& s) {
    d.id = s.id; d.owner = s.owner; d.x0 = s.x0; d.y0 = s.y0; d.x1 = s.x1; d.y1 = s.y1; d.t0 = s.t0; d.t1 = s.t1;
}

inline void block_rect(int32_t b, int32_t& x0, int32_t& y0, int32_t& x1, int32_t& y1) {
    x0 = (b % kBX) * kBlockW; y0 = (b / kBX) * kBlockH;
    x1 = std::min(x0 + kBlockW, kMapSize); y1 = std::min(y0 + kBlockH, kMapSize);
}

// Fill a block snapshot of any model (std, XOffset, cista) from the world.
// request_id / tick: those of the block's last delta (so a snapshot and the
// deltas before it leave the client with the same view).
template <class V> void fill_view(V& v, const BlockWorld& w, int32_t b, int64_t request_id, int64_t tick) {
    v.request_id = request_id; v.tick = tick;
    block_rect(b, v.x0, v.y0, v.x1, v.y1);
    const auto& us = w.block_units(b);
    v.units.clear();
    v.units.reserve(us.size());
    for (int32_t i : us) {
        const WorldUnit& s = w.unit(i);
        auto& d = v.units.emplace_back();
        d.id = s.id; d.owner = s.owner; d.x = s.x; d.y = s.y; d.hp = s.hp; d.state = s.state;
        d.name = std::string_view(s.name);
    }
    const auto& ms = w.block_marches(b);
    v.marches.clear();
    v.marches.reserve(ms.size());
    for (int32_t i : ms) copy_vmarch(v.marches.emplace_back(), w.march(i));
}

// The client reads every unit and march of a block it holds (what a renderer
// touches); order-independent, and the result must match across formats. Zeroed
// (free) slots add nothing.
template <class V> uint64_t query_view(const V& v) {
    uint64_t h = uint64_t(v.request_id) * 31u + uint64_t(v.tick) + uint64_t(v.x0) * 7u + uint64_t(v.y1);
    for (const auto& u : v.units) {
        h += uint64_t(u.hp) * 3u + uint64_t(u.x ^ u.y) + uint64_t(u.state) + uint64_t(u.id) * 1315423911u;
        h += uint64_t(u.owner) + sv(u.name).size();
    }
    for (const auto& m : v.marches)
        h += uint64_t(m.id) * 2654435761u + uint64_t(m.x0 ^ m.y1) + uint64_t(m.x1 ^ m.y0) + uint64_t(m.t0) * 5u + uint64_t(m.t1) + uint64_t(m.owner);
    return h;
}

// ---- deltas -----------------------------------------------------------------
//
// Server bookkeeping (not timed): for every block, what its subscribers hold.
// Every block is planned every tick, so a block's record is always the state a
// client gets from that tick's snapshot.

struct DeltaPlan {
    std::vector<int32_t> units;            // world unit indices: entered or changed
    std::vector<int32_t> removed;          // unit ids that left
    std::vector<int32_t> marches;          // world march indices: new or changed
    std::vector<int32_t> removed_marches;  // march ids that left
    bool empty() const { return units.empty() && removed.empty() && marches.empty() && removed_marches.empty(); }
};

class BlockTracker {
public:
    void setup() { u_.assign(std::size_t(kBlocks), {}); m_.assign(std::size_t(kBlocks), {}); }
    void plan(const BlockWorld& w, int32_t b, DeltaPlan& p) {
        p.units.clear(); p.removed.clear(); p.marches.clear(); p.removed_marches.clear();
        ++gen_;
        auto& lu = u_[std::size_t(b)];
        for (int32_t i : w.block_units(b)) {
            const WorldUnit& u = w.unit(i);
            auto [it, fresh] = lu.try_emplace(u.id);
            SentU& s = it->second;
            if (fresh || s.x != u.x || s.y != u.y || s.hp != u.hp || s.state != u.state || s.owner != u.owner) {
                s.x = u.x; s.y = u.y; s.hp = u.hp; s.state = u.state; s.owner = u.owner;
                p.units.push_back(i);
            }
            s.gen = gen_;
        }
        for (auto it = lu.begin(); it != lu.end();) {
            if (it->second.gen != gen_) { p.removed.push_back(it->first); it = lu.erase(it); }
            else ++it;
        }
        auto& lm = m_[std::size_t(b)];
        for (int32_t i : w.block_marches(b)) {
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
        // Deterministic order (unordered_map iteration order is the same code in
        // every runner, but sorting keeps the messages independent of it).
        std::sort(p.removed.begin(), p.removed.end());
        std::sort(p.removed_marches.begin(), p.removed_marches.end());
    }
private:
    struct SentU { int32_t x{}, y{}, hp{}, state{}, owner{}; uint64_t gen{}; };
    struct SentM { int32_t x1{}, y1{}; int64_t t1{}; uint64_t gen{}; };
    std::vector<std::unordered_map<int32_t, SentU>> u_;
    std::vector<std::unordered_map<int32_t, SentM>> m_;
    uint64_t gen_ = 0;
};

template <class D> void fill_delta(D& d, const BlockWorld& w, int32_t b, const DeltaPlan& p, int64_t request_id) {
    d.request_id = request_id; d.tick = w.tick_no();
    block_rect(b, d.x0, d.y0, d.x1, d.y1);
    d.upserts.clear();
    d.upserts.reserve(p.units.size());
    for (int32_t i : p.units) {
        const WorldUnit& s = w.unit(i);
        auto& u = d.upserts.emplace_back();
        u.id = s.id; u.owner = s.owner; u.x = s.x; u.y = s.y; u.hp = s.hp; u.state = s.state;
        u.name = std::string_view(s.name);
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
}

// Client side: id -> position of a held block view (units and marches).
struct HeldIndex { std::unordered_map<int32_t, int32_t> units, marches; };

template <class V> void build_index(const V& v, HeldIndex& at) {
    at.units.clear(); at.marches.clear();
    at.units.reserve(v.units.size()); at.marches.reserve(v.marches.size());
    for (std::size_t i = 0; i < v.units.size(); ++i) at.units.emplace(v.units[i].id, int32_t(i));
    for (std::size_t i = 0; i < v.marches.size(); ++i) at.marches.emplace(v.marches[i].id, int32_t(i));
}

// Apply a received delta to a held block view (any model) through its index. A
// record that left is replaced by the last one (order is not kept); a changed
// one is updated in place; a new one is appended.
template <class V, class D> void apply_delta(V& v, HeldIndex& at, const D& d) {
    v.request_id = d.request_id; v.tick = d.tick; v.x0 = d.x0; v.y0 = d.y0; v.x1 = d.x1; v.y1 = d.y1;
    for (const auto& id : d.removed) {
        const auto it = at.units.find(id);
        if (it == at.units.end()) continue;
        const int32_t pos = it->second, last = int32_t(v.units.size()) - 1;
        if (pos != last) {
            auto& dst = v.units[std::size_t(pos)];
            const auto& src = v.units[std::size_t(last)];
            dst.id = src.id; dst.owner = src.owner; dst.x = src.x; dst.y = src.y; dst.hp = src.hp; dst.state = src.state;
            if (sv(dst.name) != sv(src.name)) dst.name = std::string(sv(src.name));
            at.units[dst.id] = pos;
        }
        v.units.pop_back();
        at.units.erase(it);
    }
    for (const auto& u : d.upserts) {
        const auto it = at.units.find(u.id);
        if (it != at.units.end()) {
            auto& dst = v.units[std::size_t(it->second)];
            dst.owner = u.owner; dst.x = u.x; dst.y = u.y; dst.hp = u.hp; dst.state = u.state;
            if (sv(dst.name) != sv(u.name)) dst.name = sv(u.name);
        } else {
            at.units.emplace(u.id, int32_t(v.units.size()));
            auto& dst = v.units.emplace_back();
            dst.id = u.id; dst.owner = u.owner; dst.x = u.x; dst.y = u.y; dst.hp = u.hp; dst.state = u.state;
            dst.name = sv(u.name);
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
}

// ---- the driver ---------------------------------------------------------------
//
// Ops (one per world, provided by each runner):
//   void setup(int clients)                    held views: clients x 9 slots
//   std::size_t snap_encode(int b, auto fill)  fill(model&) then encode into block b's slot
//   std::string_view snap_wire(int b)
//   void snap_recv(int c, int k, int b)        receive block b's wire into held slot (c, k)
//   auto& held(int c, int k)                   the held view (model object)
//   std::size_t delta_encode(auto fill)        fill(model&) then encode the delta
//   std::string_view delta_wire()
//   void delta_recv()                          receive the delta into the reused receive object
//   const auto& delta()                        that object
// Optional: std::string_view snap_compact(int b) (XOffset: the compact wire, for bytes only).
// Optional native delta (XOffset with XOFFSET_DELTA_SYNC_API), keys N* and K*:
//   void n_plan(int b, const BlockWorld&, const DeltaPlan&)   which slot each change uses (untimed)
//   void n_update(int b, const BlockWorld&, const DeltaPlan&, int64_t id)   mutate the server's block buffer
//   std::size_t n_diff(int b)                  save_delta_into against the block's shared baseline
//   std::size_t n_key(int b)                   key delta of the block for a new subscriber
//   void n_apply(int c, int k)                 subscriber applies the block's delta (patch + verify)
//   void n_key_recv(int c, int k, int b)       new subscriber applies the key delta
//   auto& n_held(int c, int k)                 its view; must read the same as held(c, k)

struct BlockKeys {
    std::string esend, erecv, eindex, usend, urecv, uapply;
    std::string ewin, ereq, eenc, ezstd, ecomp, ecompz, udelta, umsg, uenc, uzstd, uunits, umarch;
    std::string nsend, ndiff, nrecv, ndelta, kenc, krecv, kbytes;
    BlockKeys(const char* e, const char* u, const char* n, const char* kk) {
        const std::string E(e), U(u), N(n), K(kk);
        nsend = N + ".send"; ndiff = N + ".diff"; nrecv = N + ".recv"; ndelta = N + ".delta";
        kenc = K + ".send"; krecv = K + ".recv"; kbytes = K + ".window";
        esend = E + ".send"; erecv = E + ".recv"; eindex = E + ".index";
        usend = U + ".send"; urecv = U + ".recv"; uapply = U + ".apply";
        ewin = E + ".window"; ereq = E + ".requests"; eenc = E + ".encoded"; ezstd = E + ".window_zstd";
        ecomp = E + ".window_compact"; ecompz = E + ".window_compact_zstd";
        udelta = U + ".delta"; umsg = U + ".deliveries"; uenc = U + ".encoded"; uzstd = U + ".delta_zstd";
        uunits = U + ".units"; umarch = U + ".marches";
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

struct BlockSession {
    BlockWorld world;
    BlockTracker tracker;
    DeltaPlan plan;
    std::vector<std::vector<std::pair<int32_t, int32_t>>> subs;   // block -> (client, slot)
    std::vector<HeldIndex> index;                                  // client * 9 + slot
    std::vector<int64_t> snap_tick;                                // block -> tick its snapshot was encoded
    std::vector<std::size_t> snap_z, snap_c, snap_cz;              // zstd / compact sizes of that snapshot
    std::vector<int64_t> last_id, last_tick;                       // block -> id and tick of its last delta
    std::vector<std::size_t> nkey;                                 // native: key delta size, this tick
    void setup(uint64_t seed, const ViewScale& vs, Rate rate) {
        world.setup(seed, vs, rate);
        tracker.setup();
        subs.assign(std::size_t(kBlocks), {});
        index.assign(std::size_t(vs.clients) * 9, {});
        snap_tick.assign(std::size_t(kBlocks), -1);
        snap_z.assign(std::size_t(kBlocks), 0); snap_c.assign(std::size_t(kBlocks), 0); snap_cz.assign(std::size_t(kBlocks), 0);
        last_id.assign(std::size_t(kBlocks), 0); last_tick.assign(std::size_t(kBlocks), 0);
        nkey.assign(std::size_t(kBlocks), 0);
    }
};

template <class Ops> void run_blocks(BlockSession& s, Ops& ops, const BlockKeys& k, int cycle, CycleStats& st, std::string* keep_view, std::string* keep_delta) {
    BlockWorld& w = s.world;
    const ViewScale& vs = w.scale();
    constexpr bool native = requires { ops.n_diff(0); };
    for (int r = 0; r < vs.rounds; ++r) {
        w.tick();
        const int64_t base = (int64_t(cycle) * vs.rounds + r) * (kBlocks + 1) * 2;
        // U: every block's changes since the last tick, encoded once, to every subscriber.
        for (int32_t b = 0; b < kBlocks; ++b) {
            s.tracker.plan(w, b, s.plan);
            if (s.plan.empty()) continue;
            s.last_id[std::size_t(b)] = base + b; s.last_tick[std::size_t(b)] = w.tick_no();
            std::size_t nd = 0;
            if constexpr (native) {
                // The server's block buffer follows every change (it is the state new
                // subscribers' key deltas come from), subscribed or not.
                ops.n_plan(b, w, s.plan);   // slot bookkeeping, untimed like the tracker
                timed(st, k.nsend.c_str(), [&] { ops.n_update(b, w, s.plan, base + b); });
                timed(st, k.ndiff.c_str(), [&] { nd = ops.n_diff(b); });
            }
            const auto& subs = s.subs[std::size_t(b)];
            if (subs.empty()) continue;
            std::size_t n = 0;
            timed(st, k.usend.c_str(), [&] { n = ops.delta_encode([&](auto& d) { fill_delta(d, w, b, s.plan, base + b); }); });
            const std::size_t z = zstd_size(ops.delta_wire());
            st.bytes[k.uenc] += 1;
            st.bytes[k.uunits] += s.plan.units.size() + s.plan.removed.size();
            st.bytes[k.umarch] += s.plan.marches.size() + s.plan.removed_marches.size();
            if (keep_delta) *keep_delta = std::string(ops.delta_wire());
            for (const auto& [c, slot] : subs) {
                timed(st, k.urecv.c_str(), [&] { ops.delta_recv(); });
                HeldIndex& at = s.index[std::size_t(c) * 9 + std::size_t(slot)];
                timed(st, k.uapply.c_str(), [&] { apply_delta(ops.held(c, slot), at, ops.delta()); });
                const uint64_t q = query_view(ops.held(c, slot));
                st.report_sum += q;
                if constexpr (native) {
                    timed(st, k.nrecv.c_str(), [&] { ops.n_apply(c, slot); });
                    if (query_view(ops.n_held(c, slot)) != q) throw std::runtime_error("native delta: view differs from the application-level one");
                    st.bytes[k.ndelta] += nd;
                }
                st.bytes[k.udelta] += n;
                st.bytes[k.uzstd] += z;
                st.bytes[k.umsg] += 1;
            }
        }
        // E: clients move; one that entered another block pulls its 3 x 3 window.
        for (int c = 0; c < vs.clients; ++c) {
            const int32_t old_block = w.client(c).block;
            int32_t old_win[9]; int32_t old_n = w.client(c).n;
            std::copy(w.client(c).win, w.client(c).win + 9, old_win);
            if (!w.move(c)) continue;
            if (old_block >= 0)
                for (int32_t slot = 0; slot < old_n; ++slot) {
                    auto& v = s.subs[std::size_t(old_win[slot])];
                    v.erase(std::find(v.begin(), v.end(), std::pair<int32_t, int32_t>(c, slot)));
                }
            const ViewClient& vc = w.client(c);
            st.bytes[k.ereq] += 1;
            for (int32_t slot = 0; slot < vc.n; ++slot) {
                const int32_t b = vc.win[slot];
                if (s.snap_tick[std::size_t(b)] != w.tick_no()) {
                    timed(st, k.esend.c_str(), [&] {
                        ops.snap_encode(b, [&](auto& v) { fill_view(v, w, b, s.last_id[std::size_t(b)], s.last_tick[std::size_t(b)]); });
                    });
                    if constexpr (native) timed(st, k.kenc.c_str(), [&] { s.nkey[std::size_t(b)] = ops.n_key(b); });
                    s.snap_tick[std::size_t(b)] = w.tick_no();
                    s.snap_z[std::size_t(b)] = zstd_size(ops.snap_wire(b));
                    if constexpr (requires { ops.snap_compact(b); }) {
                        const std::string_view cw = ops.snap_compact(b);
                        s.snap_c[std::size_t(b)] = cw.size();
                        s.snap_cz[std::size_t(b)] = zstd_size(cw);
                    }
                    st.bytes[k.eenc] += 1;
                    if (keep_view) *keep_view = std::string(ops.snap_wire(b));
                }
                uint64_t q = 0;
                timed(st, k.erecv.c_str(), [&] {
                    ops.snap_recv(c, slot, b);
                    q = query_view(ops.held(c, slot));
                });
                st.report_sum += q;
                if constexpr (native) {
                    timed(st, k.krecv.c_str(), [&] { ops.n_key_recv(c, slot, b); });
                    if (query_view(ops.n_held(c, slot)) != q) throw std::runtime_error("native key delta: view differs from the snapshot");
                    st.bytes[k.kbytes] += s.nkey[std::size_t(b)];
                }
                HeldIndex& at = s.index[std::size_t(c) * 9 + std::size_t(slot)];
                timed(st, k.eindex.c_str(), [&] { build_index(ops.held(c, slot), at); });
                s.subs[std::size_t(b)].emplace_back(c, slot);
                st.bytes[k.ewin] += ops.snap_wire(b).size();
                st.bytes[k.ezstd] += s.snap_z[std::size_t(b)];
                if constexpr (requires { ops.snap_compact(b); }) {
                    st.bytes[k.ecomp] += s.snap_c[std::size_t(b)];
                    st.bytes[k.ecompz] += s.snap_cz[std::size_t(b)];
                }
            }
        }
    }
}

// The three demo worlds, by change rate: peace (P), combat (C), stress (S).
// E* block snapshots, U* application-level deltas; XOffset native: N* deltas,
// K* key deltas (its snapshots for new subscribers).
struct BlockRates {
    static constexpr Rate rates[3] = {Rate::Peace, Rate::Combat, Rate::Stress};
    static const BlockKeys& keys(int i) {
        static const BlockKeys k[3] = {{"EP", "UP", "NP", "KP"}, {"EC", "UC", "NC", "KC"}, {"ES", "US", "NS", "KS"}};
        return k[i];
    }
    static uint64_t seed(uint64_t s, int i) { return mix_seed(s, 0xb10c + uint64_t(i)); }
};

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
