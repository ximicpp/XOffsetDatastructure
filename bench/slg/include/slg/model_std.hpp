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

} // namespace slg::stdm
