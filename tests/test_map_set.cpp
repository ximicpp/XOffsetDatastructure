// ============================================================================
// Test: Map and Set Operations
// Purpose: Test XMap and XSet containers
// ============================================================================

#include <iostream>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include "test_allocator_ctors.hpp"

using namespace XOffsetDatastructure;

struct MapSetTest {
#if XOFFSET_TEST_ALLOCATOR_CTORS
    template <typename Allocator>
    MapSetTest(Allocator allocator) 
        : intSet(allocator),
          stringSet(allocator),
          intMap(allocator),
          stringMap(allocator) {}
#endif
    
    XSet<int> intSet;
    XSet<XString> stringSet;
    XMap<int, XString> intMap;
    XMap<XString, int> stringMap;
};
#if !XOFFSET_TEST_ALLOCATOR_CTORS
XOFFSET_RECORD_DERIVED(MapSetTest, (), intSet, stringSet, intMap, stringMap)
#endif

bool test_map_set_operations() {
    std::cout << "\n[TEST] Map and Set Operations\n";
    std::cout << std::string(50, '-') << "\n";
    
    // Fixed-schema buffers now carry allocator state plus frozen container
    // metadata, so this legacy map/set scenario needs a slightly larger arena.
    XBuffer xbuf(8192);
    auto* obj = xbuf.make<MapSetTest>();
    
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
        obj->stringSet.emplace(str.c_str());
    }
    assert(obj->stringSet.size() == 10);
    std::cout << "[OK]\n";
    
    // Test 4: Map insertion
    std::cout << "Test 4: Map insertion... ";
    for (int i = 0; i < 20; ++i) {
        std::string value = "Value_" + std::to_string(i);
        
        obj->intMap.emplace(i, value.c_str());
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
        
        obj->stringMap.emplace(key.c_str(), i * 10);
    }
    assert(obj->stringMap.size() == 15);
    
    
    auto it2 = obj->stringMap.find("Key_5");
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
    auto buffer = xbuf.get_buffer();
    XBuffer loaded_buf(buffer.data(), buffer.size());
    auto& loaded_obj = loaded_buf.unsafe_root<MapSetTest>();
    assert(loaded_obj.intSet.size() == 50);
    assert(loaded_obj.intMap.size() == 20);
    assert(loaded_obj.stringMap.size() == 15);
    std::cout << "[OK]\n";
    
    // Test 9: inserts in every order against std::set / std::map. Keys in
    // ascending order take the append path (one compare with the last key);
    // a key equal to the last, below it, or in between takes the search.
    std::cout << "Test 9: Insert order model... ";
    {
        XBuffer order_buf(1 << 20);
        auto* o = order_buf.make<MapSetTest>();
        std::set<int> model_set;
        std::map<int, std::string> model_map;
        std::set<std::string> model_strings;
        std::uint32_t state = 12345u;
        auto next = [&] { state = state * 1103515245u + 12345u; return int((state >> 8) % 600u); };
        auto same = [&] {
            if (o->intSet.size() != model_set.size() || o->intMap.size() != model_map.size() ||
                o->stringSet.size() != model_strings.size()) return false;
            auto s = model_set.begin();
            for (int v : o->intSet) if (v != *s++) return false;
            auto m = model_map.begin();
            for (const auto& kv : o->intMap) { if (kv.first != m->first || kv.second != m->second.c_str()) return false; ++m; }
            auto t = model_strings.begin();
            for (const auto& v : o->stringSet) if (v != t->c_str()) return false; else ++t;
            return true;
        };
        for (int step = 0; step < 900; ++step) {
            int key;
            switch (step % 6) {
            case 0: key = model_set.empty() ? 0 : *model_set.rbegin() + 1 + next() % 3; break; // append
            case 1: key = model_set.empty() ? 0 : *model_set.rbegin(); break;                 // equal to last
            case 2: key = model_set.empty() ? 0 : *model_set.begin() - 1; break;              // below all
            default: key = next(); break;                                                      // anywhere
            }
            const std::string value = "V" + std::to_string(key);
            const bool set_new = o->intSet.insert(key).second;
            if (set_new != model_set.insert(key).second) return false;
            const bool map_new = o->intMap.try_emplace(key, value.c_str()).second;
            if (map_new != model_map.try_emplace(key, value).second) return false;
            char name[16];
            std::snprintf(name, sizeof name, "k%06d", key + 1000);
            if (o->stringSet.emplace(name).second != model_strings.insert(name).second) return false;
            if (step % 50 == 0 && !same()) return false;
        }
        if (!same()) return false;
        o->intMap[*model_set.rbegin() + 7] = "tail";
        model_map[*model_set.rbegin() + 7] = "tail";
        o->intMap[*model_set.begin()] = "head";
        model_map[*model_set.begin()] = "head";
        if (!same()) return false;
    }
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
