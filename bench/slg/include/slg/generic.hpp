// SLG prototype: game logic, deep copy and digest, written once as templates.
// The same code runs on slg::stdm (std containers) and slg::xm (XOffset containers):
// both models use the same member names, and the containers share the subset of the
// std interface used here (size, operator[], emplace_back, erase, clear, reserve,
// map operator[]/find/erase, pair-like map elements with first/second).
#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace slg {

// ---- helpers --------------------------------------------------------------

inline std::string_view sv(const std::string& s) { return s; }
template <class S>
    requires requires(const S& s) { s.view(); }
std::string_view sv(const S& s) { return s.view(); }

// splitmix64: small, fast and identical on every platform.
struct Rng {
    uint64_t state;
    explicit Rng(uint64_t seed) : state(seed) {}
    uint64_t next() {
        uint64_t z = (state += 0x9e3779b97f4a7c15ull);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }
    int32_t below(int32_t n) { return n <= 0 ? 0 : static_cast<int32_t>(next() % static_cast<uint64_t>(n)); }
    int32_t range(int32_t lo, int32_t hi) { return lo + below(hi - lo + 1); }
};

inline uint64_t mix_seed(uint64_t a, uint64_t b) { return Rng(a * 0x100000001b3ull ^ b).next(); }

// FNV-1a over a defined traversal of the logical content (not the bytes of any format).
struct Digest {
    uint64_t h = 1469598103934665603ull;
    void u(uint64_t v) {
        for (int i = 0; i < 8; ++i) { h ^= (v >> (8 * i)) & 0xff; h *= 1099511628211ull; }
    }
    void s(std::string_view v) {
        u(v.size());
        for (unsigned char c : v) { h ^= c; h *= 1099511628211ull; }
    }
};

// ---- deep copy between any two models --------------------------------------

template <class D, class S> void copy_buff(D& d, const S& s) { d.id = s.id; d.stacks = s.stacks; d.expire_ms = s.expire_ms; }
template <class D, class S> void copy_skill(D& d, const S& s) { d.id = s.id; d.level = s.level; }
template <class D, class S> void copy_troop(D& d, const S& s) { d.type = s.type; d.count = s.count; d.wounded = s.wounded; }
template <class D, class S> void copy_event(D& d, const S& s) {
    d.round = s.round; d.hero_id = s.hero_id; d.skill_id = s.skill_id; d.target = s.target; d.damage = s.damage;
}
template <class D, class S> void copy_tile(D& d, const S& s) {
    d.x = s.x; d.y = s.y; d.terrain = s.terrain; d.level = s.level; d.owner = s.owner;
}

template <class DV, class SV, class F> void copy_vec(DV& d, const SV& s, F&& f) {
    d.clear();
    d.reserve(s.size());
    for (const auto& e : s) f(d.emplace_back(), e);
}
template <class DM, class SM> void copy_map(DM& d, const SM& s) {
    d.clear();
    for (const auto& kv : s) d[kv.first] = kv.second;
}

template <class D, class S> void copy_hero(D& d, const S& s) {
    d.id = s.id; d.level = s.level; d.star = s.star;
    d.name = sv(s.name);
    copy_vec(d.skills, s.skills, [](auto& a, const auto& b) { copy_skill(a, b); });
    copy_vec(d.buffs, s.buffs, [](auto& a, const auto& b) { copy_buff(a, b); });
}
template <class D, class S> void copy_army(D& d, const S& s) {
    d.army_id = s.army_id; d.owner_id = s.owner_id; d.x = s.x; d.y = s.y;
    copy_vec(d.heroes, s.heroes, [](auto& a, const auto& b) { copy_hero(a, b); });
    copy_vec(d.troops, s.troops, [](auto& a, const auto& b) { copy_troop(a, b); });
    copy_vec(d.buffs, s.buffs, [](auto& a, const auto& b) { copy_buff(a, b); });
}
template <class D, class S> void copy_city(D& d, const S& s) {
    d.city_id = s.city_id; d.level = s.level;
    copy_map(d.buildings, s.buildings);
    copy_map(d.resources, s.resources);
}
template <class D, class S> void copy_player(D& d, const S& s) {
    d.player_id = s.player_id;
    d.name = sv(s.name);
    copy_city(d.city, s.city);
    copy_vec(d.armies, s.armies, [](auto& a, const auto& b) { copy_army(a, b); });
    copy_vec(d.heroes, s.heroes, [](auto& a, const auto& b) { copy_hero(a, b); });
    copy_map(d.research, s.research);
    copy_map(d.items, s.items);
}
template <class D, class S> void copy_report(D& d, const S& s) {
    d.battle_id = s.battle_id;
    d.summary = sv(s.summary);
    copy_vec(d.events, s.events, [](auto& a, const auto& b) { copy_event(a, b); });
}
template <class D, class S> void copy_march(D& d, const S& s) {
    d.march_id = s.march_id; d.owner = s.owner;
    d.from_x = s.from_x; d.from_y = s.from_y; d.to_x = s.to_x; d.to_y = s.to_y; d.eta = s.eta;
    copy_vec(d.troops, s.troops, [](auto& a, const auto& b) { copy_troop(a, b); });
}
template <class D, class S> void copy_shard(D& d, const S& s) {
    d.shard_id = s.shard_id; d.tick = s.tick; d.width = s.width;
    copy_vec(d.tiles, s.tiles, [](auto& a, const auto& b) { copy_tile(a, b); });
    copy_vec(d.cities, s.cities, [](auto& a, const auto& b) { copy_city(a, b); });
    copy_vec(d.marches, s.marches, [](auto& a, const auto& b) { copy_march(a, b); });
}

// ---- digest of logical content ---------------------------------------------

template <class V, class F> void dig_vec(Digest& g, const V& v, F&& f) { g.u(v.size()); for (const auto& e : v) f(g, e); }
template <class M> void dig_map(Digest& g, const M& m) { g.u(m.size()); for (const auto& kv : m) { g.u(uint64_t(kv.first)); g.u(uint64_t(kv.second)); } }

template <class T> void dig_buff(Digest& g, const T& b) { g.u(b.id); g.u(b.stacks); g.u(b.expire_ms); }
template <class T> void dig_troop(Digest& g, const T& t) { g.u(t.type); g.u(t.count); g.u(t.wounded); }
template <class T> void dig_hero(Digest& g, const T& h) {
    g.u(h.id); g.u(h.level); g.u(h.star); g.s(sv(h.name));
    dig_vec(g, h.skills, [](Digest& d, const auto& s) { d.u(s.id); d.u(s.level); });
    dig_vec(g, h.buffs, [](Digest& d, const auto& b) { dig_buff(d, b); });
}
template <class T> void dig_army(Digest& g, const T& a) {
    g.u(a.army_id); g.u(a.owner_id); g.u(a.x); g.u(a.y);
    dig_vec(g, a.heroes, [](Digest& d, const auto& h) { dig_hero(d, h); });
    dig_vec(g, a.troops, [](Digest& d, const auto& t) { dig_troop(d, t); });
    dig_vec(g, a.buffs, [](Digest& d, const auto& b) { dig_buff(d, b); });
}
template <class T> void dig_city(Digest& g, const T& c) { g.u(c.city_id); g.u(c.level); dig_map(g, c.buildings); dig_map(g, c.resources); }
template <class T> void dig_player(Digest& g, const T& p) {
    g.u(p.player_id); g.s(sv(p.name)); dig_city(g, p.city);
    dig_vec(g, p.armies, [](Digest& d, const auto& a) { dig_army(d, a); });
    dig_vec(g, p.heroes, [](Digest& d, const auto& h) { dig_hero(d, h); });
    dig_map(g, p.research); dig_map(g, p.items);
}
template <class T> void dig_report(Digest& g, const T& r) {
    g.u(r.battle_id); g.s(sv(r.summary));
    dig_vec(g, r.events, [](Digest& d, const auto& e) { d.u(e.round); d.u(e.hero_id); d.u(e.skill_id); d.u(e.target); d.u(e.damage); });
}
template <class T> void dig_shard(Digest& g, const T& s) {
    g.u(s.shard_id); g.u(s.tick); g.u(s.width);
    dig_vec(g, s.tiles, [](Digest& d, const auto& t) { d.u(t.x); d.u(t.y); d.u(t.terrain); d.u(t.level); d.u(t.owner); });
    dig_vec(g, s.cities, [](Digest& d, const auto& c) { dig_city(d, c); });
    dig_vec(g, s.marches, [](Digest& d, const auto& m) {
        d.u(m.march_id); d.u(m.owner); d.u(m.from_x); d.u(m.from_y); d.u(m.to_x); d.u(m.to_y); d.u(m.eta);
        dig_vec(d, m.troops, [](Digest& e, const auto& t) { dig_troop(e, t); });
    });
}

// ---- scenario A: battle simulation (battle server, in place) ----------------

constexpr int kBattleRounds = 8;
constexpr std::size_t kMaxHeroBuffs = 6;

template <class H> int64_t hero_power(const H& h) {
    int64_t p = int64_t(h.level) * 10 + int64_t(h.star) * 25;
    for (const auto& b : h.buffs) p += int64_t(b.stacks) * 7;
    return p;
}

template <class A> int64_t living(const A& a) {
    int64_t n = 0;
    for (const auto& t : a.troops) n += t.count;
    return n;
}

template <class Msg> void simulate_battle(Msg& m) {
    Rng rng(static_cast<uint64_t>(m.seed));
    m.report.battle_id = m.battle_id;
    int32_t event_no = 0;
    for (int32_t r = 0; r < kBattleRounds; ++r) {
        for (int side = 0; side < 2; ++side) {
            auto& self = side == 0 ? m.attacker : m.defender;
            auto& foe = side == 0 ? m.defender : m.attacker;
            for (std::size_t i = 0; i < self.heroes.size(); ++i) {
                auto& h = self.heroes[i];
                if (h.skills.size() == 0 || foe.troops.size() == 0) continue;
                const auto& sk = h.skills[(std::size_t(r) + i) % h.skills.size()];
                const int32_t t = rng.below(static_cast<int32_t>(foe.troops.size()));
                auto& troop = foe.troops[std::size_t(t)];
                const int64_t raw = (hero_power(h) + int64_t(sk.level) * 40) * (4 + rng.below(5)) / 4;
                const int32_t dmg = static_cast<int32_t>(std::min<int64_t>(troop.count, raw));
                troop.count -= dmg;
                troop.wounded += dmg / 3;
                auto& e = m.report.events.emplace_back();
                e.round = r; e.hero_id = h.id; e.skill_id = sk.id; e.target = t; e.damage = dmg;
                if (++event_no % 3 == 0) {
                    if (h.buffs.size() < kMaxHeroBuffs) {
                        auto& b = h.buffs.emplace_back();
                        b.id = sk.id; b.stacks = 1; b.expire_ms = int64_t(r + 2 + rng.below(3)) * 1000;
                    } else {
                        h.buffs[std::size_t(rng.below(static_cast<int32_t>(h.buffs.size())))].stacks += 1;
                    }
                }
            }
            // Destroyed troop groups leave the army.
            for (std::size_t k = foe.troops.size(); k-- > 0;)
                if (foe.troops[k].count == 0) foe.troops.erase(foe.troops.begin() + std::ptrdiff_t(k));
        }
        // Expire buffs at the end of the round.
        for (auto* army : {&m.attacker, &m.defender})
            for (auto& h : army->heroes)
                for (std::size_t k = h.buffs.size(); k-- > 0;)
                    if (h.buffs[k].expire_ms <= int64_t(r + 1) * 1000) h.buffs.erase(h.buffs.begin() + std::ptrdiff_t(k));
        if (living(m.attacker) == 0 || living(m.defender) == 0) break;
    }
    const int64_t a = living(m.attacker), d = living(m.defender);
    m.winner = a > d ? 1 : (d > a ? 2 : 0);
    const std::string summary = "battle " + std::to_string(m.battle_id) +
        (m.winner == 1 ? " won by attacker" : m.winner == 2 ? " won by defender" : " draw");
    m.report.summary = std::string_view(summary);
}

// Scene server applies a battle result to the army it owns.
template <class Dst, class Src> void apply_battle_army(Dst& army, const Src& result) {
    copy_vec(army.troops, result.troops, [](auto& a, const auto& b) { copy_troop(a, b); });
    for (std::size_t i = 0; i < army.heroes.size() && i < result.heroes.size(); ++i)
        copy_vec(army.heroes[i].buffs, result.heroes[i].buffs, [](auto& a, const auto& b) { copy_buff(a, b); });
}

// ---- scenario B: season edits on a migrated player ------------------------

template <class P> void season_edits(P& p, uint64_t seed, int ops) {
    Rng rng(seed);
    for (int i = 0; i < ops; ++i) {
        switch (rng.below(8)) {
        case 0: p.items[rng.below(200)] += rng.range(1, 100); break;
        case 1: { auto& lv = p.research[rng.below(60)]; lv += 1; break; }
        case 2: {
            auto& h = p.heroes.emplace_back();
            h.id = 10000 + rng.below(100000); h.level = 1; h.star = 1;
            const std::string name = (rng.below(2) ? "recruit_" : "season_champion_") + std::to_string(h.id);
            h.name = std::string_view(name);
            for (int k = 0, n = rng.range(3, 4); k < n; ++k) { auto& s = h.skills.emplace_back(); s.id = rng.below(500); s.level = 1; }
            break;
        }
        case 3:
            if (p.heroes.size() > 5) p.heroes.erase(p.heroes.begin() + rng.below(static_cast<int32_t>(p.heroes.size())));
            break;
        case 4: {
            const std::string name = "lord_" + std::to_string(p.player_id) + "_s" + std::to_string(rng.below(10));
            p.name = std::string_view(name);
            break;
        }
        case 5:
            if (p.armies.size() > 0) {
                auto& a = p.armies[std::size_t(rng.below(static_cast<int32_t>(p.armies.size())))];
                if (a.troops.size() > 0) a.troops[std::size_t(rng.below(static_cast<int32_t>(a.troops.size())))].count += rng.range(10, 500);
                else { auto& t = a.troops.emplace_back(); t.type = rng.below(4); t.count = rng.range(100, 1000); }
            }
            break;
        case 6:
            p.city.buildings[rng.below(40)] += 1;
            p.city.resources[rng.below(6)] += rng.range(100, 10000);
            break;
        default: p.items.erase(rng.below(200)); break;
        }
    }
}

// ---- scenario C: map shard ticks ----------------------------------------

// One stable compaction per tick: std containers use std::erase_if, XOffset
// vectors stable_erase_if (March opts in, see model_xoffset.hpp).
template <class V> void remove_arrived(V& marches) {
    if constexpr (requires { marches.stable_erase_if([](const auto&) { return false; }); })
        marches.stable_erase_if([](const auto& m) { return m.eta <= 0; });
    else
        std::erase_if(marches, [](const auto& m) { return m.eta <= 0; });
}

template <class S> void shard_ticks(S& s, uint64_t seed, int ticks) {
    Rng rng(seed);
    for (int t = 0; t < ticks; ++t) {
        s.tick += 1;
        int arrivals = 0;
        for (std::size_t k = s.marches.size(); k-- > 0;) {
            auto& m = s.marches[k];
            if (--m.eta > 0) continue;
            const std::size_t idx = std::size_t(m.to_y) * std::size_t(s.width) + std::size_t(m.to_x);
            if (idx < s.tiles.size()) { s.tiles[idx].owner = m.owner; s.tiles[idx].level = std::min(s.tiles[idx].level + 1, 10); }
            ++arrivals;
        }
        // Arrived marches leave in one stable pass, keeping the order of the rest.
        if (arrivals > 0) remove_arrived(s.marches);
        for (int a = 0; a < arrivals; ++a) {
            auto& m = s.marches.emplace_back();
            m.march_id = int64_t(s.tick) * 100000 + a; m.owner = 1 + rng.below(5000);
            m.from_x = rng.below(s.width); m.from_y = rng.below(s.width);
            m.to_x = rng.below(s.width); m.to_y = rng.below(s.width);
            m.eta = rng.range(1, 30);
            for (int k = 0, n = rng.range(1, 4); k < n; ++k) { auto& tr = m.troops.emplace_back(); tr.type = rng.below(4); tr.count = rng.range(100, 5000); }
        }
        for (auto& c : s.cities) {
            for (auto& kv : c.resources) kv.second += 10 + (kv.first % 3);
            if (rng.below(50) == 0) { c.level += 1; c.buildings[rng.below(40)] += 1; }
        }
    }
}

// Middle-erase stress item (S.ticks_middle): the original shard ticks, which erase
// arrived marches one by one from the middle of the vector (O(n*k) shifts).
// Same rng sequence and the same resulting state as shard_ticks.
template <class S> void shard_ticks_middle_erase(S& s, uint64_t seed, int ticks) {
    Rng rng(seed);
    for (int t = 0; t < ticks; ++t) {
        s.tick += 1;
        int arrivals = 0;
        for (std::size_t k = s.marches.size(); k-- > 0;) {
            auto& m = s.marches[k];
            if (--m.eta > 0) continue;
            const std::size_t idx = std::size_t(m.to_y) * std::size_t(s.width) + std::size_t(m.to_x);
            if (idx < s.tiles.size()) { s.tiles[idx].owner = m.owner; s.tiles[idx].level = std::min(s.tiles[idx].level + 1, 10); }
            s.marches.erase(s.marches.begin() + std::ptrdiff_t(k));
            ++arrivals;
        }
        for (int a = 0; a < arrivals; ++a) {
            auto& m = s.marches.emplace_back();
            m.march_id = int64_t(s.tick) * 100000 + a; m.owner = 1 + rng.below(5000);
            m.from_x = rng.below(s.width); m.from_y = rng.below(s.width);
            m.to_x = rng.below(s.width); m.to_y = rng.below(s.width);
            m.eta = rng.range(1, 30);
            for (int k = 0, n = rng.range(1, 4); k < n; ++k) { auto& tr = m.troops.emplace_back(); tr.type = rng.below(4); tr.count = rng.range(100, 5000); }
        }
        for (auto& c : s.cities) {
            for (auto& kv : c.resources) kv.second += 10 + (kv.first % 3);
            if (rng.below(50) == 0) { c.level += 1; c.buildings[rng.below(40)] += 1; }
        }
    }
}

// ---- scenario D: read-only query on a battle report -------------------------

template <class R> uint64_t query_report(const R& r) {
    uint64_t even_damage = 0, skill_hits = 0;
    for (const auto& e : r.events) {
        if ((e.hero_id & 1) == 0) even_damage += uint64_t(e.damage);
        if (e.skill_id % 7 == 0) ++skill_hits;
    }
    return even_damage * 1000003ull + skill_hits * 31ull + sv(r.summary).size();
}

} // namespace slg
