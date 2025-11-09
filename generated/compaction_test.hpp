#ifndef GENERATED_COMPACTION_TEST_HPP_
#define GENERATED_COMPACTION_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) MemoryTestType {
	// Default constructor
	template <typename Allocator>
	MemoryTestType(Allocator allocator) : data(allocator), strings(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	MemoryTestType(Allocator allocator, int value_val)
		: value(value_val)
		, data(allocator)
		, strings(allocator)
	{}

	int value{0};
	XVector<int> data;
	XVector<XString> strings;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) MemoryTestTypeReflectionHint {
	int32_t value;
	XVector<int32_t> data;
	XVector<XString> strings;
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Compile-time validation for MemoryTestType

// 1. Type Safety Check
static_assert(XOffsetDatastructure2::is_xbuffer_safe<MemoryTestTypeReflectionHint>::value,
              "Type safety error for MemoryTestTypeReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(MemoryTestType) == sizeof(MemoryTestTypeReflectionHint),
              "Size mismatch: MemoryTestType runtime and reflection types must have identical size");
static_assert(alignof(MemoryTestType) == alignof(MemoryTestTypeReflectionHint),
              "Alignment mismatch: MemoryTestType runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification using boost::pfr::tuple_element (lightweight API)
// Compatible with MSVC, GCC, and Clang
static_assert(XTypeSignature::get_XTypeSignature<MemoryTestTypeReflectionHint>() ==
             "struct[s:72,a:8]{"
             "@0:i32[s:4,a:4],"
             "@8:vector[s:32,a:8]<i32[s:4,a:4]>,"
             "@40:vector[s:32,a:8]<string[s:32,a:8]>}",
              "Type signature mismatch for MemoryTestTypeReflectionHint");

#endif // GENERATED_COMPACTION_TEST_HPP_
