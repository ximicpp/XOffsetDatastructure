#ifndef GENERATED_GAME_DATA_HPP_
#define GENERATED_GAME_DATA_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) Item {
	template <typename Allocator>
	Item(Allocator allocator) : name(allocator) {}
	int item_id{0};
	int item_type{0};
	int quantity{0};
	XString name;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) GameData {
	template <typename Allocator>
	GameData(Allocator allocator) : player_name(allocator), items(allocator), achievements(allocator), quest_progress(allocator) {}
	int player_id{0};
	int level{0};
	float health{0.0f};
	XString player_name;
	XVector<Item> items;
	XSet<int> achievements;
	XMap<XString, int> quest_progress;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) ItemReflectionHint {
	int32_t item_id;
	int32_t item_type;
	int32_t quantity;
	XString name;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) GameDataReflectionHint {
	int32_t player_id;
	int32_t level;
	float health;
	XString player_name;
	XVector<ItemReflectionHint> items;
	XSet<int32_t> achievements;
	XMap<XString, int32_t> quest_progress;
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Type signature validation for Item
// This ensures binary compatibility across compilations

static_assert(sizeof(Item) == sizeof(ItemReflectionHint),
              "Size mismatch: Item runtime and reflection types must have identical size");
static_assert(alignof(Item) == alignof(ItemReflectionHint),
              "Alignment mismatch: Item runtime and reflection types must have identical alignment");

// Expected type signature for ItemReflectionHint:
// This signature is computed at compile-time by XTypeSignature::get_XTypeSignature<>
// Format: struct[s:<size>,a:<align>]{@<offset>:<field_type>,...}
//
// You can verify the actual signature by uncommenting this line:
// #pragma message(XTypeSignature::get_XTypeSignature<ItemReflectionHint>().value)

// Type signature validation for GameData
// This ensures binary compatibility across compilations

static_assert(sizeof(GameData) == sizeof(GameDataReflectionHint),
              "Size mismatch: GameData runtime and reflection types must have identical size");
static_assert(alignof(GameData) == alignof(GameDataReflectionHint),
              "Alignment mismatch: GameData runtime and reflection types must have identical alignment");

// Expected type signature for GameDataReflectionHint:
// This signature is computed at compile-time by XTypeSignature::get_XTypeSignature<>
// Format: struct[s:<size>,a:<align>]{@<offset>:<field_type>,...}
//
// You can verify the actual signature by uncommenting this line:
// #pragma message(XTypeSignature::get_XTypeSignature<GameDataReflectionHint>().value)

#endif // GENERATED_GAME_DATA_HPP_
