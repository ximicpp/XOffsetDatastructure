#ifndef GENERATED_GAME_DATA_HPP_
#define GENERATED_GAME_DATA_HPP_

#include "xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Runtime Types - Used for actual data storage
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) Item {
	// Default constructor
	template <typename Allocator>
	Item(Allocator allocator) : name(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	Item(Allocator allocator, int item_id_val, int item_type_val, int quantity_val, const char* name_val)
		: item_id(item_id_val)
		, item_type(item_type_val)
		, quantity(quantity_val)
		, name(name_val, allocator)
	{}

	int item_id{0};
	int item_type{0};
	int quantity{0};
	XString name;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) GameData {
	// Default constructor
	template <typename Allocator>
	GameData(Allocator allocator) : player_name(allocator), items(allocator), achievements(allocator), quest_progress(allocator) {}

	// Full constructor for emplace_back
	template <typename Allocator>
	GameData(Allocator allocator, int player_id_val, int level_val, float health_val, const char* player_name_val)
		: player_id(player_id_val)
		, level(level_val)
		, health(health_val)
		, player_name(player_name_val, allocator)
		, items(allocator)
		, achievements(allocator)
		, quest_progress(allocator)
	{}

	int player_id{0};
	int level{0};
	float health{0.0f};
	XString player_name;
	XVector<Item> items;
	XSet<int> achievements;
	XMap<XString, int> quest_progress;
};

// ============================================================================
// Reflection Hint Types - Used for compile-time type analysis
// ============================================================================
// These are aggregate versions of runtime types that satisfy boost::pfr
// requirements for reflection. They must have identical memory layout
// to their runtime counterparts.
// ============================================================================

struct alignas(XTypeSignature::BASIC_ALIGNMENT) ItemReflectionHint {
	int32_t item_id;
	int32_t item_type;
	int32_t quantity;
	XString name;
};

struct alignas(XTypeSignature::BASIC_ALIGNMENT) GameDataReflectionHint {
	int32_t player_id;
	int32_t level;
	float health;
	XString player_name;
	XVector<ItemReflectionHint> items;
	XSet<int32_t> achievements;
	XMap<XString, int32_t> quest_progress;
};

// ============================================================================
// MSVC Field Registration
// ============================================================================
// Manual field registration for MSVC to avoid Boost.PFR instantiation issues
// ============================================================================

// MSVC Field Registry for ItemReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<ItemReflectionHint> {
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
            using type = int32_t;
        };

        template<>
        struct FieldTypeAt<3> {
            using type = XString;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(ItemReflectionHint, item_id);
            } else if constexpr (Index == 1) {
                return offsetof(ItemReflectionHint, item_type);
            } else if constexpr (Index == 2) {
                return offsetof(ItemReflectionHint, quantity);
            } else if constexpr (Index == 3) {
                return offsetof(ItemReflectionHint, name);
            } else {
                return 0;
            }
        }
    };
}
#endif // _MSC_VER

// MSVC Field Registry for GameDataReflectionHint
// Manual field registration to avoid Boost.PFR issues on MSVC
#ifdef _MSC_VER
namespace XTypeSignature {
    template<>
    struct MSVCFieldRegistry<GameDataReflectionHint> {
        static constexpr size_t field_count = 7;

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
            using type = float;
        };

        template<>
        struct FieldTypeAt<3> {
            using type = XString;
        };

        template<>
        struct FieldTypeAt<4> {
            using type = XVector<ItemReflectionHint>;
        };

        template<>
        struct FieldTypeAt<5> {
            using type = XSet<int32_t>;
        };

        template<>
        struct FieldTypeAt<6> {
            using type = XMap<XString, int32_t>;
        };

        template<size_t Index>
        static constexpr size_t get_offset() noexcept {
            if constexpr (Index == 0) {
                return offsetof(GameDataReflectionHint, player_id);
            } else if constexpr (Index == 1) {
                return offsetof(GameDataReflectionHint, level);
            } else if constexpr (Index == 2) {
                return offsetof(GameDataReflectionHint, health);
            } else if constexpr (Index == 3) {
                return offsetof(GameDataReflectionHint, player_name);
            } else if constexpr (Index == 4) {
                return offsetof(GameDataReflectionHint, items);
            } else if constexpr (Index == 5) {
                return offsetof(GameDataReflectionHint, achievements);
            } else if constexpr (Index == 6) {
                return offsetof(GameDataReflectionHint, quest_progress);
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

// Compile-time validation for Item

// 1. Type Safety Check (disabled on MSVC)
// Type safety verification uses Boost.PFR for recursive member checking.
// MSVC has template instantiation issues with PFR on types containing XString/XVector.
#ifndef _MSC_VER
static_assert(XOffsetDatastructure2::is_xbuffer_safe<ItemReflectionHint>::value,
              "Type safety error for ItemReflectionHint");
#endif // _MSC_VER

// 2. Size and Alignment Check
static_assert(sizeof(Item) == sizeof(ItemReflectionHint),
              "Size mismatch: Item runtime and reflection types must have identical size");
static_assert(alignof(Item) == alignof(ItemReflectionHint),
              "Alignment mismatch: Item runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
static_assert(XTypeSignature::get_XTypeSignature<ItemReflectionHint>() == "struct[s:48,a:8]{@0:i32[s:4,a:4],@4:i32[s:4,a:4],@8:i32[s:4,a:4],@16:string[s:32,a:8]}",
              "Type signature mismatch for ItemReflectionHint");

// Compile-time validation for GameData

// 1. Type Safety Check (disabled on MSVC)
// Type safety verification uses Boost.PFR for recursive member checking.
// MSVC has template instantiation issues with PFR on types containing XString/XVector.
#ifndef _MSC_VER
static_assert(XOffsetDatastructure2::is_xbuffer_safe<GameDataReflectionHint>::value,
              "Type safety error for GameDataReflectionHint");
#endif // _MSC_VER

// 2. Size and Alignment Check
static_assert(sizeof(GameData) == sizeof(GameDataReflectionHint),
              "Size mismatch: GameData runtime and reflection types must have identical size");
static_assert(alignof(GameData) == alignof(GameDataReflectionHint),
              "Alignment mismatch: GameData runtime and reflection types must have identical alignment");

// 3. Type Signature Check
// Type signature verification now works on all compilers
// - GCC/Clang: Uses Boost.PFR for automatic reflection
// - MSVC: Uses manual MSVCFieldRegistry (generated above)
static_assert(XTypeSignature::get_XTypeSignature<GameDataReflectionHint>() ==
             "struct[s:144,a:8]{"
             "@0:i32[s:4,a:4],"
             "@4:i32[s:4,a:4],"
             "@8:f32[s:4,a:4],"
             "@16:string[s:32,a:8],"
             "@48:vector[s:32,a:8]<struct[s:48,a:8]{@0:i32[s:4,a:4],"
             "@4:i32[s:4,a:4],"
             "@8:i32[s:4,a:4],"
             "@16:string[s:32,a:8]}>,"
             "@80:set[s:32,a:8]<i32[s:4,a:4]>,"
             "@112:map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>}",
              "Type signature mismatch for GameDataReflectionHint");

#endif // GENERATED_GAME_DATA_HPP_
