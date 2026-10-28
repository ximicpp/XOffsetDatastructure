// ============================================================================
// Test: Vector Operations
// Purpose: Test XVector with various data types and operations
// ============================================================================

#include <iostream>
#include <cassert>
#include <algorithm>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include "test_allocator_ctors.hpp"

using namespace XOffsetDatastructure;

struct VectorTest {
#if XOFFSET_TEST_ALLOCATOR_CTORS
    template <typename Allocator>
    VectorTest(Allocator allocator) 
        : intVector(allocator), 
          floatVector(allocator),
          stringVector(allocator) {}
#endif
    
    XVector<int> intVector;
    XVector<float> floatVector;
    XVector<XString> stringVector;
};
#if !XOFFSET_TEST_ALLOCATOR_CTORS
XOFFSET_RECORD_DERIVED(VectorTest, (), intVector, floatVector, stringVector)
#endif

bool test_vector_operations() {
    std::cout << "\n[TEST] Vector Operations\n";
    std::cout << std::string(50, '-') << "\n";
    
    XBuffer xbuf(4096);
    auto* obj = xbuf.make<VectorTest>();
    
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
        obj->stringVector.push_back(str.c_str());
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
    auto buffer = xbuf.get_buffer();
    XBuffer loaded_buf(buffer.data(), buffer.size());
    auto& loaded_obj = loaded_buf.unsafe_root<VectorTest>();
    assert(loaded_obj.floatVector.size() == 100);
    assert(loaded_obj.stringVector.size() == 10);
    assert(loaded_obj.stringVector[5] == "String_5");
    std::cout << "[OK]\n";
    
    std::cout << "[PASS] All vector tests passed!\n";
    return true;
}

// The containers' iterators are a contiguous iterator class, not T* (3.2):
// operator* returns T&, which lets loops over them be if-converted and
// vectorized like loops over std::vector.
using IntIt = XVector<int>::iterator;
using IntCIt = XVector<int>::const_iterator;
static_assert(std::contiguous_iterator<IntIt>);
static_assert(std::contiguous_iterator<IntCIt>);
static_assert(std::contiguous_iterator<XSet<int>::const_iterator>);
static_assert(std::contiguous_iterator<XMap<int, int>::iterator>);
static_assert(std::ranges::contiguous_range<XVector<int>>);
static_assert(std::ranges::contiguous_range<const XMap<int, int>>);
static_assert(std::is_convertible_v<IntIt, IntCIt> && !std::is_convertible_v<IntCIt, IntIt>);
static_assert(std::is_same_v<decltype(*std::declval<IntIt>()), int&>);
static_assert(std::is_same_v<decltype(*std::declval<IntCIt>()), const int&>);
static_assert(sizeof(IntIt) == sizeof(int*) && std::is_trivially_copyable_v<IntIt>);

bool test_vector_iterators() {
    std::cout << "\n[TEST] Vector iterators\n";
    XBuffer xbuf(4096);
    auto* obj = xbuf.make<VectorTest>();
    auto& v = obj->intVector;
    assert(v.begin() == v.end());
    for (int i = 0; i < 8; ++i) v.push_back(7 - i);

    int* data = v.data();
    IntIt it = v.begin();
    assert(std::to_address(it) == data && std::to_address(v.end()) == data + 8);
    assert(&*it == data && it[3] == 4 && *(it + 2) == 5 && *(2 + it) == 5);
    assert(v.end() - v.begin() == 8 && (v.end() - 1)[0] == 0);
    IntCIt cit = it;
    assert(cit == it && cit < v.end() && (v.end() <=> cit) > 0);
    ++it; it++; --it; it += 3; it -= 1;
    assert(it - v.begin() == 3);

    std::sort(v.begin(), v.end());
    assert(std::ranges::is_sorted(v) && v[0] == 0 && v[7] == 7);
    assert(std::ranges::find(v, 5) - v.begin() == 5);
    std::span<int> s(v);
    assert(s.data() == data && s.size() == 8);

    // T* still converts to an iterator, so pointer positions keep working.
    v.erase(data + 1);
    v.insert(v.data() + 1, 1);
    assert(v.size() == 8 && v[1] == 1 && v[2] == 2);

    const auto& cv = v;
    int sum = 0;
    for (const int& x : cv) sum += x;
    assert(sum == 28);
    std::cout << "[PASS] Vector iterators\n";
    return true;
}

int main() {
    try {
        return test_vector_operations() && test_vector_iterators() ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
