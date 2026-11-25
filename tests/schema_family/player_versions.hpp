// Three versions of one type tree for the add-only family tests
// (docs/SCHEMA_EVOLUTION.md). v2 takes fields from v1's XSpare
// bytes and gives the list v1 kept for later a record type; v3 takes more
// fields from what v2 left spare. Every version registers the same schema
// name and declares the one before it, so all three have one fingerprint.
// Field lists, so every build (C++20 clients too) uses them.
#pragma once

#include "../../xoffsetdatastructure.hpp"

#include <cstdint>

namespace fam {
namespace xo = XOffsetDatastructure;
enum class Class : std::uint8_t { none = 0, warrior = 1, mage = 2 };

namespace v1 {
struct Stats {
    std::int32_t atk{};
    std::int32_t def{};
    xo::XSpare<8> spare{};
};
struct Hero {
    std::int32_t id{};
    std::int32_t level{};
    xo::XString name;
    xo::XVector<std::int32_t> skills;
    Stats stats;
    xo::XSpare<8> spare{};
};
struct Player {
    std::uint64_t uid{};
    std::int32_t gold{};
    std::int32_t gems{};
    xo::XString nick;
    xo::XVector<Hero> heroes;
    xo::XMap<std::int32_t, Stats> buffs;
    xo::XVector<xo::XSpare<16>> runes;   // kept for later
    xo::XSpare<16> spare{};
};
} // namespace v1

namespace v2 {
struct Rune {
    std::int64_t id{};
    std::int32_t kind{};
    std::int32_t level{};
};
struct Stats {
    std::int32_t atk{};
    std::int32_t def{};
    std::int32_t crit{};                  // new
    xo::XSpare<4> spare{};
};
struct Hero {
    std::int32_t id{};
    std::int32_t level{};
    xo::XString name;
    xo::XVector<std::int32_t> skills;
    Stats stats;
    std::uint8_t star{};                  // new
    bool awake{};                         // new
    xo::XSpare<6> spare{};
};
struct Player {
    std::uint64_t uid{};
    std::int32_t gold{};
    std::int32_t gems{};
    xo::XString nick;
    xo::XVector<Hero> heroes;
    xo::XMap<std::int32_t, Stats> buffs;
    xo::XVector<Rune> runes;              // the kept list gets its records
    std::int32_t vip{};                   // new
    xo::XSpare<12> spare{};
};
} // namespace v2

namespace v3 {
using Rune = v2::Rune;
struct Stats {
    std::int32_t atk{};
    std::int32_t def{};
    std::int32_t crit{};
    std::int32_t dodge{};                 // new; nothing spare left
};
struct Hero {
    std::int32_t id{};
    std::int32_t level{};
    xo::XString name;
    xo::XVector<std::int32_t> skills;
    Stats stats;
    std::uint8_t star{};
    bool awake{};
    std::uint16_t skin{};                 // new
    Class cls{};                          // new
    xo::XSpare<3> spare{};
};
struct Player {
    std::uint64_t uid{};
    std::int32_t gold{};
    std::int32_t gems{};
    xo::XString nick;
    xo::XVector<Hero> heroes;
    xo::XMap<std::int32_t, Stats> buffs;
    xo::XVector<Rune> runes;
    std::int32_t vip{};
    std::uint32_t flags{};                // new
    std::int64_t guild{};                 // new
};
} // namespace v3

// Another add-only successor of v1, beside v2 (a diverging branch).
namespace v2b {
using v1::Stats;
using v1::Hero;
struct Player {
    std::uint64_t uid{};
    std::int32_t gold{};
    std::int32_t gems{};
    xo::XString nick;
    xo::XVector<Hero> heroes;
    xo::XMap<std::int32_t, Stats> buffs;
    xo::XVector<xo::XSpare<16>> runes;
    std::int64_t season{};                // new, other than v2's
    xo::XSpare<8> spare{};
};
} // namespace v2b

// The layout of v1 under the same name, declared as no family's member: the
// same fingerprint as v1 (the first version keeps its own).
namespace solo {
using v1::Stats;
using v1::Hero;
struct Player {
    std::uint64_t uid{};
    std::int32_t gold{};
    std::int32_t gems{};
    xo::XString nick;
    xo::XVector<Hero> heroes;
    xo::XMap<std::int32_t, Stats> buffs;
    xo::XVector<xo::XSpare<16>> runes;
    xo::XSpare<16> spare{};
};
} // namespace solo
} // namespace fam

XOFFSET_RECORD(fam::v1::Stats, atk, def, spare)
XOFFSET_RECORD(fam::v1::Hero, id, level, name, skills, stats, spare)
XOFFSET_RECORD(fam::v1::Player, uid, gold, gems, nick, heroes, buffs, runes, spare)
XOFFSET_RECORD(fam::v2::Rune, id, kind, level)
XOFFSET_RECORD(fam::v2::Stats, atk, def, crit, spare)
XOFFSET_RECORD(fam::v2::Hero, id, level, name, skills, stats, star, awake, spare)
XOFFSET_RECORD(fam::v2::Player, uid, gold, gems, nick, heroes, buffs, runes, vip, spare)
XOFFSET_RECORD(fam::v3::Stats, atk, def, crit, dodge)
XOFFSET_RECORD(fam::v3::Hero, id, level, name, skills, stats, star, awake, skin, cls, spare)
XOFFSET_RECORD(fam::v3::Player, uid, gold, gems, nick, heroes, buffs, runes, vip, flags, guild)
XOFFSET_RECORD(fam::v2b::Player, uid, gold, gems, nick, heroes, buffs, runes, season, spare)
XOFFSET_RECORD(fam::solo::Player, uid, gold, gems, nick, heroes, buffs, runes, spare)

XOFFSET_REGISTER_SCHEMA_NAME(fam::v1::Player, "test.family.Player")
XOFFSET_REGISTER_SCHEMA_NAME(fam::v2::Player, "test.family.Player")
XOFFSET_REGISTER_SCHEMA_NAME(fam::v3::Player, "test.family.Player")
XOFFSET_REGISTER_SCHEMA_NAME(fam::v2b::Player, "test.family.Player")
XOFFSET_REGISTER_SCHEMA_NAME(fam::solo::Player, "test.family.Player")

XOFFSET_SCHEMA_FAMILY(fam::v2::Player, fam::v1::Player)
XOFFSET_SCHEMA_FAMILY(fam::v3::Player, fam::v2::Player)
XOFFSET_SCHEMA_FAMILY(fam::v2b::Player, fam::v1::Player)
