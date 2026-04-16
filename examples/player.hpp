#ifndef PLAYER_HPP_
#define PLAYER_HPP_

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// Plain aggregate used by examples, tests, and signature export.
struct Player {
	int32_t id{0};
	int32_t level{0};
	XString name;
	XVector<int32_t> items;
};

#endif // PLAYER_HPP_
