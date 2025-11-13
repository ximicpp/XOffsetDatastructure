#ifndef GENERATED_MAP_SET_TEST_HPP_
#define GENERATED_MAP_SET_TEST_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) MapSetTest {
	// Default constructor
	template <typename Allocator>
	MapSetTest(Allocator allocator) : intSet(allocator), stringSet(allocator), intMap(allocator), stringMap(allocator) {}

	XSet<int> intSet;
	XSet<XString> stringSet;
	XMap<int, XString> intMap;
	XMap<XString, int> stringMap;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) MapSetTestReflectionHint {
	XSet<int32_t> intSet;
	XSet<XString> stringSet;
	XMap<int32_t, XString> intMap;
	XMap<XString, int32_t> stringMap;
};

// ============================================================================
// MSVC Field Registration
// ============================================================================
// Manual field registration for MSVC to avoid Boost.PFR instantiation issues
// ============================================================================

// MSVC Field Registry for MapSetTestReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<MapSetTestReflectionHint> {
        static constexpr size_t field_count = 4;

        template<size_t Index>
        struct FieldTypeAt;

        template<>
        struct FieldTypeAt<0> {
            using type = XSet<int32_t>;
        };

        template<>
        struct FieldTypeAt<1> {
            using type = XSet<XString>;
        };

        template<>
        struct FieldTypeAt<2> {
            using type = XMap<int32_t, XString>;
        };

        template<>
        struct FieldTypeAt<3> {
            using type = XMap<XString, int32_t>;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(MapSetTestReflectionHint, intSet);
            } else if constexpr (Index == 1) {
                return offsetof(MapSetTestReflectionHint, stringSet);
            } else if constexpr (Index == 2) {
                return offsetof(MapSetTestReflectionHint, intMap);
            } else if constexpr (Index == 3) {
                return offsetof(MapSetTestReflectionHint, stringMap);
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

// Compile-time validation for MapSetTest

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<MapSetTestReflectionHint>::value,
              "Type safety error for MapSetTestReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(MapSetTest) == sizeof(MapSetTestReflectionHint),
              "Size mismatch: MapSetTest runtime and reflection types must have identical size");
static_assert(alignof(MapSetTest) == alignof(MapSetTestReflectionHint),
              "Alignment mismatch: MapSetTest runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
static_assert(XTypeSignature::get_XTypeSignature<MapSetTestReflectionHint>() ==
             "struct[s:128,a:8]{"
             "@0:set[s:32,a:8]<i32[s:4,a:4]>,"
             "@32:set[s:32,a:8]<string[s:32,a:8]>,"
             "@64:map[s:32,a:8]<i32[s:4,a:4],string[s:32,a:8]>,"
             "@96:map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>}",
              "Type signature mismatch for MapSetTestReflectionHint");

#endif // GENERATED_MAP_SET_TEST_HPP_
