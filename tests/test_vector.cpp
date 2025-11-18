// Test XVector operations

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

struct alignas(BASIC_ALIGNMENT) VectorTest {
    template <typename Allocator>
    VectorTest(Allocator allocator) 
        : intVector(allocator), 
          floatVector(allocator),
          stringVector(allocator) {}
    
    XVector<int> intVector;
    XVector<float> floatVector;
    XVector<XString> stringVector;
};

bool test_vector_operations() {
    std::cout << "\nTesting vector operations...\n";
    
    XBuffer xbuf(4096);
    auto* obj = xbuf.make_root<VectorTest>("VectorTest");
    
    // push_back
    std::cout << "  push_back... ";
    for (int i = 0; i < 100; ++i) {
        obj->intVector.push_back(i);
        obj->floatVector.push_back(i * 1.5f);
    }
    assert(obj->intVector.size() == 100);
    assert(obj->floatVector.size() == 100);
    std::cout << "ok\n";
    
    // Element access
    std::cout << "  element access... ";
    assert(obj->intVector[0] == 0);
    assert(obj->intVector[50] == 50);
    assert(obj->intVector[99] == 99);
    assert(obj->floatVector[10] == 15.0f);
    std::cout << "ok\n";
    
    // String vector
    std::cout << "  string vector... ";
    for (int i = 0; i < 20; ++i) {
        std::string str = "String_" + std::to_string(i);
        obj->stringVector.emplace_back(XString(str.c_str(), xbuf.allocator<XString>()));
    }
    assert(obj->stringVector.size() == 20);
    assert(obj->stringVector[0] == "String_0");
    assert(obj->stringVector[19] == "String_19");
    std::cout << "ok\n";
    
    // Iteration
    std::cout << "  iteration... ";
    int sum = 0;
    for (const auto& val : obj->intVector) {
        sum += val;
    }
    assert(sum == 4950);
    std::cout << "ok\n";
    
    // Clear
    std::cout << "  clear... ";
    obj->intVector.clear();
    assert(obj->intVector.empty());
    assert(obj->intVector.size() == 0);
    std::cout << "ok\n";
    
    // Persistence
    std::cout << "  persistence... ";
    auto* buffer = xbuf.get_buffer();
    XBuffer loaded_buf(buffer->data(), buffer->size());
    auto* loaded_obj = loaded_buf.find_root<VectorTest>("VectorTest").first;
    assert(loaded_obj->floatVector.size() == 100);
    assert(loaded_obj->stringVector.size() == 20);
    assert(loaded_obj->stringVector[5] == "String_5");
    assert(loaded_obj->stringVector[19] == "String_19");
    std::cout << "ok\n";
    
    std::cout << "All tests passed\n";
    return true;
}

int main() {
    try {
        return test_vector_operations() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
