#ifndef GENERATED_TEST_TYPES_HPP_
#define GENERATED_TEST_TYPES_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestTypeInner {
	// Default constructor
	template <typename Allocator>
	TestTypeInner(Allocator allocator) : mVector(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	TestTypeInner(Allocator allocator, int mInt_val)
		: mInt(mInt_val)
		, mVector(allocator)
	{}

	int mInt{0};
	XVector<int> mVector;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestType {
	// Default constructor
	template <typename Allocator>
	TestType(Allocator allocator) : mVector(allocator), mStringVector(allocator), TestTypeInnerObj(allocator), mXXTypeVector(allocator), mComplexMap(allocator), mStringSet(allocator), mSet(allocator), mString(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	TestType(Allocator allocator, int mInt_val, float mFloat_val, const char* mString_val)
		: mInt(mInt_val)
		, mFloat(mFloat_val)
		, mVector(allocator)
		, mStringVector(allocator)
		, TestTypeInnerObj(allocator)
		, mXXTypeVector(allocator)
		, mComplexMap(allocator)
		, mStringSet(allocator)
		, mSet(allocator)
		, mString(mString_val, allocator)
	{}

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

	// Field names metadata for XTypeSignature
	static constexpr std::string_view _field_names[] = {
		"mInt",
		"mVector",
	};
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

	// Field names metadata for XTypeSignature
	static constexpr std::string_view _field_names[] = {
		"mInt",
		"mFloat",
		"mVector",
		"mStringVector",
		"TestTypeInnerObj",
		"mXXTypeVector",
		"mComplexMap",
		"mStringSet",
		"mSet",
		"mString",
	};
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Compile-time validation for TestTypeInner

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<TestTypeInnerReflectionHint>::value,
              "Type safety error for TestTypeInnerReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(TestTypeInner) == sizeof(TestTypeInnerReflectionHint),
              "Size mismatch: TestTypeInner runtime and reflection types must have identical size");
static_assert(alignof(TestTypeInner) == alignof(TestTypeInnerReflectionHint),
              "Alignment mismatch: TestTypeInner runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification uses unified Boost.PFR implementation
// All compilers use lightweight tuple_element and tuple_size_v APIs
static_assert(XTypeSignature::get_XTypeSignature<TestTypeInnerReflectionHint>() == "struct[s:40,a:8]{@0[mInt]:i32[s:4,a:4],@8[mVector]:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for TestTypeInnerReflectionHint");

// Compile-time validation for TestType

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<TestTypeReflectionHint>::value,
              "Type safety error for TestTypeReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(TestType) == sizeof(TestTypeReflectionHint),
              "Size mismatch: TestType runtime and reflection types must have identical size");
static_assert(alignof(TestType) == alignof(TestTypeReflectionHint),
              "Alignment mismatch: TestType runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification uses unified Boost.PFR implementation
// All compilers use lightweight tuple_element and tuple_size_v APIs
static_assert(XTypeSignature::get_XTypeSignature<TestTypeReflectionHint>() ==
             "struct[s:272,a:8]{@0[mInt]:i32[s:4,a:4],@4[mFloat]:f32[s:4,a:4],@8[mVector]:vect"
             "or[s:32,a:8]<i32[s:4,a:4]>,@40[mStringVector]:vector[s:32,a:8]<string[s:32,a:8]>"
             ",@72[TestTypeInnerObj]:struct[s:40,a:8]{@0[mInt]:i32[s:4,a:4],@8[mVector]:vector"
             "[s:32,a:8]<i32[s:4,a:4]>},@112[mXXTypeVector]:vector[s:32,a:8]<struct[s:40,a:8]{"
             "@0[mInt]:i32[s:4,a:4],@8[mVector]:vector[s:32,a:8]<i32[s:4,a:4]>}>,@144[mComplex"
             "Map]:map[s:32,a:8]<string[s:32,a:8],struct[s:40,a:8]{@0[mInt]:i32[s:4,a:4],@8[mV"
             "ector]:vector[s:32,a:8]<i32[s:4,a:4]>}>,@176[mStringSet]:set[s:32,a:8]<string[s:"
             "32,a:8]>,@208[mSet]:set[s:32,a:8]<i32[s:4,a:4]>,@240[mString]:string[s:32,a:8]}"
              , "Type signature mismatch for TestTypeReflectionHint");

#endif // GENERATED_TEST_TYPES_HPP_
