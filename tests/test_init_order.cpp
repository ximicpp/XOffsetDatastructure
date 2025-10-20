// ============================================================================
// Test: Member Init Order Test
// Purpose: Test if member initialization order matters
// ============================================================================

#include <iostream>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

// Test 1: Init list skips first members (like GameData)
struct StructSkipFirst {
    template <typename Allocator>
    StructSkipFirst(Allocator allocator) 
        : name(allocator), items(allocator), string_map(allocator) {}
    
    int id{0};           // Not in init list
    int level{0};        // Not in init list
    float health{0.0f};  // Not in init list
    XString name;        // First in init list
    XVector<int> items;
    XMap<XString, int> string_map;
};

// Test 2: Init list includes all members
struct StructInitAll {
    template <typename Allocator>
    StructInitAll(Allocator allocator) 
        : name(allocator), items(allocator), string_map(allocator) {
        id = 0;
        level = 0;
        health = 0.0f;
    }
    
    int id;
    int level;
    float health;
    XString name;
    XVector<int> items;
    XMap<XString, int> string_map;
};

int main() {
    std::cout << "\n[TEST] Member Init Order Test\n\n";
    
    try {
        XBufferExt xbuf(4096);
        
        std::cout << "1. Creating StructSkipFirst... ";
        auto* s1 = xbuf.make<StructSkipFirst>("s1");
        std::cout << "[OK]\n";
        
        std::cout << "2. Assigning to name... ";
        s1->name = XString("Test", xbuf.allocator<XString>());
        std::cout << "[OK]\n";
        
        std::cout << "3. Adding to string_map... ";
        XString key("key1", xbuf.allocator<XString>());
        s1->string_map.emplace(key, 100);
        std::cout << "[OK]\n";
        
        std::cout << "4. Creating StructInitAll... ";
        auto* s2 = xbuf.make<StructInitAll>("s2");
        std::cout << "[OK]\n";
        
        std::cout << "\n[PASS] All tests successful!\n\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
