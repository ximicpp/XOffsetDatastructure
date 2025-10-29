#ifndef PLAYER_HPP_
#define PLAYER_HPP_

#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Player - Direct Type Definition (C++26 Reflection)
// ============================================================================
// Note: In next_cpp26, we don't need separate ReflectionHint types!
// C++26 reflection can directly analyze types with constructors.
// ============================================================================

// struct alignas(XTypeSignature::BASIC_ALIGNMENT) Player {
class alignas(8) Player {
public:
	// Default constructor
	template <typename Allocator>
	Player(Allocator allocator) : name(allocator), items(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	Player(Allocator allocator, int id_val, int level_val, const char* name_val)
		: id(id_val)
		, level(level_val)
		, name(name_val, allocator)
		, items(allocator)
	{}

	int32_t id{0};
	int32_t level{0};
	XString name;
	XVector<int32_t> items;
};

// ============================================================================
// Compile-Time Type Signature Validation (C++26 Reflection)
// ============================================================================
// Using C++26 reflection, we can directly validate the Player type
// without needing a separate ReflectionHint aggregate type.
//
// When __cpp_reflection >= 202306L:
//   - std::meta::members_of(^Player) works even with constructors
//   - No need for separate aggregate types
//   - Direct field introspection using template for
// ============================================================================

// Type safety validation
static_assert(is_xbuffer_safe<Player>::value, 
              "Player must be safe for XBuffer");

// Full type signature validation using C++26 reflection
static_assert(XTypeSignature::get_XTypeSignature<Player>() ==
             "struct[s:72,a:8]{"
             "@0:i32[s:4,a:4],"
             "@4:i32[s:4,a:4],"
             "@8:string[s:32,a:8],"
             "@40:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for Player - "
              "Binary layout changed! This breaks serialization compatibility.");

#endif // PLAYER_HPP_
