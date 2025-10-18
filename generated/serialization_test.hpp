#ifndef GENERATED_SERIALIZATION_TEST_HPP_
#define GENERATED_SERIALIZATION_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) SimpleData {
	template <typename Allocator>
	SimpleData(Allocator allocator) : name(allocator) {}
	int id{0};
	float value{0.0f};
	XString name;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) ComplexData {
	template <typename Allocator>
	ComplexData(Allocator allocator) : title(allocator), items(allocator), tags(allocator), metadata(allocator) {}
	XString title;
	XVector<int> items;
	XSet<int> tags;
	XMap<XString, int> metadata;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) SimpleDataReflectionHint {
	int32_t id;
	float value;
	XString name;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) ComplexDataReflectionHint {
	XString title;
	XVector<int32_t> items;
	XSet<int32_t> tags;
	XMap<XString, int32_t> metadata;
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Type signature validation for SimpleData
// This ensures binary compatibility across compilations

static_assert(sizeof(SimpleData) == sizeof(SimpleDataReflectionHint),
              "Size mismatch: SimpleData runtime and reflection types must have identical size");
static_assert(alignof(SimpleData) == alignof(SimpleDataReflectionHint),
              "Alignment mismatch: SimpleData runtime and reflection types must have identical alignment");

// Expected type signature for SimpleDataReflectionHint:
// This signature is computed at compile-time by XTypeSignature::get_XTypeSignature<>
// Format: struct[s:<size>,a:<align>]{@<offset>:<field_type>,...}
//
// You can verify the actual signature by uncommenting this line:
// #pragma message(XTypeSignature::get_XTypeSignature<SimpleDataReflectionHint>().value)

// Type signature validation for ComplexData
// This ensures binary compatibility across compilations

static_assert(sizeof(ComplexData) == sizeof(ComplexDataReflectionHint),
              "Size mismatch: ComplexData runtime and reflection types must have identical size");
static_assert(alignof(ComplexData) == alignof(ComplexDataReflectionHint),
              "Alignment mismatch: ComplexData runtime and reflection types must have identical alignment");

// Expected type signature for ComplexDataReflectionHint:
// This signature is computed at compile-time by XTypeSignature::get_XTypeSignature<>
// Format: struct[s:<size>,a:<align>]{@<offset>:<field_type>,...}
//
// You can verify the actual signature by uncommenting this line:
// #pragma message(XTypeSignature::get_XTypeSignature<ComplexDataReflectionHint>().value)

#endif // GENERATED_SERIALIZATION_TEST_HPP_
