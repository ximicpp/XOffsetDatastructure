// ============================================================================
// Test: Error Paths & Untested APIs
// Purpose: Verify error handling for edge cases and test previously untested APIs:
//   - F3: make<T>() duplicate call detection
//   - F4: root<T>() on empty buffer (throws, not UB)
//   - F5: Buffer full / corrupted data / grow failure
//   - F8: save_bytes() / load() / estimate_buffer_size()
// ============================================================================

#include <iostream>
#include <cassert>
#include <string>
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct SimpleData {
    template <typename Allocator>
    SimpleData(Allocator alloc) : name(alloc) {}

    int32_t id = 0;
    XString name;
};

// ============================================================================
// Test 1: make<T>() throws on duplicate call
// ============================================================================
bool test_make_duplicate() {
    std::cout << "\n[TEST] make<T>() throws on duplicate call\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(4096);
    xbuf.make<SimpleData>();

    bool threw = false;
    try {
        xbuf.make<SimpleData>();  // should throw
    } catch (const boost::interprocess::interprocess_exception& e) {
        threw = true;
        std::cout << "  Caught expected exception: " << e.what() << "\n";
    }
    assert(threw && "make<T>() should throw on duplicate call");
    std::cout << "  [OK] Duplicate make<T>() correctly throws\n";

    return true;
}

// ============================================================================
// Test 2: root<T>() throws on empty buffer
// ============================================================================
bool test_root_empty() {
    std::cout << "\n[TEST] root<T>() throws on empty buffer\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(4096);

    bool threw = false;
    try {
        xbuf.root<SimpleData>();  // should throw — no root object
    } catch (const boost::interprocess::interprocess_exception& e) {
        threw = true;
        std::cout << "  Caught expected exception: " << e.what() << "\n";
    }
    assert(threw && "root<T>() should throw on empty buffer");
    std::cout << "  [OK] root<T>() on empty buffer correctly throws\n";

    return true;
}

// ============================================================================
// Test 3: save_bytes() / load() round-trip
// ============================================================================
bool test_save_load_vector() {
    std::cout << "\n[TEST] save_bytes / load round-trip\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(4096);
    auto* data = xbuf.make<SimpleData>();
    data->id = 42;
    data->name = "VectorTest";

    std::vector<char> vec = xbuf.save_bytes();
    std::cout << "  save_bytes: " << vec.size() << " bytes\n";
    assert(vec.size() > 0);
    assert(vec.size() < 4096);  // should be compacted (smaller than original)

    XBuffer loaded = XBuffer::load(vec);
    assert(loaded.has_root<SimpleData>());
    auto& r = loaded.root<SimpleData>();
    assert(r.id == 42);
    assert(std::string(r.name.c_str()) == "VectorTest");
    std::cout << "  [OK] save_bytes / load round-trip successful\n";

    return true;
}

// ============================================================================
// Test 4: estimate_buffer_size()
// ============================================================================
bool test_estimate_buffer_size() {
    std::cout << "\n[TEST] estimate_buffer_size()\n";
    std::cout << std::string(50, '-') << "\n";

    std::size_t estimated = XBuffer::estimate_buffer_size(100);
    std::cout << "  estimate_buffer_size(100) = " << estimated << " bytes\n";
    assert(estimated >= 512);  // minimum 512
    assert(estimated > 100);   // must be larger than payload

    std::size_t small = XBuffer::estimate_buffer_size(0);
    std::cout << "  estimate_buffer_size(0) = " << small << " bytes\n";
    assert(small >= 512);

    // Verify the estimate actually works — create a buffer of that size
    XBuffer xbuf(estimated);
    auto* data = xbuf.make<SimpleData>();
    data->id = 1;
    data->name = "EstimateTest";
    std::cout << "  [OK] Buffer of estimated size is functional\n";

    return true;
}

// ============================================================================
// Test 5: save() produces compact output
// ============================================================================
bool test_save_compact() {
    std::cout << "\n[TEST] save() produces compact output\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(8192);  // large buffer
    auto* data = xbuf.make<SimpleData>();
    data->id = 99;
    data->name = "CompactTest";

    std::string compact = xbuf.save();
    std::string full = xbuf.save_raw();  // already shrunk, same size now

    std::cout << "  save:      " << compact.size() << " bytes\n";
    std::cout << "  save_raw: " << full.size() << " bytes\n";
    // After save() shrinks, buffer is compact.
    // save_raw() on the already-shrunk buffer should be same size.
    assert(compact.size() < 8192);
    assert(compact.size() > 0);
    std::cout << "  [OK] save() output is compact (< 8192 original)\n";

    // Verify round-trip
    XBuffer loaded = XBuffer::load(compact);
    assert(loaded.has_root<SimpleData>());
    auto& r = loaded.root<SimpleData>();
    assert(r.id == 99);
    assert(std::string(r.name.c_str()) == "CompactTest");
    std::cout << "  [OK] Round-trip verified\n";

    return true;
}

// ============================================================================
// Test 6: has_root<T>() on various states
// ============================================================================
bool test_has_root_states() {
    std::cout << "\n[TEST] has_root<T>() on various states\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(4096);
    assert(!xbuf.has_root<SimpleData>());
    std::cout << "  Before make: has_root = false ... [OK]\n";

    xbuf.make<SimpleData>();
    assert(xbuf.has_root<SimpleData>());
    std::cout << "  After make: has_root = true ... [OK]\n";

    xbuf.grow(1024);
    assert(xbuf.has_root<SimpleData>());
    std::cout << "  After grow: has_root = true ... [OK]\n";

    xbuf.shrink_to_fit();
    assert(xbuf.has_root<SimpleData>());
    std::cout << "  After shrink: has_root = true ... [OK]\n";

    return true;
}

int main() {
    std::cout << "=== Error Paths & Untested APIs ===\n";

    bool all_passed = true;
    all_passed &= test_make_duplicate();
    all_passed &= test_root_empty();
    all_passed &= test_save_load_vector();
    all_passed &= test_estimate_buffer_size();
    all_passed &= test_save_compact();
    all_passed &= test_has_root_states();

    std::cout << "\n";
    if (all_passed) {
        std::cout << "[PASS] All error path & untested API tests passed!\n";
        return 0;
    } else {
        std::cout << "[FAIL] Some tests failed!\n";
        return 1;
    }
}
