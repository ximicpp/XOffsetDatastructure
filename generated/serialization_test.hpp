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

static_assert(XTypeSignature::get_XTypeSignature<SimpleDataReflectionHint>() == "struct[s:40,a:8]{@0:i32[s:4,a:4],@4:f32[s:4,a:4],@8:string[s:32,a:8]}",
              "Type signature mismatch for SimpleDataReflectionHint");

// Type signature validation for ComplexData
// This ensures binary compatibility across compilations

static_assert(sizeof(ComplexData) == sizeof(ComplexDataReflectionHint),
              "Size mismatch: ComplexData runtime and reflection types must have identical size");
static_assert(alignof(ComplexData) == alignof(ComplexDataReflectionHint),
              "Alignment mismatch: ComplexData runtime and reflection types must have identical alignment");

static_assert(XTypeSignature::get_XTypeSignature<ComplexDataReflectionHint>() ==
             "struct[s:128,a:8]{"
             "@0:string[s:32,a:8],"
             "@32:vector[s:32,a:8]<i32[s:4,a:4]>,"
             "@64:set[s:32,a:8]<i32[s:4,a:4]>,"
             "@96:map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>}",
              "Type signature mismatch for ComplexDataReflectionHint");

#endif // GENERATED_SERIALIZATION_TEST_HPP_
