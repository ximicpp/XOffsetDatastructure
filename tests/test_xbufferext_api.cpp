// ============================================================================
// Test: XBufferExt API Coverage
// Purpose: Test find_ex, find_or_make, allocator, and stats APIs
// ============================================================================

#include <iostream>
#include <cassert>
#include "../xoffsetdatastructure2.hpp"

using namespace XOffsetDatastructure2;

struct TestData {
    int32_t id;
    XString name;
    XVector<int32_t> values;

    template <typename Allocator>
    TestData(Allocator allocator)
        : id(0), name(allocator), values(allocator) {}
};

bool test_find_ex() {
    std::cout << "\n[TEST] find_ex<T>\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt xbuf(4096);

    // Test 1: find_ex on non-existent object
    std::cout << "Test 1: find_ex non-existent... ";
    auto [ptr1, found1] = xbuf.find_ex<TestData>("missing");
    assert(ptr1 == nullptr);
    std::cout << "[OK]\n";

    // Test 2: Create object, then find_ex
    std::cout << "Test 2: find_ex existing... ";
    auto* obj = xbuf.make<TestData>("myobj");
    obj->id = 42;
    obj->name = XString("hello", xbuf.allocator<XString>());
    auto [ptr2, found2] = xbuf.find_ex<TestData>("myobj");
    assert(ptr2 != nullptr);
    assert(ptr2->id == 42);
    assert(std::string(ptr2->name.c_str()) == "hello");
    std::cout << "[OK]\n";

    // Test 3: find_ex returns same pointer as make
    std::cout << "Test 3: find_ex returns same pointer... ";
    assert(ptr2 == obj);
    std::cout << "[OK]\n";

    return true;
}

bool test_find_or_make() {
    std::cout << "\n[TEST] find_or_make<T>\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt xbuf(4096);

    // Test 1: find_or_make creates new object
    std::cout << "Test 1: find_or_make creates... ";
    auto* obj1 = xbuf.find_or_make<TestData>("data1");
    assert(obj1 != nullptr);
    obj1->id = 100;
    obj1->name = XString("first", xbuf.allocator<XString>());
    std::cout << "[OK]\n";

    // Test 2: find_or_make returns existing object
    std::cout << "Test 2: find_or_make finds existing... ";
    auto* obj2 = xbuf.find_or_make<TestData>("data1");
    assert(obj2 != nullptr);
    assert(obj2 == obj1);
    assert(obj2->id == 100);
    assert(std::string(obj2->name.c_str()) == "first");
    std::cout << "[OK]\n";

    // Test 3: find_or_make creates second distinct object
    std::cout << "Test 3: find_or_make second object... ";
    auto* obj3 = xbuf.find_or_make<TestData>("data2");
    assert(obj3 != nullptr);
    assert(obj3 != obj1);
    obj3->id = 200;
    assert(obj1->id == 100);
    assert(obj3->id == 200);
    std::cout << "[OK]\n";

    return true;
}

bool test_stats() {
    std::cout << "\n[TEST] stats()\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt xbuf(4096);

    // Test 1: Stats on empty buffer
    std::cout << "Test 1: stats on empty buffer... ";
    auto s1 = xbuf.stats();
    assert(s1.total_size == 4096);
    assert(s1.used_size > 0);  // overhead from segment manager
    assert(s1.free_size > 0);
    assert(s1.total_size == s1.used_size + s1.free_size);
    std::cout << "[OK]\n";

    // Test 2: Stats after adding data
    std::cout << "Test 2: stats after adding data... ";
    auto* obj = xbuf.make<TestData>("stats_test");
    obj->id = 1;
    for (int i = 0; i < 50; ++i) {
        obj->values.push_back(i);
    }
    auto s2 = xbuf.stats();
    assert(s2.total_size == 4096);
    assert(s2.used_size > s1.used_size);
    assert(s2.free_size < s1.free_size);
    std::cout << "[OK]\n";

    // Test 3: usage_percent and free_percent
    std::cout << "Test 3: percent calculations... ";
    assert(s2.usage_percent() > 0.0 && s2.usage_percent() < 100.0);
    assert(s2.free_percent() > 0.0 && s2.free_percent() < 100.0);
    double total_pct = s2.usage_percent() + s2.free_percent();
    assert(total_pct > 99.9 && total_pct < 100.1);
    std::cout << "[OK]\n";

    return true;
}

bool test_allocator_api() {
    std::cout << "\n[TEST] allocator<T>()\n";
    std::cout << std::string(50, '-') << "\n";

    XBufferExt xbuf(4096);

    // Test 1: Get allocator for XString
    std::cout << "Test 1: allocator for XString... ";
    auto alloc = xbuf.allocator<XString>();
    XString str("allocated_string", alloc);
    assert(std::string(str.c_str()) == "allocated_string");
    std::cout << "[OK]\n";

    // Test 2: Use allocator in struct construction
    std::cout << "Test 2: allocator in struct... ";
    auto* obj = xbuf.make<TestData>("alloc_test");
    obj->name = XString("via_allocator", xbuf.allocator<XString>());
    assert(std::string(obj->name.c_str()) == "via_allocator");
    std::cout << "[OK]\n";

    return true;
}

int main() {
    std::cout << "========================================\n";
    std::cout << "  XBufferExt API Test\n";
    std::cout << "========================================\n";

    bool all_passed = true;
    all_passed &= test_find_ex();
    all_passed &= test_find_or_make();
    all_passed &= test_stats();
    all_passed &= test_allocator_api();

    std::cout << "\n========================================\n";
    if (all_passed) {
        std::cout << "[PASS] All XBufferExt API tests passed!\n";
        return 0;
    } else {
        std::cout << "[FAIL] Some tests failed!\n";
        return 1;
    }
}
