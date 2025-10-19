#ifndef GENERATED_VECTOR_TEST_HPP_
#define GENERATED_VECTOR_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) VectorTest {
	// Default constructor
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

static_assert(XTypeSignature::get_XTypeSignature<VectorTestReflectionHint>() ==
             "struct[s:96,a:8]{"
             "@0:vector[s:32,a:8]<i32[s:4,a:4]>,"
             "@32:vector[s:32,a:8]<f32[s:4,a:4]>,"
             "@64:vector[s:32,a:8]<string[s:32,a:8]>}",
              "Type signature mismatch for VectorTestReflectionHint");

#endif // GENERATED_VECTOR_TEST_HPP_
