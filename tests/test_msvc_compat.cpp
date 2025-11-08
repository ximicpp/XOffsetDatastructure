// ============================================================================
// Test: MSVC Compatibility Tests
// Purpose: Comprehensive MSVC-specific compatibility verification
// ============================================================================

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"
#include "../generated/game_data.hpp"

using namespace XOffsetDatastructure2;

// ============================================================================
// Test Structures
// ============================================================================

// Simple struct with XString
struct TestStruct {
    template <typename Allocator>
    TestStruct(Allocator allocator) : name(allocator) {}
    
    XString name;
};

// Struct with XMap<XString, int>
struct MapTest {
    template <typename Allocator>
    MapTest(Allocator allocator) : string_map(allocator) {}
    
    XMap<XString, int> string_map;
};

// Complex struct with multiple containers
struct ComplexStruct {
    template <typename Allocator>
    ComplexStruct(Allocator allocator) 
        : str1(allocator), 
          str2(allocator),
          str_vec(allocator),
          str_map(allocator) {}
    
    XString str1;
    XString str2;
    XVector<XString> str_vec;
    XMap<XString, int> str_map;
};

// Nested struct with XString
struct NestedItem {
    template <typename Allocator>
    NestedItem(Allocator allocator) : name(allocator) {}
    
    template <typename Allocator>
    NestedItem(Allocator allocator, int id_val, const char* name_val)
        : id(id_val), name(name_val, allocator) {}
    
    int id{0};
    XString name;
};

// Struct with vector of nested items
struct NestedContainer {
    template <typename Allocator>
    NestedContainer(Allocator allocator)
        : label(allocator),
          items(allocator) {}
    
    XString label;
    XVector<NestedItem> items;
};

// Struct similar to GameData but with skipped members in init list
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

// Test 1: Basic XString construction with allocator
bool test_xstring_construction() {
    std::cout << "\n[TEST 1] XString Construction\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBufferExt xbuf(1024);
    
    // Test 1.1: Direct construction with allocator
    std::cout << "  1.1 Direct construction... ";
    XString str1("Hello", xbuf.allocator<XString>());
    assert(str1 == "Hello");
    std::cout << "[OK]\n";
    
    // Test 1.2: Construction then assignment
    std::cout << "  1.2 Assignment from temporary... ";
    XString str2 = XString("World", xbuf.allocator<XString>());
    assert(str2 == "World");
    std::cout << "[OK]\n";
    
    // Test 1.3: Assignment operator
    std::cout << "  1.3 Assignment operator... ";
    XString str3("Test", xbuf.allocator<XString>());
    str3 = XString("Updated", xbuf.allocator<XString>());
    assert(str3 == "Updated");
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] XString construction tests\n";
    return true;
}

// Test 2: XString in containers
bool test_xstring_in_vector() {
    std::cout << "\n[TEST 2] XString in XVector\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBufferExt xbuf(2048);
    
    // Test 2.1: Create struct with XString member
    std::cout << "  2.1 Struct with XString member... ";
    auto* obj = xbuf.make<TestStruct>("test");
    std::cout << "[OK]\n";
    
    // Test 2.2: Assign to XString member
    std::cout << "  2.2 Assign to XString member... ";
    obj->name = XString("TestName", xbuf.allocator<XString>());
    assert(obj->name == "TestName");
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] XString in containers tests\n";
    return true;
}

// Test 3: XMap with XString keys
bool test_xmap_xstring_keys() {
    std::cout << "\n[TEST 3] XMap with XString Keys\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBufferExt xbuf(2048);
    auto* obj = xbuf.make<MapTest>("map_test");
    
    // Test 3.1: emplace with lvalue XString
    std::cout << "  3.1 emplace with lvalue... ";
    XString key1("key1", xbuf.allocator<XString>());
    obj->string_map.emplace(key1, 100);
    assert(obj->string_map.size() == 1);
    std::cout << "[OK]\n";
    
    // Test 3.2: emplace with rvalue XString (std::move)
    std::cout << "  3.2 emplace with rvalue... ";
    XString key2("key2", xbuf.allocator<XString>());
    obj->string_map.emplace(std::move(key2), 200);
    assert(obj->string_map.size() == 2);
    std::cout << "[OK]\n";
    
    // Test 3.3: emplace with temporary XString
    std::cout << "  3.3 emplace with temporary... ";
    obj->string_map.emplace(XString("key3", xbuf.allocator<XString>()), 300);
    assert(obj->string_map.size() == 3);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] XMap with XString keys tests\n";
    return true;
}

// Test 4: Multiple XString operations
bool test_multiple_xstring_ops() {
    std::cout << "\n[TEST 4] Multiple XString Operations\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBufferExt xbuf(4096);
    auto* obj = xbuf.make<ComplexStruct>("complex");
    
    // Test 4.1: Multiple string assignments
    std::cout << "  4.1 Multiple assignments... ";
    obj->str1 = XString("First", xbuf.allocator<XString>());
    obj->str2 = XString("Second", xbuf.allocator<XString>());
    assert(obj->str1 == "First");
    assert(obj->str2 == "Second");
    std::cout << "[OK]\n";
    
    // Test 4.2: Vector of XString
    std::cout << "  4.2 Vector of XString... ";
    for (int i = 0; i < 3; i++) {
        XString str(("Item" + std::to_string(i)).c_str(), xbuf.allocator<XString>());
        obj->str_vec.push_back(str);
    }
    assert(obj->str_vec.size() == 3);
    std::cout << "[OK]\n";
    
    // Test 4.3: Map with XString keys
    std::cout << "  4.3 Map with XString keys... ";
    for (int i = 0; i < 3; i++) {
        std::string key_str = "Key" + std::to_string(i);
        XString key(key_str.c_str(), xbuf.allocator<XString>());
        obj->str_map.emplace(key, i * 10);
    }
    assert(obj->str_map.size() == 3);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Multiple XString operations tests\n";
    return true;
}

// Test 5: Allocator type compatibility
bool test_allocator_types() {
    std::cout << "\n[TEST 5] Allocator Type Compatibility\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBufferExt xbuf(1024);
    
    // Test 5.1: Using allocator<char>
    std::cout << "  5.1 allocator<char>... ";
    XString str1("Test1", xbuf.allocator<char>());
    assert(str1 == "Test1");
    std::cout << "[OK]\n";
    
    // Test 5.2: Using allocator<XString>
    std::cout << "  5.2 allocator<XString>... ";
    XString str2("Test2", xbuf.allocator<XString>());
    assert(str2 == "Test2");
    std::cout << "[OK]\n";
    
    // Test 5.3: Using get_segment_manager()
    std::cout << "  5.3 get_segment_manager()... ";
    XString str3("Test3", xbuf.get_segment_manager());
    assert(str3 == "Test3");
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Allocator type tests\n";
    return true;
}

// Test 6: Nested structures with XString
bool test_nested_structures() {
    std::cout << "\n[TEST 6] Nested Structures\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBufferExt xbuf(4096);
    
    // Test 6.1: Create nested container
    std::cout << "  6.1 Creating nested container... ";
    auto* container = xbuf.make<NestedContainer>("nested");
    std::cout << "[OK]\n";
    
    // Test 6.2: Set label
    std::cout << "  6.2 Setting label... ";
    container->label = XString("Container1", xbuf.allocator<XString>());
    assert(container->label == "Container1");
    std::cout << "[OK]\n";
    
    // Test 6.3: Add nested items
    std::cout << "  6.3 Adding nested items... ";
    for (int i = 0; i < 3; i++) {
        std::string name = "Item" + std::to_string(i);
        container->items.emplace_back(
            xbuf.allocator<NestedItem>(),
            i,
            name.c_str()
        );
    }
    assert(container->items.size() == 3);
    assert(container->items[0].name == "Item0");
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Nested structures tests\n";
    return true;
}

// Test 7: Generated types compatibility
bool test_generated_types() {
    std::cout << "\n[TEST 7] Generated Types (GameData)\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBufferExt xbuf(4096);
    
    // Test 7.1: Create GameData
    std::cout << "  7.1 Creating GameData... ";
    auto* game = xbuf.make<GameData>("game");
    std::cout << "[OK]\n";
    
    // Test 7.2: Set player info
    std::cout << "  7.2 Setting player info... ";
    game->player_name = XString("Hero", xbuf.allocator<XString>());
    game->player_id = 12345;
    game->level = 42;
    game->health = 100.0f;
    assert(game->player_name == "Hero");
    assert(game->player_id == 12345);
    std::cout << "[OK]\n";
    
    // Test 7.3: Add items with emplace_back
    std::cout << "  7.3 Adding items... ";
    for (int i = 0; i < 3; i++) {
        std::string name = "Item" + std::to_string(i);
        game->items.emplace_back(
            xbuf.allocator<Item>(),
            i, i % 3, i * 10,
            name.c_str()
        );
    }
    assert(game->items.size() == 3);
    assert(game->items[0].name == "Item0");
    std::cout << "[OK]\n";
    
    // Test 7.4: Add achievements
    std::cout << "  7.4 Adding achievements... ";
    for (int i = 0; i < 5; i++) {
        game->achievements.insert(i);
    }
    assert(game->achievements.size() == 5);
    std::cout << "[OK]\n";
    
    // Test 7.5: Add quest progress
    std::cout << "  7.5 Adding quest progress... ";
    XString quest1("Quest1", xbuf.allocator<XString>());
    game->quest_progress.emplace(quest1, 75);
    game->quest_progress.emplace(XString("Quest2", xbuf.allocator<XString>()), 100);
    assert(game->quest_progress.size() == 2);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] Generated types tests\n";
    return true;
}

// Test 8: Init list with skipped members
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
        
        all_passed &= test_xstring_construction();
        all_passed &= test_xstring_in_vector();
        all_passed &= test_xmap_xstring_keys();
        all_passed &= test_multiple_xstring_ops();
        all_passed &= test_allocator_types();
        all_passed &= test_nested_structures();
        all_passed &= test_generated_types();
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
