// Relocating storage (3.6 growth policy, PR 1 gate): resident memory of many
// small live buffers, and the cost of create + fill + destroy, per storage.
//
//   bench_relocating_storage rss MODE N     one process per measurement
//   bench_relocating_storage time MODE N    ns per create + fill + destroy
//
// MODE: relocating (1 MiB first reservation), relocating4k (with_reserve 4 KiB),
// explicit (create<T>(4096): a 4 KiB movable slot, today's default),
// automatic (Automatic with maximum SLOT_MAX: a pinned 256 KiB slot).
// Each buffer holds a small record (id, a 24-byte name, 4 items): about
// 200 payload bytes, a typical small message or entity.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include "xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct Unit {
    std::int32_t id{0};
    std::int32_t hp{0};
    XString name;
    XVector<std::int32_t> items;
};
XOFFSET_RECORD_DERIVED(Unit, (), id, hp, name, items)
XOFFSET_REGISTER_SCHEMA_NAME(Unit, "bench.reloc.Unit")

namespace {
XBuffer make(const std::string& mode) {
    if (mode == "relocating") return XBuffer::create<Unit>(256, XBuffer::relocating());
    if (mode == "relocating4k") return XBuffer::create<Unit>(256, XBuffer::relocating().with_reserve(4096));
    if (mode == "explicit") return XBuffer::create<Unit>(4096);
    if (mode == "automatic")
        return XBuffer::create<Unit>(256, XBuffer::maximum_capacity(XBuffer::SLOT_MAX).with_growth(XGrowPolicy::Automatic));
    std::fprintf(stderr, "unknown mode %s\n", mode.c_str());
    std::exit(2);
}

void fill(XBuffer& b, int i) {
    auto& u = b.root<Unit>();
    u.id = i;
    u.hp = 100;
    u.name = "unit-name-0123456789-" + std::to_string(i % 1000);
    for (int k = 0; k < 4; ++k) u.items.push_back(i + k);
}

// kB from /proc/self/status
long status_kb(const char* key) {
    std::ifstream in("/proc/self/status");
    std::string line;
    const auto n = std::strlen(key);
    while (std::getline(in, line))
        if (line.compare(0, n, key) == 0 && line[n] == ':') return std::atol(line.c_str() + n + 1);
    return -1;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s rss|time MODE N\n", argv[0]);
        return 2;
    }
    const std::string what = argv[1], mode = argv[2];
    const int n = std::atoi(argv[3]);
    if (what == "rss") {
        {   // warm the library's one-time state (owner map, caches)
            auto w = make(mode);
            fill(w, 0);
        }
        const long rss0 = status_kb("VmRSS"), pte0 = status_kb("VmPTE");
        std::vector<XBuffer> live;
        live.reserve(n);
        for (int i = 0; i < n; ++i) {
            live.push_back(make(mode));
            fill(live.back(), i);
        }
        const long rss1 = status_kb("VmRSS"), pte1 = status_kb("VmPTE");
        const auto stats = XBufferCore::slot_storage_stats();
        const double per = (rss1 - rss0) * 1024.0 / n;
        std::printf("%-13s n=%-7d rss +%8.1f MB  (%7.0f B/buffer)  page tables +%6.1f MB (%5.0f B/buffer)  "
                    "slot chunks %zu  used %zu B/buffer\n",
                    mode.c_str(), n, (rss1 - rss0) / 1024.0, per, (pte1 - pte0) / 1024.0,
                    (pte1 - pte0) * 1024.0 / n, stats.chunks, std::size_t(live.back().arena()->used_end));
        return 0;
    }
    if (what == "time") {
        double best = 1e300;
        for (int round = 0; round < 7; ++round) {
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < n; ++i) {
                auto b = make(mode);
                fill(b, i);
            }
            const auto t1 = std::chrono::steady_clock::now();
            const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / n;
            if (ns < best) best = ns;
        }
        std::printf("%-13s create + fill + destroy: %7.1f ns (best of 7, n=%d)\n", mode.c_str(), best, n);
        return 0;
    }
    return 2;
}
