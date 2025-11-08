// ============================================================================
// Test: Map and Set Operations
// Purpose: Test XMap and XSet containers
// ============================================================================

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

struct alignas(BASIC_ALIGNMENT) MapSetTest {
    template <typename Allocator>
    MapSetTest(Allocator allocator) 
        : intSet(allocator),
          stringSet(allocator),
          intMap(allocator),
          stringMap(allocator) {}
    
    XSet<int> intSet;
    XSet<XString> stringSet;
    XMap<int, XString> intMap;
    XMap<XString, int> stringMap;
};

bool test_map_set_operations() {
    std::cout << "\n[TEST] Map and Set Operations\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBufferExt xbuf(4096);
    auto* obj = xbuf.make<MapSetTest>("MapSetTest");
    
    // Test 1: Set insertion
    std::cout << "Test 1: Set insertion... ";
    for (int i = 0; i < 50; ++i) {
        obj->intSet.insert(i);
    }
    // Insert duplicates (should be ignored)
    for (int i = 0; i < 25; ++i) {
        obj->intSet.insert(i);
    }
    assert(obj->intSet.size() == 50); // Only unique values
    std::cout << "[OK]\n";
    
    // Test 2: Set find
    std::cout << "Test 2: Set find operations... ";
    assert(obj->intSet.find(25) != obj->intSet.end());
    assert(obj->intSet.find(100) == obj->intSet.end());
    std::cout << "[OK]\n";
    
    // Test 3: String set
    std::cout << "Test 3: String set operations... ";
    for (int i = 0; i < 10; ++i) {
        std::string str = "Item_" + std::to_string(i);
        obj->stringSet.emplace(XString(str.c_str(), xbuf.allocator<XString>()));
    }
    assert(obj->stringSet.size() == 10);
    std::cout << "[OK]\n";
    
    // Test 4: Map insertion
    std::cout << "Test 4: Map insertion... ";
    for (int i = 0; i < 20; ++i) {
        std::string value = "Value_" + std::to_string(i);
        XString xvalue = XString(value.c_str(), xbuf.allocator<XString>());
        obj->intMap.emplace(i, xvalue);
    }
    assert(obj->intMap.size() == 20);
    std::cout << "[OK]\n";
    
    // Test 5: Map access
    std::cout << "Test 5: Map access... ";
    auto it = obj->intMap.find(10);
    assert(it != obj->intMap.end());
    assert(it->second == "Value_10");
    std::cout << "[OK]\n";
    
    // Test 6: String key map
    std::cout << "Test 6: String key map... ";
    for (int i = 0; i < 15; ++i) {
        std::string key = "Key_" + std::to_string(i);
        XString xkey = XString(key.c_str(), xbuf.allocator<XString>());
        obj->stringMap.emplace(xkey, i * 10);
    }
    assert(obj->stringMap.size() == 15);
    
    XString search_key = XString("Key_5", xbuf.allocator<XString>());
    auto it2 = obj->stringMap.find(search_key);
    assert(it2 != obj->stringMap.end());
    assert(it2->second == 50);
    std::cout << "[OK]\n";
    
    // Test 7: Iteration
    std::cout << "Test 7: Map iteration... ";
    int count = 0;
    for (const auto& pair : obj->intMap) {
        count++;
    }
    assert(count == 20);
    std::cout << "[OK]\n";
    
    // Test 8: Persistence
    std::cout << "Test 8: Persistence test... ";
    auto* buffer = xbuf.get_buffer();
    XBufferExt loaded_buf(buffer->data(), buffer->size());
    auto* loaded_obj = loaded_buf.find<MapSetTest>("MapSetTest").first;
    assert(loaded_obj->intSet.size() == 50);
    assert(loaded_obj->intMap.size() == 20);
    assert(loaded_obj->stringMap.size() == 15);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] All map/set tests passed!\n";
    return true;
}

int main() {
    try {
        return test_map_set_operations() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
