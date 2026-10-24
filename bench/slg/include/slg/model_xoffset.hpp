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

// Audited: every record above has only ordinary members (no bases, bit-fields,
// unions or [[no_unique_address]]), so all are declared ordinary records, as a
// user following the guide would. Element vectors (Hero, Skill, Buff, March...)
// then relocate by SHIFT on erase/reserve, and the shard ticks remove arrived
// marches with stable_erase_if. Before 2026-10-22 only March was declared
// (StableEraseOrdinaryMembers); see bench/slg/MANIFEST.
#ifdef XOFFSET_ORDINARY_RECORDS
XOFFSET_ORDINARY_RECORDS(slg::xm::Buff, slg::xm::Skill, slg::xm::Troop, slg::xm::Hero, slg::xm::Army,
                         slg::xm::City, slg::xm::PlayerState, slg::xm::Event, slg::xm::Report,
                         slg::xm::BattleMsg, slg::xm::Tile, slg::xm::March, slg::xm::ShardState)
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
#undef SLG_XM_ORDINARY_RECORD
#endif

XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::PlayerState, "slg.PlayerState.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::BattleMsg, "slg.BattleMsg.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::ShardState, "slg.ShardState.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xm::Report, "slg.Report.v1")
