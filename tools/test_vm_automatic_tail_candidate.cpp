// Boundary checks for the generated N5 experiment; not a production policy.
#include "xoffsetdatastructure.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>

#ifndef XOFFSET_CANDIDATE_TAIL_FLUSH
#define XOFFSET_CANDIDATE_TAIL_FLUSH 0
#endif
namespace {
thread_local bool fail_host_allocation = false;
thread_local unsigned injected_host_failures = 0;
}
void* operator new(std::size_t bytes) {
    if (fail_host_allocation) {
        fail_host_allocation = false; ++injected_host_failures; throw std::bad_alloc();
    }
    if (auto* ptr = std::malloc(bytes ? bytes : 1)) return ptr;
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }

using namespace XOffsetDatastructure;
struct CandidateRoot { std::uint32_t marker; XVector<std::uint32_t> values; };
XOFFSET_REGISTER_SCHEMA_NAME(CandidateRoot, "test.automatic-tail-candidate.v1")

void check(detail::VmOperation fault, bool inject_os, bool inject_host, bool exceed_max) {
    constexpr std::size_t maximum = 256 * 1024;
    auto b = TypedXBuffer<CandidateRoot>::create(4096,
        XBuffer::maximum_capacity(maximum).with_growth(XGrowPolicy::Automatic));
    b.root().marker = 42;
    auto alloc = fixed_detail::allocator_ref_of(b.allocator_state());
    auto* cached = fixed_detail::allocate_dynamic_bytes(alloc, 512, 8);
    auto* tail = static_cast<char*>(fixed_detail::allocate_dynamic_bytes(alloc, 128, 8));
    fixed_detail::retire_dynamic_bytes(alloc, cached, 512);
    std::memset(tail, 0x5a, 128);
    b.shrink_to_fit();
    const auto used = b.arena()->used_end;
    const auto logical = b.get_size();
    const auto base = b.get_address();
    const auto previous_failures = injected_host_failures;
    if (inject_os) detail::fail_next_vm_operation(fault);
    if (inject_host && XOFFSET_CANDIDATE_TAIL_FLUSH) fail_host_allocation = true;
    bool threw = false, expanded = false;
    try {
        expanded = fixed_detail::try_expand_dynamic_bytes(alloc, tail,
            exceed_max ? maximum : 65536, 8);
    } catch (const XBadAlloc&) { threw = true; }
    fail_host_allocation = false;
    detail::clear_vm_failures();
    assert(b.get_address() == base && b.root().marker == 42);
    for (unsigned i = 0; i < 128; ++i) assert(tail[i] == 0x5a);
    if (inject_os || exceed_max) {
        assert(!expanded && threw == inject_os);
        assert(b.get_size() == logical && b.arena()->used_end == used);
        // Cache transfer may precede a failed OS operation. It does not promise
        // byte-for-byte rollback of allocator bookkeeping or OOM reclamation.
        assert(b.allocator_state()->free_bytes ==
            (XOFFSET_CANDIDATE_TAIL_FLUSH && inject_os ? 0 : 512));
    } else {
        assert(expanded && !threw && b.get_size() > logical);
        assert(b.allocator_state()->free_bytes == (XOFFSET_CANDIDATE_TAIL_FLUSH ? 0 : 512));
    }
    assert(injected_host_failures - previous_failures ==
        unsigned(XOFFSET_CANDIDATE_TAIL_FLUSH && inject_host && !exceed_max));
    // Existing live values remain valid after each failure and subsequent work.
    b.root().values.push_back(71);
    auto loaded = TypedXBuffer<CandidateRoot>::load(b.save_verified());
    assert(loaded.root().marker == 42 && loaded.root().values[0] == 71);
}

int main() {
    check(detail::VmOperation::Enable, true, false, false);
    check(detail::VmOperation::Prepare, true, false, false);
    check(detail::VmOperation::Enable, true, true, false);
    check(detail::VmOperation::Prepare, false, true, false);
    check(detail::VmOperation::Prepare, false, false, true);
    std::cout << "candidate boundary checks passed\n";
}
