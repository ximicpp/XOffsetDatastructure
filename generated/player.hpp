#ifndef GENERATED_PLAYER_HPP_
#define GENERATED_PLAYER_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
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

	int id{0};
	int level{0};
	XString name;
	XVector<int> items;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) PlayerReflectionHint {
	int32_t id;
	int32_t level;
	XString name;
	XVector<int32_t> items;
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Compile-time validation for Player

// 1. Type Safety Check
static_assert(XOffsetDatastructure2::is_xbuffer_safe<PlayerReflectionHint>::value,
              "Type safety error for PlayerReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(Player) == sizeof(PlayerReflectionHint),
              "Size mismatch: Player runtime and reflection types must have identical size");
static_assert(alignof(Player) == alignof(PlayerReflectionHint),
              "Alignment mismatch: Player runtime and reflection types must have identical alignment");

// 3. Type Signature Check (disabled on MSVC)
// Type signature verification disabled on MSVC due to deep template instantiation issues
// with Boost.PFR reflection on aggregate types containing XString in containers.
// See: https://github.com/boostorg/pfr/issues
#ifndef _MSC_VER
static_assert(XTypeSignature::get_XTypeSignature<PlayerReflectionHint>() ==
             "struct[s:72,a:8]{"
             "@0:i32[s:4,a:4],"
             "@4:i32[s:4,a:4],"
             "@8:string[s:32,a:8],"
             "@40:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for PlayerReflectionHint");

#endif // _MSC_VER

#endif // GENERATED_PLAYER_HPP_
