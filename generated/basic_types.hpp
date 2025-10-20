#ifndef GENERATED_BASIC_TYPES_HPP_
#define GENERATED_BASIC_TYPES_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) BasicTypes {
	// Default constructor
	template <typename Allocator>
	BasicTypes(Allocator allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	BasicTypes(Allocator allocator, int mInt_val, float mFloat_val, double mDouble_val, char mChar_val, bool mBool_val, int64_t mInt64_val)
		: mInt(mInt_val)
		, mFloat(mFloat_val)
		, mDouble(mDouble_val)
		, mChar(mChar_val)
		, mBool(mBool_val)
		, mInt64(mInt64_val)
	{}

	int mInt{0};
	float mFloat{0.0f};
	double mDouble{0.0};
	char mChar{'\0'};
	bool mBool{false};
	int64_t mInt64{0};
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) BasicTypesReflectionHint {
	int32_t mInt;
	float mFloat;
	double mDouble;
	char mChar;
	bool mBool;
	int64_t mInt64;
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Type signature validation for BasicTypes
// This ensures binary compatibility across compilations

static_assert(sizeof(BasicTypes) == sizeof(BasicTypesReflectionHint),
              "Size mismatch: BasicTypes runtime and reflection types must have identical size");
static_assert(alignof(BasicTypes) == alignof(BasicTypesReflectionHint),
              "Alignment mismatch: BasicTypes runtime and reflection types must have identical alignment");

// Type signature verification disabled on MSVC due to deep template instantiation issues
// with Boost.PFR reflection on aggregate types containing XString in containers.
// See: https://github.com/boostorg/pfr/issues
#ifndef _MSC_VER
static_assert(XTypeSignature::get_XTypeSignature<BasicTypesReflectionHint>() ==
             "struct[s:32,a:8]{"
             "@0:i32[s:4,a:4],"
             "@4:f32[s:4,a:4],"
             "@8:f64[s:8,a:8],"
             "@16:char[s:1,a:1],"
             "@17:bool[s:1,a:1],"
             "@24:i64[s:8,a:8]}",
              "Type signature mismatch for BasicTypesReflectionHint");

#endif // _MSC_VER

#endif // GENERATED_BASIC_TYPES_HPP_
