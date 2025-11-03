#ifndef GAME_DATA_HPP_
#define GAME_DATA_HPP_

#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Item - Direct Type Definition (C++26 Reflection)
// ============================================================================

class alignas(XTypeSignature::BASIC_ALIGNMENT) Item {
public:
	// Default constructor
	template <typename Allocator>
	Item(Allocator allocator) : name(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	Item(Allocator allocator, int item_id_val, int item_type_val, int quantity_val, const char* name_val)
		: item_id(item_id_val)
		, item_type(item_type_val)
		, quantity(quantity_val)
		, name(name_val, allocator)
	{}

	int32_t item_id{0};
	int32_t item_type{0};
	int32_t quantity{0};
	XString name;
};

// ============================================================================
// GameData - Direct Type Definition (C++26 Reflection)
// ============================================================================

class alignas(XTypeSignature::BASIC_ALIGNMENT) GameData {
public:
	// Default constructor
	template <typename Allocator>
	GameData(Allocator allocator) 
		: player_name(allocator)
		, items(allocator)
		, achievements(allocator)
		, quest_progress(allocator) 
	{}

	// Full constructor for emplace_back
	template <typename Allocator>
	GameData(Allocator allocator, int player_id_val, int level_val, float health_val, const char* player_name_val)
		: player_id(player_id_val)
		, level(level_val)
		, health(health_val)
		, player_name(player_name_val, allocator)
		, items(allocator)
		, achievements(allocator)
		, quest_progress(allocator)
	{}

	int32_t player_id{0};
	int32_t level{0};
	float health{0.0f};
	XString player_name;
	XVector<Item> items;
	XSet<int32_t> achievements;
	XMap<XString, int32_t> quest_progress;
};

// Item type signature validation
static_assert(XTypeSignature::get_XTypeSignature<Item>() == 
             "struct[s:48,a:8]{"
             "@0:i32[s:4,a:4],"
             "@4:i32[s:4,a:4],"
             "@8:i32[s:4,a:4],"
             "@16:string[s:32,a:8]}",
              "Type signature mismatch for Item - "
              "Binary layout changed! This breaks serialization compatibility.");

// GameData type signature validation
static_assert(XTypeSignature::get_XTypeSignature<GameData>() ==
             "struct[s:144,a:8]{"
             "@0:i32[s:4,a:4],"
             "@4:i32[s:4,a:4],"
             "@8:f32[s:4,a:4],"
             "@16:string[s:32,a:8],"
             "@48:vector[s:32,a:8]<struct[s:48,a:8]{"
                 "@0:i32[s:4,a:4],"
                 "@4:i32[s:4,a:4],"
                 "@8:i32[s:4,a:4],"
                 "@16:string[s:32,a:8]}>,"
             "@80:set[s:32,a:8]<i32[s:4,a:4]>,"
             "@112:map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>}",
              "Type signature mismatch for GameData - "
              "Binary layout changed! This breaks serialization compatibility.");

#endif // GAME_DATA_HPP_
