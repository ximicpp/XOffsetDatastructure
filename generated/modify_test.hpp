#ifndef GENERATED_MODIFY_TEST_HPP_
#define GENERATED_MODIFY_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) ModifyTestData {
	// Default constructor
	template <typename Allocator>
	ModifyTestData(Allocator allocator) : numbers(allocator), names(allocator), scores(allocator), tags(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	ModifyTestData(Allocator allocator, int counter_val, float ratio_val, bool active_val)
		: counter(counter_val)
		, ratio(ratio_val)
		, active(active_val)
		, numbers(allocator)
		, names(allocator)
		, scores(allocator)
		, tags(allocator)
	{}

	int counter{0};
	float ratio{0.0f};
	bool active{false};
	XVector<int> numbers;
	XVector<XString> names;
	XMap<XString, int> scores;
	XSet<int> tags;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) ModifyTestDataReflectionHint {
	int32_t counter;
	float ratio;
	bool active;
	XVector<int32_t> numbers;
	XVector<XString> names;
	XMap<XString, int32_t> scores;
	XSet<int32_t> tags;
};

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Compile-time validation for ModifyTestData

// 1. Type Safety Check
static_assert(XOffsetDatastructure2::is_xbuffer_safe<ModifyTestDataReflectionHint>::value,
              "Type safety error for ModifyTestDataReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(ModifyTestData) == sizeof(ModifyTestDataReflectionHint),
              "Size mismatch: ModifyTestData runtime and reflection types must have identical size");
static_assert(alignof(ModifyTestData) == alignof(ModifyTestDataReflectionHint),
              "Alignment mismatch: ModifyTestData runtime and reflection types must have identical alignment");

// 3. Type Signature Check (disabled on MSVC)
// Type signature verification disabled on MSVC due to deep template instantiation issues
// with Boost.PFR reflection on aggregate types containing XString in containers.
// See: https://github.com/boostorg/pfr/issues
#ifndef _MSC_VER
static_assert(XTypeSignature::get_XTypeSignature<ModifyTestDataReflectionHint>() ==
             "struct[s:144,a:8]{"
             "@0:i32[s:4,a:4],"
             "@4:f32[s:4,a:4],"
             "@8:bool[s:1,a:1],"
             "@16:vector[s:32,a:8]<i32[s:4,a:4]>,"
             "@48:vector[s:32,a:8]<string[s:32,a:8]>,"
             "@80:map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>,"
             "@112:set[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for ModifyTestDataReflectionHint");

#endif // _MSC_VER

#endif // GENERATED_MODIFY_TEST_HPP_
