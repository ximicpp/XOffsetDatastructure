#ifndef GENERATED_SERIALIZATION_TEST_HPP_
#define GENERATED_SERIALIZATION_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) SimpleData {
	// Default constructor
	template <typename Allocator>
	SimpleData(Allocator allocator) : name(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	SimpleData(Allocator allocator, int id_val, float value_val, const char* name_val)
		: id(id_val)
		, value(value_val)
		, name(name_val, allocator)
	{}

	int id{0};
	float value{0.0f};
	XString name;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) ComplexData {
	// Default constructor
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

	// Field names metadata for XTypeSignature
	static constexpr std::string_view _field_names[] = {
		"id",
		"value",
		"name",
	};
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) ComplexDataReflectionHint {
	XString title;
	XVector<int32_t> items;
	XSet<int32_t> tags;
	XMap<XString, int32_t> metadata;

	// Field names metadata for XTypeSignature
	static constexpr std::string_view _field_names[] = {
		"title",
		"items",
		"tags",
		"metadata",
	};
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Compile-time validation for SimpleData

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<SimpleDataReflectionHint>::value,
              "Type safety error for SimpleDataReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(SimpleData) == sizeof(SimpleDataReflectionHint),
              "Size mismatch: SimpleData runtime and reflection types must have identical size");
static_assert(alignof(SimpleData) == alignof(SimpleDataReflectionHint),
              "Alignment mismatch: SimpleData runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification uses unified Boost.PFR implementation
// All compilers use lightweight tuple_element and tuple_size_v APIs
static_assert(XTypeSignature::get_XTypeSignature<SimpleDataReflectionHint>() == "struct[s:40,a:8]{@0[id]:i32[s:4,a:4],@4[value]:f32[s:4,a:4],@8[name]:string[s:32,a:8]}",
              "Type signature mismatch for SimpleDataReflectionHint");

// Compile-time validation for ComplexData

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<ComplexDataReflectionHint>::value,
              "Type safety error for ComplexDataReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(ComplexData) == sizeof(ComplexDataReflectionHint),
              "Size mismatch: ComplexData runtime and reflection types must have identical size");
static_assert(alignof(ComplexData) == alignof(ComplexDataReflectionHint),
              "Alignment mismatch: ComplexData runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification uses unified Boost.PFR implementation
// All compilers use lightweight tuple_element and tuple_size_v APIs
static_assert(XTypeSignature::get_XTypeSignature<ComplexDataReflectionHint>() ==
             "struct[s:128,a:8]{@0[title]:string[s:32,a:8],@32[items]:vector[s:32,a:8]<i32[s:4"
             ",a:4]>,@64[tags]:set[s:32,a:8]<i32[s:4,a:4]>,@96[metadata]:map[s:32,a:8]<string["
             "s:32,a:8],i32[s:4,a:4]>}"
              , "Type signature mismatch for ComplexDataReflectionHint");

#endif // GENERATED_SERIALIZATION_TEST_HPP_
