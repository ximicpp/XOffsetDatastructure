#ifndef GAME_DATA_HPP_
#define GAME_DATA_HPP_

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// ============================================================================
// Item - Direct Type Definition (C++26 Reflection)
// ============================================================================

class alignas(8) Item {
public:
	using allocator_type = XAllocator;

	// Default allocator constructor (suffix mode — required by uses_allocator protocol)
	template <typename Allocator>
		requires (!std::is_same_v<std::decay_t<Allocator>, std::allocator_arg_t>)
	Item(Allocator allocator) : name(allocator) {}

	// Full constructor (suffix mode — allocator last)
	template <typename Allocator>
	Item(int item_id_val, int item_type_val, int quantity_val, const char* name_val, Allocator allocator)
		: item_id(item_id_val)
		, item_type(item_type_val)
		, quantity(quantity_val)
		, name(name_val, allocator)
	{}

	// Move + allocator constructor (required for vector reallocation)
	template <typename Allocator>
	Item(Item&& other, Allocator allocator)
		: item_id(other.item_id)
		, item_type(other.item_type)
		, quantity(other.quantity)
		, name(std::move(other.name), allocator)
	{}

	int32_t item_id{0};
	int32_t item_type{0};
	int32_t quantity{0};
	XString name;
};

// ============================================================================
// GameData - Direct Type Definition (C++26 Reflection)
// ============================================================================

class alignas(8) GameData {
public:
	using allocator_type = XAllocator;

	// Allocator constructor (suffix mode — standard uses_allocator protocol)
	template <typename Allocator>
		requires (!std::is_same_v<std::decay_t<Allocator>, std::allocator_arg_t>)
	GameData(Allocator allocator) 
		: player_name(allocator)
		, items(allocator)
		, achievements(allocator)
		, quest_progress(allocator) 
	{}

	// Move + allocator constructor (required for vector reallocation)
	template <typename Allocator>
	GameData(GameData&& other, Allocator allocator)
		: player_id(other.player_id)
		, level(other.level)
		, health(other.health)
		, player_name(std::move(other.player_name), allocator)
		, items(std::move(other.items), allocator)
		, achievements(std::move(other.achievements), allocator)
		, quest_progress(std::move(other.quest_progress), allocator)
	{}

	int32_t player_id{0};
	int32_t level{0};
	float health{0.0f};
	XString player_name;
	XVector<Item> items;
	XSet<int32_t> achievements;
	XMap<XString, int32_t> quest_progress;
};

// Item type signature validation using boost::typelayout (Definition Signature)
static_assert(boost::typelayout::get_definition_signature<Item>() == 
             "[64-le]record[s:48,a:8]{"
             "@0[item_id]:i32[s:4,a:4],"
             "@4[item_type]:i32[s:4,a:4],"
             "@8[quantity]:i32[s:4,a:4],"
             "@16[name]:string[s:32,a:8]}",
              "Type signature mismatch for Item - "
              "Binary layout changed! This breaks serialization compatibility.");

// GameData type signature validation using boost::typelayout (Definition Signature)
static_assert(boost::typelayout::get_definition_signature<GameData>() ==
             "[64-le]record[s:144,a:8]{"
             "@0[player_id]:i32[s:4,a:4],"
             "@4[level]:i32[s:4,a:4],"
             "@8[health]:f32[s:4,a:4],"
             "@16[player_name]:string[s:32,a:8],"
             "@48[items]:vector[s:32,a:8]<record[s:48,a:8]{"
                 "@0[item_id]:i32[s:4,a:4],"
                 "@4[item_type]:i32[s:4,a:4],"
                 "@8[quantity]:i32[s:4,a:4],"
                 "@16[name]:string[s:32,a:8]}>,"
             "@80[achievements]:set[s:32,a:8]<i32[s:4,a:4]>,"
             "@112[quest_progress]:map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>}",
              "Type signature mismatch for GameData - "
              "Binary layout changed! This breaks serialization compatibility.");

#endif // GAME_DATA_HPP_
