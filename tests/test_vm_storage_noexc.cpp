// SC-8: the OS storage layer (C++17, no reflection) built without exceptions,
// as an engine client builds it (-fno-exceptions; MSVC /EHs-c-). Each
// receive-path step reports its failure as false plus the pending error,
// with the code and message the exceptions build throws, and leaves the
// owner as it was.
#include "../xoffset_virtual_memory.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#if !XOFFSET_NO_EXCEPTIONS
#error "test_vm_storage_noexc is built with exceptions disabled"
#endif

using namespace XOffsetDatastructure;
using namespace XOffsetDatastructure::detail;

namespace {
int failures = 0;
void need(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
bool refused(vstatus status, XErrc code, const char* message) {
    if (status) return false;
    const auto error = take_pending_error();
    return error.code == code && error.message == message;
}
} // namespace

bool record_in_second_unit(); // test_vm_storage_noexc_tu2.cpp

int main() {
    const std::string bytes(4096, 'x');
    {
        VirtualMemoryStorage owner;
        need(bool(VirtualMemoryStorage::create_v(owner, 64, 1u << 20)), "create_v");
        need(owner.size() == 64 && owner.maximum_capacity() == (1u << 20), "create_v geometry");
    }
    {
        VirtualMemoryStorage owner;
        need(refused(VirtualMemoryStorage::from_bytes_v(owner, bytes.data(), bytes.size(), 1024, {}),
                     XErrc::invalid_argument, "VM input length exceeds maximum capacity"),
             "from_bytes_v over the maximum");
        need(owner.empty(), "owner after a refused from_bytes_v");
        need(refused(VirtualMemoryStorage::from_bytes_v(owner, nullptr, 64, 1024, {}), XErrc::invalid_argument,
                     "VM input is null"),
             "from_bytes_v of null input");
        need(bool(VirtualMemoryStorage::from_bytes_v(owner, bytes.data(), bytes.size(), 1u << 20, {})),
             "from_bytes_v");
        need(owner.size() == bytes.size() && std::memcmp(owner.data(), bytes.data(), bytes.size()) == 0,
             "from_bytes_v content");
        // A refused in-place assignment keeps the old bytes and length.
        const std::string longer(owner.maximum_capacity() + 8, 'y');
        need(refused(owner.assign_bytes_v(longer.data(), longer.size()), XErrc::invalid_argument,
                     "VM input length exceeds maximum capacity"),
             "assign_bytes_v over the maximum");
        need(owner.size() == bytes.size() && std::memcmp(owner.data(), bytes.data(), bytes.size()) == 0,
             "owner after a refused assign_bytes_v");
    }
    {
        VirtualMemoryStorage owner;
        need(refused(VirtualMemoryStorage::for_receive_v(owner, 4096, 1024), XErrc::invalid_argument,
                     "VM receive length exceeds maximum capacity"),
             "for_receive_v over the maximum");
        need(refused(VirtualMemoryStorage::for_slot_v(owner, SlotGeometry::classes, 64, true),
                     XErrc::invalid_argument, "slot class out of range"),
             "for_slot_v of a bad class");
        need(owner.empty(), "owner after refusals");
    }
    need(refused(record_in_second_unit(), XErrc::invalid_argument,
                 "recorded in the second translation unit"),
         "a failure recorded in another translation unit");
    need(take_pending_error().code == XErrc::ok, "no pending error is left behind");
    if (failures) return 1;
    std::printf("test_vm_storage_noexc: ok\n");
    return 0;
}
