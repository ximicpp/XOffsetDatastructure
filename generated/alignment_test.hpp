#ifndef GENERATED_ALIGNMENT_TEST_HPP_
#define GENERATED_ALIGNMENT_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) AlignedStruct {
	template <typename Allocator>
	AlignedStruct(Allocator allocator) : name(allocator) {}
	char a{'\0'};
	int b{0};
	double c{0.0};
	XString name;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) AlignedStructReflectionHint {
	char a;
	int32_t b;
	double c;
	XString name;
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Type signature validation for AlignedStruct
// This ensures binary compatibility across compilations

static_assert(sizeof(AlignedStruct) == sizeof(AlignedStructReflectionHint),
              "Size mismatch: AlignedStruct runtime and reflection types must have identical size");
static_assert(alignof(AlignedStruct) == alignof(AlignedStructReflectionHint),
              "Alignment mismatch: AlignedStruct runtime and reflection types must have identical alignment");

static_assert(XTypeSignature::get_XTypeSignature<AlignedStructReflectionHint>() == "struct[s:48,a:8]{@0:char[s:1,a:1],@4:i32[s:4,a:4],@8:f64[s:8,a:8],@16:string[s:32,a:8]}",
              "Type signature mismatch for AlignedStructReflectionHint");

#endif // GENERATED_ALIGNMENT_TEST_HPP_
