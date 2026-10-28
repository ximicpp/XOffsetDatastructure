// SC-12: how much address space and memory one process may use for XBuffers, so that
// DEPLOYMENT can recommend maximum_capacity for clients. Runs as a hosted program on
// iPhone (test host app) and as a plain program elsewhere.
//
//   xo_mem_probe --va CAP_MIB [MAX_COUNT]
//       Keeps creating VM-backed XBuffers with maximum_capacity CAP_MIB (Automatic
//       growth, 64 KiB used each) until one fails: how many live buffers of that
//       reservation the process gets, and the address space they hold.
//   xo_mem_probe --single
//       The largest maximum_capacity one buffer gets (binary search up to the 2 GiB
//       payload limit).
//   xo_mem_probe --commit STEP_MIB LIMIT_MIB PROGRESS_FILE
//       Fills buffers STEP_MIB at a time (written, so the pages count) up to LIMIT_MIB,
//       one line per step in PROGRESS_FILE, flushed to disk before the next step: when
//       the system kills the process (iOS jetsam) the file still says how far it got.
//
// Every line printed is "key value"; the available-memory figure is
// os_proc_available_memory() on iOS, MemAvailable on Linux and Android.
#include "xoffsetdatastructure.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE && __has_include(<os/proc.h>)
#include <os/proc.h>
#define XO_PROBE_OS_PROC 1
#endif
#endif

using namespace XOffsetDatastructure;

namespace {

constexpr std::size_t MiB = std::size_t(1) << 20;

long long available_mib() {
#if defined(XO_PROBE_OS_PROC)
    if (__builtin_available(iOS 13.0, *)) return static_cast<long long>(os_proc_available_memory() / MiB);
    return -1;
#elif defined(__linux__)
    std::ifstream f("/proc/meminfo");
    for (std::string key; f >> key;) {
        long long kb = 0;
        f >> kb;
        if (key == "MemAvailable:") return kb / 1024;
        f.ignore(256, '\n');
    }
    return -1;
#else
    return -1;
#endif
}

XBuffer::MaximumCapacity vm_cap(std::size_t bytes) {
    return XBuffer::maximum_capacity(bytes).with_growth(XGrowPolicy::Automatic);
}

int probe_va(std::size_t cap_mib, std::size_t max_count) {
    std::vector<XBuffer> live;
    live.reserve(max_count);
    std::string why = "max_count reached";
    while (live.size() < max_count) {
        try {
            live.emplace_back(64 * 1024, vm_cap(cap_mib * MiB));
            // flushed progress: when the address space runs out the C library's allocator
            // may abort the process (Android's scudo does) before the summary is printed
            if (live.size() % 8 == 0) std::cout << "va_progress " << live.size() << std::endl;
        } catch (const std::exception& e) {
            why = e.what();
            break;
        }
    }
    std::cout << "va_cap_mib " << cap_mib << "\nva_buffers " << live.size() << "\nva_reserved_gib "
              << static_cast<double>(live.size() * cap_mib) / 1024.0 << "\nva_stop " << why
              << "\navailable_mib " << available_mib() << '\n';
    return 0;
}

bool single_ok(std::size_t bytes) {
    try {
        XBuffer b(64 * 1024, vm_cap(bytes));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

int probe_single() {
    const std::size_t limit = 0x7ffffff8;  // payload hard limit
    std::size_t lo = 0, hi = limit;
    if (single_ok(hi)) {
        lo = hi;
    } else {
        hi /= MiB;
        while (lo + 1 < hi) {  // in MiB
            const std::size_t mid = (lo + hi) / 2;
            (single_ok(mid * MiB) ? lo : hi) = mid;
        }
        lo *= MiB;
    }
    std::cout << "single_max_capacity_mib " << lo / MiB << "\navailable_mib " << available_mib() << '\n';
    return 0;
}

int probe_commit(std::size_t step_mib, std::size_t limit_mib, const char* progress_path) {
    const int fd = ::open(progress_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        std::cerr << "cannot open " << progress_path << '\n';
        return 2;
    }
    auto note = [&](const std::string& line) {
        (void)!::write(fd, line.data(), line.size());
        ::fsync(fd);
        std::cout << line;
        std::cout.flush();
    };
    const std::size_t per_buffer = 1024 * MiB;  // below the 2 GiB payload limit
    std::vector<XBuffer> live;
    std::size_t committed = 0, in_current = 0;
    std::string why = "limit reached";
    note("step_mib " + std::to_string(step_mib) + " limit_mib " + std::to_string(limit_mib) +
         " available_mib " + std::to_string(available_mib()) + "\n");
    while (committed + step_mib <= limit_mib) {
        try {
            if (live.empty() || in_current + step_mib * MiB > per_buffer - 64 * 1024) {
                live.emplace_back(64 * 1024, vm_cap(per_buffer));
                in_current = 0;
            }
            XBuffer& b = live.back();
            const std::size_t before = b.get_size();
            if (!b.grow(step_mib * MiB)) {
                why = "grow failed";
                break;
            }
            std::memset(b.data() + before, 0x5a, b.get_size() - before);
            in_current += b.get_size() - before;
            committed += step_mib;
        } catch (const std::exception& e) {
            why = e.what();
            break;
        }
        note("committed_mib " + std::to_string(committed) + " available_mib " + std::to_string(available_mib()) + "\n");
    }
    note("commit_stop " + why + " committed_mib " + std::to_string(committed) + "\n");
    ::close(fd);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    std::cout << "sizeof_pointer " << sizeof(void*) << "\npage_size " << ::sysconf(_SC_PAGESIZE)
              << "\navailable_mib_at_start " << available_mib() << '\n';
    if (mode == "--va" && argc >= 3)
        return probe_va(std::strtoull(argv[2], nullptr, 10), argc >= 4 ? std::strtoull(argv[3], nullptr, 10) : 100000);
    if (mode == "--single") return probe_single();
    if (mode == "--commit" && argc >= 5)
        return probe_commit(std::strtoull(argv[2], nullptr, 10), std::strtoull(argv[3], nullptr, 10), argv[4]);
    std::cerr << "usage: xo_mem_probe --va CAP_MIB [MAX_COUNT] | --single | --commit STEP_MIB LIMIT_MIB PROGRESS_FILE\n";
    return 2;
}
