#ifndef GAME_DATA_HPP_
#define GAME_DATA_HPP_

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// Plain aggregate used by examples, tests, and signature export.
struct alignas(8) Item {
	int32_t item_id{0};
	int32_t item_type{0};
	int32_t quantity{0};
	XString name;
};

// Root example type spanning the supported container mix.
struct alignas(8) GameData {
	int32_t player_id{0};
	int32_t level{0};
	float health{0.0f};
	XString player_name;
	XVector<Item> items;
	XSet<int32_t> achievements;
	XMap<XString, int32_t> quest_progress;
};

#endif // GAME_DATA_HPP_
