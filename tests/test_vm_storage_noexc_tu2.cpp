// Second translation unit of test_vm_storage_noexc: the pending error is one
// object per thread across translation units, and a program with two units
// that include the error header links (MinGW GCC once emitted the TLS init
// function of a namespace-scope inline thread_local in every unit).
#include "../xoffset_virtual_memory.hpp"

bool record_in_second_unit() {
    return XOffsetDatastructure::detail::record_failure(
        XOffsetDatastructure::XErrc::invalid_argument, "recorded in the second translation unit");
}
