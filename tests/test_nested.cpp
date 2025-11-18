// Test nested structures

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

struct alignas(BASIC_ALIGNMENT) InnerObject {
    template <typename Allocator>
    InnerObject(Allocator allocator) : name(allocator), data(allocator) {}
    
    int id;
    XString name;
    XVector<int> data;
};

struct alignas(BASIC_ALIGNMENT) MiddleObject {
    template <typename Allocator>
    MiddleObject(Allocator allocator) 
        : name(allocator), inner(allocator), values(allocator) {}
    
    XString name;
    InnerObject inner;
    XVector<int> values;
};

struct alignas(BASIC_ALIGNMENT) OuterObject {
    template <typename Allocator>
    OuterObject(Allocator allocator) 
        : title(allocator), middle(allocator), innerList(allocator) {}
    
    XString title;
    MiddleObject middle;
    XVector<InnerObject> innerList;
};

bool test_nested_structures() {
    std::cout << "\nTesting nested structures...\n";
    
    XBuffer xbuf(8192);
    auto* obj = xbuf.make_root<OuterObject>("Nested");
    
    // Initialize nested structure
    std::cout << "  initialize... ";
    obj->title = XString("OuterTitle", xbuf.allocator<XString>());
    obj->middle.name = XString("MiddleName", xbuf.allocator<XString>());
    obj->middle.inner.id = 42;
    for (int i = 0; i < 10; ++i) {
        obj->middle.inner.data.push_back(i * 2);
    }
    std::cout << "ok\n";
    
    // Verify nested data
    std::cout << "  verify data... ";
    assert(obj->title == "OuterTitle");
    assert(obj->middle.name == "MiddleName");
    assert(obj->middle.inner.id == 42);
    assert(obj->middle.inner.data.size() == 10);
    assert(obj->middle.inner.data[5] == 10);
    std::cout << "ok\n";
    
    // Vector of nested objects
    std::cout << "  vector of objects... ";
    for (int i = 0; i < 5; ++i) {
        obj->innerList.emplace_back(xbuf.allocator<InnerObject>());
        obj->innerList.back().id = i * 100;
        for (int j = 0; j < i + 1; ++j) {
            obj->innerList.back().data.push_back(j);
        }
    }
    assert(obj->innerList.size() == 5);
    assert(obj->innerList[0].data.size() == 1);
    assert(obj->innerList[4].data.size() == 5);
    assert(obj->innerList[3].id == 300);
    std::cout << "ok\n";
    
    // Deep access
    std::cout << "  deep access... ";
    std::size_t total_elements = 0;
    for (const auto& inner : obj->innerList) {
        total_elements += inner.data.size();
    }
    assert(total_elements == 15); // 1+2+3+4+5
    std::cout << "ok\n";
    
    // Persistence
    std::cout << "  persistence... ";
    auto* buffer = xbuf.get_buffer();
    XBuffer loaded_buf(buffer->data(), buffer->size());
    auto* loaded = loaded_buf.find_root<OuterObject>("Nested").first;
    
    assert(loaded->title == "OuterTitle");
    assert(loaded->middle.name == "MiddleName");
    assert(loaded->middle.inner.id == 42);
    assert(loaded->innerList.size() == 5);
    assert(loaded->innerList[2].data.size() == 3);
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

int main() {
    try {
        return test_nested_structures() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }
}
