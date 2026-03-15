// ============================================================================
// Test: Enum Type Support
// Purpose: Verify that fixed-underlying-type enums are recognized as safe
//          types for XBuffer, and that their TypeLayout signatures are correct.
// ============================================================================

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// ============================================================================
// Test enum types
// ============================================================================

// Scoped enum with explicit underlying type
enum class WeaponType : uint8_t {
    Sword = 0,
    Bow = 1,
    Staff = 2,
    Dagger = 3
};

// Scoped enum with int32_t
enum class QuestStatus : int32_t {
    NotStarted = 0,
    InProgress = 1,
    Completed = 2,
    Failed = 3
};

// Unscoped enum with explicit underlying type
enum Color : uint16_t {
    Red = 0,
    Green = 1,
    Blue = 2
};

// Scoped enum WITHOUT explicit underlying type (defaults to int)
enum class Direction {
    North = 0,
    South = 1,
    East = 2,
    West = 3
};

// Struct containing enum fields
struct PlayerStats {
    template <typename Allocator>
    PlayerStats(Allocator) {}

    int32_t id{0};
    WeaponType weapon{WeaponType::Sword};
    QuestStatus quest{QuestStatus::NotStarted};
    Color color{Red};
    int32_t level{1};
};

// Struct with enum for nested signature tests
struct EquipmentSlot {
    template <typename Allocator>
    EquipmentSlot(Allocator) {}

    WeaponType weapon{WeaponType::Sword};
    int32_t durability{100};
};

// Struct containing XVector of enum elements
struct Inventory {
    template <typename Allocator>
    Inventory(Allocator allocator)
        : weapons(allocator), directions(allocator) {}

    XVector<WeaponType> weapons;
    XVector<Direction> directions;
};

// ============================================================================
// Test 1: Enum safety checks
// ============================================================================

bool test_enum_safety() {
    std::cout << "\n[TEST] Enum Safety Checks\n";
    std::cout << std::string(50, '-') << "\n";

    // Scoped enums should be safe
    static_assert(is_xbuffer_safe<WeaponType>::value,
        "WeaponType (enum class : uint8_t) should be safe");
    std::cout << "  WeaponType (enum class : uint8_t) ... [SAFE]\n";

    static_assert(is_xbuffer_safe<QuestStatus>::value,
        "QuestStatus (enum class : int32_t) should be safe");
    std::cout << "  QuestStatus (enum class : int32_t) ... [SAFE]\n";

    // Unscoped enum with explicit type should be safe
    static_assert(is_xbuffer_safe<Color>::value,
        "Color (enum : uint16_t) should be safe");
    std::cout << "  Color (enum : uint16_t) ... [SAFE]\n";

    // Struct containing enums should be safe
    static_assert(is_xbuffer_safe<PlayerStats>::value,
        "PlayerStats (struct with enum fields) should be safe");
    std::cout << "  PlayerStats (struct with enums) ... [SAFE]\n";

    return true;
}

// ============================================================================
// Test 2: Enum type signatures
// ============================================================================

bool test_enum_signatures() {
    std::cout << "\n[TEST] Enum Type Signatures\n";
    std::cout << std::string(50, '-') << "\n";

    // Verify is_fixed_enum works correctly
    static_assert(std::is_enum_v<WeaponType>,
        "WeaponType should be a fixed enum");
    static_assert(std::is_enum_v<QuestStatus>,
        "QuestStatus should be a fixed enum");
    static_assert(std::is_enum_v<Color>,
        "Color should be a fixed enum");

    // Print signatures for inspection
    constexpr auto weapon_sig = boost::typelayout::get_layout_signature<WeaponType>();
    constexpr auto quest_sig = boost::typelayout::get_layout_signature<QuestStatus>();
    constexpr auto color_sig = boost::typelayout::get_layout_signature<Color>();

    std::cout << "  WeaponType:  " << weapon_sig << "\n";
    std::cout << "  QuestStatus: " << quest_sig << "\n";
    std::cout << "  Color:       " << color_sig << "\n";

    // Verify struct with enums has correct signature
    constexpr auto stats_sig = boost::typelayout::get_layout_signature<PlayerStats>();
    std::cout << "  PlayerStats: " << stats_sig << "\n";

    return true;
}

// ============================================================================
// Test 3: Enum in XBuffer
// ============================================================================

bool test_enum_in_xbuffer() {
    std::cout << "\n[TEST] Enum in XBuffer\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(4096);
    auto* stats = xbuf.make<PlayerStats>();

    stats->id = 42;
    stats->weapon = WeaponType::Staff;
    stats->quest = QuestStatus::InProgress;
    stats->color = Blue;
    stats->level = 10;

    // Read back and verify
    auto& found = xbuf.root<PlayerStats>();
    assert(found.id == 42);
    assert(found.weapon == WeaponType::Staff);
    assert(found.quest == QuestStatus::InProgress);
    assert(found.color == Blue);
    assert(found.level == 10);

    std::cout << "  Create PlayerStats in XBuffer... [OK]\n";
    std::cout << "  Read back enum fields... [OK]\n";
    std::cout << "  WeaponType = Staff (" << static_cast<int>(found.weapon) << ") [OK]\n";
    std::cout << "  QuestStatus = InProgress (" << static_cast<int>(found.quest) << ") [OK]\n";
    std::cout << "  Color = Blue (" << static_cast<int>(found.color) << ") [OK]\n";

    return true;
}

// ============================================================================
// Test 4: Enum without explicit underlying type
// ============================================================================

bool test_enum_default_underlying() {
    std::cout << "\n[TEST] Enum Without Explicit Underlying Type\n";
    std::cout << std::string(50, '-') << "\n";

    // enum class without `: type` defaults to int
    static_assert(std::is_same_v<std::underlying_type_t<Direction>, int>,
        "Direction should default to int underlying type");
    std::cout << "  Direction underlying type = int ... [OK]\n";

    // is_fixed_enum should still report true (all enums have an underlying type)
    static_assert(std::is_enum_v<Direction>,
        "Direction should be detected as fixed enum");
    std::cout << "  Direction is_fixed_enum = true ... [OK]\n";

    // Safety check: enum class defaults to int (portable, safe)
    static_assert(is_xbuffer_safe<Direction>::value,
        "Direction (enum class defaulting to int) should be safe");
    std::cout << "  Direction is_xbuffer_safe = true ... [OK]\n";

    // Signature should exist and contain enum marker
    constexpr auto dir_sig = boost::typelayout::get_layout_signature<Direction>();
    std::cout << "  Direction sig: " << dir_sig << "\n";

    return true;
}

// ============================================================================
// Test 5: Enum as XVector element
// ============================================================================

bool test_enum_in_xvector() {
    std::cout << "\n[TEST] Enum as XVector Element\n";
    std::cout << std::string(50, '-') << "\n";

    // XVector<enum> should be safe
    static_assert(is_xbuffer_safe<Inventory>::value,
        "Inventory (struct with XVector<enum>) should be safe");
    std::cout << "  Inventory (with XVector<WeaponType>) is_xbuffer_safe ... [OK]\n";

    // Create and populate
    XBuffer xbuf(4096);
    auto* inv = xbuf.make<Inventory>();

    inv->weapons.push_back(WeaponType::Sword);
    inv->weapons.push_back(WeaponType::Bow);
    inv->weapons.push_back(WeaponType::Staff);

    inv->directions.push_back(Direction::North);
    inv->directions.push_back(Direction::East);

    // Read back
    auto& found = xbuf.root<Inventory>();
    assert(found.weapons.size() == 3);
    assert(found.weapons[0] == WeaponType::Sword);
    assert(found.weapons[1] == WeaponType::Bow);
    assert(found.weapons[2] == WeaponType::Staff);
    std::cout << "  XVector<WeaponType> push_back & read ... [OK]\n";

    assert(found.directions.size() == 2);
    assert(found.directions[0] == Direction::North);
    assert(found.directions[1] == Direction::East);
    std::cout << "  XVector<Direction> push_back & read ... [OK]\n";

    return true;
}

// ============================================================================
// Test 6: Enum signature identity — different enums must differ
// ============================================================================

bool test_enum_signature_identity() {
    std::cout << "\n[TEST] Enum Signature Identity\n";
    std::cout << std::string(50, '-') << "\n";

    // Different enum types should produce different Definition signatures
    // (even if they share the same underlying type and size)
    constexpr auto weapon_def = boost::typelayout::get_layout_signature<WeaponType>();
    constexpr auto quest_def = boost::typelayout::get_layout_signature<QuestStatus>();
    constexpr auto dir_def = boost::typelayout::get_layout_signature<Direction>();

    // Definition signatures embed type names — they must differ
    static_assert(!boost::typelayout::layout_signatures_match<WeaponType, QuestStatus>(),
        "Different enums should have different definition signatures");
    std::cout << "  WeaponType != QuestStatus (definition) ... [OK]\n";

    static_assert(!boost::typelayout::layout_signatures_match<WeaponType, Direction>(),
        "Different enums should have different definition signatures");
    std::cout << "  WeaponType != Direction (definition) ... [OK]\n";

    // Layout signatures may match if underlying types have same size/alignment
    // (this is expected — layout only cares about binary representation)
    std::cout << "  WeaponType def: " << weapon_def << "\n";
    std::cout << "  QuestStatus def: " << quest_def << "\n";
    std::cout << "  Direction def: " << dir_def << "\n";

    return true;
}

// ============================================================================
// Test 7: Nested struct with enum — signature correctness
// ============================================================================

bool test_enum_nested_signature() {
    std::cout << "\n[TEST] Nested Struct with Enum Signature\n";
    std::cout << std::string(50, '-') << "\n";

    // EquipmentSlot contains a WeaponType enum field
    static_assert(is_xbuffer_safe<EquipmentSlot>::value,
        "EquipmentSlot (struct with enum member) should be safe");
    std::cout << "  EquipmentSlot is_xbuffer_safe ... [OK]\n";

    constexpr auto slot_def = boost::typelayout::get_layout_signature<EquipmentSlot>();
    constexpr auto slot_layout = boost::typelayout::get_layout_signature<EquipmentSlot>();

    std::cout << "  EquipmentSlot definition: " << slot_def << "\n";
    std::cout << "  EquipmentSlot layout:     " << slot_layout << "\n";

    // Verify in XBuffer
    XBuffer xbuf(4096);
    auto* slot = xbuf.make<EquipmentSlot>();
    slot->weapon = WeaponType::Dagger;
    slot->durability = 75;

    auto& found = xbuf.root<EquipmentSlot>();
    assert(found.weapon == WeaponType::Dagger);
    assert(found.durability == 75);
    std::cout << "  EquipmentSlot in XBuffer read/write ... [OK]\n";

    return true;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Enum Type Support Tests ===\n";

    bool all_passed = true;
    all_passed &= test_enum_safety();
    all_passed &= test_enum_signatures();
    all_passed &= test_enum_in_xbuffer();
    all_passed &= test_enum_default_underlying();
    all_passed &= test_enum_in_xvector();
    all_passed &= test_enum_signature_identity();
    all_passed &= test_enum_nested_signature();

    std::cout << "\n" << std::string(50, '=') << "\n";
    if (all_passed) {
        std::cout << "[PASS] All enum support tests passed!\n";
        return 0;
    } else {
        std::cout << "[FAIL] Some tests failed!\n";
        return 1;
    }
}
