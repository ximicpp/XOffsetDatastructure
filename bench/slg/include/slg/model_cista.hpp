// SLG prototype: the same domain model as model_std.hpp, built on cista (v0.16)
// offset-mode containers. Member names and order match model_std.hpp so that the
// templates in generic.hpp run unchanged.
//
// Maps: cista has no ordered map, and its hash map cannot be used here: after an
// erase, cista::offset::hash_map can serialize into a buffer that cista's own
// deserialize check rejects ("hash storage: growth left"; reproduced on v0.16 and
// on master 2026-10-20 with insert/erase of int32 keys). The maps are therefore a
// sorted cista vector of key/value pairs, the same kind of map as XOffset's XMap
// (a flat sorted map): binary-search lookup, insert and erase shift the tail.
#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <cista/containers/string.h>
#include <cista/containers/vector.h>

namespace slg::cm {
namespace data = cista::offset;

template <class K, class V> struct FlatMap {
    struct Entry { K first; V second; };
    data::vector<Entry> v;

    std::size_t size() const { return v.size(); }
    Entry* begin() { return v.begin(); }
    Entry* end() { return v.end(); }
    const Entry* begin() const { return v.begin(); }
    const Entry* end() const { return v.end(); }
    std::less<K> key_comp() const { return {}; }
    void clear() { v.clear(); }
    Entry* lower(K k) { return std::lower_bound(v.begin(), v.end(), k, [](const Entry& e, K x) { return e.first < x; }); }
    V& operator[](K k) {
        Entry* it = lower(k);
        if (it == v.end() || it->first != k) {
            // cista's vector::insert returns std::rotate's result (one past the new
            // element), so locate the new element by index.
            const auto idx = it - v.begin();
            v.insert(it, Entry{k, V{}});
            it = v.begin() + idx;
        }
        return it->second;
    }
    std::size_t erase(K k) {
        Entry* it = lower(k);
        if (it == v.end() || it->first != k) return 0;
        v.erase(it, it + 1);
        return 1;
    }
};

struct Buff  { int32_t id{}; int32_t stacks{}; int64_t expire_ms{}; };
struct Skill { int32_t id{}; int32_t level{}; };
struct Troop { int32_t type{}; int32_t count{}; int32_t wounded{}; };

struct Hero {
    int32_t id{}; int32_t level{}; int32_t star{};
    data::string name;
    data::vector<Skill> skills;
    data::vector<Buff> buffs;
};

struct Army {
    int64_t army_id{}; int64_t owner_id{}; int32_t x{}; int32_t y{};
    data::vector<Hero> heroes;
    data::vector<Troop> troops;
    data::vector<Buff> buffs;
};

struct City {
    int64_t city_id{}; int32_t level{};
    FlatMap<int32_t, int32_t> buildings;
    FlatMap<int32_t, int64_t> resources;
};

struct PlayerState {
    int64_t player_id{};
    data::string name;
    City city;
    data::vector<Army> armies;
    data::vector<Hero> heroes;
    FlatMap<int32_t, int32_t> research;
    FlatMap<int32_t, int64_t> items;
};

struct Event { int32_t round{}; int32_t hero_id{}; int32_t skill_id{}; int32_t target{}; int32_t damage{}; };

struct Report {
    int64_t battle_id{};
    data::string summary;
    data::vector<Event> events;
};

struct BattleMsg {
    int64_t battle_id{}; int64_t seed{}; int32_t winner{};
    Army attacker;
    Army defender;
    Report report;
};

struct Tile { int32_t x{}; int32_t y{}; int32_t terrain{}; int32_t level{}; int64_t owner{}; };

struct March {
    int64_t march_id{}; int64_t owner{};
    int32_t from_x{}; int32_t from_y{}; int32_t to_x{}; int32_t to_y{}; int32_t eta{};
    data::vector<Troop> troops;
};

struct ShardState {
    int32_t shard_id{}; int64_t tick{}; int32_t width{};
    data::vector<Tile> tiles;
    data::vector<City> cities;
    data::vector<March> marches;
};

} // namespace slg::cm
