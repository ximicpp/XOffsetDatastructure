// SLG prototype: the same domain model as model_std.hpp, built on XOffset containers.
// Member names and order match model_std.hpp so that the templates in
// generic.hpp run unchanged on both. Requires P2996 Clang (see README).
#pragma once
#include <cstdint>
#include "xoffsetdatastructure.hpp"

namespace slg::xm {
using namespace XOffsetDatastructure;

struct alignas(8) Buff  { int32_t id{}; int32_t stacks{}; int64_t expire_ms{}; };
struct alignas(8) Skill { int32_t id{}; int32_t level{}; };
struct alignas(8) Troop { int32_t type{}; int32_t count{}; int32_t wounded{}; };

struct alignas(8) Hero {
    int32_t id{}; int32_t level{}; int32_t star{};
    XString name;
    XVector<Skill> skills;
    XVector<Buff> buffs;
};

struct alignas(8) Army {
    int64_t army_id{}; int64_t owner_id{}; int32_t x{}; int32_t y{};
    XVector<Hero> heroes;
    XVector<Troop> troops;
    XVector<Buff> buffs;
};

struct alignas(8) City {
    int64_t city_id{}; int32_t level{};
    XMap<int32_t, int32_t> buildings;
    XMap<int32_t, int64_t> resources;
};

struct alignas(8) PlayerState {
    int64_t player_id{};
    XString name;
    City city;
    XVector<Army> armies;
    XVector<Hero> heroes;
    XMap<int32_t, int32_t> research;
    XMap<int32_t, int64_t> items;
};

struct alignas(8) Event { int32_t round{}; int32_t hero_id{}; int32_t skill_id{}; int32_t target{}; int32_t damage{}; };

struct alignas(8) Report {
    int64_t battle_id{};
    XString summary;
    XVector<Event> events;
};

struct alignas(8) BattleMsg {
    int64_t battle_id{}; int64_t seed{}; int32_t winner{};
    Army attacker;
    Army defender;
    Report report;
};

struct alignas(8) Tile { int32_t x{}; int32_t y{}; int32_t terrain{}; int32_t level{}; int64_t owner{}; };

struct alignas(8) March {
    int64_t march_id{}; int64_t owner{};
    int32_t from_x{}; int32_t from_y{}; int32_t to_x{}; int32_t to_y{}; int32_t eta{};
    XVector<Troop> troops;
};

struct alignas(8) ShardState {
    int32_t shard_id{}; int64_t tick{}; int32_t width{};
    XVector<Tile> tiles;
    XVector<City> cities;
    XVector<March> marches;
};

} // namespace slg::xm

namespace XOffsetDatastructure {
// Audited: March has only ordinary members, no bases or [[no_unique_address]].
// Lets the shard ticks remove arrived marches with stable_erase_if.
template<> struct StableEraseOrdinaryMembers<slg::xm::March> : std::true_type {};
}

XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::PlayerState, "slg.PlayerState.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::BattleMsg, "slg.BattleMsg.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::ShardState, "slg.ShardState.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::Report, "slg.Report.v1")
