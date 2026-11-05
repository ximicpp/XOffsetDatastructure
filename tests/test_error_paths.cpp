// ============================================================================
// Test: Error Paths
// Purpose: Verify error handling for edge cases:
//   - F3: make<T>() duplicate call detection
//   - F4: root<T>() on empty buffer (throws, not UB)
//   - F5: save() produces compact output
//   - F6: has_root<T>() on various states
// ============================================================================

#include <iostream>
#include <cassert>
#include <string>
#include "test_allocator_ctors.hpp"

using namespace XOffsetDatastructure;

struct SimpleData {
#if XOFFSET_TEST_ALLOCATOR_CTORS
    template <typename Allocator>
    SimpleData(Allocator alloc) : name(alloc) {}
#endif

    int32_t id = 0;
    XString name;
};
#if !XOFFSET_TEST_ALLOCATOR_CTORS
XOFFSET_RECORD_DERIVED(SimpleData, (), id, name)
#endif

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
    } catch (const XException& e) {
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
    } catch (const XException& e) {
        threw = true;
        std::cout << "  Caught expected exception: " << e.what() << "\n";
    }
    assert(threw && "root<T>() should throw on empty buffer");
    std::cout << "  [OK] root<T>() on empty buffer correctly throws\n";

    return true;
}

// ============================================================================
// Test 3: save() produces compact output
// ============================================================================
bool test_save_compact() {
    std::cout << "\n[TEST] save() produces compact output\n";
    std::cout << std::string(50, '-') << "\n";

    XBuffer xbuf(8192);  // large buffer
    auto* data = xbuf.make<SimpleData>();
    data->id = 99;
    data->name = "CompactTest";

    std::string compact = xbuf.save();

    std::cout << "  save: " << compact.size() << " bytes\n";
    assert(compact.size() < 8192);
    assert(compact.size() > 0);
    std::cout << "  [OK] save() output is compact (< 8192 original)\n";

    // Verify round-trip
    XBuffer loaded = XBuffer::load_unverified(compact);
    assert(loaded.unsafe_has_root<SimpleData>());
    auto& r = loaded.unsafe_root<SimpleData>();
    assert(r.id == 99);
    assert(std::string(r.name.c_str()) == "CompactTest");
    std::cout << "  [OK] Round-trip verified\n";

    return true;
}

// ============================================================================
// Test 4: has_root<T>() on various states
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

// ============================================================================
// Test 5: a maximum capacity above MAX_PAYLOAD (2 GiB - 8) fails at create,
// with capacity_exceeded and a message that names the limit; the limit itself
// is accepted and grows.
// ============================================================================
bool test_payload_limit() {
    std::cout << "\n[TEST] maximum capacity above the 2 GiB payload limit\n";
    std::cout << std::string(50, '-') << "\n";
    const std::size_t gib = std::size_t(1) << 30;
    for (std::size_t bytes : {2 * gib, 4 * gib, std::size_t(XBufferCore::MAX_PAYLOAD) + 1}) {
        for (auto growth : {XGrowPolicy::Explicit, XGrowPolicy::Automatic}) {
            bool rejected = false;
            try {
                auto b = XBuffer::create<SimpleData>(4096, XBuffer::maximum_capacity(bytes).with_growth(growth));
            } catch (const XException& e) {
                rejected = e.code() == XErrc::capacity_exceeded &&
                           std::string(e.what()).find("2 GiB") != std::string::npos;
            }
            assert(rejected);
            if (!rejected) return false;
        }
        bool legacy = false;
        try {
            XBuffer b(4096, XBuffer::max_capacity(bytes));
        } catch (const XException& e) {
            legacy = e.code() == XErrc::capacity_exceeded;
        }
        assert(legacy);
        if (!legacy) return false;
        std::cout << "  " << (bytes >> 20) << " MiB rejected at create ... [OK]\n";
    }
    auto b = XBuffer::create<SimpleData>(4096, XBuffer::maximum_capacity(XBufferCore::MAX_PAYLOAD)
                                                  .with_growth(XGrowPolicy::Automatic));
    b.root<SimpleData>().name.assign(std::string(1u << 20, 'x'));
    assert(b.root<SimpleData>().name.size() == (1u << 20));
    std::cout << "  MAX_PAYLOAD accepted and grows ... [OK]\n";
    return true;
}

int main() {
    std::cout << "=== Error Paths & Untested APIs ===\n";

    bool all_passed = true;
    all_passed &= test_make_duplicate();
    all_passed &= test_root_empty();
    all_passed &= test_save_compact();
    all_passed &= test_has_root_states();
    all_passed &= test_payload_limit();

    std::cout << "\n";
    if (all_passed) {
        std::cout << "[PASS] All error path & untested API tests passed!\n";
        return 0;
    } else {
        std::cout << "[FAIL] Some tests failed!\n";
        return 1;
    }
}
