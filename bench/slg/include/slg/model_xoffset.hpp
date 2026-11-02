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

// Demo case set, scenario E (view.hpp).
struct alignas(8) ViewUnit { int32_t id{}; int32_t owner{}; int32_t x{}; int32_t y{}; int32_t hp{}; int32_t state{}; XString name; };

struct alignas(8) ViewMarch { int32_t id{}; int32_t owner{}; int32_t x0{}; int32_t y0{}; int32_t x1{}; int32_t y1{}; int64_t t0{}; int64_t t1{}; };

struct alignas(8) ViewSnapshot {
    int64_t request_id{}; int64_t tick{};
    int32_t x0{}; int32_t y0{}; int32_t x1{}; int32_t y1{};
    XVector<ViewUnit> units;
    XVector<ViewMarch> marches;
};

// Scenario U (view.hpp).
struct alignas(8) ViewDelta {
    int64_t request_id{}; int64_t tick{};
    int32_t x0{}; int32_t y0{}; int32_t x1{}; int32_t y1{};
    XVector<ViewUnit> upserts;
    XVector<int32_t> removed;
    XVector<ViewMarch> marches;
    XVector<int32_t> removed_marches;
};

} // namespace slg::xm

// Audited: every record above has only ordinary members (no bases, bit-fields,
// unions or [[no_unique_address]]), so all are declared ordinary records, as a
// user following the guide would. Element vectors (Hero, Skill, Buff, March...)
// then relocate by SHIFT on erase/reserve, and the shard ticks remove arrived
// marches with stable_erase_if. Before 2026-10-22 only March was declared
// (StableEraseOrdinaryMembers); see bench/slg/MANIFEST.
//
// XOFFSET_RECORD carries the same declaration plus the field list a build
// without reflection needs; a C++26 build checks each line against reflection.
// The lines are tools/describe_dump's output for the three roots, verbatim
// (the describe_dump_slg test keeps it that way).
#if defined(XOFFSET_RECORD)
XOFFSET_RECORD(slg::xm::City, city_id, level, buildings, resources)
XOFFSET_RECORD(slg::xm::Skill, id, level)
XOFFSET_RECORD(slg::xm::Buff, id, stacks, expire_ms)
XOFFSET_RECORD(slg::xm::Hero, id, level, star, name, skills, buffs)
XOFFSET_RECORD(slg::xm::Troop, type, count, wounded)
XOFFSET_RECORD(slg::xm::Army, army_id, owner_id, x, y, heroes, troops, buffs)
XOFFSET_RECORD(slg::xm::PlayerState, player_id, name, city, armies, heroes, research, items)
XOFFSET_RECORD(slg::xm::Event, round, hero_id, skill_id, target, damage)
XOFFSET_RECORD(slg::xm::Report, battle_id, summary, events)
XOFFSET_RECORD(slg::xm::BattleMsg, battle_id, seed, winner, attacker, defender, report)
XOFFSET_RECORD(slg::xm::Tile, x, y, terrain, level, owner)
XOFFSET_RECORD(slg::xm::March, march_id, owner, from_x, from_y, to_x, to_y, eta, troops)
XOFFSET_RECORD(slg::xm::ShardState, shard_id, tick, width, tiles, cities, marches)
XOFFSET_RECORD(slg::xm::ViewUnit, id, owner, x, y, hp, state, name)
XOFFSET_RECORD(slg::xm::ViewMarch, id, owner, x0, y0, x1, y1, t0, t1)
XOFFSET_RECORD(slg::xm::ViewSnapshot, request_id, tick, x0, y0, x1, y1, units, marches)
XOFFSET_RECORD(slg::xm::ViewDelta, request_id, tick, x0, y0, x1, y1, upserts, removed, marches, removed_marches)
#elif defined(XOFFSET_ORDINARY_RECORDS)
XOFFSET_ORDINARY_RECORDS(slg::xm::Buff, slg::xm::Skill, slg::xm::Troop, slg::xm::Hero, slg::xm::Army,
                         slg::xm::City, slg::xm::PlayerState, slg::xm::Event, slg::xm::Report,
                         slg::xm::BattleMsg, slg::xm::Tile, slg::xm::March, slg::xm::ShardState,
                         slg::xm::ViewUnit, slg::xm::ViewMarch, slg::xm::ViewSnapshot, slg::xm::ViewDelta)
#else
// Libraries without the macro: its two specializations, written out.
#define SLG_XM_ORDINARY_RECORD(T)                                                      \
    template<> struct XOffsetDatastructure::wire_record_layout<T> {                    \
        static constexpr bool ordinary_members = true;                                 \
    };                                                                                 \
    template<> struct XOffsetDatastructure::StableEraseOrdinaryMembers<T> : std::true_type {};
SLG_XM_ORDINARY_RECORD(slg::xm::Buff)
SLG_XM_ORDINARY_RECORD(slg::xm::Skill)
SLG_XM_ORDINARY_RECORD(slg::xm::Troop)
SLG_XM_ORDINARY_RECORD(slg::xm::Hero)
SLG_XM_ORDINARY_RECORD(slg::xm::Army)
SLG_XM_ORDINARY_RECORD(slg::xm::City)
SLG_XM_ORDINARY_RECORD(slg::xm::PlayerState)
SLG_XM_ORDINARY_RECORD(slg::xm::Event)
SLG_XM_ORDINARY_RECORD(slg::xm::Report)
SLG_XM_ORDINARY_RECORD(slg::xm::BattleMsg)
SLG_XM_ORDINARY_RECORD(slg::xm::Tile)
SLG_XM_ORDINARY_RECORD(slg::xm::March)
SLG_XM_ORDINARY_RECORD(slg::xm::ShardState)
SLG_XM_ORDINARY_RECORD(slg::xm::ViewUnit)
SLG_XM_ORDINARY_RECORD(slg::xm::ViewMarch)
SLG_XM_ORDINARY_RECORD(slg::xm::ViewSnapshot)
SLG_XM_ORDINARY_RECORD(slg::xm::ViewDelta)
#undef SLG_XM_ORDINARY_RECORD
#endif

XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::PlayerState, "slg.PlayerState.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::BattleMsg, "slg.BattleMsg.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::ShardState, "slg.ShardState.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::Report, "slg.Report.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::ViewSnapshot, "slg.ViewSnapshot.v2")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::ViewDelta, "slg.ViewDelta.v2")
