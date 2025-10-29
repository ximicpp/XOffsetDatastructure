// ============================================================================
// Test: Nested Structures
// Purpose: Test nested objects and complex hierarchies
// ============================================================================

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

struct InnerObject {
    template <typename Allocator>
    InnerObject(Allocator allocator) : data(allocator) {}
    
    int id;
    XVector<int> data;
};

struct MiddleObject {
    template <typename Allocator>
    MiddleObject(Allocator allocator) 
        : name(allocator), inner(allocator) {}
    
    XString name;
    InnerObject inner;
};

struct OuterObject {
    template <typename Allocator>
    OuterObject(Allocator allocator) 
        : title(allocator), middle(allocator), innerList(allocator) {}
    
    XString title;
    MiddleObject middle;
    XVector<InnerObject> innerList;
};

bool test_nested_structures() {
    std::cout << "\n[TEST] Nested Structures\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBuffer xbuf(8192);
    auto* obj = xbuf.construct<OuterObject>("Nested")(xbuf.get_segment_manager());
    
    // Test 1: Initialize nested structure
    std::cout << "Test 1: Initialize nested objects... ";
    obj->title = XString("OuterTitle", xbuf.get_segment_manager());
    obj->middle.name = XString("MiddleName", xbuf.get_segment_manager());
    obj->middle.inner.id = 42;
    for (int i = 0; i < 10; ++i) {
        obj->middle.inner.data.push_back(i * 2);
    }
    std::cout << "[OK]\n";
    
    // Test 2: Verify nested data
    std::cout << "Test 2: Verify nested data... ";
    assert(obj->title == "OuterTitle");
    assert(obj->middle.name == "MiddleName");
    assert(obj->middle.inner.id == 42);
    assert(obj->middle.inner.data.size() == 10);
    assert(obj->middle.inner.data[5] == 10);
    std::cout << "[OK]\n";
    
    // Test 3: Vector of nested objects
    std::cout << "Test 3: Vector of nested objects... ";
    for (int i = 0; i < 5; ++i) {
        obj->innerList.emplace_back(xbuf.get_segment_manager());
        obj->innerList.back().id = i * 100;
        for (int j = 0; j < i + 1; ++j) {
            obj->innerList.back().data.push_back(j);
        }
    }
    assert(obj->innerList.size() == 5);
    assert(obj->innerList[0].data.size() == 1);
    assert(obj->innerList[4].data.size() == 5);
    assert(obj->innerList[3].id == 300);
    std::cout << "[OK]\n";
    
    // Test 4: Deep access
    std::cout << "Test 4: Deep access patterns... ";
    size_t total_elements = 0;
    for (const auto& inner : obj->innerList) {
        total_elements += inner.data.size();
    }
    assert(total_elements == 15); // 1+2+3+4+5
    std::cout << "[OK]\n";
    
    // Test 5: Persistence
    std::cout << "Test 5: Persistence of nested structures... ";
    auto* buffer = xbuf.get_buffer();
    XBuffer loaded_buf(buffer->data(), buffer->size());
    auto* loaded = loaded_buf.find<OuterObject>("Nested").first;
    
    assert(loaded->title == "OuterTitle");
    assert(loaded->middle.name == "MiddleName");
    assert(loaded->middle.inner.id == 42);
    assert(loaded->innerList.size() == 5);
    assert(loaded->innerList[2].data.size() == 3);
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] All nested structure tests passed!\n";
    return true;
}

int main() {
    try {
        return test_nested_structures() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
