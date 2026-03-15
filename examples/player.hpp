#ifndef PLAYER_HPP_
#define PLAYER_HPP_

#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// ============================================================================
// Player — Zero-Boilerplate Type Definition (C++26 Reflection)
//
// No constructors, no macros, no typedefs needed!
// C++26 reflection automatically handles:
//   - Root object construction (XBuffer::make<Player>())
//   - Container element construction (XVector<Player>::emplace_back())
//   - Move during vector reallocation (allocator auto-injected)
// ============================================================================

struct Player {
	int32_t id{0};
	int32_t level{0};
	XString name;
	XVector<int32_t> items;
};

// Type signature validation deferred to runtime/CI — layout signature
// depends on platform-specific sizeof(XString)/sizeof(XVector).
// Use tools/export_signatures + tools/check_compat for cross-platform validation.

#endif // PLAYER_HPP_