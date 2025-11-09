#ifndef GENERATED_NESTED_TEST_HPP_
#define GENERATED_NESTED_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) InnerObject {
	// Default constructor
	template <typename Allocator>
	InnerObject(Allocator allocator) : name(allocator), data(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	InnerObject(Allocator allocator, int id_val, const char* name_val)
		: id(id_val)
		, name(name_val, allocator)
		, data(allocator)
	{}

	int id{0};
	XString name;
	XVector<int> data;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) MiddleObject {
	// Default constructor
	template <typename Allocator>
	MiddleObject(Allocator allocator) : name(allocator), inner(allocator), values(allocator) {}

	XString name;
	InnerObject inner;
	XVector<int> values;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) OuterObject {
	// Default constructor
	template <typename Allocator>
	OuterObject(Allocator allocator) : title(allocator), middle(allocator), innerList(allocator) {}

	XString title;
	MiddleObject middle;
	XVector<InnerObject> innerList;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) InnerObjectReflectionHint {
	int32_t id;
	XString name;
	XVector<int32_t> data;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) MiddleObjectReflectionHint {
	XString name;
	InnerObjectReflectionHint inner;
	XVector<int32_t> values;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) OuterObjectReflectionHint {
	XString title;
	MiddleObjectReflectionHint middle;
	XVector<InnerObjectReflectionHint> innerList;
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Compile-time validation for InnerObject

// 1. Type Safety Check
static_assert(XOffsetDatastructure2::is_xbuffer_safe<InnerObjectReflectionHint>::value,
              "Type safety error for InnerObjectReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(InnerObject) == sizeof(InnerObjectReflectionHint),
              "Size mismatch: InnerObject runtime and reflection types must have identical size");
static_assert(alignof(InnerObject) == alignof(InnerObjectReflectionHint),
              "Alignment mismatch: InnerObject runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification using boost::pfr::tuple_element (lightweight API)
// Compatible with MSVC, GCC, and Clang
static_assert(XTypeSignature::get_XTypeSignature<InnerObjectReflectionHint>() == "struct[s:72,a:8]{@0:i32[s:4,a:4],@8:string[s:32,a:8],@40:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for InnerObjectReflectionHint");

// Compile-time validation for MiddleObject

// 1. Type Safety Check
static_assert(XOffsetDatastructure2::is_xbuffer_safe<MiddleObjectReflectionHint>::value,
              "Type safety error for MiddleObjectReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(MiddleObject) == sizeof(MiddleObjectReflectionHint),
              "Size mismatch: MiddleObject runtime and reflection types must have identical size");
static_assert(alignof(MiddleObject) == alignof(MiddleObjectReflectionHint),
              "Alignment mismatch: MiddleObject runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification using boost::pfr::tuple_element (lightweight API)
// Compatible with MSVC, GCC, and Clang
static_assert(XTypeSignature::get_XTypeSignature<MiddleObjectReflectionHint>() ==
             "struct[s:136,a:8]{"
             "@0:string[s:32,a:8],"
             "@32:struct[s:72,a:8]{@0:i32[s:4,a:4],"
             "@8:string[s:32,a:8],"
             "@40:vector[s:32,a:8]<i32[s:4,a:4]>},"
             "@104:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for MiddleObjectReflectionHint");

// Compile-time validation for OuterObject

// 1. Type Safety Check
static_assert(XOffsetDatastructure2::is_xbuffer_safe<OuterObjectReflectionHint>::value,
              "Type safety error for OuterObjectReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(OuterObject) == sizeof(OuterObjectReflectionHint),
              "Size mismatch: OuterObject runtime and reflection types must have identical size");
static_assert(alignof(OuterObject) == alignof(OuterObjectReflectionHint),
              "Alignment mismatch: OuterObject runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification using boost::pfr::tuple_element (lightweight API)
// Compatible with MSVC, GCC, and Clang
static_assert(XTypeSignature::get_XTypeSignature<OuterObjectReflectionHint>() ==
             "struct[s:200,a:8]{"
             "@0:string[s:32,a:8],"
             "@32:struct[s:136,a:8]{@0:string[s:32,a:8],"
             "@32:struct[s:72,a:8]{@0:i32[s:4,a:4],"
             "@8:string[s:32,a:8],"
             "@40:vector[s:32,a:8]<i32[s:4,a:4]>},"
             "@104:vector[s:32,a:8]<i32[s:4,a:4]>},"
             "@168:vector[s:32,a:8]<struct[s:72,a:8]{@0:i32[s:4,a:4],"
             "@8:string[s:32,a:8],"
             "@40:vector[s:32,a:8]<i32[s:4,a:4]>}>}",
              "Type signature mismatch for OuterObjectReflectionHint");

#endif // GENERATED_NESTED_TEST_HPP_
