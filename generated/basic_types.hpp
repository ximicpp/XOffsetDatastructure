#ifndef GENERATED_BASIC_TYPES_HPP_
#define GENERATED_BASIC_TYPES_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) BasicTypes {
	// Default constructor
	template <typename Allocator>
	BasicTypes(Allocator allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	BasicTypes(Allocator allocator, int mInt_val, float mFloat_val, double mDouble_val, char mChar_val, bool mBool_val, int64_t mInt64_val)
		: mInt(mInt_val)
		, mFloat(mFloat_val)
		, mDouble(mDouble_val)
		, mChar(mChar_val)
		, mBool(mBool_val)
		, mInt64(mInt64_val)
	{}

	int mInt{0};
	float mFloat{0.0f};
	double mDouble{0.0};
	char mChar{'\0'};
	bool mBool{false};
	int64_t mInt64{0};
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) BasicTypesReflectionHint {
	int32_t mInt;
	float mFloat;
	double mDouble;
	char mChar;
	bool mBool;
	int64_t mInt64;
};

// ============================================================================
// MSVC Field Registration
// ============================================================================
// Manual field registration for MSVC to avoid Boost.PFR instantiation issues
// ============================================================================

// MSVC Field Registry for BasicTypesReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<BasicTypesReflectionHint> {
        static constexpr size_t field_count = 6;

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
            using type = double;
        };

        template<>
        struct FieldTypeAt<3> {
            using type = char;
        };

        template<>
        struct FieldTypeAt<4> {
            using type = bool;
        };

        template<>
        struct FieldTypeAt<5> {
            using type = int64_t;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(BasicTypesReflectionHint, mInt);
            } else if constexpr (Index == 1) {
                return offsetof(BasicTypesReflectionHint, mFloat);
            } else if constexpr (Index == 2) {
                return offsetof(BasicTypesReflectionHint, mDouble);
            } else if constexpr (Index == 3) {
                return offsetof(BasicTypesReflectionHint, mChar);
            } else if constexpr (Index == 4) {
                return offsetof(BasicTypesReflectionHint, mBool);
            } else if constexpr (Index == 5) {
                return offsetof(BasicTypesReflectionHint, mInt64);
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

// Compile-time validation for BasicTypes

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<BasicTypesReflectionHint>::value,
              "Type safety error for BasicTypesReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(BasicTypes) == sizeof(BasicTypesReflectionHint),
              "Size mismatch: BasicTypes runtime and reflection types must have identical size");
static_assert(alignof(BasicTypes) == alignof(BasicTypesReflectionHint),
              "Alignment mismatch: BasicTypes runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
static_assert(XTypeSignature::get_XTypeSignature<BasicTypesReflectionHint>() ==
             "struct[s:32,a:8]{"
             "@0:i32[s:4,a:4],"
             "@4:f32[s:4,a:4],"
             "@8:f64[s:8,a:8],"
             "@16:char[s:1,a:1],"
             "@17:bool[s:1,a:1],"
             "@24:i64[s:8,a:8]}",
              "Type signature mismatch for BasicTypesReflectionHint");

#endif // GENERATED_BASIC_TYPES_HPP_
