// SLG prototype: deterministic synthetic world.
// Sizes are assumptions for a mid-size SLG server (see README, "Data model"),
// not measurements from a real game. Replace them with anonymised real
// distributions when available.
#pragma once
#include <string>
#include <vector>
#include "generic.hpp"
#include "model_std.hpp"

namespace slg {

struct Scale {
    const char* name;
    int players;        // players on the scene server (scenario A sources, scenario B migrants)
    int battles;        // battles per cycle (scenario A)
    int migrants;       // players migrated per cycle (scenario B)
    int season_ops;     // edits per migrated player (scenario B)
    int shard_width;    // tiles per side of the map shard (scenario C)
    int shard_cities;
    int shard_marches;
    int shard_ticks;    // ticks between two snapshots (scenario C)
    int report_reads;   // report reads per cycle (scenario D)
};

inline const Scale* find_scale(std::string_view n) {
    static const Scale scales[] = {
        {"S", 64, 64, 32, 64, 64, 100, 400, 10, 256},
        {"M", 512, 512, 256, 64, 128, 500, 2000, 10, 2048},
        {"L", 2048, 2048, 1024, 64, 256, 2000, 8000, 10, 8192},
    };
    for (const auto& s : scales) if (n == s.name) return &s;
    return nullptr;
}

inline std::string make_name(Rng& rng, const char* prefix) {
    std::string s = prefix;
    const int n = rng.range(2, 12);
    for (int i = 0; i < n; ++i) s.push_back(char('a' + rng.below(26)));
    return s;
}

inline stdm::Hero make_hero(Rng& rng, int32_t id) {
    stdm::Hero h;
    h.id = id; h.level = rng.range(1, 60); h.star = rng.range(1, 6);
    h.name = make_name(rng, "h_");
    for (int k = 0, n = rng.range(3, 4); k < n; ++k) h.skills.push_back({rng.below(500), rng.range(1, 10)});
    for (int k = 0, n = rng.range(0, 3); k < n; ++k) h.buffs.push_back({rng.below(300), rng.range(1, 3), int64_t(rng.range(2, 9)) * 1000});
    return h;
}

inline stdm::Army make_army(Rng& rng, int64_t owner, int64_t id) {
    stdm::Army a;
    a.army_id = id; a.owner_id = owner; a.x = rng.below(1200); a.y = rng.below(1200);
    for (int k = 0, n = rng.range(3, 5); k < n; ++k) a.heroes.push_back(make_hero(rng, rng.below(2000)));
    for (int k = 0, n = rng.range(2, 4); k < n; ++k) a.troops.push_back({rng.below(4), rng.range(2000, 20000), 0});
    for (int k = 0, n = rng.range(0, 3); k < n; ++k) a.buffs.push_back({rng.below(300), 1, int64_t(rng.range(2, 9)) * 1000});
    return a;
}

inline stdm::City make_city(Rng& rng, int64_t id) {
    stdm::City c;
    c.city_id = id; c.level = rng.range(1, 25);
    for (int k = 0, n = rng.range(20, 30); k < n; ++k) c.buildings[rng.below(40)] = rng.range(1, 25);
    for (int k = 0; k < 6; ++k) c.resources[k] = rng.range(1000, 5000000);
    return c;
}

inline stdm::PlayerState make_player(uint64_t seed, int64_t id) {
    Rng rng(mix_seed(seed, uint64_t(id)));
    stdm::PlayerState p;
    p.player_id = id;
    p.name = make_name(rng, "p_");
    p.city = make_city(rng, id);
    for (int k = 0, n = rng.range(3, 5); k < n; ++k) p.armies.push_back(make_army(rng, id, id * 16 + k));
    for (int k = 0, n = rng.range(20, 60); k < n; ++k) p.heroes.push_back(make_hero(rng, rng.below(2000)));
    for (int k = 0, n = rng.range(40, 60); k < n; ++k) p.research[rng.below(60)] = rng.range(1, 20);
    for (int k = 0, n = rng.range(50, 150); k < n; ++k) p.items[rng.below(200)] = rng.range(1, 100000);
    return p;
}

inline stdm::ShardState make_shard(uint64_t seed, const Scale& sc) {
    Rng rng(mix_seed(seed, 0x5a4d));
    stdm::ShardState s;
    s.shard_id = 7; s.tick = 0; s.width = sc.shard_width;
    s.tiles.reserve(std::size_t(sc.shard_width) * std::size_t(sc.shard_width));
    for (int y = 0; y < sc.shard_width; ++y)
        for (int x = 0; x < sc.shard_width; ++x)
            s.tiles.push_back({x, y, rng.below(6), rng.range(1, 10), rng.below(4) == 0 ? int64_t(1 + rng.below(5000)) : 0});
    for (int k = 0; k < sc.shard_cities; ++k) s.cities.push_back(make_city(rng, 900000 + k));
    for (int k = 0; k < sc.shard_marches; ++k) {
        stdm::March m;
        m.march_id = k; m.owner = 1 + rng.below(5000);
        m.from_x = rng.below(sc.shard_width); m.from_y = rng.below(sc.shard_width);
        m.to_x = rng.below(sc.shard_width); m.to_y = rng.below(sc.shard_width);
        m.eta = rng.range(1, 30);
        for (int t = 0, n = rng.range(1, 4); t < n; ++t) m.troops.push_back({rng.below(4), rng.range(100, 5000), 0});
        s.marches.push_back(std::move(m));
    }
    return s;
}

// Battle pairing for one cycle: attacker (player, army) vs defender (player, army).
struct Pairing { int ap, aa, dp, da; int64_t battle_id; int64_t seed; };

inline std::vector<Pairing> make_pairings(uint64_t seed, int cycle, const Scale& sc,
                                          const std::vector<int>& armies_per_player) {
    Rng rng(mix_seed(seed, 0xba77 + uint64_t(cycle)));
    std::vector<Pairing> out;
    out.reserve(std::size_t(sc.battles));
    for (int b = 0; b < sc.battles; ++b) {
        Pairing p{};
        p.ap = rng.below(sc.players);
        do { p.dp = rng.below(sc.players); } while (p.dp == p.ap && sc.players > 1);
        p.aa = rng.below(armies_per_player[std::size_t(p.ap)]);
        p.da = rng.below(armies_per_player[std::size_t(p.dp)]);
        p.battle_id = int64_t(cycle) * 1000000 + b;
        p.seed = static_cast<int64_t>(rng.next() >> 1);
        out.push_back(p);
    }
    return out;
}

inline std::vector<int> pick_migrants(uint64_t seed, int cycle, const Scale& sc) {
    Rng rng(mix_seed(seed, 0x316 + uint64_t(cycle)));
    std::vector<int> out;
    for (int i = 0; i < sc.migrants; ++i) out.push_back(rng.below(sc.players));
    return out;
}

} // namespace slg
