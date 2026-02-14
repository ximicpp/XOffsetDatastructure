// ============================================================================
// Test: XBuffer API (Single-Object Model)
// Purpose: Test the simplified single-root-object API:
//   make<T>(), root<T>(), has_root<T>(), handle<T>(),
//   make_handle<T>(), stats(), save_to_string(), load_from_string()
// ============================================================================

#include <iostream>
#include <cassert>
#include <string>
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct TestData {
    template <typename Allocator>
    TestData(Allocator alloc) : name(alloc), items(alloc) {}
    
    int32_t id = 0;
    int32_t score = 0;
    XString name;
    XVector<int32_t> items;
};

// ============================================================================
// Test 1: make<T>() + root<T>() + has_root<T>()
// ============================================================================
bool test_make_and_root() {
    std::cout << "\n[TEST] make<T>() + root<T>() + has_root<T>()\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(4096);

    // Before make
    assert(!xbuf.has_root<TestData>());
    std::cout << "  has_root before make: false ... [OK]\n";

    // make
    auto* obj = xbuf.make<TestData>();
    assert(obj != nullptr);
    obj->id = 42;
    obj->name = "Test";
    std::cout << "  make<TestData>() ... [OK]\n";

    // has_root + root
    assert(xbuf.has_root<TestData>());
    auto& r = xbuf.root<TestData>();
    assert(r.id == 42);
    assert(std::string(r.name.c_str()) == "Test");
    std::cout << "  root<TestData>() matches ... [OK]\n";

    return true;
}

// ============================================================================
// Test 2: Serialization round-trip
// ============================================================================
bool test_serialization() {
    std::cout << "\n[TEST] save_to_string / load_from_string\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(4096);
    auto* obj = xbuf.make<TestData>();
    obj->id = 99;
    obj->score = 1000;
    obj->name = "Serialized";
    obj->items.push_back(10);
    obj->items.push_back(20);

    std::string data = xbuf.save_to_string();
    std::cout << "  Serialized: " << data.size() << " bytes ... [OK]\n";

    XBuffer loaded = XBuffer::load_from_string(data);
    assert(loaded.has_root<TestData>());
    auto& r = loaded.root<TestData>();
    assert(r.id == 99);
    assert(r.score == 1000);
    assert(std::string(r.name.c_str()) == "Serialized");
    assert(r.items.size() == 2);
    assert(r.items[0] == 10);
    assert(r.items[1] == 20);
    std::cout << "  load_from_string round-trip ... [OK]\n";

    return true;
}

// ============================================================================
// Test 3: stats()
// ============================================================================
bool test_stats() {
    std::cout << "\n[TEST] stats()\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(4096);
    auto* obj = xbuf.make<TestData>();
    obj->name = "StatsTest";

    auto s = xbuf.stats();
    assert(s.total_size == 4096);
    assert(s.used_size > 0);
    assert(s.free_size > 0);
    assert(s.used_size + s.free_size == s.total_size);
    std::cout << "  total=" << s.total_size
              << " used=" << s.used_size
              << " free=" << s.free_size
              << " (" << s.usage_percent() << "%) ... [OK]\n";

    return true;
}

// ============================================================================
// Test 4: allocator<T>()
// ============================================================================
bool test_allocator() {
    std::cout << "\n[TEST] allocator<T>()\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(4096);
    auto* obj = xbuf.make<TestData>();

    auto alloc = xbuf.allocator<int32_t>();
    // Allocator should be valid (no crash)
    (void)alloc;
    std::cout << "  allocator<int32_t>() created ... [OK]\n";

    return true;
}

// ============================================================================
// Test 5: grow() + root() re-acquire
// ============================================================================
bool test_grow_root() {
    std::cout << "\n[TEST] grow() + root() re-acquire\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(1024);
    auto* obj = xbuf.make<TestData>();
    obj->id = 55;
    obj->name = "GrowTest";

    xbuf.grow(4096);

    auto& r = xbuf.root<TestData>();
    assert(r.id == 55);
    assert(std::string(r.name.c_str()) == "GrowTest");
    std::cout << "  root() valid after grow() ... [OK]\n";

    return true;
}

int main() {
    std::cout << "=== XBuffer API Tests (Single-Object Model) ===\n";

    bool all_passed = true;
    all_passed &= test_make_and_root();
    all_passed &= test_serialization();
    all_passed &= test_stats();
    all_passed &= test_allocator();
    all_passed &= test_grow_root();

    std::cout << "\n";
    if (all_passed) {
        std::cout << "[PASS] All XBuffer API tests passed!\n";
        return 0;
    } else {
        std::cout << "[FAIL] Some tests failed!\n";
        return 1;
    }
}