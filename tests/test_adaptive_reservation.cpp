// ============================================================================
// Test: Adaptive mmap Reservation & Remap Fallback
// Purpose: Verify that the adaptive reservation policy works correctly for
//          both single-buffer and many-buffer scenarios, and that the remap
//          fallback path functions when grow() exceeds the initial reservation.
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

    using VMB = boost::interprocess::VirtualMemoryBuffer;

    // Verify compute_reservation formula
    std::cout << "Test 1a: compute_reservation values... ";
    assert(VMB::compute_reservation(512) == VMB::MIN_RESERVE);      // 512*16=8KB < 64KB → 64KB
    assert(VMB::compute_reservation(4096) == VMB::MIN_RESERVE);     // 4K*16=64KB = MIN
    assert(VMB::compute_reservation(8192) == 8192 * 16);            // 8K*16=128KB
    assert(VMB::compute_reservation(1024*1024) == 1024*1024*16);    // 1M*16=16MB
    assert(VMB::compute_reservation(32*1024*1024) == VMB::MAX_RESERVE); // 32M*16=512MB > 256MB → 256MB
    std::cout << "[OK]\n";

    // Verify actual buffer uses adaptive reservation
    std::cout << "Test 1b: Small buffer uses small reservation... ";
    {
        XBuffer buf(4096);
        auto* vmb = buf.get_buffer();
        std::size_t expected = VMB::compute_reservation(4096);
        // capacity() should be close to expected (rounded up to page size)
        assert(vmb->capacity() <= expected + 65536);  // allow page rounding
        assert(vmb->capacity() < VMB::MAX_RESERVE);   // NOT 256MB!
        std::cout << "reserved=" << vmb->capacity() / 1024 << "KB [OK]\n";
    }

    // Verify large buffer gets proportionally larger reservation
    std::cout << "Test 1c: Large buffer uses large reservation... ";
    {
        XBuffer buf(1024 * 1024);  // 1MB
        auto* vmb = buf.get_buffer();
        assert(vmb->capacity() >= 1024 * 1024);  // at least initial size
        std::cout << "reserved=" << vmb->capacity() / (1024*1024) << "MB [OK]\n";
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
        auto* vmb = buf.get_buffer();
        // Should be close to 64MB (page-rounded)
        assert(vmb->capacity() >= 64 * 1024 * 1024 - 65536);
        assert(vmb->capacity() <= 64 * 1024 * 1024 + 65536);
        std::cout << "reserved=" << vmb->capacity() / (1024*1024) << "MB [OK]\n";
    }

    std::cout << "Test 2b: Explicit 256MB reservation (max)... ";
    {
        XBuffer buf(4096, XBuffer::max_capacity(256 * 1024 * 1024));
        auto* vmb = buf.get_buffer();
        assert(vmb->capacity() >= 256 * 1024 * 1024 - 65536);
        std::cout << "reserved=" << vmb->capacity() / (1024*1024) << "MB [OK]\n";
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
// Test 4: Remap fallback (grow beyond reservation via explicit grow())
// ============================================================================
bool test_remap_fallback() {
    std::cout << "\n[TEST] Remap Fallback (grow beyond reservation)\n";
    std::cout << std::string(50, '-') << "\n";

    // Create a buffer with small adaptive reservation (4KB → 64KB reserved).
    // Then explicitly grow() past the reservation to trigger the remap path.
    std::cout << "Test 4a: Create, populate, then grow beyond reservation... ";
    XBuffer buf(4096);
    auto* vmb = buf.get_buffer();
    std::size_t initial_capacity = vmb->capacity();
    std::cout << "initial_reserved=" << initial_capacity / 1024 << "KB... ";

    auto* data = buf.make<SmallData>();
    data->id = 42;
    data->name = "remap_test";
    for (int i = 0; i < 10; ++i) {
        data->numbers.push_back(i);
    }

    // Record epoch before remap
    uint64_t epoch_before = buf.epoch();

    // Explicitly grow beyond the 64KB reservation
    bool grew = buf.grow(initial_capacity + 4096);
    assert(grew);

    std::size_t new_capacity = vmb->capacity();
    std::cout << "new_reserved=" << new_capacity / 1024 << "KB... ";
    assert(new_capacity > initial_capacity);

    // Epoch should have incremented (remap happened)
    assert(buf.epoch() > epoch_before);
    std::cout << "[OK]\n";

    // Verify data integrity after remap
    std::cout << "Test 4b: Verify data integrity after remap... ";
    auto& ref = buf.root<SmallData>();
    assert(ref.id == 42);
    assert(ref.name == "remap_test");
    assert(ref.numbers.size() == 10);
    for (int i = 0; i < 10; ++i) {
        assert(ref.numbers[i] == i);
    }
    std::cout << "[OK]\n";

    // Continue using the buffer after remap — add more data
    std::cout << "Test 4c: Continue adding data after remap... ";
    auto& ref2 = buf.root<SmallData>();
    for (int i = 10; i < 1000; ++i) {
        ref2.numbers.push_back(i);
    }
    assert(buf.root<SmallData>().numbers.size() == 1000);
    assert(buf.root<SmallData>().numbers[999] == 999);
    std::cout << "[OK]\n";

    // XHandle works correctly after remap
    std::cout << "Test 4d: XHandle works after remap... ";
    auto handle = buf.handle<SmallData>();
    assert(handle->id == 42);
    assert(handle->numbers.size() == 1000);
    std::cout << "[OK]\n";

    // Save/load round-trip after remap
    std::cout << "Test 4e: Save/load round-trip after remap... ";
    std::string saved = buf.save();
    XBuffer loaded = XBuffer::load(saved);
    auto& loaded_data = loaded.root<SmallData>();
    assert(loaded_data.id == 42);
    assert(loaded_data.name == "remap_test");
    assert(loaded_data.numbers.size() == 1000);
    assert(loaded_data.numbers[999] == 999);
    std::cout << "[OK]\n";

    std::cout << "[PASS] Remap fallback tests passed!\n";
    return true;
}

// ============================================================================
// Test 5: Explicit forced remap via grow()
// ============================================================================
bool test_forced_remap() {
    std::cout << "\n[TEST] Forced Remap via Manual grow()\n";
    std::cout << std::string(50, '-') << "\n";

    // Create with small reservation to guarantee remap
    // 4096 bytes → 64KB reservation
    std::cout << "Test 5a: Force grow beyond reservation... ";
    XBuffer buf(4096);
    auto initial_capacity = buf.get_buffer()->capacity();
    auto* data = buf.make<SmallData>();
    data->id = 99;

    // Record initial epoch
    uint64_t epoch_before = buf.epoch();

    // Force grow way beyond 64KB reservation
    bool grew = buf.grow(initial_capacity + 4096);  // exceed reservation
    assert(grew);
    
    auto new_capacity = buf.get_buffer()->capacity();
    std::cout << "before=" << initial_capacity/1024 << "KB "
              << "after=" << new_capacity/1024 << "KB... ";
    
    // Reservation should have grown
    assert(new_capacity > initial_capacity);
    std::cout << "[OK]\n";

    // If remap happened, epoch should have changed
    std::cout << "Test 5b: Epoch incremented on remap... ";
    uint64_t epoch_after = buf.epoch();
    if (new_capacity > initial_capacity) {
        // Remap should have happened
        assert(epoch_after > epoch_before);
        std::cout << "epoch " << epoch_before << " → " << epoch_after << " [OK]\n";
    } else {
        std::cout << "no remap needed [OK]\n";
    }

    // Verify data still accessible via root()
    std::cout << "Test 5c: Data intact after forced remap... ";
    auto& ref = buf.root<SmallData>();
    assert(ref.id == 99);
    std::cout << "[OK]\n";

    std::cout << "[PASS] Forced remap tests passed!\n";
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
        assert(buf.get_buffer()->capacity() >= 32 * 1024 * 1024 - 65536);
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
        all_passed &= test_remap_fallback();
        all_passed &= test_forced_remap();
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
