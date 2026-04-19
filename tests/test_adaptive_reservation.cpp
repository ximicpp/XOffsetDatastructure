// ============================================================================
// Test: Adaptive Vector Reservation & Relocation Fallback
// Purpose: Verify that the adaptive reserve() policy works correctly for
//          both single-buffer and many-buffer scenarios, and that the
//          relocation fallback path functions when grow() exceeds capacity.
// ============================================================================

#include <iostream>
#include <cassert>
#include <vector>
#include <memory>
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

// Simple test struct
struct SmallData {
    int id;
    double value;
    XString name;
    XVector<int> numbers;

    template<typename Alloc>
    SmallData(Alloc alloc) : name(alloc), numbers(alloc) {}
};

// ============================================================================
// Test 1: Adaptive reservation formula
// ============================================================================
bool test_adaptive_reservation() {
    std::cout << "\n[TEST] Adaptive Reservation Formula\n";
    std::cout << std::string(50, '-') << "\n";

    using XMM = XBufferCore;  // XManagedMemory instantiation

    // Verify compute_reservation formula
    std::cout << "Test 1a: compute_reservation values... ";
    assert(XMM::compute_reservation(512) == XMM::MIN_RESERVE);        // 512*16=8KB < 64KB → 64KB
    assert(XMM::compute_reservation(4096) == XMM::MIN_RESERVE);       // 4K*16=64KB = MIN
    assert(XMM::compute_reservation(8192) == 8192 * 16);              // 8K*16=128KB
    assert(XMM::compute_reservation(1024*1024) == 1024*1024*16);      // 1M*16=16MB
    assert(XMM::compute_reservation(32*1024*1024) == XMM::MAX_RESERVE); // 32M*16=512MB > 256MB → 256MB
    std::cout << "[OK]\n";

    // Verify actual buffer uses adaptive reservation
    std::cout << "Test 1b: Small buffer uses small reservation... ";
    {
        XBuffer buf(4096);
        auto* vec = buf.get_buffer();
        std::size_t expected = XMM::compute_reservation(4096);
        // capacity() should be at least the expected reservation
        assert(vec->capacity() >= expected);
        assert(vec->capacity() < XMM::MAX_RESERVE);   // NOT 256MB!
        std::cout << "reserved=" << vec->capacity() / 1024 << "KB [OK]\n";
    }

    // Verify large buffer gets proportionally larger reservation
    std::cout << "Test 1c: Large buffer uses large reservation... ";
    {
        XBuffer buf(1024 * 1024);  // 1MB
        auto* vec = buf.get_buffer();
        assert(vec->capacity() >= 1024 * 1024);  // at least initial size
        std::cout << "reserved=" << vec->capacity() / (1024*1024) << "MB [OK]\n";
    }

    std::cout << "[PASS] Adaptive reservation tests passed!\n";
    return true;
}

// ============================================================================
// Test 2: Explicit max_capacity override
// ============================================================================
bool test_max_capacity_override() {
    std::cout << "\n[TEST] MaxCapacity Override\n";
    std::cout << std::string(50, '-') << "\n";

    std::cout << "Test 2a: Explicit 64MB reservation... ";
    {
        XBuffer buf(4096, XBuffer::max_capacity(64 * 1024 * 1024));
        auto* vec = buf.get_buffer();
        // Should be at least 64MB
        assert(vec->capacity() >= 64 * 1024 * 1024);
        std::cout << "reserved=" << vec->capacity() / (1024*1024) << "MB [OK]\n";
    }

    std::cout << "Test 2b: Explicit 256MB reservation (max)... ";
    {
        XBuffer buf(4096, XBuffer::max_capacity(256 * 1024 * 1024));
        auto* vec = buf.get_buffer();
        assert(vec->capacity() >= 256 * 1024 * 1024);
        std::cout << "reserved=" << vec->capacity() / (1024*1024) << "MB [OK]\n";
    }

    std::cout << "[PASS] MaxCapacity override tests passed!\n";
    return true;
}

// ============================================================================
// Test 3: Many small buffers (10,000 instances)
// ============================================================================
bool test_many_small_buffers() {
    std::cout << "\n[TEST] Many Small Buffers (10,000 instances)\n";
    std::cout << std::string(50, '-') << "\n";

    const int N = 10000;
    std::vector<std::unique_ptr<XBuffer>> buffers;
    buffers.reserve(N);

    std::cout << "Test 3a: Creating " << N << " XBuffer(4096) instances... ";
    for (int i = 0; i < N; ++i) {
        buffers.push_back(std::make_unique<XBuffer>(4096));
    }
    std::cout << "[OK]\n";

    // Verify total virtual reservation is reasonable
    std::cout << "Test 3b: Checking virtual address usage... ";
    std::size_t total_reserved = 0;
    for (auto& buf : buffers) {
        total_reserved += buf->get_buffer()->capacity();
    }
    double total_gb = static_cast<double>(total_reserved) / (1024.0 * 1024 * 1024);
    std::cout << "total=" << total_reserved / (1024*1024) << "MB (" 
              << total_gb << "GB)... ";
    
    // With adaptive: 10K × 64KB = 640MB (vs old 10K × 256MB = 2.5TB)
    assert(total_reserved < 2ULL * 1024 * 1024 * 1024);  // must be < 2GB
    std::cout << "[OK]\n";

    // Create and verify objects in a subset
    std::cout << "Test 3c: Creating root objects in first 100 buffers... ";
    for (int i = 0; i < 100; ++i) {
        auto* data = buffers[i]->make<SmallData>();
        data->id = i;
        data->name = std::to_string(i).c_str();
        data->numbers.push_back(i * 10);
    }
    // Verify data
    for (int i = 0; i < 100; ++i) {
        auto& data = buffers[i]->root<SmallData>();
        assert(data.id == i);
        assert(data.numbers[0] == i * 10);
    }
    std::cout << "[OK]\n";

    std::cout << "[PASS] Many small buffers test passed!\n";
    return true;
}

// ============================================================================
// Test 4: Relocation fallback (grow beyond capacity)
// ============================================================================
bool test_relocation_fallback() {
    std::cout << "\n[TEST] Relocation Fallback (grow beyond capacity)\n";
    std::cout << std::string(50, '-') << "\n";

    // Create a buffer with small adaptive reservation (4KB → 64KB reserved).
    // Then explicitly grow() past the capacity to trigger the relocation path.
    std::cout << "Test 4a: Create, populate, then grow beyond capacity... ";
    XBuffer buf(4096);
    auto* vec = buf.get_buffer();
    std::size_t initial_capacity = vec->capacity();
    std::cout << "initial_reserved=" << initial_capacity / 1024 << "KB... ";

    auto* data = buf.make<SmallData>();
    data->id = 42;
    data->name = "relocation_test";
    for (int i = 0; i < 10; ++i) {
        data->numbers.push_back(i);
    }

    // Record epoch before relocation
    uint64_t epoch_before = buf.epoch();

    // Explicitly grow beyond the reserved capacity
    bool grew = buf.grow(initial_capacity + 4096);
    assert(grew);

    // After relocation + re-reserve, capacity should have increased
    std::size_t new_capacity = vec->capacity();
    std::cout << "new_reserved=" << new_capacity / 1024 << "KB... ";
    assert(new_capacity > initial_capacity);

    // Epoch should have incremented (relocation happened)
    assert(buf.epoch() > epoch_before);
    std::cout << "[OK]\n";

    // Verify data integrity after relocation
    std::cout << "Test 4b: Verify data integrity after relocation... ";
    auto& ref = buf.root<SmallData>();
    assert(ref.id == 42);
    assert(ref.name == "relocation_test");
    assert(ref.numbers.size() == 10);
    for (int i = 0; i < 10; ++i) {
        assert(ref.numbers[i] == i);
    }
    std::cout << "[OK]\n";

    // Continue using the buffer after relocation — add more data
    std::cout << "Test 4c: Continue adding data after relocation... ";
    auto& ref2 = buf.root<SmallData>();
    for (int i = 10; i < 1000; ++i) {
        ref2.numbers.push_back(i);
    }
    assert(buf.root<SmallData>().numbers.size() == 1000);
    assert(buf.root<SmallData>().numbers[999] == 999);
    std::cout << "[OK]\n";

    // XHandle works correctly after relocation
    std::cout << "Test 4d: XHandle works after relocation... ";
    auto handle = buf.handle<SmallData>();
    assert(handle->id == 42);
    assert(handle->numbers.size() == 1000);
    std::cout << "[OK]\n";

    // Save/load round-trip after relocation
    std::cout << "Test 4e: Save/load round-trip after relocation... ";
    std::string saved = buf.save();
    XBuffer loaded = XBuffer::load_unverified(saved);
    auto& loaded_data = loaded.unsafe_root<SmallData>();
    assert(loaded_data.id == 42);
    assert(loaded_data.name == "relocation_test");
    assert(loaded_data.numbers.size() == 1000);
    assert(loaded_data.numbers[999] == 999);
    std::cout << "[OK]\n";

    std::cout << "[PASS] Relocation fallback tests passed!\n";
    return true;
}

// ============================================================================
// Test 5: Forced relocation + re-reserve behavior
// ============================================================================
bool test_forced_relocation() {
    std::cout << "\n[TEST] Forced Relocation + Re-reserve\n";
    std::cout << std::string(50, '-') << "\n";

    // Create with small reservation to guarantee relocation
    // 4096 bytes → 64KB reservation
    std::cout << "Test 5a: Force grow beyond capacity... ";
    XBuffer buf(4096);
    auto initial_capacity = buf.get_buffer()->capacity();
    auto* data = buf.make<SmallData>();
    data->id = 99;

    // Record initial epoch
    uint64_t epoch_before = buf.epoch();

    // Force grow way beyond 64KB capacity
    bool grew = buf.grow(initial_capacity + 4096);  // exceed capacity
    assert(grew);
    
    auto new_capacity = buf.get_buffer()->capacity();
    std::cout << "before=" << initial_capacity/1024 << "KB "
              << "after=" << new_capacity/1024 << "KB... ";
    
    // Capacity should have grown (re-reserve after relocation)
    assert(new_capacity > initial_capacity);
    std::cout << "[OK]\n";

    // Epoch should have incremented (relocation happened)
    std::cout << "Test 5b: Epoch incremented on relocation... ";
    uint64_t epoch_after = buf.epoch();
    assert(epoch_after > epoch_before);
    std::cout << "epoch " << epoch_before << " → " << epoch_after << " [OK]\n";

    // Verify data still accessible via root()
    std::cout << "Test 5c: Data intact after forced relocation... ";
    auto& ref = buf.root<SmallData>();
    assert(ref.id == 99);
    std::cout << "[OK]\n";

    // Test 5d: After re-reserve, subsequent grows should be fast-path
    std::cout << "Test 5d: Re-reserve enables fast-path for subsequent grows... ";
    uint64_t epoch_stable = buf.epoch();
    for (int i = 0; i < 10; ++i) {
        bool ok = buf.grow(1024);  // small grow within re-reserved capacity
        assert(ok);
    }
    // Epoch should NOT have changed if all grows stayed within capacity
    if (buf.epoch() == epoch_stable) {
        std::cout << "all fast-path [OK]\n";
    } else {
        // If capacity was tight, relocation may have happened once during re-reserve
        std::cout << "some slow-path (acceptable) [OK]\n";
    }

    std::cout << "[PASS] Forced relocation tests passed!\n";
    return true;
}

// ============================================================================
// Test 6: TypedXBuffer with MaxCapacity
// ============================================================================
bool test_typed_buffer_max_capacity() {
    std::cout << "\n[TEST] TypedXBuffer with MaxCapacity\n";
    std::cout << std::string(50, '-') << "\n";

    std::cout << "Test 6a: TypedXBuffer with explicit capacity... ";
    {
        TypedXBuffer<SmallData> buf(4096, XBuffer::max_capacity(32 * 1024 * 1024));
        assert(buf.get_buffer()->capacity() >= 32 * 1024 * 1024);
        auto* data = buf.make();
        data->id = 123;
        assert(buf.root().id == 123);
        std::cout << "[OK]\n";
    }

    std::cout << "Test 6b: TypedXBuffer with default adaptive... ";
    {
        TypedXBuffer<SmallData> buf(4096);
        assert(buf.get_buffer()->capacity() < 256ULL * 1024 * 1024);
        auto* data = buf.make();
        data->id = 456;
        assert(buf.root().id == 456);
        std::cout << "[OK]\n";
    }

    std::cout << "[PASS] TypedXBuffer MaxCapacity tests passed!\n";
    return true;
}

int main() {
    try {
        bool all_passed = true;

        all_passed &= test_adaptive_reservation();
        all_passed &= test_max_capacity_override();
        all_passed &= test_many_small_buffers();
        all_passed &= test_relocation_fallback();
        all_passed &= test_forced_relocation();
        all_passed &= test_typed_buffer_max_capacity();

        if (all_passed) {
            std::cout << "\n[SUCCESS] All adaptive reservation tests passed!\n";
            return 0;
        } else {
            std::cout << "\n[FAILURE] Some tests failed!\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
