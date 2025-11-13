#ifndef GENERATED_ALIGNMENT_TEST_HPP_
#define GENERATED_ALIGNMENT_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) AlignedStruct {
	// Default constructor
	template <typename Allocator>
	AlignedStruct(Allocator allocator) : name(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	AlignedStruct(Allocator allocator, char a_val, int b_val, double c_val, const char* name_val)
		: a(a_val)
		, b(b_val)
		, c(c_val)
		, name(name_val, allocator)
	{}

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
// MSVC Field Registration
// ============================================================================
// Manual field registration for MSVC to avoid Boost.PFR instantiation issues
// ============================================================================

// MSVC Field Registry for AlignedStructReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<AlignedStructReflectionHint> {
        static constexpr size_t field_count = 4;

        template<size_t Index>
        struct FieldTypeAt;

        template<>
        struct FieldTypeAt<0> {
            using type = char;
        };

        template<>
        struct FieldTypeAt<1> {
            using type = int32_t;
        };

        template<>
        struct FieldTypeAt<2> {
            using type = double;
        };

        template<>
        struct FieldTypeAt<3> {
            using type = XString;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(AlignedStructReflectionHint, a);
            } else if constexpr (Index == 1) {
                return offsetof(AlignedStructReflectionHint, b);
            } else if constexpr (Index == 2) {
                return offsetof(AlignedStructReflectionHint, c);
            } else if constexpr (Index == 3) {
                return offsetof(AlignedStructReflectionHint, name);
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

// Compile-time validation for AlignedStruct

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<AlignedStructReflectionHint>::value,
              "Type safety error for AlignedStructReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(AlignedStruct) == sizeof(AlignedStructReflectionHint),
              "Size mismatch: AlignedStruct runtime and reflection types must have identical size");
static_assert(alignof(AlignedStruct) == alignof(AlignedStructReflectionHint),
              "Alignment mismatch: AlignedStruct runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
static_assert(XTypeSignature::get_XTypeSignature<AlignedStructReflectionHint>() == "struct[s:48,a:8]{@0:char[s:1,a:1],@4:i32[s:4,a:4],@8:f64[s:8,a:8],@16:string[s:32,a:8]}",
              "Type signature mismatch for AlignedStructReflectionHint");

#endif // GENERATED_ALIGNMENT_TEST_HPP_
