#ifndef GAME_DATA_HPP_
#define GAME_DATA_HPP_

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// ============================================================================
// Item — Zero-Boilerplate Type Definition (C++26 Reflection)
//
// Pure aggregate: no constructors, no macros, no typedefs.
// Works as both root object and XVector element automatically.
// ============================================================================

struct alignas(8) Item {
	int32_t item_id{0};
	int32_t item_type{0};
	int32_t quantity{0};
	XString name;
};

// ============================================================================
// GameData — Zero-Boilerplate Type Definition (C++26 Reflection)
//
// Complex type with XString, XVector<Item>, XSet, and XMap members.
// All allocator plumbing is handled by C++26 reflection — the user
// just writes a plain struct.
// ============================================================================

struct alignas(8) GameData {
	int32_t player_id{0};
	int32_t level{0};
	float health{0.0f};
	XString player_name;
	XVector<Item> items;
	XSet<int32_t> achievements;
	XMap<XString, int32_t> quest_progress;
};

// Type signature validation deferred to runtime/CI — layout signature
// depends on platform-specific sizeof(XString)/sizeof(XVector).
// Use tools/export_signatures + tools/check_compat for cross-platform validation.

#endif // GAME_DATA_HPP_