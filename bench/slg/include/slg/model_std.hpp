// SLG prototype: domain model on std containers.
// Used by the cereal, protobuf and FlatBuffers paths (each decodes into these
// types, runs the shared game logic, and encodes back) and by the trace generator.
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace slg::stdm {

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
    std::map<int32_t, int32_t> buildings;
    std::map<int32_t, int64_t> resources;
};

struct PlayerState {
    int64_t player_id{};
    std::string name;
    City city;
    std::vector<Army> armies;
    std::vector<Hero> heroes;
    std::map<int32_t, int32_t> research;
    std::map<int32_t, int64_t> items;
};

// Login self deltas and dirty-module saves (login.hpp): changed armies (whole,
// by index), changed item counts, the city when it changed.
struct PlayerPatch {
    int64_t player_id{}; int64_t seq{};
    std::vector<int32_t> army_index;
    std::vector<Army> armies;
    std::map<int32_t, int64_t> items;
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
struct ViewUnit { int32_t id{}; int32_t owner{}; int32_t x{}; int32_t y{}; int32_t hp{}; int32_t state{}; std::string name; };

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
