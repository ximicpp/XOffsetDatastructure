#ifndef GENERATED_MODIFY_TEST_HPP_
#define GENERATED_MODIFY_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) ModifyTestData {
	template <typename Allocator>
	ModifyTestData(Allocator allocator) : numbers(allocator), names(allocator), scores(allocator), tags(allocator) {}
	int counter{0};
	float ratio{0.0f};
	bool active{false};
	XVector<int> numbers;
	XVector<XString> names;
	XMap<XString, int> scores;
	XSet<int> tags;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) ModifyTestDataReflectionHint {
	int32_t counter;
	float ratio;
	bool active;
	XVector<int32_t> numbers;
	XVector<XString> names;
	XMap<XString, int32_t> scores;
	XSet<int32_t> tags;
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Type signature validation for ModifyTestData
// This ensures binary compatibility across compilations

static_assert(sizeof(ModifyTestData) == sizeof(ModifyTestDataReflectionHint),
              "Size mismatch: ModifyTestData runtime and reflection types must have identical size");
static_assert(alignof(ModifyTestData) == alignof(ModifyTestDataReflectionHint),
              "Alignment mismatch: ModifyTestData runtime and reflection types must have identical alignment");

// Expected type signature for ModifyTestDataReflectionHint:
// This signature is computed at compile-time by XTypeSignature::get_XTypeSignature<>
// Format: struct[s:<size>,a:<align>]{@<offset>:<field_type>,...}
//
// You can verify the actual signature by uncommenting this line:
// #pragma message(XTypeSignature::get_XTypeSignature<ModifyTestDataReflectionHint>().value)

#endif // GENERATED_MODIFY_TEST_HPP_
