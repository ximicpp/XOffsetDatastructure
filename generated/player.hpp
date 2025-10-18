#ifndef GENERATED_PLAYER_HPP_
#define GENERATED_PLAYER_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) Player {
	template <typename Allocator>
	Player(Allocator allocator) : name(allocator), items(allocator) {}
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

// Type signature validation for Player
// This ensures binary compatibility across compilations

static_assert(sizeof(Player) == sizeof(PlayerReflectionHint),
              "Size mismatch: Player runtime and reflection types must have identical size");
static_assert(alignof(Player) == alignof(PlayerReflectionHint),
              "Alignment mismatch: Player runtime and reflection types must have identical alignment");

// Expected type signature for PlayerReflectionHint:
// This signature is computed at compile-time by XTypeSignature::get_XTypeSignature<>
// Format: struct[s:<size>,a:<align>]{@<offset>:<field_type>,...}
//
// You can verify the actual signature by uncommenting this line:
// #pragma message(XTypeSignature::get_XTypeSignature<PlayerReflectionHint>().value)

#endif // GENERATED_PLAYER_HPP_
