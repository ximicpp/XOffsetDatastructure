// Test MSVC compatibility

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"
#include "../generated/game_data.hpp"

using namespace XOffsetDatastructure2;

// MSVC Issue: Struct with skipped members in init list
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

bool test_init_list_compatibility() {
    std::cout << "\nTesting init list compatibility...\n";
    
    XBufferExt xbuf(2048);
    
    // Create struct with skipped members
    std::cout << "  create object... ";
    auto* obj = xbuf.make<GameLikeStruct>("game_like");
    std::cout << "ok\n";
    
    // Verify default values
    std::cout << "  verify defaults... ";
    assert(obj->id == 0);
    assert(obj->level == 0);
    assert(obj->health == 0.0f);
    std::cout << "ok\n";
    
    // Set all fields
    std::cout << "  set fields... ";
    obj->id = 999;
    obj->level = 50;
    obj->health = 75.5f;
    obj->name = XString("TestName", xbuf.allocator<XString>());
    assert(obj->name == "TestName");
    std::cout << "ok\n";
    
    // Add to containers
    std::cout << "  add to containers... ";
    obj->items.push_back(1);
    obj->int_set.insert(100);
    XString key("key1", xbuf.allocator<XString>());
    obj->string_map.emplace(key, 200);
    assert(obj->items.size() == 1);
    assert(obj->int_set.size() == 1);
    assert(obj->string_map.size() == 1);
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

int main() {
    try {
        bool all_passed = test_init_list_compatibility();
        
        if (all_passed) {
            std::cout << "\nAll MSVC compatibility tests passed\n";
        } else {
            std::cout << "\nSome tests failed\n";
        }
        
        return all_passed ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
}
