// ============================================================================
// Test: Vector Operations
// Purpose: Test XVector with various data types and operations
// ============================================================================

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
    std::cout << "\n[TEST] Vector Operations\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBufferExt xbuf(4096);
    auto* obj = xbuf.make<VectorTest>("VectorTest");
    
    // Test 1: push_back
    std::cout << "Test 1: push_back operations... ";
    for (int i = 0; i < 100; ++i) {
        obj->intVector.push_back(i);
        obj->floatVector.push_back(i * 1.5f);
    }
    assert(obj->intVector.size() == 100);
    assert(obj->floatVector.size() == 100);
    std::cout << "[OK]\n";
    
    // Test 2: Element access
    std::cout << "Test 2: Element access... ";
    assert(obj->intVector[0] == 0);
    assert(obj->intVector[50] == 50);
    assert(obj->intVector[99] == 99);
    assert(obj->floatVector[10] == 15.0f);
    std::cout << "[OK]\n";
    
    // Test 3: String vector
    std::cout << "Test 3: String vector operations... ";
    for (int i = 0; i < 10; ++i) {
        std::string str = "String_" + std::to_string(i);
        obj->stringVector.emplace_back(xbuf.make<XString>(str));
    }
    assert(obj->stringVector.size() == 10);
    assert(obj->stringVector[0] == "String_0");
    assert(obj->stringVector[9] == "String_9");
    std::cout << "[OK]\n";
    
    // Test 4: Iteration
    std::cout << "Test 4: Iterator operations... ";
    int sum = 0;
    for (const auto& val : obj->intVector) {
        sum += val;
    }
    assert(sum == 4950); // Sum of 0-99
    std::cout << "[OK]\n";
    
    // Test 5: Clear and empty
    std::cout << "Test 5: Clear operations... ";
    obj->intVector.clear();
    assert(obj->intVector.empty());
    assert(obj->intVector.size() == 0);
    std::cout << "[OK]\n";
    
    // Test 6: Persistence
    std::cout << "Test 6: Persistence test... ";
    auto* buffer = xbuf.get_buffer();
    XBufferExt loaded_buf(buffer->data(), buffer->size());
    auto* loaded_obj = loaded_buf.find<VectorTest>("VectorTest").first;
    assert(loaded_obj->floatVector.size() == 100);
    assert(loaded_obj->stringVector.size() == 10);
    assert(loaded_obj->stringVector[5] == "String_5");
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] All vector tests passed!\n";
    return true;
}

int main() {
    try {
        return test_vector_operations() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
