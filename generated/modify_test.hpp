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
// MSVC Field Registration
// ============================================================================
// Manual field registration for MSVC to avoid Boost.PFR instantiation issues
// ============================================================================

// MSVC Field Registry for ModifyTestDataReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<ModifyTestDataReflectionHint> {
        static constexpr size_t field_count = 7;

        template<size_t Index>
        struct FieldTypeAt;

        template<>
        struct FieldTypeAt<0> {
            using type = int32_t;
        };

        template<>
        struct FieldTypeAt<1> {
            using type = float;
        };

        template<>
        struct FieldTypeAt<2> {
            using type = bool;
        };

        template<>
        struct FieldTypeAt<3> {
            using type = XVector<int32_t>;
        };

        template<>
        struct FieldTypeAt<4> {
            using type = XVector<XString>;
        };

        template<>
        struct FieldTypeAt<5> {
            using type = XMap<XString, int32_t>;
        };

        template<>
        struct FieldTypeAt<6> {
            using type = XSet<int32_t>;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(ModifyTestDataReflectionHint, counter);
            } else if constexpr (Index == 1) {
                return offsetof(ModifyTestDataReflectionHint, ratio);
            } else if constexpr (Index == 2) {
                return offsetof(ModifyTestDataReflectionHint, active);
            } else if constexpr (Index == 3) {
                return offsetof(ModifyTestDataReflectionHint, numbers);
            } else if constexpr (Index == 4) {
                return offsetof(ModifyTestDataReflectionHint, names);
            } else if constexpr (Index == 5) {
                return offsetof(ModifyTestDataReflectionHint, scores);
            } else if constexpr (Index == 6) {
                return offsetof(ModifyTestDataReflectionHint, tags);
            } else {
                return 0;
            }
        }
    };
}
#endif // _MSC_VER

// ============================================================================
// Compile-Time Validation
// ============================================================================

// Compile-time validation for ModifyTestData

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<ModifyTestDataReflectionHint>::value,
              "Type safety error for ModifyTestDataReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(ModifyTestData) == sizeof(ModifyTestDataReflectionHint),
              "Size mismatch: ModifyTestData runtime and reflection types must have identical size");
static_assert(alignof(ModifyTestData) == alignof(ModifyTestDataReflectionHint),
              "Alignment mismatch: ModifyTestData runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
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

#endif // GENERATED_MODIFY_TEST_HPP_
