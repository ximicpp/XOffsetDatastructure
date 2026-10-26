// This test intentionally includes only the OS storage layer: ordinary C++17
// compilers can verify it independently of the full P2996 integration suite.
#include "../xoffset_virtual_memory.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#if defined(__APPLE__)
#  include <mach/mach.h>
#  include <mach/mach_vm.h>
#endif

using XOffsetDatastructure::detail::VirtualMemoryStorage;
using XOffsetDatastructure::detail::VmOperation;
using XOffsetDatastructure::detail::VmReclaimStatus;
using XOffsetDatastructure::detail::VmStoragePool;
using XOffsetDatastructure::detail::checked_vm_round_up;
using XOffsetDatastructure::detail::clear_vm_failures;
using XOffsetDatastructure::detail::fail_next_vm_operation;

namespace {

void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        std::abort();
    }
}

template<class Exception, class F>
void expect_exception(F&& function, const char* description) {
    bool caught = false;
    try {
        function();
    } catch (const Exception&) {
        caught = true;
    }
    check(caught, description);
}

void check_bytes(const char* bytes, std::size_t length, char value,
                 const char* description) {
    for (std::size_t i = 0; i < length; ++i) check(bytes[i] == value, description);
}

enum class PageAccess { Unmapped, Inaccessible, ReadWrite };

PageAccess query_page_access(const void* pointer) {
#if defined(_WIN32)
    MEMORY_BASIC_INFORMATION info{};
    const SIZE_T found = ::VirtualQuery(pointer, &info, sizeof(info));
    if (!found || info.State == MEM_FREE) return PageAccess::Unmapped;
    if (info.State == MEM_RESERVE) return PageAccess::Inaccessible;
    check(info.State == MEM_COMMIT, "VirtualQuery reports committed pages");
    check(info.Protect == PAGE_READWRITE, "VirtualQuery reports PAGE_READWRITE");
    return PageAccess::ReadWrite;
#elif defined(__APPLE__)
    const auto target = reinterpret_cast<mach_vm_address_t>(pointer);
    mach_vm_address_t region = target;
    mach_vm_size_t length = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    const kern_return_t status = ::mach_vm_region(
        ::mach_task_self(), &region, &length, VM_REGION_BASIC_INFO_64,
        reinterpret_cast<vm_region_info_t>(&info), &count, &object);
    if (object != MACH_PORT_NULL) ::mach_port_deallocate(::mach_task_self(), object);
    if (status == KERN_INVALID_ADDRESS || (status == KERN_SUCCESS && region > target)) {
        return PageAccess::Unmapped;
    }
    check(status == KERN_SUCCESS && target - region < length,
          "mach_vm_region finds queried address");
    if (info.protection == VM_PROT_NONE) return PageAccess::Inaccessible;
    check((info.protection & (VM_PROT_READ | VM_PROT_WRITE)) ==
              (VM_PROT_READ | VM_PROT_WRITE),
          "mach_vm_region reports read/write permissions");
    return PageAccess::ReadWrite;
#else
    std::ifstream maps("/proc/self/maps");
    check(maps.is_open(), "open /proc/self/maps");
    const auto target = reinterpret_cast<std::uintptr_t>(pointer);
    std::string line;
    while (std::getline(maps, line)) {
        unsigned long long begin = 0, end = 0;
        char permissions[5]{};
        if (std::sscanf(line.c_str(), "%llx-%llx %4s", &begin, &end, permissions) == 3 &&
            target >= begin && target < end) {
            if (permissions[0] == '-' && permissions[1] == '-') {
                return PageAccess::Inaccessible;
            }
            check(permissions[0] == 'r' && permissions[1] == 'w',
                  "/proc/self/maps reports read/write permissions");
            return PageAccess::ReadWrite;
        }
    }
    return PageAccess::Unmapped;
#endif
}

void check_discarded_pages(char* pointer, std::size_t bytes, std::size_t page) {
#if defined(_WIN32)
    for (std::size_t offset = 0; offset < bytes; offset += page) {
        MEMORY_BASIC_INFORMATION info{};
        check(::VirtualQuery(pointer + offset, &info, sizeof(info)) != 0 &&
                  info.State == MEM_RESERVE,
              "reclaimed Windows pages are reserved without a commitment");
    }
#else
#  if defined(__APPLE__)
    std::vector<char> residency(bytes / page);
#  else
    std::vector<unsigned char> residency(bytes / page);
#  endif
    check(::mincore(pointer, bytes, residency.data()) == 0,
          "mincore can query the retained reservation");
    for (const auto state : residency)
        check((state & 1) == 0, "discarded tail pages are no longer resident");
#endif
}

void test_empty_and_geometry() {
    VirtualMemoryStorage empty;
    check(empty.data() == nullptr && empty.size() == 0 && empty.capacity() == 0,
          "default storage is empty");
    check(empty.grow_to(0), "grow_to(0) is a no-op on empty storage");
    check(!empty.grow_to(1), "default empty storage cannot grow");
    empty.resize_logical(0);

    VirtualMemoryStorage geometry(0, 1);
    const auto page = geometry.page_size();
    check(page > 1 && geometry.reserved_bytes() == page, "runtime page geometry");
    check(geometry.reservation_granularity() >= page, "reservation granularity recorded");
    check(geometry.data() != nullptr && geometry.empty(), "zero logical length may own reservation");
    check(geometry.accessible_bytes() == 0, "zero initial length enables no pages");
    check(query_page_access(geometry.data()) == PageAccess::Inaccessible,
          "OS confirms initial reservation is inaccessible");
    check(geometry.grow_to(1), "grow into first page");
    check(query_page_access(geometry.data()) == PageAccess::ReadWrite,
          "OS confirms enabled page is read/write");
}

void test_growth_and_hard_cap(std::size_t page) {
    const auto maximum = page * 5 + 7;
    VirtualMemoryStorage storage(13, maximum);
    const char* original = storage.data();
    check(storage.size() == 13 && storage.accessible_bytes() == page,
          "small initial range enables one page");
    check(storage.reserved_bytes() == page * 6 && storage.capacity() == maximum,
          "page rounding does not expand logical hard cap");
    check(query_page_access(storage.data() + page) == PageAccess::Inaccessible,
          "OS confirms reservation tail remains inaccessible");

    std::memset(storage.data(), 'a', storage.size());
    check(storage.grow_to(page - 1), "page-internal growth succeeds");
    check(storage.data() == original && storage.accessible_bytes() == page,
          "page-internal growth preserves base and permissions extent");
    check_bytes(storage.data(), 13, 'a', "old bytes survive page-internal growth");
    check_bytes(storage.data() + 13, page - 14, 0, "new page-internal bytes are zero");

    for (std::size_t new_size : {page, page + 1, page * 2, page * 3 + 1, maximum}) {
        const auto old_size = storage.size();
        std::memset(storage.data(), 'a', old_size);
        check(storage.grow_to(new_size), "growth at/crossing page boundary and exact cap");
        check(storage.data() == original, "all growth preserves base");
        check_bytes(storage.data(), old_size, 'a', "all preexisting bytes survive growth");
        check_bytes(storage.data() + old_size, new_size - old_size, 0,
                    "only newly exposed range is zero initialized");
    }
    check(storage.size() == maximum && storage.accessible_bytes() == page * 6,
          "exact non-page-aligned cap remains exact");
    check(!storage.grow_to(maximum + 1), "rounded page slack is not usable capacity");
    check(!storage.grow_to((std::numeric_limits<std::size_t>::max)()),
          "huge growth rejected before any offset addition");
    check(!storage.grow_to(0), "grow_to rejects logical shrink");
    check(storage.size() == maximum && storage.data() == original,
          "rejected growth leaves logical extent and base unchanged");
    expect_exception<std::length_error>([&] { storage.resize_logical(maximum + 1); },
                                        "resize beyond hard cap throws length_error");
}

void test_shrink_regrow(std::size_t page) {
    VirtualMemoryStorage storage(page * 3 + 31, page * 5);
    char* original = storage.data();
    const auto enabled = storage.accessible_bytes();
    for (std::size_t shortened : {page + 5, std::size_t(7), std::size_t(0), page * 2}) {
        const auto old_size = storage.size();
        std::memset(storage.data(), 'b', old_size);
        storage.resize_logical(shortened);
        check(storage.data() == original && storage.size() == shortened,
              "logical shrink preserves address");
        check(storage.accessible_bytes() == enabled, "logical shrink retains enabled pages");
        check_bytes(storage.data(), shortened, 'b', "shrink preserves retained bytes");
        check(storage.grow_to(page * 3 + 31), "regrowth after shrink succeeds");
        check_bytes(storage.data(), shortened, 'b', "regrowth preserves retained bytes");
        check_bytes(storage.data() + shortened, storage.size() - shortened, 0,
                    "regrowth zeroes previously used exposed bytes");
    }
    check(query_page_access(storage.data() + page * 4) == PageAccess::Inaccessible,
          "never-enabled tail stays inaccessible after repeated shrink/regrow");
}

void test_tail_reclaim(std::size_t page) {
    VirtualMemoryStorage empty;
    check(empty.reclaim_tail().status == VmReclaimStatus::NoOp,
          "empty reclaim is a successful no-op");
    VirtualMemoryStorage storage(page * 6, page * 8 + 1);
    char* const base = storage.data();
    const auto reservation = storage.reserved_bytes();
    check(storage.reclaim_tail().status == VmReclaimStatus::NoOp,
          "reclaim never discards logical pages");
    for (const auto shortened : {page + 7, page, std::size_t(0)}) {
        std::memset(storage.data(), 'r', storage.size());
        storage.resize_logical(shortened);
        std::size_t retained = 0;
        check(checked_vm_round_up(shortened, page, retained), "round reclaim boundary");
        const auto result = storage.reclaim_tail();
        check(result.status == VmReclaimStatus::Reclaimed && result.complete() &&
                  result.attempted_bytes == page * 6 - retained &&
                  result.discarded_bytes == result.attempted_bytes &&
                  result.inaccessible_bytes == result.attempted_bytes,
              "reclaim reports exactly the complete unused pages");
        check(storage.data() == base && storage.size() == shortened &&
                  storage.accessible_bytes() == retained &&
                  storage.reserved_bytes() == reservation && storage.capacity() == page * 8 + 1,
              "reclaim preserves address, logical bytes, hard cap and reservation");
        check_bytes(base, shortened, 'r', "reclaim preserves every live byte");
        if (shortened != retained) {
            check_bytes(base + shortened, retained - shortened, 'r',
                        "reclaim does not overwrite unused bytes in the retained partial page");
        }
        check(query_page_access(base + retained) == PageAccess::Inaccessible,
              "OS confirms reclaimed tail has no access");
        check_discarded_pages(base + retained, page * 6 - retained, page);
        check(query_page_access(base + page * 8) == PageAccess::Inaccessible,
              "never-enabled reservation tail remains mapped and inaccessible");
        const auto again = storage.reclaim_tail();
        check(again.status == VmReclaimStatus::NoOp && again.complete() &&
                  again.attempted_bytes == 0 && again.discarded_bytes == 0 &&
                  again.inaccessible_bytes == 0,
              "repeated reclaim is an idempotent no-op");
        check(storage.grow_to(page * 6), "reclaimed storage can regrow");
        check(storage.data() == base, "reclaim and regrow retain the original address");
        check_bytes(base, shortened, 'r', "regrowth preserves the live prefix");
        check_bytes(base + shortened, page * 6 - shortened, 0,
                    "regrowth zeroes reclaimed pages and retained partial-page bytes");
    }
}

void test_reclaim_failures_and_move(std::size_t page) {
    clear_vm_failures();
    VirtualMemoryStorage storage(page * 5, page * 7);
    char* const base = storage.data();
    std::memset(base, 's', storage.size());
    const auto shortened = page + 11;
    storage.resize_logical(shortened);
    fail_next_vm_operation(VmOperation::Reclaim);
    const auto rejected = storage.reclaim_tail();
    check(rejected.status == VmReclaimStatus::Failed && !rejected.complete() &&
              rejected.attempted_bytes == page * 3 && rejected.discarded_bytes == 0 &&
              rejected.inaccessible_bytes == 0,
          "injected discard failure reports no completed operation");
    check(storage.size() == shortened && storage.accessible_bytes() == page * 5 &&
              storage.data() == base,
          "injected discard failure leaves storage extents unchanged");
    check_bytes(base, page * 5, 's', "injected discard failure preserves all old bytes");
    check(query_page_access(base + page * 4) == PageAccess::ReadWrite,
          "failed discard leaves actual page permissions intact");

#if defined(__linux__)
    fail_next_vm_operation(VmOperation::Protect);
    const auto partial = storage.reclaim_tail();
    check(partial.status == VmReclaimStatus::DiscardedButAccessible && !partial.complete() &&
              partial.discarded_bytes == page * 3 && partial.inaccessible_bytes == 0,
          "Linux distinguishes completed discard from failed protection");
    check(storage.accessible_bytes() == page * 5 && storage.size() == shortened,
          "partial reclaim retains the accurate read/write extent");
    check(query_page_access(base + page * 4) == PageAccess::ReadWrite,
          "OS confirms partial reclaim retained page permissions");
    check_discarded_pages(base + page * 2, page * 3, page);
    check_bytes(base, shortened, 's', "partial reclaim preserves all live bytes");
#endif

    VirtualMemoryStorage moved(std::move(storage));
    check(storage.reclaim_tail().status == VmReclaimStatus::NoOp && moved.data() == base &&
              moved.size() == shortened && moved.accessible_bytes() == page * 5,
          "move transfers a failed or partially completed reclaim state");
    check(moved.reclaim_tail().complete() && moved.accessible_bytes() == page * 2,
          "retry after move completes reclamation of the same range");
    fail_next_vm_operation(VmOperation::Enable);
    check(!moved.grow_to(page * 5) && moved.accessible_bytes() == page * 2 &&
              moved.size() == shortened,
          "failed recommit retains logical and accessible extents");
    fail_next_vm_operation(VmOperation::Prepare);
    check(!moved.grow_to(page * 5) && moved.accessible_bytes() == page * 5 &&
              moved.size() == shortened,
          "failed preparation records recommitted pages without logical publication");
    check(moved.reclaim_tail().complete() && moved.accessible_bytes() == page * 2,
          "reclaim can discard pages retained by failed preparation");
    check(moved.grow_to(page * 5), "regrowth remains possible after reclaim and growth failures");
    check_bytes(base, shortened, 's', "all retries preserve the live prefix");
    check_bytes(base + shortened, page * 5 - shortened, 0,
                "retry zeroes both retained stale bytes and newly recommitted pages");

    fail_next_vm_operation(VmOperation::Reclaim);
    check(moved.reclaim_tail().status == VmReclaimStatus::NoOp,
          "no-op reclaim does not consume a pending failure");
    moved.resize_logical(page);
    check(moved.reclaim_tail().status == VmReclaimStatus::Failed,
          "later reclaim consumes the pending failure");
    check(moved.reclaim_tail().complete(), "one-shot reclaim failure permits retry");
    clear_vm_failures();
}

void test_loaded_reclaim_and_transfer(std::size_t page) {
    const std::string input(page * 4 + 7, 't');
    auto loaded = VirtualMemoryStorage::from_bytes(input.data(), input.size(), page * 8);
    char* const base = loaded.data();
    loaded.resize_logical(page + 3);
    check(loaded.reclaim_tail().complete() && loaded.accessible_bytes() == page * 2,
          "fresh copied storage supports explicit reclamation");
    VirtualMemoryStorage other(17, page * 3);
    char* const other_base = other.data();
    swap(loaded, other);
    check(other.data() == base && other.size() == page + 3 &&
              other.accessible_bytes() == page * 2 && other.capacity() == page * 8,
          "swap transfers all reclaimed storage geometry");
    loaded = std::move(other);
    check(loaded.data() == base && other.data() == nullptr &&
              query_page_access(other_base) == PageAccess::Unmapped,
          "move assignment releases replaced mapping and transfers reclaimed storage");
    check(loaded.grow_to(input.size()), "loaded reclaimed owner remains growable after transfer");
    check_bytes(base, page + 3, 't', "loaded owner retains live data through reclaim and transfer");
    check_bytes(base + page + 3, input.size() - page - 3, 0,
                "loaded owner clears old copied bytes on regrowth");
}

void test_failure_paths(std::size_t page) {
    clear_vm_failures();
    for (VmOperation operation : {VmOperation::Reserve, VmOperation::Enable, VmOperation::Prepare}) {
        fail_next_vm_operation(operation);
        expect_exception<std::bad_alloc>([&] { VirtualMemoryStorage rejected(1, page * 4); },
                                         "constructor reports injected OS/preparation failure");
        VirtualMemoryStorage retry(1, page * 4);
        check(retry.size() == 1, "constructor failure is one-shot and permits retry");
    }

    VirtualMemoryStorage storage(17, page * 5);
    char* original = storage.data();
    std::memset(storage.data(), 'c', storage.size());
    fail_next_vm_operation(VmOperation::Enable);
    check(!storage.grow_to(page + 9), "injected enable failure is reported");
    check(storage.data() == original && storage.size() == 17 && storage.accessible_bytes() == page,
          "enable failure preserves logical range and enabled pages");
    check(query_page_access(storage.data() + page) == PageAccess::Inaccessible,
          "enable failure does not open new pages");
    check_bytes(storage.data(), 17, 'c', "enable failure preserves old bytes");

    fail_next_vm_operation(VmOperation::Prepare);
    check(!storage.grow_to(page + 9), "post-enable preparation failure is reported");
    check(storage.data() == original && storage.size() == 17 &&
              storage.accessible_bytes() == page * 2,
          "preparation failure records extra pages without publishing logical range");
    check(query_page_access(storage.data() + page) == PageAccess::ReadWrite,
          "OS confirms pages retained after preparation failure");
    check_bytes(storage.data(), 17, 'c', "preparation failure preserves old bytes");
    check(storage.grow_to(page + 9), "retry after preparation failure succeeds");
    check_bytes(storage.data() + 17, page - 8, 0, "retry zeroes newly exposed bytes");

    const auto old_size = storage.size();
    fail_next_vm_operation(VmOperation::Prepare);
    check(!storage.grow_to(old_size + 1), "page-internal preparation failure is reported");
    check(storage.size() == old_size, "page-internal failure retains old length");
    check(storage.grow_to(old_size + 1), "page-internal retry succeeds");

    fail_next_vm_operation(VmOperation::Enable);
    expect_exception<std::bad_alloc>([&] { storage.resize_logical(page * 3); },
                                     "throwing resize reports page enable failure");
    check(storage.size() == old_size + 1, "throwing resize retains old length");
    check(storage.grow_to(page * 3), "storage remains usable after throwing resize");

    fail_next_vm_operation(VmOperation::Prepare);
    check(storage.grow_to(storage.size()), "no-op growth succeeds without preparation");
    check(!storage.grow_to(storage.size() + 1), "no-op does not consume pending preparation failure");
    clear_vm_failures();
}

void test_overflow_and_input_validation(std::size_t page) {
    std::size_t rounded = 101;
    check(!checked_vm_round_up((std::numeric_limits<std::size_t>::max)(), 2, rounded),
          "page rounding addition overflow rejected");
    check(rounded == 101, "failed rounding leaves output unchanged");
    check(!checked_vm_round_up(1, 0, rounded), "zero page geometry rejected");
    check(checked_vm_round_up(13, 5, rounded) && rounded == 15,
          "rounding does not assume power-of-two geometry");
    check(checked_vm_round_up(0, page, rounded) && rounded == 0, "zero rounds to zero");
    expect_exception<std::invalid_argument>([] { VirtualMemoryStorage rejected(2, 1); },
                                           "initial range beyond cap rejected");
    expect_exception<std::length_error>([] {
        VirtualMemoryStorage rejected(0, (std::numeric_limits<std::size_t>::max)());
    }, "reservation overflow/range rejected before OS call");
    expect_exception<std::length_error>([] {
        VirtualMemoryStorage rejected(0, static_cast<std::size_t>(
            (std::numeric_limits<std::ptrdiff_t>::max)()));
    }, "rounded reservation beyond pointer difference range rejected");
}

void test_fresh_from_bytes(std::size_t page) {
    for (const auto length : {std::size_t(1), page - 1, page, page + 1, page * 3 + 17}) {
        std::string input(length, '\0');
        for (std::size_t i = 0; i < input.size(); ++i)
            input[i] = static_cast<char>((i * 37 + 11) % 251 + 1);
        auto storage = VirtualMemoryStorage::from_bytes(input.data(), input.size(), length + page);
        const auto* base = storage.data();
        check(storage.size() == length && storage.capacity() == length + page,
              "from_bytes publishes exact logical size and hard cap");
        check(std::memcmp(storage.data(), input.data(), length) == 0,
              "from_bytes copies every byte across page and partial-page boundaries");
        check(storage.accessible_bytes() >= length &&
              storage.accessible_bytes() < length + page,
              "from_bytes only enables pages containing input");
        const auto shortened = length / 2;
        storage.resize_logical(shortened);
        check(storage.grow_to(length), "loaded storage can regrow retained pages");
        check(storage.data() == base &&
              std::memcmp(storage.data(), input.data(), shortened) == 0,
              "loaded storage shrink/regrow preserves old prefix and base");
        check_bytes(storage.data() + shortened, length - shortened, 0,
                    "loaded storage shrink/regrow zeroes previously copied bytes");
        check(storage.grow_to(length + page), "loaded storage can grow to exact cap");
        check_bytes(storage.data() + length, page, 0,
                    "loaded storage growth beyond copied prefix is zero initialized");
        check(!storage.grow_to(length + page + 1), "loaded storage obeys hard cap");
    }

    auto empty = VirtualMemoryStorage::from_bytes(nullptr, 0, 0);
    check(empty.data() == nullptr && empty.size() == 0, "zero-length copy accepts null input");
    auto reserved = VirtualMemoryStorage::from_bytes(nullptr, 0, page);
    check(reserved.data() != nullptr && reserved.size() == 0 && reserved.accessible_bytes() == 0,
          "zero-length copy can reserve without publishing pages");
    expect_exception<std::invalid_argument>([&] {
        (void)VirtualMemoryStorage::from_bytes(nullptr, 1, page);
    }, "non-empty copy rejects null input");
    expect_exception<std::invalid_argument>([] {
        (void)VirtualMemoryStorage::from_bytes("x", 2, 1);
    }, "copy rejects input larger than hard cap before reading");
    expect_exception<std::length_error>([] {
        (void)VirtualMemoryStorage::from_bytes(nullptr, 0,
            (std::numeric_limits<std::size_t>::max)());
    }, "copy rejects overflowing reservation");
}

void test_full_capacity_from_bytes(std::size_t page) {
    for (const auto length : {std::size_t(1), page - 1, page, page + 1, page * 3 + 17}) {
        std::string input(length, '\0');
        for (std::size_t i = 0; i < length; ++i)
            input[i] = static_cast<char>((i * 19 + 7) % 251 + 1);
        auto storage = VirtualMemoryStorage::from_bytes(input.data(), length, length);
        const auto* base = storage.data();
        std::size_t rounded = 0;
        check(checked_vm_round_up(length, page, rounded), "round full copied load");
        check(storage.size() == length && storage.capacity() == length &&
                  storage.reserved_bytes() == rounded && storage.accessible_bytes() == rounded,
              "exact-capacity load exposes exactly the page-rounded reservation");
        check(std::memcmp(base, input.data(), length) == 0,
              "full copied load initializes every logical byte");
        check(query_page_access(base + rounded - page) == PageAccess::ReadWrite,
              "OS confirms the complete copied reservation is read/write");
        check_bytes(base + length, rounded - length, 0,
                    "fresh partial last page retains anonymous zero fill");
        const auto retained = length / 2;
        storage.resize_logical(retained);
        check(storage.reclaim_tail().complete(), "full copied owner supports tail reclamation");
        check(storage.grow_to(length) && storage.data() == base,
              "full copied owner can regrow without relocation");
        check(std::memcmp(base, input.data(), retained) == 0,
              "full copied regrowth preserves the retained prefix");
        check_bytes(base + retained, length - retained, 0,
                    "full copied regrowth clears all newly exposed bytes");
        check(!storage.grow_to(length + 1), "full copied load preserves the exact hard cap");
    }
    const std::string input(page + 17, 'n');
    auto spare = VirtualMemoryStorage::from_bytes(input.data(), input.size(), page * 4);
    check(query_page_access(spare.data() + page * 3) == PageAccess::Inaccessible,
          "copied loads with unused reserved pages retain an inaccessible tail");
    auto same_page = VirtualMemoryStorage::from_bytes(input.data(), input.size(), input.size() + 1);
    check(same_page.grow_to(input.size() + 1) && same_page.data()[input.size()] == 0,
          "a nonexact hard cap preserves ordinary zero-initialized growth within the last page");

    VmStoragePool pool({1, page * 4, page * 4});
    const void* pooled_base = nullptr;
    {
        VirtualMemoryStorage dirty(input.size(), input.size(), pool);
        pooled_base = dirty.data();
        std::memset(dirty.data(), 'd', dirty.size());
    }
    auto pooled = VirtualMemoryStorage::from_bytes(input.data(), input.size(), input.size(), pool);
    check(pooled.data() == pooled_base && pool.stats().hits == 1,
          "exact-capacity loads preserve pool acquisition instead of creating a fresh mapping");
    check(std::memcmp(pooled.data(), input.data(), input.size()) == 0,
          "exact-capacity pooled loads overwrite all dirty logical bytes");
}

void test_from_bytes_failure_and_live_input(std::size_t page) {
    VirtualMemoryStorage source(page + 17, page * 3);
    std::memset(source.data(), 'f', source.size());
    VirtualMemoryStorage target(19, page * 2);
    std::memset(target.data(), 'g', target.size());
    const auto* source_base = source.data();
    const auto* target_base = target.data();
    for (const auto capacity : {source.size(), page * 3}) {
      for (auto operation : {VmOperation::Reserve, VmOperation::Enable, VmOperation::Prepare}) {
        fail_next_vm_operation(operation);
        expect_exception<std::bad_alloc>([&] {
            target = VirtualMemoryStorage::from_bytes(source.data(), source.size(), capacity);
        }, "from_bytes reports reserve/enable/prepare failure");
        check(source.data() == source_base && target.data() == target_base && target.size() == 19,
              "failed replacement retains both live owners");
        check_bytes(source.data(), source.size(), 'f', "failed load preserves live input");
        check_bytes(target.data(), target.size(), 'g', "failed load preserves replacement target");
        auto retry = VirtualMemoryStorage::from_bytes(source.data(), source.size(), capacity);
        check_bytes(retry.data(), retry.size(), 'f', "one-shot failed copy can be retried");
      }
      fail_next_vm_operation(VmOperation::Reserve);
      fail_next_vm_operation(VmOperation::Enable);
      fail_next_vm_operation(VmOperation::Prepare);
      for (unsigned failure = 0; failure < 3; ++failure) {
          expect_exception<std::bad_alloc>([&] {
              target = VirtualMemoryStorage::from_bytes(source.data(), source.size(), capacity);
          }, "exact and spare loads consume Reserve, Enable and Prepare in separate ordered attempts");
          check(target.data() == target_base && target.size() == 19,
                "stacked failures leave the replacement target unchanged");
      }
      auto retry = VirtualMemoryStorage::from_bytes(source.data(), source.size(), capacity);
      check_bytes(retry.data(), retry.size(), 'f', "retry succeeds after all three ordered failures");
    }
    target = VirtualMemoryStorage::from_bytes(target.data(), target.size(), target.size());
    check(target.data() != target_base && target.size() == 19,
          "self-source replacement copies before releasing old owner");
    check_bytes(target.data(), target.size(), 'g', "self-source replacement preserves copied bytes");

    fail_next_vm_operation(VmOperation::Enable);
    fail_next_vm_operation(VmOperation::Prepare);
    auto zero = VirtualMemoryStorage::from_bytes(nullptr, 0, page);
    check(!zero.grow_to(1), "empty copy leaves enable failure pending");
    check(!zero.grow_to(1), "empty copy leaves prepare failure pending");
    check(zero.grow_to(1), "empty copy remains usable after both failures");
    clear_vm_failures();
}

void test_move_swap_and_release(std::size_t page) {
    static_assert(!std::is_copy_constructible<VirtualMemoryStorage>::value, "move-only owner");
    static_assert(!std::is_copy_assignable<VirtualMemoryStorage>::value, "move-only owner");
    static_assert(std::is_nothrow_move_constructible<VirtualMemoryStorage>::value, "no-throw move");
    static_assert(std::is_nothrow_move_assignable<VirtualMemoryStorage>::value, "no-throw move assignment");
    const void* released = nullptr;
    {
        VirtualMemoryStorage first(19, page * 2 + 1);
        char* first_base = first.data();
        std::memset(first.data(), 'd', first.size());
        VirtualMemoryStorage moved(std::move(first));
        check(first.data() == nullptr && first.size() == 0 && first.reserved_bytes() == 0,
              "move constructor empties source");
        check(moved.data() == first_base && moved.size() == 19,
              "move constructor transfers reservation without copying");
        check_bytes(moved.data(), moved.size(), 'd', "move preserves payload");

        VirtualMemoryStorage second(23, page * 4);
        char* second_base = second.data();
        std::memset(second.data(), 'e', second.size());
        swap(moved, second);
        check(moved.data() == second_base && second.data() == first_base,
              "swap exchanges complete storage ownership");
        check(moved.size() == 23 && moved.capacity() == page * 4 && second.size() == 19,
              "swap exchanges extent and hard cap");
        check_bytes(moved.data(), 23, 'e', "swap preserves second payload");
        check_bytes(second.data(), 19, 'd', "swap preserves first payload");

        moved = std::move(second);
        check(moved.data() == first_base && second.data() == nullptr,
              "move assignment replaces ownership and empties source");
        check(query_page_access(second_base) == PageAccess::Unmapped,
              "move assignment releases replaced reservation");
        auto& alias = moved;
        moved = std::move(alias);
        check(moved.data() == first_base && moved.size() == 19,
              "self move assignment preserves reservation");
        released = first_base;
    }
    check(query_page_access(released) == PageAccess::Unmapped,
          "destructor releases reservation exactly once");
}

// G1: growth zeroes only bytes that may be dirty; everything newly exposed
// must still read as zero (shrink/regrow, pool reuse, from_bytes partial and
// full copies, tail reclamation). Receive storage keeps full zeroing.
bool all_zero(const char* p, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) if (p[i] != 0) return false;
    return true;
}
void test_g1_zeroing() {
    const std::size_t max_bytes = 1 << 20, n = 256 << 10, small_bytes = 1000;
    {
        VirtualMemoryStorage s(0, max_bytes);
        check(s.grow_to(n), "G1 grow failed");
        std::memset(s.data(), 0x5a, n);
        s.resize_logical(small_bytes);
        check(s.grow_to(n) && all_zero(s.data() + small_bytes, n - small_bytes) && s.data()[small_bytes - 1] == 0x5a,
              "G1 shrink+regrow exposed stale bytes");
        check(s.grow_to(max_bytes) && all_zero(s.data() + small_bytes, max_bytes - small_bytes), "G1 growth beyond high-water not zero");
    }
    {
        VmStoragePool pool({2, 4 * max_bytes, 4 * max_bytes});
        {
            VirtualMemoryStorage a(0, max_bytes, pool);
            check(a.grow_to(n), "G1 pooled grow failed");
            std::memset(a.data(), 0x6b, n);
        }
        check(pool.stats().cached_blocks == 1, "G1 pooled owner not cached");
        VirtualMemoryStorage b(0, max_bytes, pool);
        check(pool.stats().hits == 1, "G1 pool reuse not exercised");
        check(b.grow_to(n) && all_zero(b.data(), n), "G1 pool reuse exposed stale bytes");
        std::memset(b.data(), 0x6c, 64);
        b.resize_logical(0);
        check(b.grow_to(64) && all_zero(b.data(), 64), "G1 pooled regrow exposed stale bytes");
    }
    {
        std::vector<char> input(5000, 'x');
        auto s = VirtualMemoryStorage::from_bytes(input.data(), input.size(), max_bytes);
        check(s.size() == input.size() && s.data()[4999] == 'x', "G1 from_bytes contents differ");
        check(s.grow_to(n) && all_zero(s.data() + input.size(), n - input.size()), "G1 growth after from_bytes not zero");
        s.resize_logical(10);
        check(s.grow_to(input.size()) && all_zero(s.data() + 10, input.size() - 10), "G1 regrow over copied bytes not zero");
        std::vector<char> full(64 << 10, 'y');
        auto f = VirtualMemoryStorage::from_bytes(full.data(), full.size(), full.size());
        f.resize_logical(100);
        check(f.grow_to(full.size()) && all_zero(f.data() + 100, full.size() - 100), "G1 full-copy regrow not zero");
    }
    {
        VirtualMemoryStorage s(0, max_bytes);
        check(s.grow_to(max_bytes), "G1 grow failed");
        std::memset(s.data(), 0x7d, max_bytes);
        s.resize_logical(small_bytes);
        check(s.reclaim_tail().complete(), "G1 reclaim failed");
        check(s.grow_to(max_bytes) && all_zero(s.data() + small_bytes, max_bytes - small_bytes), "G1 regrow after reclaim not zero");
    }
    {   // Receive storage: the whole maximum is read/write before a transport
        // writes, possibly past the logical length; growth must zero it.
        auto r = VirtualMemoryStorage::for_receive(100, max_bytes);
        check(r.accessible_bytes() == r.reserved_bytes(), "receive storage not fully accessible");
        std::memset(r.data() + 100, 0xab, 8192);
        check(r.grow_to(100 + 8192) && all_zero(r.data() + 100, 8192),
              "receive growth exposed bytes written beyond the logical length");
    }
    {   // Pooled receive storage keeps its full-zeroing high-water mark.
        VmStoragePool pool({2, 4 * max_bytes, 4 * max_bytes});
        {
            auto r = VirtualMemoryStorage::for_receive(100, max_bytes, pool);
            std::memset(r.data(), 0x3c, max_bytes);
        }
        VirtualMemoryStorage b(0, max_bytes, pool);
        check(pool.stats().hits == 1, "receive pool reuse not exercised");
        check(b.grow_to(max_bytes) && all_zero(b.data(), max_bytes), "pooled receive storage exposed stale bytes");
    }
}

} // namespace

// R1 slot backend: geometry, growth inside the slot, the owner word at
// payload - 8, and zeroing of a reused slot through its recorded dirty_end.
void test_slot_owner() {
    using XOffsetDatastructure::detail::SlotGeometry;
    using XOffsetDatastructure::detail::SlotSlab;
    if (!SlotGeometry::enabled) return;
    static_assert(SlotGeometry::class_for(1) == 0, "smallest class");
    static_assert(SlotGeometry::class_for(1024 - 64) == 0 && SlotGeometry::class_for(1024 - 63) == 1,
                  "payload capacity is 2^k - 64");
    static_assert(SlotGeometry::class_for(SlotGeometry::max_payload) == SlotGeometry::classes - 1, "SLOT_MAX");
    static_assert(SlotGeometry::class_for(SlotGeometry::max_payload + 1) == SlotGeometry::classes, "beyond");

    const auto cls = SlotGeometry::class_for(3000);
    const void* first = nullptr;
    {
        auto slot = VirtualMemoryStorage::for_slot(cls, 1u << 20, true);
        check(slot.is_slot() && slot.movable(), "slot owner kind");
        check(slot.reserved_bytes() == 4096 && slot.accessible_bytes() == 4096 - 64, "4 KiB class");
        check(reinterpret_cast<std::uintptr_t>(slot.data()) % 64 == 0, "payload 64-byte aligned");
        check(reinterpret_cast<std::uintptr_t>(slot.data() - 64) % 4096 == 0, "slot aligned to its size");
        check(slot.grow_to(slot.accessible_bytes()), "grow to the slot capacity");
        check(!slot.grow_to(slot.accessible_bytes() + 1), "no growth past the slot");
        check(slot.reclaim_tail().status == VmReclaimStatus::NoOp, "reclaim is a no-op on a slot");
        int owner = 0;
        slot.set_host_owner(&owner);
        check(*reinterpret_cast<void**>(slot.data() - sizeof(void*)) == &owner, "owner word at payload - 8");
        std::memset(slot.data(), 'd', slot.size());
        slot.resize_logical(100);
        first = slot.data();
    }
    auto again = VirtualMemoryStorage::for_slot(cls, 4000, false);
    check(again.data() == first, "this thread's cache returns the slot just freed");
    check(again.host_owner() == nullptr, "a reused slot has no owner");
    check(again.grow_to(again.maximum_capacity()), "regrow the reused slot");
    check_bytes(again.data(), again.size(), '\0', "a reused slot exposes only zero bytes");

    expect_exception<std::invalid_argument>([&] {
        (void)VirtualMemoryStorage::for_slot(cls, 1u << 20, false);
    }, "a pinned slot must hold its maximum");
    fail_next_vm_operation(VmOperation::Reserve);
    expect_exception<std::bad_alloc>([&] {
        (void)VirtualMemoryStorage::for_slot(cls, 4000, false);
    }, "slot acquisition failure");
    VirtualMemoryStorage moved(std::move(again));
    check(moved.is_slot() && !again.is_slot() && again.data() == nullptr, "move transfers the slot");
    check(SlotSlab::stats().chunks > 0, "chunks mapped");
}

int main() {
    clear_vm_failures();
    VirtualMemoryStorage probe(0, 1);
    const auto page = probe.page_size();
    test_empty_and_geometry();
    test_growth_and_hard_cap(page);
    test_shrink_regrow(page);
    test_tail_reclaim(page);
    test_reclaim_failures_and_move(page);
    test_loaded_reclaim_and_transfer(page);
    test_failure_paths(page);
    test_overflow_and_input_validation(page);
    test_fresh_from_bytes(page);
    test_full_capacity_from_bytes(page);
    test_from_bytes_failure_and_live_input(page);
    test_move_swap_and_release(page);
    test_g1_zeroing();
    test_slot_owner();
    clear_vm_failures();
    std::cout << "Virtual memory storage tests passed (page_size=" << page
              << ", reservation_granularity=" << probe.reservation_granularity() << ")\n";
}
