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