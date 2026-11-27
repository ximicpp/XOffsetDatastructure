// The schema of the game server example (docs/GAME_SERVER_ARCHITECTURE.md).
//
// In a real project this header lives in its own small library that the
// servers (C++26 reflection) and every client build (C++20, Windows /
// Android / iOS) include. It holds only data types, no behavior.
//
// One root type per entity kind: every player, city and alliance is its own
// TypedXBuffer. Entities refer to each other by id only (Player::city_id,
// Player::alliance_id, Alliance::members), never by a reference into another
// buffer, so each buffer can be saved, sent, migrated and loaded on its own.
//
// Evolution: every record ends with XSpare<N>, and a later release only adds
// fields inside those bytes. game_v1::Player is the Player of the previous
// release; game::Player took `vip_level` from its spare bytes and declares
// itself the next version with XOFFSET_SCHEMA_FAMILY. Both versions have one
// fingerprint and the same bytes: v1 clients keep working against v2
// servers, and v1 saves load as v2 with no conversion step
// (docs/SCHEMA_EVOLUTION.md).
#ifndef XOFFSET_EXAMPLES_GAME_SERVER_SCHEMA_HPP
#define XOFFSET_EXAMPLES_GAME_SERVER_SCHEMA_HPP

#include "../../xoffsetdatastructure.hpp"

#include <cstdint>

namespace game {
using namespace XOffsetDatastructure;

// ---- Nested records (unchanged since the first release) ---------------------

struct Hero {
    std::uint32_t id;
    std::int32_t level;
    std::int32_t star;
    XSpare<4> spare{};
};

// ---- Player: owned by one logic shard, synced to its own client -------------

struct Player {
    std::uint64_t player_id;
    std::uint64_t alliance_id;  // 0: none. An id, not a reference into the alliance's buffer
    std::uint64_t city_id;      // the player's city, its own buffer
    std::int64_t gold;
    std::int64_t food;
    std::int32_t level;
    XString name;
    XVector<Hero> heroes;
    XMap<std::uint32_t, std::int64_t> items;  // item id -> count
    std::int32_t vip_level;                   // added in v2, from the spare bytes of v1
    XSpare<12> spare{};
};

// ---- City: one per player, lives on the owner's shard -----------------------

enum class Building : std::uint32_t { town_hall = 1, farm = 2, barracks = 3 };

struct City {
    std::uint64_t city_id;
    std::uint64_t owner_id;
    std::int32_t x;
    std::int32_t y;
    std::int32_t level;
    XMap<std::uint32_t, std::int32_t> buildings;  // Building -> level
    XSpare<16> spare{};
};

// ---- Alliance: lives on the shard alliance_id selects -----------------------

struct Alliance {
    std::uint64_t alliance_id;
    std::uint64_t leader_id;
    XString name;
    XVector<std::uint64_t> members;  // player ids
    XString notice;
    XSpare<12> spare{};
};

// ---- Client -> server: one command per frame --------------------------------

enum class CommandKind : std::uint32_t { collect = 1, upgrade_building = 2, rename = 3, join_alliance = 4 };

struct Command {
    std::uint64_t player_id;  // must match the session; the server checks it
    std::uint32_t kind;       // CommandKind; any value can arrive, the server checks it
    std::uint32_t target;     // building for upgrade_building
    std::int64_t arg;         // alliance id for join_alliance
    XString text;             // new name for rename
    XSpare<12> spare{};
};

// ---- Static configuration: built offline, loaded read-only by every shard ---

struct BuildingConfig {
    std::uint32_t building;  // Building
    std::int32_t max_level;
    std::int64_t gold_per_level;  // upgrading to level L costs L * gold_per_level
    XSpare<8> spare{};
};

struct ConfigTable {
    std::uint32_t version;
    XVector<BuildingConfig> buildings;
    XSpare<16> spare{};
};

}  // namespace game

// ---- The previous release's Player (kept forever, see SCHEMA_EVOLUTION 5.3) --

namespace game_v1 {
using namespace XOffsetDatastructure;
using game::Hero;

struct Player {
    std::uint64_t player_id;
    std::uint64_t alliance_id;
    std::uint64_t city_id;
    std::int64_t gold;
    std::int64_t food;
    std::int32_t level;
    XString name;
    XVector<Hero> heroes;
    XMap<std::uint32_t, std::int64_t> items;
    XSpare<16> spare{};
};

}  // namespace game_v1

// ---- Field lists (the C++20 build reads them, the C++26 build checks them) ---

XOFFSET_RECORD(game::Hero, id, level, star, spare)
XOFFSET_RECORD(game::Player, player_id, alliance_id, city_id, gold, food, level, name, heroes, items, vip_level, spare)
XOFFSET_RECORD(game::City, city_id, owner_id, x, y, level, buildings, spare)
XOFFSET_RECORD(game::Alliance, alliance_id, leader_id, name, members, notice, spare)
XOFFSET_RECORD(game::Command, player_id, kind, target, arg, text, spare)
XOFFSET_RECORD(game::BuildingConfig, building, max_level, gold_per_level, spare)
XOFFSET_RECORD(game::ConfigTable, version, buildings, spare)
XOFFSET_RECORD(game_v1::Player, player_id, alliance_id, city_id, gold, food, level, name, heroes, items, spare)

XOFFSET_REGISTER_SCHEMA_NAME(game::Player, "game.Player")
XOFFSET_REGISTER_SCHEMA_NAME(game_v1::Player, "game.Player")  // the same name: one family
XOFFSET_REGISTER_SCHEMA_NAME(game::City, "game.City")
XOFFSET_REGISTER_SCHEMA_NAME(game::Alliance, "game.Alliance")
XOFFSET_REGISTER_SCHEMA_NAME(game::Command, "game.Command")
XOFFSET_REGISTER_SCHEMA_NAME(game::ConfigTable, "game.ConfigTable")

// v2 adds vip_level inside the spare bytes of v1. A change that breaks the
// add-only rule (a field moved, removed or retyped) fails to compile here.
XOFFSET_SCHEMA_FAMILY(game::Player, game_v1::Player)

// ---- Layout pins (generated by schema_pin<T>("name"); regenerate when a type
// changes). Every compiler that builds this header checks the layout against
// them, so a client toolchain that would lay a type out differently fails to
// compile (docs/SCHEMA_PINS.md).
XOFFSET_PIN_SCHEMA(game::Player, "record[s:96,a:8]{@0:u64,@8:u64,@16:u64,@24:i64,@32:i64,@40:i32,@44:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10},@56:xvector[s:12,a:4]{data:rel32,size:u32,capacity:u32}<record[s:16,a:4]{@0:u32,@4:i32,@8:i32,@12:spare[4]}>,@68:xflatmap[s:12,a:4]{data:rel32,size:u32,capacity:u32}<u32,i64>,@80:i32,@84:spare[12]}");
XOFFSET_PIN_SCHEMA(game_v1::Player, "record[s:96,a:8]{@0:u64,@8:u64,@16:u64,@24:i64,@32:i64,@40:i32,@44:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10},@56:xvector[s:12,a:4]{data:rel32,size:u32,capacity:u32}<record[s:16,a:4]{@0:u32,@4:i32,@8:i32,@12:spare[4]}>,@68:xflatmap[s:12,a:4]{data:rel32,size:u32,capacity:u32}<u32,i64>,@80:spare[16]}");
XOFFSET_PIN_SCHEMA(game::City, "record[s:56,a:8]{@0:u64,@8:u64,@16:i32,@20:i32,@24:i32,@28:xflatmap[s:12,a:4]{data:rel32,size:u32,capacity:u32}<u32,i32>,@40:spare[16]}");
XOFFSET_PIN_SCHEMA(game::Alliance, "record[s:64,a:8]{@0:u64,@8:u64,@16:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10},@28:xvector[s:12,a:4]{data:rel32,size:u32,capacity:u32}<u64>,@40:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10},@52:spare[12]}");
XOFFSET_PIN_SCHEMA(game::Command, "record[s:48,a:8]{@0:u64,@8:u32,@12:u32,@16:i64,@24:xstring[s:12,a:4]{data:rel32,size:u32,capacity:u32}{inline:10},@36:spare[12]}");
XOFFSET_PIN_SCHEMA(game::ConfigTable, "record[s:32,a:4]{@0:u32,@4:xvector[s:12,a:4]{data:rel32,size:u32,capacity:u32}<record[s:24,a:8]{@0:u32,@4:i32,@8:i64,@16:spare[8]}>,@16:spare[16]}");

#endif
