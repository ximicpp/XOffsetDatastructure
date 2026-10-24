// SLG prototype: model_xoffset.hpp's model again (namespace slg::xmr), with every
// record type declaring XOffsetDatastructure::wire_record_layout<T>::ordinary_members
// (the source-level promise that all members are ordinary subobjects, no
// [[no_unique_address]]; audited: plain members only). begin_verified_receive needs
// this declaration for every type it receives. The declaration also enables the
// library's other paths that rely on it (e.g. shift-relocating erase of record
// vectors), so "xoffset_optin" (this model, reload_verified) is reported next to
// "xoffset" (the original model) and "xoffset_direct" (this model, direct receive).
#pragma once
#include <cstdint>
#include <type_traits>
#include "xoffsetdatastructure.hpp"
#include "xoffset_verified_receive.hpp"

namespace slg::xmr {
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

} // namespace slg::xmr

namespace XOffsetDatastructure {
template<> struct wire_record_layout<slg::xmr::Buff> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<slg::xmr::Skill> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<slg::xmr::Troop> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<slg::xmr::Hero> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<slg::xmr::Army> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<slg::xmr::City> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<slg::xmr::PlayerState> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<slg::xmr::Event> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<slg::xmr::Report> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<slg::xmr::BattleMsg> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<slg::xmr::Tile> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<slg::xmr::March> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<slg::xmr::ShardState> { static constexpr bool ordinary_members = true; };
template<> struct StableEraseOrdinaryMembers<slg::xmr::March> : std::true_type {};
}

XOFFSET_REGISTER_SCHEMA_NAME(slg::xmr::PlayerState, "slg.PlayerState.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xmr::BattleMsg, "slg.BattleMsg.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xmr::ShardState, "slg.ShardState.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg::xmr::Report, "slg.Report.v1")
