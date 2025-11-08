#ifndef GENERATED_MAP_SET_TEST_HPP_
#define GENERATED_MAP_SET_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) MapSetTest {
	// Default constructor
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

// Compile-time validation for MapSetTest

// 1. Type Safety Check
static_assert(XOffsetDatastructure2::is_xbuffer_safe<MapSetTestReflectionHint>::value,
              "Type safety error for MapSetTestReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(MapSetTest) == sizeof(MapSetTestReflectionHint),
              "Size mismatch: MapSetTest runtime and reflection types must have identical size");
static_assert(alignof(MapSetTest) == alignof(MapSetTestReflectionHint),
              "Alignment mismatch: MapSetTest runtime and reflection types must have identical alignment");

// 3. Type Signature Check (disabled on MSVC)
// Type signature verification disabled on MSVC due to deep template instantiation issues
// with Boost.PFR reflection on aggregate types containing XString in containers.
// See: https://github.com/boostorg/pfr/issues
#ifndef _MSC_VER
static_assert(XTypeSignature::get_XTypeSignature<MapSetTestReflectionHint>() ==
             "struct[s:128,a:8]{"
             "@0:set[s:32,a:8]<i32[s:4,a:4]>,"
             "@32:set[s:32,a:8]<string[s:32,a:8]>,"
             "@64:map[s:32,a:8]<i32[s:4,a:4],string[s:32,a:8]>,"
             "@96:map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>}",
              "Type signature mismatch for MapSetTestReflectionHint");

#endif // _MSC_VER

#endif // GENERATED_MAP_SET_TEST_HPP_
