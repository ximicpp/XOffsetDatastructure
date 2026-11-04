// HFT demo: measured regions. Built with HFT_IR, a region toggles callgrind
// collection (run one phase per process with --ir PHASE; --ir all counts
// every region and dumps each one on its own, for regions that run once);
// otherwise it reads the steady clock. Only the regions are measured: decoding the exchange
// feed, building the inputs and checking digests are outside them.
#pragma once
#include <chrono>
#include <cstdint>
#include <string>

#if defined(HFT_IR)
#include <valgrind/callgrind.h>
#endif

namespace hft {

struct Region {
    std::string phase;       // this region's name
    const std::string* ir;   // the phase callgrind counts (HFT_IR builds), empty: none
    double ns = 0;
    std::uint64_t runs = 0;
    std::chrono::steady_clock::time_point t0{};

    void begin() {
#if defined(HFT_IR)
        if (*ir == phase || *ir == "all") CALLGRIND_TOGGLE_COLLECT;
#else
        t0 = std::chrono::steady_clock::now();
#endif
    }
    void end() {
#if defined(HFT_IR)
        if (*ir == phase) CALLGRIND_TOGGLE_COLLECT;
        if (*ir == "all") {
            CALLGRIND_TOGGLE_COLLECT;
            CALLGRIND_DUMP_STATS_AT(phase.c_str());
        }
#else
        ns += double(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count());
#endif
        ++runs;
    }
    template <class F>
    void time(F&& f) {
        begin();
        f();
        end();
    }
};

}  // namespace hft
