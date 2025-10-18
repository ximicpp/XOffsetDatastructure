#ifndef GENERATED_MAP_SET_TEST_HPP_
#define GENERATED_MAP_SET_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) MapSetTest {
	template <typename Allocator>
	MapSetTest(Allocator allocator) : intSet(allocator), stringSet(allocator), intMap(allocator), stringMap(allocator) {}
	XSet<int> intSet;
	XSet<XString> stringSet;
	XMap<int, XString> intMap;
	XMap<XString, int> stringMap;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) MapSetTestReflectionHint {
	XSet<int32_t> intSet;
	XSet<XString> stringSet;
	XMap<int32_t, XString> intMap;
	XMap<XString, int32_t> stringMap;
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Type signature validation for MapSetTest
// This ensures binary compatibility across compilations

static_assert(sizeof(MapSetTest) == sizeof(MapSetTestReflectionHint),
              "Size mismatch: MapSetTest runtime and reflection types must have identical size");
static_assert(alignof(MapSetTest) == alignof(MapSetTestReflectionHint),
              "Alignment mismatch: MapSetTest runtime and reflection types must have identical alignment");

// Expected type signature for MapSetTestReflectionHint:
// This signature is computed at compile-time by XTypeSignature::get_XTypeSignature<>
// Format: struct[s:<size>,a:<align>]{@<offset>:<field_type>,...}
//
// You can verify the actual signature by uncommenting this line:
// #pragma message(XTypeSignature::get_XTypeSignature<MapSetTestReflectionHint>().value)

#endif // GENERATED_MAP_SET_TEST_HPP_
