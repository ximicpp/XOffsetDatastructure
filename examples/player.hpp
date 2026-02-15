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

// Full type signature validation using boost::typelayout (Definition Signature)
static_assert(boost::typelayout::get_definition_signature<Player>() ==
             "[64-le]record[s:72,a:8]{"
             "@0[id]:i32[s:4,a:4],"
             "@4[level]:i32[s:4,a:4],"
             "@8[name]:string[s:32,a:8],"
             "@40[items]:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for Player - "
              "Binary layout changed! This breaks serialization compatibility.");

#endif // PLAYER_HPP_