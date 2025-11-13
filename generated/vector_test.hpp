#ifndef GENERATED_VECTOR_TEST_HPP_
#define GENERATED_VECTOR_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) VectorTest {
	// Default constructor
	template <typename Allocator>
	VectorTest(Allocator allocator) : intVector(allocator), floatVector(allocator), stringVector(allocator) {}

	XVector<int> intVector;
	XVector<float> floatVector;
	XVector<XString> stringVector;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) VectorTestReflectionHint {
	XVector<int32_t> intVector;
	XVector<float> floatVector;
	XVector<XString> stringVector;
};

// ============================================================================
// MSVC Field Registration
// ============================================================================
// Manual field registration for MSVC to avoid Boost.PFR instantiation issues
// ============================================================================

// MSVC Field Registry for VectorTestReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<VectorTestReflectionHint> {
        static constexpr size_t field_count = 3;

        template<size_t Index>
        struct FieldTypeAt;

        template<>
        struct FieldTypeAt<0> {
            using type = XVector<int32_t>;
        };

        template<>
        struct FieldTypeAt<1> {
            using type = XVector<float>;
        };

        template<>
        struct FieldTypeAt<2> {
            using type = XVector<XString>;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(VectorTestReflectionHint, intVector);
            } else if constexpr (Index == 1) {
                return offsetof(VectorTestReflectionHint, floatVector);
            } else if constexpr (Index == 2) {
                return offsetof(VectorTestReflectionHint, stringVector);
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

// Compile-time validation for VectorTest

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<VectorTestReflectionHint>::value,
              "Type safety error for VectorTestReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(VectorTest) == sizeof(VectorTestReflectionHint),
              "Size mismatch: VectorTest runtime and reflection types must have identical size");
static_assert(alignof(VectorTest) == alignof(VectorTestReflectionHint),
              "Alignment mismatch: VectorTest runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
static_assert(XTypeSignature::get_XTypeSignature<VectorTestReflectionHint>() ==
             "struct[s:96,a:8]{"
             "@0:vector[s:32,a:8]<i32[s:4,a:4]>,"
             "@32:vector[s:32,a:8]<f32[s:4,a:4]>,"
             "@64:vector[s:32,a:8]<string[s:32,a:8]>}",
              "Type signature mismatch for VectorTestReflectionHint");

#endif // GENERATED_VECTOR_TEST_HPP_
