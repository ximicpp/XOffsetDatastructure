#ifndef GENERATED_TEST_TYPES_HPP_
#define GENERATED_TEST_TYPES_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestTypeInner {
	template <typename Allocator>
	TestTypeInner(Allocator allocator) : mVector(allocator) {}
	int mInt{0};
	XVector<int> mVector;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestType {
	template <typename Allocator>
	TestType(Allocator allocator) : mVector(allocator), mStringVector(allocator), TestTypeInnerObj(allocator), mXXTypeVector(allocator), mComplexMap(allocator), mStringSet(allocator), mSet(allocator), mString(allocator) {}
	int mInt{0};
	float mFloat{0.0f};
	XVector<int> mVector;
	XVector<XString> mStringVector;
	TestTypeInner TestTypeInnerObj;
	XVector<TestTypeInner> mXXTypeVector;
	XMap<XString, TestTypeInner> mComplexMap;
	XSet<XString> mStringSet;
	XSet<int> mSet;
	XString mString;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestTypeInnerReflectionHint {
	int32_t mInt;
	XVector<int32_t> mVector;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestTypeReflectionHint {
	int32_t mInt;
	float mFloat;
	XVector<int32_t> mVector;
	XVector<XString> mStringVector;
	TestTypeInnerReflectionHint TestTypeInnerObj;
	XVector<TestTypeInnerReflectionHint> mXXTypeVector;
	XMap<XString, TestTypeInnerReflectionHint> mComplexMap;
	XSet<XString> mStringSet;
	XSet<int32_t> mSet;
	XString mString;
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Type signature validation for TestTypeInner
// This ensures binary compatibility across compilations

static_assert(sizeof(TestTypeInner) == sizeof(TestTypeInnerReflectionHint),
              "Size mismatch: TestTypeInner runtime and reflection types must have identical size");
static_assert(alignof(TestTypeInner) == alignof(TestTypeInnerReflectionHint),
              "Alignment mismatch: TestTypeInner runtime and reflection types must have identical alignment");

// Expected type signature for TestTypeInnerReflectionHint:
// This signature is computed at compile-time by XTypeSignature::get_XTypeSignature<>
// Format: struct[s:<size>,a:<align>]{@<offset>:<field_type>,...}
//
// You can verify the actual signature by uncommenting this line:
// #pragma message(XTypeSignature::get_XTypeSignature<TestTypeInnerReflectionHint>().value)

// Type signature validation for TestType
// This ensures binary compatibility across compilations

static_assert(sizeof(TestType) == sizeof(TestTypeReflectionHint),
              "Size mismatch: TestType runtime and reflection types must have identical size");
static_assert(alignof(TestType) == alignof(TestTypeReflectionHint),
              "Alignment mismatch: TestType runtime and reflection types must have identical alignment");

// Expected type signature for TestTypeReflectionHint:
// This signature is computed at compile-time by XTypeSignature::get_XTypeSignature<>
// Format: struct[s:<size>,a:<align>]{@<offset>:<field_type>,...}
//
// You can verify the actual signature by uncommenting this line:
// #pragma message(XTypeSignature::get_XTypeSignature<TestTypeReflectionHint>().value)

#endif // GENERATED_TEST_TYPES_HPP_
