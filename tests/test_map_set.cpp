// Test XMap and XSet containers

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
    std::cout << "\nTesting map and set operations...\n";
    
    XBufferExt xbuf(4096);
    auto* obj = xbuf.make<MapSetTest>("MapSetTest");
    
    // Set insertion
    std::cout << "  set insertion... ";
    for (int i = 0; i < 50; ++i) {
        obj->intSet.insert(i);
    }
    // Insert duplicates (should be ignored)
    for (int i = 0; i < 25; ++i) {
        obj->intSet.insert(i);
    }
    assert(obj->intSet.size() == 50);
    std::cout << "ok\n";
    
    // Set find
    std::cout << "  set find... ";
    assert(obj->intSet.find(25) != obj->intSet.end());
    assert(obj->intSet.find(100) == obj->intSet.end());
    std::cout << "ok\n";
    
    // String set
    std::cout << "  string set... ";
    for (int i = 0; i < 10; ++i) {
        std::string str = "Item_" + std::to_string(i);
        obj->stringSet.emplace(XString(str.c_str(), xbuf.allocator<XString>()));
    }
    assert(obj->stringSet.size() == 10);
    std::cout << "ok\n";
    
    // Map insertion
    std::cout << "  map insertion... ";
    for (int i = 0; i < 20; ++i) {
        std::string value = "Value_" + std::to_string(i);
        XString xvalue = XString(value.c_str(), xbuf.allocator<XString>());
        obj->intMap.emplace(i, xvalue);
    }
    assert(obj->intMap.size() == 20);
    std::cout << "ok\n";
    
    // Map access
    std::cout << "  map access... ";
    auto it = obj->intMap.find(10);
    assert(it != obj->intMap.end());
    assert(it->second == "Value_10");
    std::cout << "ok\n";
    
    // String key map
    std::cout << "  string key map... ";
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
    std::cout << "ok\n";
    
    // Map iteration
    std::cout << "  iteration... ";
    int count = 0;
    for (const auto& pair : obj->intMap) {
        count++;
    }
    assert(count == 20);
    std::cout << "ok\n";
    
    // Persistence
    std::cout << "  persistence... ";
    auto* buffer = xbuf.get_buffer();
    XBufferExt loaded_buf(buffer->data(), buffer->size());
    auto* loaded_obj = loaded_buf.find<MapSetTest>("MapSetTest").first;
    assert(loaded_obj->intSet.size() == 50);
    assert(loaded_obj->intMap.size() == 20);
    assert(loaded_obj->stringMap.size() == 15);
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

int main() {
    try {
        return test_map_set_operations() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
