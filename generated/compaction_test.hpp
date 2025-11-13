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
// MSVC Field Registration
// ============================================================================
// Manual field registration for MSVC to avoid Boost.PFR instantiation issues
// ============================================================================

// MSVC Field Registry for MemoryTestTypeReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<MemoryTestTypeReflectionHint> {
        static constexpr size_t field_count = 3;

        template<size_t Index>
        struct FieldTypeAt;

        template<>
        struct FieldTypeAt<0> {
            using type = int32_t;
        };

        template<>
        struct FieldTypeAt<1> {
            using type = XVector<int32_t>;
        };

        template<>
        struct FieldTypeAt<2> {
            using type = XVector<XString>;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(MemoryTestTypeReflectionHint, value);
            } else if constexpr (Index == 1) {
                return offsetof(MemoryTestTypeReflectionHint, data);
            } else if constexpr (Index == 2) {
                return offsetof(MemoryTestTypeReflectionHint, strings);
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

// Compile-time validation for MemoryTestType

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<MemoryTestTypeReflectionHint>::value,
              "Type safety error for MemoryTestTypeReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(MemoryTestType) == sizeof(MemoryTestTypeReflectionHint),
              "Size mismatch: MemoryTestType runtime and reflection types must have identical size");
static_assert(alignof(MemoryTestType) == alignof(MemoryTestTypeReflectionHint),
              "Alignment mismatch: MemoryTestType runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
static_assert(XTypeSignature::get_XTypeSignature<MemoryTestTypeReflectionHint>() ==
             "struct[s:72,a:8]{"
             "@0:i32[s:4,a:4],"
             "@8:vector[s:32,a:8]<i32[s:4,a:4]>,"
             "@40:vector[s:32,a:8]<string[s:32,a:8]>}",
              "Type signature mismatch for MemoryTestTypeReflectionHint");

#endif // GENERATED_COMPACTION_TEST_HPP_
