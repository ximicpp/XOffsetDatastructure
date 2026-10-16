// ============================================================================
// Test: Virtual Address Reservation Policies
// Purpose: Verify the default limit, legacy reservation hints, explicit hard
//          limits, and stable growth without committing the whole reservation.
// ============================================================================

#include <iostream>
#include <cassert>
#include <vector>
#include <memory>
#include "../xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct SmallData {
    int id;
    double value;
    XString name;
    XVector<int> numbers;

    template<typename Alloc>
    SmallData(Alloc alloc) : name(alloc), numbers(alloc) {}
};

bool test_default_reservation() {
    std::cout << "\n[TEST] Default virtual address reservation\n";
    constexpr auto default_limit = XBufferCore::DEFAULT_MAXIMUM_CAPACITY;
    static_assert(default_limit == 64ULL * 1024 * 1024);

    for (const auto initial_size : {4096u, 1024u * 1024u}) {
        XBuffer buf(initial_size);
        assert(buf.maximum_capacity() == default_limit);
        assert(buf.reserved_capacity() == default_limit);
        assert(buf.get_size() == initial_size);
        assert(buf.get_buffer().size() == initial_size);
        assert(buf.accessible_bytes() >= initial_size);
        assert(buf.accessible_bytes() < buf.reserved_bytes());
        assert(buf.reserved_bytes() >= buf.maximum_capacity());
    }

    // An initial arena larger than the default must still fit its reservation.
    XBuffer large(default_limit + 4096);
    assert(large.maximum_capacity() == default_limit + 4096);
    assert(large.reserved_capacity() == large.maximum_capacity());
    assert(large.get_size() == default_limit + 4096);
    std::cout << "[PASS] Default reservation and initial-size floor\n";
    return true;
}

bool test_capacity_policies() {
    std::cout << "\n[TEST] Legacy hints and explicit maximum capacity\n";
    constexpr auto small_limit = 64ULL * 1024;
    constexpr auto large_hint = 256ULL * 1024 * 1024;

    XBuffer legacy_small(4096, XBuffer::max_capacity(small_limit));
    assert(legacy_small.maximum_capacity() == XBufferCore::DEFAULT_MAXIMUM_CAPACITY);
    const void* legacy_base = legacy_small.get_address();
    const auto legacy_epoch = legacy_small.epoch();
    assert(legacy_small.grow(2 * small_limit - legacy_small.get_size()));
    assert(legacy_small.get_size() > small_limit);
    assert(legacy_small.get_address() == legacy_base);
    assert(legacy_small.epoch() == legacy_epoch);

    XBuffer legacy_large(4096, XBuffer::max_capacity(large_hint));
    assert(legacy_large.maximum_capacity() == large_hint);
    assert(legacy_large.reserved_capacity() == large_hint);
    assert(legacy_large.accessible_bytes() < legacy_large.reserved_bytes());

    XBuffer limited(4096, XBuffer::maximum_capacity(small_limit));
    assert(limited.maximum_capacity() == small_limit);
    assert(limited.reserved_capacity() == small_limit);
    bool rejected = false;
    try {
        XBuffer invalid(4096, XBuffer::maximum_capacity(1024));
    } catch (const XBadAlloc&) {
        rejected = true;
    }
    assert(rejected);
    std::cout << "[PASS] Hint compatibility and hard-limit admission\n";
    return true;
}

bool test_many_small_buffers() {
    std::cout << "\n[TEST] 10,000 buffers with explicit 64 KiB limits\n";
    constexpr int count = 10000;
    constexpr auto limit = 64ULL * 1024;
    std::vector<std::unique_ptr<XBuffer>> buffers;
    buffers.reserve(count);
    std::size_t total_reserved = 0;
    std::size_t total_accessible = 0;

    for (int i = 0; i < count; ++i) {
        auto buf = std::make_unique<XBuffer>(4096, XBuffer::maximum_capacity(limit));
        assert(buf->maximum_capacity() == limit);
        assert(buf->reserved_capacity() == limit);
        assert(buf->get_size() == 4096);
        assert(buf->get_buffer().size() == 4096);
        assert(buf->accessible_bytes() >= buf->get_size());
        assert(buf->accessible_bytes() < buf->reserved_bytes());
        total_reserved += buf->reserved_bytes();
        total_accessible += buf->accessible_bytes();
        buffers.push_back(std::move(buf));
    }

    assert(total_reserved >= count * limit);
    assert(total_reserved < 2ULL * 1024 * 1024 * 1024);
    assert(total_accessible < total_reserved);

    for (int i = 0; i < 100; ++i) {
        auto* data = buffers[i]->make<SmallData>();
        data->id = i;
        data->name = std::to_string(i).c_str();
        data->numbers.push_back(i * 10);
    }
    for (int i = 0; i < 100; ++i) {
        auto& data = buffers[i]->root<SmallData>();
        assert(data.id == i);
        assert(data.name == std::to_string(i).c_str());
        assert(data.numbers[0] == i * 10);
    }
    std::cout << "[PASS] Reserved=" << total_reserved / (1024 * 1024)
              << " MiB, accessible=" << total_accessible / (1024 * 1024)
              << " MiB; subset objects verified\n";
    return true;
}

bool test_stable_growth_and_round_trip() {
    std::cout << "\n[TEST] Stable growth, continued mutation, and save/load\n";
    XBuffer buf(4096, XBuffer::maximum_capacity(256 * 1024));
    auto* data = buf.make<SmallData>();
    data->id = 42;
    data->value = 12.5;
    data->name = "stable_growth_test";
    for (int i = 0; i < 10; ++i) data->numbers.push_back(i);

    const void* base = buf.get_address();
    const auto epoch = buf.epoch();
    const auto accessible = buf.accessible_bytes();
    assert(buf.grow(128 * 1024 - buf.get_size()));
    assert(buf.accessible_bytes() > accessible);
    assert(buf.get_address() == base);
    assert(buf.epoch() == epoch);
    assert(&buf.root<SmallData>() == data);
    assert(data->id == 42);
    assert(data->name == "stable_growth_test");
    for (int i = 0; i < 10; ++i) assert(data->numbers[i] == i);

    for (int i = 10; i < 1000; ++i) data->numbers.push_back(i);
    auto handle = buf.handle<SmallData>();
    assert(handle.get() == data);
    assert(handle->numbers.size() == 1000);
    for (int i = 0; i < 1000; ++i) assert(handle->numbers[i] == i);

    std::string saved = buf.save();
    assert(buf.get_address() == base);
    assert(buf.epoch() == epoch);
    assert(handle.get() == data);
    XBuffer loaded = XBuffer::load_unverified(saved);
    auto& loaded_data = loaded.unsafe_root<SmallData>();
    assert(loaded_data.id == 42);
    assert(loaded_data.value == 12.5);
    assert(loaded_data.name == "stable_growth_test");
    assert(loaded_data.numbers.size() == 1000);
    for (int i = 0; i < 1000; ++i) assert(loaded_data.numbers[i] == i);
    std::cout << "[PASS] Raw pointers, handles, and all serialized values intact\n";
    return true;
}

bool test_hard_capacity_failure() {
    std::cout << "\n[TEST] Growth stops at the hard maximum\n";
    constexpr auto limit = 64ULL * 1024;
    XBuffer buf(4096, XBuffer::maximum_capacity(limit));
    auto* data = buf.make<SmallData>();
    data->id = 99;
    data->name = "at_limit";
    const void* base = buf.get_address();
    const auto epoch = buf.epoch();

    assert(buf.grow(limit - buf.get_size()));
    assert(buf.get_size() == limit);
    const auto accessible = buf.accessible_bytes();
    assert(!buf.grow(1));
    assert(!buf.grow(limit));
    assert(buf.grow(0));
    assert(buf.get_size() == limit);
    assert(buf.maximum_capacity() == limit);
    assert(buf.reserved_capacity() == limit);
    assert(buf.accessible_bytes() == accessible);
    assert(buf.get_address() == base);
    assert(buf.epoch() == epoch);
    assert(&buf.root<SmallData>() == data);
    assert(data->id == 99);
    assert(data->name == "at_limit");
    std::cout << "[PASS] Failed growth preserves size, mapping, and live data\n";
    return true;
}

bool test_typed_buffer_capacity() {
    std::cout << "\n[TEST] TypedXBuffer capacity policies\n";
    constexpr auto limit = 64ULL * 1024;
    TypedXBuffer<SmallData> limited(4096, XBuffer::maximum_capacity(limit));
    assert(limited.maximum_capacity() == limit);
    auto* data = limited.make();
    data->id = 123;
    const void* base = limited.get_address();
    const auto epoch = limited.epoch();
    assert(limited.grow(limit - limited.get_size()));
    assert(!limited.grow(1));
    assert(limited.get_address() == base);
    assert(limited.epoch() == epoch);
    assert(&limited.root() == data);
    assert(limited.root().id == 123);

    TypedXBuffer<SmallData> legacy(4096, XBuffer::max_capacity(limit));
    assert(legacy.maximum_capacity() == XBufferCore::DEFAULT_MAXIMUM_CAPACITY);
    TypedXBuffer<SmallData> ordinary(4096);
    assert(ordinary.maximum_capacity() == XBufferCore::DEFAULT_MAXIMUM_CAPACITY);
    ordinary.make()->id = 456;
    assert(ordinary.root().id == 456);
    std::cout << "[PASS] Typed buffers preserve hint and hard-limit semantics\n";
    return true;
}

int main() {
    try {
        bool all_passed = true;
        all_passed &= test_default_reservation();
        all_passed &= test_capacity_policies();
        all_passed &= test_many_small_buffers();
        all_passed &= test_stable_growth_and_round_trip();
        all_passed &= test_hard_capacity_failure();
        all_passed &= test_typed_buffer_capacity();
        std::cout << (all_passed ? "\n[SUCCESS] All reservation policy tests passed!\n"
                               : "\n[FAILURE] Reservation policy tests failed!\n");
        return all_passed ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
}
