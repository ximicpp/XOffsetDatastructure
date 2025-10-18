#ifndef GENERATED_VECTOR_TEST_HPP_
#define GENERATED_VECTOR_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) VectorTest {
	template <typename Allocator>
	VectorTest(Allocator allocator) : intVector(allocator), floatVector(allocator), stringVector(allocator) {}
	XVector<int> intVector;
	XVector<float> floatVector;
	XVector<XString> stringVector;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) VectorTestReflectionHint {
	XVector<int32_t> intVector;
	XVector<float> floatVector;
	XVector<XString> stringVector;
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Type signature validation for VectorTest
// This ensures binary compatibility across compilations

static_assert(sizeof(VectorTest) == sizeof(VectorTestReflectionHint),
              "Size mismatch: VectorTest runtime and reflection types must have identical size");
static_assert(alignof(VectorTest) == alignof(VectorTestReflectionHint),
              "Alignment mismatch: VectorTest runtime and reflection types must have identical alignment");

// Expected type signature for VectorTestReflectionHint:
// This signature is computed at compile-time by XTypeSignature::get_XTypeSignature<>
// Format: struct[s:<size>,a:<align>]{@<offset>:<field_type>,...}
//
// You can verify the actual signature by uncommenting this line:
// #pragma message(XTypeSignature::get_XTypeSignature<VectorTestReflectionHint>().value)

#endif // GENERATED_VECTOR_TEST_HPP_
