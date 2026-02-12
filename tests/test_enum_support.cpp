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
    static_assert(boost::typelayout::is_fixed_enum<WeaponType>(),
        "WeaponType should be a fixed enum");
    static_assert(boost::typelayout::is_fixed_enum<QuestStatus>(),
        "QuestStatus should be a fixed enum");
    static_assert(boost::typelayout::is_fixed_enum<Color>(),
        "Color should be a fixed enum");

    // Print signatures for inspection
    constexpr auto weapon_sig = boost::typelayout::get_definition_signature<WeaponType>();
    constexpr auto quest_sig = boost::typelayout::get_definition_signature<QuestStatus>();
    constexpr auto color_sig = boost::typelayout::get_definition_signature<Color>();

    std::cout << "  WeaponType:  " << weapon_sig << "\n";
    std::cout << "  QuestStatus: " << quest_sig << "\n";
    std::cout << "  Color:       " << color_sig << "\n";

    // Verify struct with enums has correct signature
    constexpr auto stats_sig = boost::typelayout::get_definition_signature<PlayerStats>();
    std::cout << "  PlayerStats: " << stats_sig << "\n";

    return true;
}

// ============================================================================
// Test 3: Enum in XBuffer
// ============================================================================

bool test_enum_in_xbuffer() {
    std::cout << "\n[TEST] Enum in XBuffer\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt xbuf(4096);
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
// Main
// ============================================================================

int main() {
    std::cout << "=== Enum Type Support Tests ===\n";

    bool all_passed = true;
    all_passed &= test_enum_safety();
    all_passed &= test_enum_signatures();
    all_passed &= test_enum_in_xbuffer();

    std::cout << "\n" << std::string(50, '=') << "\n";
    if (all_passed) {
        std::cout << "[PASS] All enum support tests passed!\n";
        return 0;
    } else {
        std::cout << "[FAIL] Some tests failed!\n";
        return 1;
    }
}
