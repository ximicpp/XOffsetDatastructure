// E-2 (Linux): output prefetch with madvise(MADV_POPULATE_WRITE).
//   - a fresh output of at least 1 MiB is populated in 256 KiB chunks;
//   - a reused, resident output and an output below 1 MiB are not;
//   - a kernel without MADV_POPULATE_WRITE (EINVAL, simulated here) gives the
//     same bytes, is asked once, and never again in the process.
// Every output equals the one written by a plain copy. Other platforms only
// check that the wire is unchanged.
#if defined(__linux__)
#include <cerrno>
#include <malloc.h>
#include <sys/mman.h>
#ifndef MADV_POPULATE_WRITE
#define MADV_POPULATE_WRITE 23
#endif
#include <cstddef>
namespace populate_hook {
inline bool einval = false;           // simulate a kernel before 5.14
inline std::size_t calls = 0, bytes = 0;
inline int madvise(void* addr, std::size_t length, int advice) {
    if (advice == MADV_POPULATE_WRITE) {
        ++calls;
        bytes += length;
        if (einval) { errno = EINVAL; return -1; }
    }
    return ::madvise(addr, length, advice);
}
} // namespace populate_hook
#define XOFFSET_WIRE_MADVISE populate_hook::madvise
#endif

#include "xoffsetdatastructure.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace xo = XOffsetDatastructure;

namespace wp {
struct Root {
    xo::XVector<std::uint64_t> data;
};
} // namespace wp

XOFFSET_REGISTER_SCHEMA_NAME(wp::Root, "test.wire_populate.root")
namespace XOffsetDatastructure {
template<> struct wire_record_layout<wp::Root> { static constexpr bool ordinary_members = true; };
}

namespace {
using Buffer = xo::TypedXBuffer<wp::Root>;

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "test_wire_populate FAILED: " << message << "\n";
    std::exit(1);
}
void need(bool ok, const std::string& what) { if (!ok) fail(what); }

Buffer make(std::size_t words) {
    auto b = Buffer::create(1u << 16, xo::XBuffer::maximum_capacity(1u << 26).with_growth(xo::XGrowPolicy::Automatic));
    auto& d = b.root().data;
    d.reserve(words);
    for (std::size_t i = 0; i < words; ++i) d.push_back(0x9e3779b97f4a7c15ull * (i + 1));
    return b;
}

// The wire written into a resident output (no prefetch), as the reference.
std::string resident_copy(Buffer& b, std::size_t size_hint) {
    std::string out(size_hint + 4096, 'x'); // touches every page
    b.save_verified_into(out);
    return out;
}
} // namespace

int main() {
    auto big = make(3u << 17);  // 3 MiB of payload
    auto small = make(1u << 15); // 256 KiB
#if defined(__linux__)
    // Every output of at least 1 MiB is a fresh mapping: glibc without its
    // dynamic threshold; sanitizer allocators map large blocks anyway (and
    // ignore mallopt).
    (void)mallopt(M_MMAP_THRESHOLD, 1 << 20);
    namespace h = populate_hook;
    const auto big_size = big.save_verified().size();
    h::calls = h::bytes = 0;
    const auto reference = resident_copy(big, big_size);
    need(h::calls == 0, "a resident output was populated");
    {   // Fresh output: populated chunk by chunk, same bytes.
        h::calls = h::bytes = 0;
        const auto wire = big.save_verified();
        need(wire == reference, "populated output differs");
        need(h::calls >= big_size / (256u << 10) && h::bytes >= big_size - 2 * 4096u,
             "fresh output not populated (calls " + std::to_string(h::calls) + ", bytes " + std::to_string(h::bytes) + ")");
        need(!xo::detail::wire_populate_unsupported.load(), "supported kernel marked unsupported");
        auto loaded = Buffer::load(wire);
        need(loaded.save_verified() == wire, "populated wire does not round-trip");
    }
    {   // Reused output with enough capacity: resident, not populated.
        std::string out = reference;
        h::calls = 0;
        big.save_verified_into(out);
        need(out == reference && h::calls == 0, "reused output populated or different");
    }
    {   // Below 1 MiB: never populated.
        h::calls = 0;
        const auto wire = small.save_verified();
        need(h::calls == 0 && wire == resident_copy(small, wire.size()), "small output populated or different");
    }
    {   // Kernel without MADV_POPULATE_WRITE: one EINVAL, then plain copies.
        h::einval = true;
        h::calls = 0;
        const auto first = big.save_verified();
        need(first == reference, "fallback output differs");
        need(h::calls == 1, "EINVAL: expected exactly one call, got " + std::to_string(h::calls));
        need(xo::detail::wire_populate_unsupported.load(), "EINVAL did not mark the kernel unsupported");
        std::string into;
        big.save_verified_into(into);
        const auto again = big.save_verified();
        need(again == reference && into == reference && h::calls == 1, "unsupported kernel asked again");
        h::einval = false;
        xo::detail::wire_populate_unsupported.store(false);
    }
    std::cout << "test_wire_populate: 4 groups passed (" << big_size << " B wire)\n";
#else
    const auto wire = big.save_verified();
    need(wire == resident_copy(big, wire.size()), "output differs");
    need(small.save_verified() == resident_copy(small, small.save_verified().size()), "small output differs");
    std::cout << "test_wire_populate: not Linux, wire checks passed\n";
#endif
    return 0;
}
