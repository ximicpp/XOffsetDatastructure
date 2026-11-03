// SLG prototype: domain model on std containers.
// Used by the cereal, protobuf and FlatBuffers paths (each decodes into these
// types, runs the shared game logic, and encodes back) and by the trace generator.
#pragma once
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <map>
#include <algorithm>
#include <utility>
#include <string>
#include <vector>

namespace slg::stdm {
// A sorted flat map (one vector of pairs ordered by key), the same structure as
// XOffset's and cista's maps (fairness review 2026-11-03). Written here so the
// std model needs no extra dependency.
template <class K, class V> class smap {
public:
    using key_type = K;
    using mapped_type = V;
    using value_type = std::pair<K, V>;
    using size_type = std::size_t;
    using iterator = typename std::vector<value_type>::iterator;
    using const_iterator = typename std::vector<value_type>::const_iterator;

    iterator begin() noexcept { return v_.begin(); }
    iterator end() noexcept { return v_.end(); }
    const_iterator begin() const noexcept { return v_.begin(); }
    const_iterator end() const noexcept { return v_.end(); }
    size_type size() const noexcept { return v_.size(); }
    bool empty() const noexcept { return v_.empty(); }
    void clear() noexcept { v_.clear(); }
    void reserve(size_type n) { v_.reserve(n); }

    iterator lower_bound(const K& k) { return std::lower_bound(v_.begin(), v_.end(), k, less); }
    const_iterator lower_bound(const K& k) const { return std::lower_bound(v_.begin(), v_.end(), k, less); }
    iterator find(const K& k) { auto it = lower_bound(k); return it != v_.end() && it->first == k ? it : v_.end(); }
    const_iterator find(const K& k) const { auto it = lower_bound(k); return it != v_.end() && it->first == k ? it : v_.end(); }
    size_type count(const K& k) const { return find(k) != v_.end() ? 1 : 0; }
    bool contains(const K& k) const { return find(k) != v_.end(); }

    V& operator[](const K& k) {
        auto it = lower_bound(k);
        if (it == v_.end() || it->first != k) it = v_.emplace(it, k, V{});
        return it->second;
    }
    template <class... A> std::pair<iterator, bool> emplace(const K& k, A&&... a) {
        auto it = lower_bound(k);
        if (it != v_.end() && it->first == k) return {it, false};
        return {v_.emplace(it, std::piecewise_construct, std::forward_as_tuple(k), std::forward_as_tuple(std::forward<A>(a)...)), true};
    }
    std::pair<iterator, bool> insert(const value_type& x) { return emplace(x.first, x.second); }
    std::pair<iterator, bool> insert(value_type&& x) { return emplace(x.first, std::move(x.second)); }
    // Hinted insert: appending keys in order (every decoder does) is O(1).
    template <class KK, class... A> iterator emplace_hint(const_iterator hint, KK&& k, A&&... a) {
        const K key(std::forward<KK>(k));
        if (hint == v_.cend() && (v_.empty() || v_.back().first < key)) {
            v_.emplace_back(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::forward<A>(a)...));
            return v_.end() - 1;
        }
        return emplace(key, std::forward<A>(a)...).first;
    }
    iterator insert(const_iterator hint, const value_type& x) { return emplace_hint(hint, x.first, x.second); }
    iterator insert(const_iterator hint, value_type&& x) { return emplace_hint(hint, x.first, std::move(x.second)); }
    size_type erase(const K& k) { auto it = find(k); if (it == v_.end()) return 0; v_.erase(it); return 1; }
    iterator erase(const_iterator it) { return v_.erase(it); }

    friend bool operator==(const smap& a, const smap& b) { return a.v_ == b.v_; }

private:
    static bool less(const value_type& e, const K& k) { return e.first < k; }
    std::vector<value_type> v_;
};

struct Buff  { int32_t id{}; int32_t stacks{}; int64_t expire_ms{}; };
struct Skill { int32_t id{}; int32_t level{}; };
struct Troop { int32_t type{}; int32_t count{}; int32_t wounded{}; };

struct Hero {
    int32_t id{}; int32_t level{}; int32_t star{};
    std::string name;
    std::vector<Skill> skills;
    std::vector<Buff> buffs;
};

struct Army {
    int64_t army_id{}; int64_t owner_id{}; int32_t x{}; int32_t y{};
    std::vector<Hero> heroes;
    std::vector<Troop> troops;
    std::vector<Buff> buffs;
};

struct City {
    int64_t city_id{}; int32_t level{};
    smap<int32_t, int32_t> buildings;
    smap<int32_t, int64_t> resources;
};

struct PlayerState {
    int64_t player_id{};
    std::string name;
    City city;
    std::vector<Army> armies;
    std::vector<Hero> heroes;
    smap<int32_t, int32_t> research;
    smap<int32_t, int64_t> items;
};

// Login self deltas and dirty-module saves (login.hpp): changed armies (whole,
// by index), changed item counts, the city when it changed.
struct PlayerPatch {
    int64_t player_id{}; int64_t seq{};
    std::vector<int32_t> army_index;
    std::vector<Army> armies;
    smap<int32_t, int64_t> items;
    int32_t city_changed{};
    City city;
};

struct Event { int32_t round{}; int32_t hero_id{}; int32_t skill_id{}; int32_t target{}; int32_t damage{}; };

struct Report {
    int64_t battle_id{};
    std::string summary;
    std::vector<Event> events;
};

// Request and result of one battle: the battle server fills `report` and `winner`
// and changes both armies in place.
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
    std::vector<Troop> troops;
};

struct ShardState {
    int32_t shard_id{}; int64_t tick{}; int32_t width{};
    std::vector<Tile> tiles;
    std::vector<City> cities;
    std::vector<March> marches;
};

// Demo case set, scenario E (view.hpp): units inside one client's viewport.
// A map unit as the client sees it (city, army, resource point): the fields a real SLG
// map entity carries, with strings, a nested commander and three lists.
struct UnitHero { int32_t id{}; int32_t level{}; int32_t star{}; int32_t skin{}; };
struct ViewUnit {
    int32_t id{}; int32_t kind{}; int32_t owner{}; int32_t alliance{}; int32_t x{}; int32_t y{}; int32_t hp{}; int32_t max_hp{}; int32_t state{}; int32_t level{}; int32_t faction{}; int32_t flags{}; int32_t speed{}; int32_t attack{}; int32_t defense{}; int32_t morale{}; int32_t load{}; int32_t icon{}; int32_t skin{}; int32_t banner{}; int32_t title{}; int32_t vip{}; int32_t rank{}; int32_t slots{};
    int64_t power{}; int64_t state_end{}; int64_t shield_end{}; int64_t cooldown_end{}; int64_t last_update{}; int64_t kills{}; bool in_battle{}; bool shielded{}; bool hidden{};
    std::string name; std::string alliance_tag; std::string owner_name;
    UnitHero commander;
    std::vector<UnitHero> heroes; std::vector<Troop> troops; std::vector<Buff> buffs;
};

// A march as the client sees it: a line from (x0,y0) to (x1,y1), start and arrival tick.
struct ViewMarch { int32_t id{}; int32_t owner{}; int32_t x0{}; int32_t y0{}; int32_t x1{}; int32_t y1{}; int64_t t0{}; int64_t t1{}; };

struct ViewSnapshot {
    int64_t request_id{}; int64_t tick{};
    int32_t x0{}; int32_t y0{}; int32_t x1{}; int32_t y1{};
    std::vector<ViewUnit> units;
    std::vector<ViewMarch> marches;
    std::vector<Tile> tiles;          // every tile under the viewport, row-major
};

// Scenario U (view.hpp): changes since the client's last sync.
struct ViewDelta {
    int64_t request_id{}; int64_t tick{};
    int32_t x0{}; int32_t y0{}; int32_t x1{}; int32_t y1{};
    std::vector<ViewUnit> upserts;
    std::vector<int32_t> removed;
    std::vector<ViewMarch> marches;
    std::vector<int32_t> removed_marches;
    std::vector<Tile> tiles;          // tiles whose owner changed
};

} // namespace slg::stdm
