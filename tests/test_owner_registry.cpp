// Exact-base owner lookup, construction rollback, and allocation-free transfers.
// V2: allocation resolves its owner from the host page before the payload; the
// registry remains the identity behind save_verified_into/canonical lookup, so
// these checks exercise both after every transfer.
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "../xoffsetdatastructure.hpp"

namespace {
std::atomic<std::size_t> host_allocations{0};
thread_local int fail_host_allocation_after = -1;
}

void* operator new(std::size_t bytes) {
    host_allocations.fetch_add(1, std::memory_order_relaxed);
    if (fail_host_allocation_after == 0) {
        fail_host_allocation_after = -1;
        throw std::bad_alloc();
    }
    if (fail_host_allocation_after > 0) --fail_host_allocation_after;
    if (auto* ptr = std::malloc(bytes ? bytes : 1)) return ptr;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }

using namespace XOffsetDatastructure;
struct RegistryRoot { std::int64_t value{}; };
XOFFSET_REGISTER_SCHEMA_NAME(RegistryRoot, "test.RegistryRoot")

namespace {
XBuffer make_buffer(std::int64_t value) {
    auto buffer = XBuffer::create<RegistryRoot>(4096, XBuffer::maximum_capacity(65536));
    buffer.root<RegistryRoot>().value = value;
    return buffer;
}

// The registry record (identity used by save_verified_into) follows the owner.
// The save normalizes the payload extent, so call it after the owner's last
// allocation in explicit-growth fixtures.
void check_registry_identity(XBuffer& owner, std::int64_t value) {
    std::string out;
    owner.save_verified_into<RegistryRoot>(out);
    auto loaded = XBuffer::load_verified<RegistryRoot>(out);
    assert(loaded.root<RegistryRoot>().value == value);
}

template <class F> void reject_registration(F&& operation) {
    detail::fail_next_registry_registration();
    bool failed = false;
    try { operation(); } catch (const XBadAlloc&) { failed = true; }
    detail::clear_registry_failures();
    assert(failed);
}

void registration_failure_preserves_owners() {
    auto existing = make_buffer(42);
    auto checked = existing.handle<RegistryRoot>();
    const auto* base = existing.get_address();
    const auto frame = existing.save_verified<RegistryRoot>();
    const auto before = existing.copy_bytes();
    reject_registration([&] { (void)make_buffer(7); });
    reject_registration([&] { existing = XBuffer::load_verified<RegistryRoot>(frame); });
    assert(existing.get_address() == base && existing.copy_bytes() == before);
    assert(checked->value == 42);
    assert(existing.grow(4096));
    auto alloc = existing.allocator<std::int64_t>();
    auto* p = alloc.allocate(1);
    *p = 101;
    assert(*p == 101);
    alloc.deallocate(p, 1);
    auto loaded = XBuffer::load_verified<RegistryRoot>(frame);
    assert(loaded.root<RegistryRoot>().value == 42);
}

void actual_heap_failure_preserves_owners(bool seek_rehash) {
    // Run in a fresh process so previous tests cannot leave a large bucket
    // allocation behind and suppress the rehash case. A live seed means both
    // failure modes must preserve an already-populated owner index.
    std::vector<XBuffer> owners;
    owners.reserve(128);
    owners.push_back(make_buffer(0));
    bool observed_failure = false;
    for (unsigned attempt = 1; attempt < 128 && !observed_failure; ++attempt) {
        fail_host_allocation_after = seek_rehash ? 1 : 0;
        try {
            auto next = make_buffer(attempt);
            // A successful insertion used only one node allocation. Disarm
            // before unrelated test work, retain it and approach rehash.
            fail_host_allocation_after = -1;
            owners.push_back(std::move(next));
        } catch (const XBadAlloc&) {
            assert(fail_host_allocation_after == -1);
            observed_failure = true;
        }
        fail_host_allocation_after = -1;
    }
    assert(observed_failure);
    for (std::size_t i = 0; i < owners.size(); ++i) {
        assert(owners[i].root<RegistryRoot>().value == static_cast<std::int64_t>(i));
        auto alloc = owners[i].allocator<std::int64_t>();
        auto* value = alloc.allocate(1);
        *value = i + 71;
        assert(*value == static_cast<std::int64_t>(i + 71));
        alloc.deallocate(value, 1);
    }
    // Retry after the real allocator failure, then release every live owner.
    // A stale key from the failed insertion would reject or misroute this owner.
    auto retry = make_buffer(999);
    assert(retry.root<RegistryRoot>().value == 999);
    auto alloc = retry.allocator<std::int64_t>();
    auto* value = alloc.allocate(1);
    *value = 123;
    assert(*value == 123);
    alloc.deallocate(value, 1);
}

void transfer_matrix() {
    auto first = make_buffer(11);
    auto second = make_buffer(22);
    auto allocator = first.allocator<std::int64_t>();
    auto handle = first.handle<RegistryRoot>();
    auto* original_base = first.get_address();
    detail::fail_next_registry_registration();
    const auto before = host_allocations.load(std::memory_order_relaxed);
    {
        XBuffer moved(std::move(first));
        assert(!handle && moved.get_address() == original_base);
        first.swap(second);
        static_cast<XBufferCore&>(second) = std::move(static_cast<XBufferCore&>(moved));
        first = std::move(second);
        first.swap(first);
        auto& self = first;
        first = std::move(self);
        assert(first.get_address() == original_base && handle->value == 11);
        assert(XBufferCore::owner_from_arena_noexcept(allocator.arena()) == &first);
    }
    assert(host_allocations.load(std::memory_order_relaxed) == before);
    // The registration failure must remain pending across move/swap/destruction.
    bool rejected = false;
    try { (void)make_buffer(99); } catch (const XBadAlloc&) { rejected = true; }
    assert(rejected);
    detail::clear_registry_failures();
    auto* p = allocator.allocate(1);
    *p = 27;
    assert(*p == 27);
    allocator.deallocate(p, 1);
    check_registry_identity(first, 11); // last: save normalizes the explicit-growth buffer
}

void many_owners_and_reused_addresses() {
    std::vector<XBuffer> buffers;
    buffers.reserve(1000);
    for (unsigned i = 0; i < 1000; ++i) buffers.push_back(make_buffer(i));
    for (unsigned i = 0; i < 1000; ++i) {
        const auto index = (i * 593u) % 1000u;
        auto alloc = buffers[index].allocator<std::int64_t>();
        auto* p = alloc.allocate(1);
        *p = index;
        assert(*p == buffers[index].root<RegistryRoot>().value);
        alloc.deallocate(p, 1);
    }
    buffers.clear();
    // Repeated reserve/release lets the OS reuse addresses; stale entries would
    // reject insertion or route allocations into the previous C++ owner.
    for (unsigned i = 0; i < 300; ++i) {
        auto owner = make_buffer(i);
        auto alloc = owner.allocator<std::int64_t>();
        auto* p = alloc.allocate(1);
        *p = i + 5;
        XBuffer moved(std::move(owner));
        assert(*p == i + 5 && moved.root<RegistryRoot>().value == i);
        assert(XBufferCore::owner_from_arena_noexcept(alloc.arena()) == &moved);
        alloc.deallocate(p, 1);
        if (i % 50 == 0) check_registry_identity(moved, i);
    }
}

void independent_threads() {
    std::vector<std::thread> threads;
    for (unsigned id = 0; id < 8; ++id) {
        threads.emplace_back([id] {
            reject_registration([] { (void)make_buffer(-1); });
            for (unsigned i = 0; i < 80; ++i) {
                auto a = make_buffer(id * 1000 + i);
                auto b = make_buffer(-1);
                auto alloc = a.allocator<std::int64_t>();
                a.swap(b);
                auto* p = alloc.allocate(1);
                *p = i;
                assert(b.root<RegistryRoot>().value == id * 1000 + i && *p == i);
                assert(XBufferCore::owner_from_arena_noexcept(alloc.arena()) == &b);
                alloc.deallocate(p, 1);
                if (i % 16 == 0) check_registry_identity(b, id * 1000 + i);
            }
        });
    }
    for (auto& thread : threads) thread.join();
}
}

int main(int argc, char** argv) {
    if (argc == 2) {
        const std::string_view mode(argv[1]);
        if (mode == "--fail-node") actual_heap_failure_preserves_owners(false);
        else if (mode == "--fail-rehash") actual_heap_failure_preserves_owners(true);
        else return 2;
        return 0;
    }
    assert(argc == 1);
    registration_failure_preserves_owners();
    transfer_matrix();
    many_owners_and_reused_addresses();
    independent_threads();
}
