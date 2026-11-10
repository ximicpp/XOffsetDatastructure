// Address-space probe for the "virtual slot" growth design (3.6 candidate): how much
// virtual address space a client process may reserve, whether reserved-but-untouched
// address space counts toward the memory footprint the OS polices (iOS jetsam
// phys_footprint, Android/Linux RSS), how many fixed-size ranges carved from large
// lazily-committed regions fit, and what creating/destroying a buffer costs each way.
// Runs as a hosted program on iPhone (test host app), on Android over adb, and as a
// plain program elsewhere. Every line printed is "key value".
//
//   xo_va_probe --raw CHUNK_MIB [rw]
//       Reserve CHUNK_MIB pieces (PROT_NONE, or read/write with "rw") until mmap fails:
//       the total reservable address space and the highest address seen.
//   xo_va_probe --footprint RESERVE_MIB
//       Footprint before, after reserving RESERVE_MIB read/write untouched, and after
//       touching 64 MiB of it.
//   xo_va_probe --vslab CAP_MIB REGION_MIB [MAX_COUNT]
//       Carve CAP_MIB ranges out of REGION_MIB read/write regions, touching 8 KiB of
//       each (a small message), until a region mapping fails or MAX_COUNT (default
//       100000) ranges are live.
//   xo_va_probe --cost CAP_MIB
//       ns per create+destroy: free-list range (virtual slot model), mmap+munmap of
//       CAP_MIB per buffer (VM model), and the library's current storage kinds.
#include "xoffsetdatastructure.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <sys/mman.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#endif

using namespace XOffsetDatastructure;

struct VaProbeRec {
    std::int32_t id{0};
    std::int32_t level{0};
    XString name;
    XVector<std::int32_t> items;
};
XOFFSET_RECORD(VaProbeRec, id, level, name, items)

namespace {

constexpr std::size_t MiB = std::size_t(1) << 20;
volatile std::size_t g_sink = 0;

// The figure the OS polices: phys_footprint on Apple platforms, VmRSS on Linux/Android.
long long footprint_kib() {
#if defined(__APPLE__)
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
        return -1;
    return static_cast<long long>(info.phys_footprint / 1024);
#else
    std::ifstream f("/proc/self/status");
    for (std::string line; std::getline(f, line);)
        if (line.rfind("VmRSS:", 0) == 0) return std::atoll(line.c_str() + 6);
    return -1;
#endif
}

void* map(std::size_t bytes, bool rw) {
    int flags = MAP_PRIVATE | MAP_ANON;
#if defined(MAP_NORESERVE)
    if (rw) flags |= MAP_NORESERVE;
#endif
    void* p = ::mmap(nullptr, bytes, rw ? (PROT_READ | PROT_WRITE) : PROT_NONE, flags, -1, 0);
    return p == MAP_FAILED ? nullptr : p;
}

int probe_raw(std::size_t chunk_mib, bool rw) {
    std::vector<void*> chunks;
    std::uintptr_t highest = 0;
    while (void* p = map(chunk_mib * MiB, rw)) {
        chunks.push_back(p);
        highest = std::max(highest, reinterpret_cast<std::uintptr_t>(p) + chunk_mib * MiB);
        if (chunks.size() >= (std::size_t(1) << 22)) break;
    }
    std::printf("raw_mode %s\nraw_chunk_mib %zu\nraw_chunks %zu\nraw_reserved_gib %.3f\nraw_highest_address 0x%llx\n"
                "raw_highest_bits %d\nfootprint_kib %lld\n",
                rw ? "rw" : "none", chunk_mib, chunks.size(), double(chunks.size() * chunk_mib) / 1024.0,
                static_cast<unsigned long long>(highest), highest ? 64 - __builtin_clzll(highest) : 0, footprint_kib());
    for (void* p : chunks) ::munmap(p, chunk_mib * MiB);
    return 0;
}

int probe_footprint(std::size_t reserve_mib) {
    const long long before = footprint_kib();
    char* p = static_cast<char*>(map(reserve_mib * MiB, true));
    if (!p) {
        std::printf("fp_reserve_failed %zu\n", reserve_mib);
        return 0;
    }
    const long long reserved = footprint_kib();
    const std::size_t touch = std::min<std::size_t>(64 * MiB, reserve_mib * MiB);
    std::memset(p, 0x5a, touch);
    const long long touched = footprint_kib();
    std::printf("fp_reserve_mib %zu\nfp_before_kib %lld\nfp_after_reserve_kib %lld\nfp_after_touch_%zu_mib_kib %lld\n",
                reserve_mib, before, reserved, touch / MiB, touched);
    ::munmap(p, reserve_mib * MiB);
    return 0;
}

int probe_vslab(std::size_t cap_mib, std::size_t region_mib, std::size_t max_count) {
    const std::size_t cap = cap_mib * MiB, region = std::max(region_mib, cap_mib) * MiB;
    std::vector<char*> regions;
    std::size_t live = 0;
    char *cur = nullptr, *end = nullptr;
    const long long before = footprint_kib();
    for (;;) {
        if (!cur || cur + cap > end) {
            char* p = static_cast<char*>(map(region, true));
            if (!p) break;
            regions.push_back(p);
            cur = p;
            end = p + region;
        }
        std::memset(cur, 1, 8192);
        cur += cap;
        ++live;
        if (live % 64 == 0) std::cout << "vslab_progress " << live << std::endl;
        if (live >= max_count) break;
    }
    std::printf("vslab_cap_mib %zu\nvslab_region_mib %zu\nvslab_live %zu\nvslab_regions %zu\nvslab_reserved_gib %.3f\n"
                "vslab_footprint_delta_kib %lld\n",
                cap_mib, region / MiB, live, regions.size(), double(regions.size() * region) / double(1u << 30),
                footprint_kib() - before);
    for (char* p : regions) ::munmap(p, region);
    return 0;
}

template <class F>
double ns_per(F&& f, int n) {
    for (int i = 0; i < n / 10; ++i) f();
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < n; ++i) f();
    return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - t0).count() / n;
}

void fill(XBuffer& b) {
    auto& r = b.root<VaProbeRec>();
    r.name = "Alice";
    for (int i = 0; i < 16; ++i) r.items.push_back(i);
    g_sink += r.items.size();
}

int probe_cost(std::size_t cap_mib) {
    const std::size_t cap = cap_mib * MiB;
    // virtual slot model: ranges of one region on a free list; reuse zeroes what it exposes
    char* region = static_cast<char*>(map(64 * cap, true));
    std::vector<char*> free_list;
    if (region)
        for (int i = 0; i < 64; ++i) free_list.push_back(region + std::size_t(i) * cap);
    const double vslab = region ? ns_per([&] {
        char* b = free_list.back();
        free_list.pop_back();
        std::memset(b, 0, 512);
        b[0] = 1;
        g_sink += static_cast<unsigned char>(b[0]);
        free_list.push_back(b);
    }, 200000) : -1;
    if (region) ::munmap(region, 64 * cap);
    // VM model: one reservation per buffer, the first page made accessible
    const std::size_t page = static_cast<std::size_t>(::sysconf(_SC_PAGESIZE));
    const double vm = ns_per([&] {
        void* p = map(cap, false);
        if (p) {
            ::mprotect(p, page, PROT_READ | PROT_WRITE);
            static_cast<char*>(p)[0] = 1;
            ::munmap(p, cap);
        }
    }, 20000);
    // the library today
    const double movable = ns_per([&] { XBuffer b = XBuffer::create<VaProbeRec>(512); fill(b); }, 100000);
    const double pinned = ns_per([&] {
        XBuffer b = XBuffer::create<VaProbeRec>(256, XBuffer::maximum_capacity(XBuffer::SLOT_MAX).with_growth(XGrowPolicy::Automatic));
        fill(b);
    }, 100000);
    const double auto_vm = ns_per([&] {
        XBuffer b = XBuffer::create<VaProbeRec>(256, XBuffer::maximum_capacity(cap).with_growth(XGrowPolicy::Automatic));
        fill(b);
    }, 20000);
    XBufferPool pool({16, 16 * cap, 64 * MiB});
    const double auto_pool = ns_per([&] {
        XBuffer b = XBuffer::create<VaProbeRec>(256, XBuffer::maximum_capacity(cap).with_growth(XGrowPolicy::Automatic).with_pool(pool));
        fill(b);
    }, 100000);
    std::printf("cost_cap_mib %zu\ncost_vslab_freelist_ns %.1f\ncost_mmap_munmap_ns %.1f\n"
                "cost_lib_movable_slot_ns %.1f\ncost_lib_pinned_slot_auto_ns %.1f\ncost_lib_vm_auto_ns %.1f\ncost_lib_vm_auto_pool_ns %.1f\n",
                cap_mib, vslab, vm, movable, pinned, auto_vm, auto_pool);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "";
    std::printf("sizeof_pointer %zu\npage_size %ld\nfootprint_kib_at_start %lld\n", sizeof(void*), ::sysconf(_SC_PAGESIZE),
                footprint_kib());
    std::fflush(stdout);
    auto num = [&](int i, std::size_t d) { return argc > i ? std::strtoull(argv[i], nullptr, 10) : d; };
    if (mode == "--raw") return probe_raw(num(2, 256), argc > 3 && std::string(argv[3]) == "rw");
    if (mode == "--footprint") return probe_footprint(num(2, 2048));
    if (mode == "--vslab") return probe_vslab(num(2, 64), num(3, 256), num(4, 100000));
    if (mode == "--cost") return probe_cost(num(2, 64));
    std::fprintf(stderr, "usage: xo_va_probe --raw CHUNK_MIB [rw] | --footprint RESERVE_MIB | --vslab CAP_MIB REGION_MIB [MAX_COUNT] | --cost CAP_MIB\n");
    return 2;
}
