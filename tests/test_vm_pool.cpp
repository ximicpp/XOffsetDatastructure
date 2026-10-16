// The pool and OS owner are independently usable without C++26 reflection.
#include "../xoffset_virtual_memory.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#  include <mach/mach.h>
#  include <mach/mach_vm.h>
#endif

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
    if (void* ptr = std::malloc(bytes ? bytes : 1)) return ptr;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }

using namespace XOffsetDatastructure::detail;

namespace {
void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::abort();
    }
}

template<class F> void expect_bad_alloc(F&& operation, const char* message) {
    bool caught = false;
    try { operation(); } catch (const std::bad_alloc&) { caught = true; }
    check(caught, message);
}

void check_bytes(const char* bytes, std::size_t length, char value, const char* message) {
    for (std::size_t i = 0; i < length; ++i) check(bytes[i] == value, message);
}

bool is_mapped(const void* pointer) {
#if defined(_WIN32)
    MEMORY_BASIC_INFORMATION info{};
    return ::VirtualQuery(pointer, &info, sizeof(info)) != 0 && info.State != MEM_FREE;
#elif defined(__APPLE__)
    const auto target = reinterpret_cast<mach_vm_address_t>(pointer);
    mach_vm_address_t region = target;
    mach_vm_size_t length = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    const auto result = ::mach_vm_region(::mach_task_self(), &region, &length,
        VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &count, &object);
    if (object != MACH_PORT_NULL) ::mach_port_deallocate(::mach_task_self(), object);
    return result == KERN_SUCCESS && region <= target && target - region < length;
#else
    std::ifstream maps("/proc/self/maps");
    check(maps.is_open(), "read Linux mappings");
    const auto target = reinterpret_cast<std::uintptr_t>(pointer);
    std::string line;
    while (std::getline(maps, line)) {
        unsigned long long begin = 0, end = 0;
        if (std::sscanf(line.c_str(), "%llx-%llx", &begin, &end) == 2 &&
            target >= begin && target < end) return true;
    }
    return false;
#endif
}

void test_default_and_host_allocations(std::size_t page) {
    const auto before = host_allocations.load();
    {
        VmStoragePool empty;
        check(!empty.enabled() && empty.stats().cached_blocks == 0, "default pool is disabled");
        empty.clear();
        VirtualMemoryStorage direct(17, page * 2, empty);
        VirtualMemoryStorage moved(std::move(direct));
        check(moved.size() == 17 && direct.data() == nullptr, "unpooled move works");
    }
    check(host_allocations.load() == before, "default construction and ownership have no host allocation");

    for (int fail_after : {0, 1}) {
        fail_host_allocation_after = fail_after;
        expect_bad_alloc([&] { VmStoragePool rejected({2, page * 8, page * 4}); },
                         "pool construction reports control-block or slot allocation failure");
        fail_host_allocation_after = -1;
    }
    VmStoragePool pool({2, page * 8, page * 4});
    { VirtualMemoryStorage seeded(17, page * 2, pool); }
    const auto cached = pool.stats();
    const auto allocation_count = host_allocations.load();
    fail_host_allocation_after = 0;
    {
        VirtualMemoryStorage first(23, page * 2, pool);
        VirtualMemoryStorage moved(std::move(first));
        VirtualMemoryStorage target;
        target = std::move(moved);
        check(target.size() == 23, "pooled acquire and transfers need no host allocation");
    }
    pool.clear();
    { VirtualMemoryStorage new_mapping(7, page * 2, pool); }
    check(fail_host_allocation_after == 0 && host_allocations.load() == allocation_count,
          "cache hit, return, clear and subsequent return reuse preallocated slots");
    fail_host_allocation_after = -1;
    check(pool.stats().hits == cached.hits + 1 && pool.stats().cached_blocks == 1,
          "cache counters retain history across clear");
}

void test_reuse_and_load_zeroing(std::size_t page) {
    VmStoragePool pool({3, page * 18, page * 12});
    char* base = nullptr;
    {
        VirtualMemoryStorage seed(page * 4, page * 6, pool);
        base = seed.data();
        std::memset(base, 'x', seed.size());
        check(pool.stats().cached_blocks == 0, "live storage is not counted as cached");
    }
    auto state = pool.stats();
    check(state.cached_blocks == 1 && state.cached_reserved_bytes == page * 6 &&
              state.cached_accessible_bytes == page * 4 && state.hits == 0 && state.misses == 1,
          "return caches bounded geometry and records first miss");
    {
        VirtualMemoryStorage reused(page + 9, page * 6, pool);
        check(reused.data() == base && reused.accessible_bytes() == page * 4,
              "matching acquisition reuses reservation and retained pages");
        check_bytes(base, reused.size(), 0, "normal acquisition zeroes every new logical byte");
        check(reused.grow_to(page * 4), "pooled owner grows into retained dirty pages");
        check_bytes(base + page + 9, page * 3 - 9, 0, "regrowth zeroes old cached contents");
        std::memset(base, 'y', reused.size());
    }
    const std::string input(page + 17, 'z');
    {
        auto loaded = VirtualMemoryStorage::from_bytes(input.data(), input.size(), page * 6, pool);
        check(loaded.data() == base && loaded.size() == input.size(), "load reuses the exact-capacity owner");
        check_bytes(base, input.size(), 'z', "load completely overwrites the exposed prefix");
        check(loaded.grow_to(page * 4), "loaded pooled owner can regrow");
        check_bytes(base + input.size(), page * 4 - input.size(), 0,
                    "load regrowth does not expose dirty pooled tail bytes");
        loaded.resize_logical(page + 1);
        check(loaded.reclaim_tail().complete(), "pooled owner can explicitly reclaim retained pages");
    }
    state = pool.stats();
    check(state.hits == 2 && state.misses == 1 && state.cached_accessible_bytes == page * 2,
          "pool accounts for explicit reclamation before return");
    pool.clear();
    check(!is_mapped(base), "clear releases cached reservation");
}

void test_limits_and_exact_matching(std::size_t page) {
    VmStoragePool exact({3, page * 8, page * 8});
    { VirtualMemoryStorage first(1, page + 1, exact); }
    { VirtualMemoryStorage second(1, page + 2, exact); }
    check(exact.stats().misses == 2 && exact.stats().cached_blocks == 2,
          "same rounded reservation with different maximum is not a cache hit");
    { VirtualMemoryStorage hit(1, page + 1, exact); }
    check(exact.stats().hits == 1, "exact maximum matches cached storage");

    for (const auto limits : {VmStoragePool::Limits{1, page * 8, page * 8},
                              VmStoragePool::Limits{3, page * 3, page * 8},
                              VmStoragePool::Limits{3, page * 8, page}}) {
        VmStoragePool bounded(limits);
        VirtualMemoryStorage first(1, page * 2, bounded);
        VirtualMemoryStorage second(1, page * 2, bounded);
        const auto* second_base = second.data();
        first = VirtualMemoryStorage{};
        second = VirtualMemoryStorage{};
        const auto state = bounded.stats();
        check(state.cached_blocks == 1 && state.cached_reserved_bytes == page * 2 &&
                  state.cached_accessible_bytes == page,
              "slot, reservation and accessible budgets independently bound idle storage");
        check(!is_mapped(second_base), "owner exceeding cache budget is released");
    }
    VmStoragePool inaccessible_only({1, page * 4, 0});
    { VirtualMemoryStorage uncommitted(0, page * 4, inaccessible_only); }
    check(inaccessible_only.stats().cached_blocks == 1 &&
              inaccessible_only.stats().cached_accessible_bytes == 0,
          "zero accessible budget can retain an entirely inaccessible reservation");
    const void* discarded = nullptr;
    {
        VirtualMemoryStorage used(1, page * 4, inaccessible_only);
        discarded = used.data();
    }
    check(inaccessible_only.stats().cached_blocks == 0 && !is_mapped(discarded),
          "enabling pages can make a returned owner exceed the accessible budget");
    VmStoragePool zero_slots({0, page * 8, page * 8});
    { VirtualMemoryStorage no_cache(1, page, zero_slots); }
    check(zero_slots.stats().cached_blocks == 0, "zero slot budget disables caching");
}

void test_failures(std::size_t page) {
    VmStoragePool pool({2, page * 12, page * 12});
    char* base = nullptr;
    { VirtualMemoryStorage seed(page, page * 6, pool); base = seed.data(); }
    fail_next_vm_operation(VmOperation::Reserve);
    expect_bad_alloc([&] { VirtualMemoryStorage rejected(1, page * 6, pool); },
                     "Reserve failure applies before cache-hit acquisition");
    check(pool.stats().cached_blocks == 1 && pool.stats().hits == 0,
          "rejected acquisition does not remove a cached owner");
    for (const auto operation : {VmOperation::Enable, VmOperation::Prepare}) {
        fail_next_vm_operation(operation);
        expect_bad_alloc([&] { VirtualMemoryStorage rejected(page * 3, page * 6, pool); },
                         "pooled constructor reports enable or prepare failure");
        check(pool.stats().cached_blocks == 1, "failed constructor safely returns its owner");
    }
    const std::string input(page * 4, 'u');
    for (const auto operation : {VmOperation::Reserve, VmOperation::Enable, VmOperation::Prepare}) {
        fail_next_vm_operation(operation);
        expect_bad_alloc([&] {
            (void)VirtualMemoryStorage::from_bytes(input.data(), input.size(), page * 6, pool);
        }, "pooled from_bytes propagates acquisition, enable and prepare failures");
        check(pool.stats().cached_blocks == 1, "failed copied load retains bounded cache ownership");
    }
    {
        auto loaded = VirtualMemoryStorage::from_bytes(input.data(), input.size(), page * 6, pool);
        check(loaded.data() == base, "failure retries preserve reusable reservation identity");
        check_bytes(base, input.size(), 'u', "successful copied retry initializes complete input");
    }
    fail_next_vm_operation(VmOperation::Enable);
    {
        VirtualMemoryStorage retained(1, page * 6, pool);
        check(!retained.grow_to(page * 5), "cache hit within retained pages leaves Enable pending");
        check(retained.grow_to(page * 5), "one-shot Enable failure permits later growth");
    }
    clear_vm_failures();
}

void test_lifetime_clear_and_transfer(std::size_t page) {
    VirtualMemoryStorage surviving;
    char* base = nullptr;
    {
        VmStoragePool temporary({2, page * 8, page * 8});
        surviving = VirtualMemoryStorage(17, page * 4, temporary);
        base = surviving.data();
        std::memset(base, 'v', 17);
    }
    check(surviving.grow_to(page), "live storage keeps pool state alive after handle destruction");
    check_bytes(base, 17, 'v', "handle destruction preserves live contents");
    surviving = VirtualMemoryStorage{};
    check(!is_mapped(base), "last live owner releases cached storage without an ownership cycle");

    VmStoragePool first_pool({2, page * 8, page * 8});
    VmStoragePool second_pool({2, page * 8, page * 8});
    VirtualMemoryStorage first(17, page * 4, first_pool);
    VirtualMemoryStorage second(19, page * 3, second_pool);
    const auto* first_base = first.data();
    const auto* second_base = second.data();
    swap(first, second);
    first = VirtualMemoryStorage{};
    check(second_pool.stats().cached_blocks == 1 && first_pool.stats().cached_blocks == 0,
          "swap moves the pool association with its reservation");
    second_pool.clear();
    check(!is_mapped(second_base) && is_mapped(first_base), "clear releases only idle mappings");
    second = VirtualMemoryStorage{};
    check(first_pool.stats().cached_blocks == 1, "still-live owner returns after another pool was cleared");
    first_pool.clear();
    check(!is_mapped(first_base), "clear releases an owner returned after a prior clear");
}

void test_shared_threads(std::size_t page) {
    VmStoragePool pool({8, page * 64, page * 32});
    std::atomic<bool> start{false};
    std::vector<std::thread> threads;
    for (int worker = 0; worker < 6; ++worker) {
        threads.emplace_back([pool, page, &start, worker] {
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int iteration = 0; iteration < 150; ++iteration) {
                VirtualMemoryStorage storage(page + 11, page * 8, pool);
                check_bytes(storage.data(), storage.size(), 0, "concurrent acquire sees zeroed logical data");
                std::memset(storage.data(), 'a' + worker, storage.size());
                check(storage.grow_to(page * 3), "concurrent independent owner can grow");
                check_bytes(storage.data(), page + 11, static_cast<char>('a' + worker),
                            "other threads do not affect a live owner's bytes");
                if ((iteration % 17) == 0) pool.clear();
                const auto state = pool.stats();
                check(state.cached_blocks <= 8 && state.cached_reserved_bytes <= page * 64 &&
                          state.cached_accessible_bytes <= page * 32,
                      "concurrent clear/acquire/return respects all idle budgets");
            }
        });
    }
    start.store(true, std::memory_order_release);
    for (auto& thread : threads) thread.join();
    check(pool.stats().hits + pool.stats().misses == 900, "every concurrent acquisition is counted once");
    pool.clear();
    check(pool.stats().cached_blocks == 0 && pool.stats().cached_reserved_bytes == 0 &&
              pool.stats().cached_accessible_bytes == 0,
          "final clear empties all idle pool accounting");
}
} // namespace

int main() {
    clear_vm_failures();
    VirtualMemoryStorage geometry(0, 1);
    const auto page = geometry.page_size();
    test_default_and_host_allocations(page);
    test_reuse_and_load_zeroing(page);
    test_limits_and_exact_matching(page);
    test_failures(page);
    test_lifetime_clear_and_transfer(page);
    test_shared_threads(page);
    clear_vm_failures();
    std::cout << "Virtual memory pool tests passed (page_size=" << page << ")\n";
}
