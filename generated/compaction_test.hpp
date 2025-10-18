#ifndef GENERATED_COMPACTION_TEST_HPP_
#define GENERATED_COMPACTION_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) MemoryTestType {
	template <typename Allocator>
	MemoryTestType(Allocator allocator) : data(allocator), strings(allocator) {}
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

// Type signature validation for MemoryTestType
// This ensures binary compatibility across compilations

static_assert(sizeof(MemoryTestType) == sizeof(MemoryTestTypeReflectionHint),
              "Size mismatch: MemoryTestType runtime and reflection types must have identical size");
static_assert(alignof(MemoryTestType) == alignof(MemoryTestTypeReflectionHint),
              "Alignment mismatch: MemoryTestType runtime and reflection types must have identical alignment");

static_assert(XTypeSignature::get_XTypeSignature<MemoryTestTypeReflectionHint>() ==
             "struct[s:72,a:8]{"
             "@0:i32[s:4,a:4],"
             "@8:vector[s:32,a:8]<i32[s:4,a:4]>,"
             "@40:vector[s:32,a:8]<string[s:32,a:8]>}",
              "Type signature mismatch for MemoryTestTypeReflectionHint");

#endif // GENERATED_COMPACTION_TEST_HPP_
