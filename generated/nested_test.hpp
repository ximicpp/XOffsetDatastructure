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
// MSVC Field Registration
// ============================================================================
// Manual field registration for MSVC to avoid Boost.PFR instantiation issues
// ============================================================================

// MSVC Field Registry for InnerObjectReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<InnerObjectReflectionHint> {
        static constexpr size_t field_count = 3;

        template<size_t Index>
        struct FieldTypeAt;

        template<>
        struct FieldTypeAt<0> {
            using type = int32_t;
        };

        template<>
        struct FieldTypeAt<1> {
            using type = XString;
        };

        template<>
        struct FieldTypeAt<2> {
            using type = XVector<int32_t>;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(InnerObjectReflectionHint, id);
            } else if constexpr (Index == 1) {
                return offsetof(InnerObjectReflectionHint, name);
            } else if constexpr (Index == 2) {
                return offsetof(InnerObjectReflectionHint, data);
            } else {
                return 0;
            }
        }
    };
}
#endif // _MSC_VER

// MSVC Field Registry for MiddleObjectReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<MiddleObjectReflectionHint> {
        static constexpr size_t field_count = 3;

        template<size_t Index>
        struct FieldTypeAt;

        template<>
        struct FieldTypeAt<0> {
            using type = XString;
        };

        template<>
        struct FieldTypeAt<1> {
            using type = InnerObjectReflectionHint;
        };

        template<>
        struct FieldTypeAt<2> {
            using type = XVector<int32_t>;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(MiddleObjectReflectionHint, name);
            } else if constexpr (Index == 1) {
                return offsetof(MiddleObjectReflectionHint, inner);
            } else if constexpr (Index == 2) {
                return offsetof(MiddleObjectReflectionHint, values);
            } else {
                return 0;
            }
        }
    };
}
#endif // _MSC_VER

// MSVC Field Registry for OuterObjectReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<OuterObjectReflectionHint> {
        static constexpr size_t field_count = 3;

        template<size_t Index>
        struct FieldTypeAt;

        template<>
        struct FieldTypeAt<0> {
            using type = XString;
        };

        template<>
        struct FieldTypeAt<1> {
            using type = MiddleObjectReflectionHint;
        };

        template<>
        struct FieldTypeAt<2> {
            using type = XVector<InnerObjectReflectionHint>;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(OuterObjectReflectionHint, title);
            } else if constexpr (Index == 1) {
                return offsetof(OuterObjectReflectionHint, middle);
            } else if constexpr (Index == 2) {
                return offsetof(OuterObjectReflectionHint, innerList);
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

// Compile-time validation for InnerObject

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<InnerObjectReflectionHint>::value,
              "Type safety error for InnerObjectReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(InnerObject) == sizeof(InnerObjectReflectionHint),
              "Size mismatch: InnerObject runtime and reflection types must have identical size");
static_assert(alignof(InnerObject) == alignof(InnerObjectReflectionHint),
              "Alignment mismatch: InnerObject runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
static_assert(XTypeSignature::get_XTypeSignature<InnerObjectReflectionHint>() == "struct[s:72,a:8]{@0:i32[s:4,a:4],@8:string[s:32,a:8],@40:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for InnerObjectReflectionHint");

// Compile-time validation for MiddleObject

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<MiddleObjectReflectionHint>::value,
              "Type safety error for MiddleObjectReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(MiddleObject) == sizeof(MiddleObjectReflectionHint),
              "Size mismatch: MiddleObject runtime and reflection types must have identical size");
static_assert(alignof(MiddleObject) == alignof(MiddleObjectReflectionHint),
              "Alignment mismatch: MiddleObject runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
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
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<OuterObjectReflectionHint>::value,
              "Type safety error for OuterObjectReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(OuterObject) == sizeof(OuterObjectReflectionHint),
              "Size mismatch: OuterObject runtime and reflection types must have identical size");
static_assert(alignof(OuterObject) == alignof(OuterObjectReflectionHint),
              "Alignment mismatch: OuterObject runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
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
