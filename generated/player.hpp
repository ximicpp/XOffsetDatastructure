#ifndef GENERATED_PLAYER_HPP_
#define GENERATED_PLAYER_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) Player {
	// Default constructor
	template <typename Allocator>
	Player(Allocator allocator) : name(allocator), items(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	Player(Allocator allocator, int id_val, int level_val, const char* name_val)
		: id(id_val)
		, level(level_val)
		, name(name_val, allocator)
		, items(allocator)
	{}

	int id{0};
	int level{0};
	XString name;
	XVector<int> items;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) PlayerReflectionHint {
	int32_t id;
	int32_t level;
	XString name;
	XVector<int32_t> items;
};

// ============================================================================
// MSVC Field Registration
// ============================================================================
// Manual field registration for MSVC to avoid Boost.PFR instantiation issues
// ============================================================================

// MSVC Field Registry for PlayerReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<PlayerReflectionHint> {
        static constexpr size_t field_count = 4;

        template<size_t Index>
        struct FieldTypeAt;

        template<>
        struct FieldTypeAt<0> {
            using type = int32_t;
        };

        template<>
        struct FieldTypeAt<1> {
            using type = int32_t;
        };

        template<>
        struct FieldTypeAt<2> {
            using type = XString;
        };

        template<>
        struct FieldTypeAt<3> {
            using type = XVector<int32_t>;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(PlayerReflectionHint, id);
            } else if constexpr (Index == 1) {
                return offsetof(PlayerReflectionHint, level);
            } else if constexpr (Index == 2) {
                return offsetof(PlayerReflectionHint, name);
            } else if constexpr (Index == 3) {
                return offsetof(PlayerReflectionHint, items);
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

// Compile-time validation for Player

// 1. Type Safety Check
// Type safety verification uses Boost.PFR for recursive member checking.
static_assert(XOffsetDatastructure2::is_xbuffer_safe<PlayerReflectionHint>::value,
              "Type safety error for PlayerReflectionHint");

// 2. Size and Alignment Check
static_assert(sizeof(Player) == sizeof(PlayerReflectionHint),
              "Size mismatch: Player runtime and reflection types must have identical size");
static_assert(alignof(Player) == alignof(PlayerReflectionHint),
              "Alignment mismatch: Player runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
static_assert(XTypeSignature::get_XTypeSignature<PlayerReflectionHint>() ==
             "struct[s:72,a:8]{"
             "@0:i32[s:4,a:4],"
             "@4:i32[s:4,a:4],"
             "@8:string[s:32,a:8],"
             "@40:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for PlayerReflectionHint");

#endif // GENERATED_PLAYER_HPP_
