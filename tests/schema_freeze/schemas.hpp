// Two releases of a small schema for test_schema_freeze: game1 is release 1,
// game2 release 2 (Item and Rarity unchanged, Player changed).
#ifndef XOFFSET_TEST_SCHEMA_FREEZE_SCHEMAS_HPP
#define XOFFSET_TEST_SCHEMA_FREEZE_SCHEMAS_HPP
#include "xoffset_evolution.hpp"

namespace game1 {
using namespace XOffsetDatastructure;
enum class Rarity : std::uint8_t { common = 0, rare = 1, legendary = 9 };
struct Item { std::int32_t id; Rarity rarity; std::uint8_t flags[3]; XString label; };
struct Player {
    XString name;
    std::int32_t level;
    std::int32_t slots[2][3];
    XVector<Item> items;
    XMap<std::int32_t, XString> notes;
    XSet<Rarity> seen;
};
} // namespace game1

namespace game2 {
using namespace XOffsetDatastructure;
using game1::Rarity;
using game1::Item;
struct Player {
    XString name;
    std::int64_t level;
    std::int32_t slots[2][3];
    XVector<Item> items;
    XMap<std::int32_t, XString> notes;
    XSet<Rarity> seen;
    XVector<std::byte> blob;
};
} // namespace game2

XOFFSET_REGISTER_SCHEMA_NAME(game1::Player, "test.freeze.Player")
XOFFSET_REGISTER_SCHEMA_NAME(game2::Player, "test.freeze.Player")
#endif
