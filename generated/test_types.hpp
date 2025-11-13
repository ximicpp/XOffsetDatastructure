#ifndef GENERATED_TEST_TYPES_HPP_
#define GENERATED_TEST_TYPES_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestTypeInner {
	// Default constructor
	template <typename Allocator>
	TestTypeInner(Allocator allocator) : mVector(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	TestTypeInner(Allocator allocator, int mInt_val)
		: mInt(mInt_val)
		, mVector(allocator)
	{}

	int mInt{0};
	XVector<int> mVector;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestType {
	// Default constructor
	template <typename Allocator>
	TestType(Allocator allocator) : mVector(allocator), mStringVector(allocator), TestTypeInnerObj(allocator), mXXTypeVector(allocator), mComplexMap(allocator), mStringSet(allocator), mSet(allocator), mString(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	TestType(Allocator allocator, int mInt_val, float mFloat_val, const char* mString_val)
		: mInt(mInt_val)
		, mFloat(mFloat_val)
		, mVector(allocator)
		, mStringVector(allocator)
		, TestTypeInnerObj(allocator)
		, mXXTypeVector(allocator)
		, mComplexMap(allocator)
		, mStringSet(allocator)
		, mSet(allocator)
		, mString(mString_val, allocator)
	{}

	int mInt{0};
	float mFloat{0.0f};
	XVector<int> mVector;
	XVector<XString> mStringVector;
	TestTypeInner TestTypeInnerObj;
	XVector<TestTypeInner> mXXTypeVector;
	XMap<XString, TestTypeInner> mComplexMap;
	XSet<XString> mStringSet;
	XSet<int> mSet;
	XString mString;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestTypeInnerReflectionHint {
	int32_t mInt;
	XVector<int32_t> mVector;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) TestTypeReflectionHint {
	int32_t mInt;
	float mFloat;
	XVector<int32_t> mVector;
	XVector<XString> mStringVector;
	TestTypeInnerReflectionHint TestTypeInnerObj;
	XVector<TestTypeInnerReflectionHint> mXXTypeVector;
	XMap<XString, TestTypeInnerReflectionHint> mComplexMap;
	XSet<XString> mStringSet;
	XSet<int32_t> mSet;
	XString mString;
};

// ============================================================================
// MSVC Field Registration
// ============================================================================
// Manual field registration for MSVC to avoid Boost.PFR instantiation issues
// ============================================================================

// MSVC Field Registry for TestTypeInnerReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<TestTypeInnerReflectionHint> {
        static constexpr size_t field_count = 2;

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

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(TestTypeInnerReflectionHint, mInt);
            } else if constexpr (Index == 1) {
                return offsetof(TestTypeInnerReflectionHint, mVector);
            } else {
                return 0;
            }
        }
    };
}
#endif // _MSC_VER

// MSVC Field Registry for TestTypeReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<TestTypeReflectionHint> {
        static constexpr size_t field_count = 10;

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
            using type = XVector<int32_t>;
        };

        template<>
        struct FieldTypeAt<3> {
            using type = XVector<XString>;
        };

        template<>
        struct FieldTypeAt<4> {
            using type = TestTypeInnerReflectionHint;
        };

        template<>
        struct FieldTypeAt<5> {
            using type = XVector<TestTypeInnerReflectionHint>;
        };

        template<>
        struct FieldTypeAt<6> {
            using type = XMap<XString, TestTypeInnerReflectionHint>;
        };

        template<>
        struct FieldTypeAt<7> {
            using type = XSet<XString>;
        };

        template<>
        struct FieldTypeAt<8> {
            using type = XSet<int32_t>;
        };

        template<>
        struct FieldTypeAt<9> {
            using type = XString;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(TestTypeReflectionHint, mInt);
            } else if constexpr (Index == 1) {
                return offsetof(TestTypeReflectionHint, mFloat);
            } else if constexpr (Index == 2) {
                return offsetof(TestTypeReflectionHint, mVector);
            } else if constexpr (Index == 3) {
                return offsetof(TestTypeReflectionHint, mStringVector);
            } else if constexpr (Index == 4) {
                return offsetof(TestTypeReflectionHint, TestTypeInnerObj);
            } else if constexpr (Index == 5) {
                return offsetof(TestTypeReflectionHint, mXXTypeVector);
            } else if constexpr (Index == 6) {
                return offsetof(TestTypeReflectionHint, mComplexMap);
            } else if constexpr (Index == 7) {
                return offsetof(TestTypeReflectionHint, mStringSet);
            } else if constexpr (Index == 8) {
                return offsetof(TestTypeReflectionHint, mSet);
            } else if constexpr (Index == 9) {
                return offsetof(TestTypeReflectionHint, mString);
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

// Compile-time validation for TestTypeInner

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<TestTypeInnerReflectionHint>::value,
              "Type safety error for TestTypeInnerReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(TestTypeInner) == sizeof(TestTypeInnerReflectionHint),
              "Size mismatch: TestTypeInner runtime and reflection types must have identical size");
static_assert(alignof(TestTypeInner) == alignof(TestTypeInnerReflectionHint),
              "Alignment mismatch: TestTypeInner runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
static_assert(XTypeSignature::get_XTypeSignature<TestTypeInnerReflectionHint>() == "struct[s:40,a:8]{@0:i32[s:4,a:4],@8:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for TestTypeInnerReflectionHint");

// Compile-time validation for TestType

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<TestTypeReflectionHint>::value,
              "Type safety error for TestTypeReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(TestType) == sizeof(TestTypeReflectionHint),
              "Size mismatch: TestType runtime and reflection types must have identical size");
static_assert(alignof(TestType) == alignof(TestTypeReflectionHint),
              "Alignment mismatch: TestType runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
static_assert(XTypeSignature::get_XTypeSignature<TestTypeReflectionHint>() ==
             "struct[s:272,a:8]{"
             "@0:i32[s:4,a:4],"
             "@4:f32[s:4,a:4],"
             "@8:vector[s:32,a:8]<i32[s:4,a:4]>,"
             "@40:vector[s:32,a:8]<string[s:32,a:8]>,"
             "@72:struct[s:40,a:8]{@0:i32[s:4,a:4],"
             "@8:vector[s:32,a:8]<i32[s:4,a:4]>},"
             "@112:vector[s:32,a:8]<struct[s:40,a:8]{@0:i32[s:4,a:4],"
             "@8:vector[s:32,a:8]<i32[s:4,a:4]>}>,"
             "@144:map[s:32,a:8]<string[s:32,a:8],struct[s:40,a:8]{@0:i32[s:4,a:4],"
             "@8:vector[s:32,a:8]<i32[s:4,a:4]>}>,"
             "@176:set[s:32,a:8]<string[s:32,a:8]>,"
             "@208:set[s:32,a:8]<i32[s:4,a:4]>,"
             "@240:string[s:32,a:8]}",
              "Type signature mismatch for TestTypeReflectionHint");

#endif // GENERATED_TEST_TYPES_HPP_
