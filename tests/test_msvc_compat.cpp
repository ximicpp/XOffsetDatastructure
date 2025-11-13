// ============================================================================
// Test: MSVC Compatibility Tests
// Purpose: Test MSVC-specific compiler issues (e.g., init list ordering)
// ============================================================================

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"
#include "../generated/game_data.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Test Structures
// ============================================================================

// MSVC Issue: Struct with skipped members in init list
// This tests the specific MSVC warning/error about initialization order
struct GameLikeStruct {
    template <typename Allocator>
    GameLikeStruct(Allocator allocator) 
        : name(allocator), items(allocator), int_set(allocator), string_map(allocator) {}
    
    int id{0};           // Not in init list
    int level{0};        // Not in init list
    float health{0.0f};  // Not in init list
    XString name;        // First in init list
    XVector<int> items;
    XSet<int> int_set;
    XMap<XString, int> string_map;
};

// Test: Init list with skipped members (MSVC-specific issue)
bool test_init_list_compatibility() {
    std::cout << "\n[TEST 8] Init List with Skipped Members\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBufferExt xbuf(2048);
    
    // Test 8.1: Create struct with skipped members
    std::cout << "  8.1 Creating GameLikeStruct... ";
    auto* obj = xbuf.make<GameLikeStruct>("game_like");
    std::cout << "[OK]\n";
    
    // Test 8.2: Verify default values
    std::cout << "  8.2 Verifying default values... ";
    assert(obj->id == 0);
    assert(obj->level == 0);
    assert(obj->health == 0.0f);
    std::cout << "[OK]\n";
    
    // Test 8.3: Set all fields
    std::cout << "  8.3 Setting all fields... ";
    obj->id = 999;
    obj->level = 50;
    obj->health = 75.5f;
    obj->name = XString("TestName", xbuf.allocator<XString>());
    assert(obj->name == "TestName");
    std::cout << "[OK]\n";
    
    // Test 8.4: Add to containers
    std::cout << "  8.4 Adding to containers... ";
    obj->items.push_back(1);
    obj->int_set.insert(100);
    XString key("key1", xbuf.allocator<XString>());
    obj->string_map.emplace(key, 200);
    assert(obj->items.size() == 1);
    assert(obj->int_set.size() == 1);
    assert(obj->string_map.size() == 1);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Init list compatibility tests\n";
    return true;
}

int main() {
    std::cout << "\n";
    std::cout << "+======================================================+\n";
    std::cout << "|         MSVC Compatibility Test Suite               |\n";
    std::cout << "+======================================================+\n";
    
#ifdef _MSC_VER
    std::cout << "| Compiler: MSVC " << _MSC_VER << "                               |\n";
#elif defined(__clang__)
    std::cout << "| Compiler: Clang " << __clang_major__ << "." << __clang_minor__ << "                              |\n";
#elif defined(__GNUC__)
    std::cout << "| Compiler: GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "                                |\n";
#endif
    std::cout << "+======================================================+\n";
    
    try {
        bool all_passed = true;
        
        // Only run the MSVC-specific test
        all_passed &= test_init_list_compatibility();
        
        std::cout << "\n";
        std::cout << "+======================================================+\n";
        if (all_passed) {
            std::cout << "|              ALL TESTS PASSED [OK]                   |\n";
        } else {
            std::cout << "|              SOME TESTS FAILED [FAIL]                |\n";
        }
        std::cout << "+======================================================+\n";
        std::cout << "\n";
        
        return all_passed ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "\n[EXCEPTION] " << e.what() << "\n";
        return 1;
    }
}
