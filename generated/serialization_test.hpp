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
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) ComplexDataReflectionHint {
	XString title;
	XVector<int32_t> items;
	XSet<int32_t> tags;
	XMap<XString, int32_t> metadata;
};

// ============================================================================
// MSVC Field Registration
// ============================================================================
// Manual field registration for MSVC to avoid Boost.PFR instantiation issues
// ============================================================================

// MSVC Field Registry for SimpleDataReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<SimpleDataReflectionHint> {
        static constexpr size_t field_count = 3;

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
            using type = XString;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(SimpleDataReflectionHint, id);
            } else if constexpr (Index == 1) {
                return offsetof(SimpleDataReflectionHint, value);
            } else if constexpr (Index == 2) {
                return offsetof(SimpleDataReflectionHint, name);
            } else {
                return 0;
            }
        }
    };
}
#endif // _MSC_VER

// MSVC Field Registry for ComplexDataReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<ComplexDataReflectionHint> {
        static constexpr size_t field_count = 4;

        template<size_t Index>
        struct FieldTypeAt;

        template<>
        struct FieldTypeAt<0> {
            using type = XString;
        };

        template<>
        struct FieldTypeAt<1> {
            using type = XVector<int32_t>;
        };

        template<>
        struct FieldTypeAt<2> {
            using type = XSet<int32_t>;
        };

        template<>
        struct FieldTypeAt<3> {
            using type = XMap<XString, int32_t>;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(ComplexDataReflectionHint, title);
            } else if constexpr (Index == 1) {
                return offsetof(ComplexDataReflectionHint, items);
            } else if constexpr (Index == 2) {
                return offsetof(ComplexDataReflectionHint, tags);
            } else if constexpr (Index == 3) {
                return offsetof(ComplexDataReflectionHint, metadata);
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
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
static_assert(XTypeSignature::get_XTypeSignature<SimpleDataReflectionHint>() == "struct[s:40,a:8]{@0:i32[s:4,a:4],@4:f32[s:4,a:4],@8:string[s:32,a:8]}",
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
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
static_assert(XTypeSignature::get_XTypeSignature<ComplexDataReflectionHint>() ==
             "struct[s:128,a:8]{"
             "@0:string[s:32,a:8],"
             "@32:vector[s:32,a:8]<i32[s:4,a:4]>,"
             "@64:set[s:32,a:8]<i32[s:4,a:4]>,"
             "@96:map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>}",
              "Type signature mismatch for ComplexDataReflectionHint");

#endif // GENERATED_SERIALIZATION_TEST_HPP_
