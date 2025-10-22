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

struct alignas(XTypeSignature::BASIC_ALIGNMENT) Player {
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

#if __cpp_reflection >= 202306L
// Full type signature validation using C++26 reflection
static_assert(XTypeSignature::get_XTypeSignature<Player>() ==
             "struct[s:72,a:8]{"
             "@0:i32[s:4,a:4],"
             "@4:i32[s:4,a:4],"
             "@8:string[s:32,a:8],"
             "@40:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for Player - "
              "Binary layout changed! This breaks serialization compatibility.");
#else
// Fallback: Basic size/alignment validation
static_assert(sizeof(Player) == 72, "Player size must be 72 bytes");
static_assert(alignof(Player) == 8, "Player alignment must be 8 bytes");

#warning "C++26 reflection not available. Using basic size/alignment validation only."
#warning "For full type signature validation, compile with -std=c++26 -freflection"
#endif

#endif // PLAYER_HPP_
