// SLG prototype: the per-format runner interface and per-cycle statistics.
#pragma once
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include "slg/trace.hpp"
#if SLG_IR_COUNT
#include <string_view>
#include <valgrind/callgrind.h>
#endif

namespace slg {

struct CycleStats {
    std::map<std::string, int64_t> ns;      // "A.req_encode" -> nanoseconds summed over the batch
    std::map<std::string, uint64_t> bytes;  // "A.req" -> wire bytes summed over the batch
    uint64_t report_sum = 0;                // scenario D query results (must match across formats)
    uint64_t report_direct_sum = 0;         // FlatBuffers in-place reads (must equal report_sum)
};

// Wires kept from the last cycle in verify mode, for size and compression statistics.
struct SampleWires { std::string req, res, player, shard, report; };

#if SLG_IR_COUNT
// Instruction-count build (-DSLG_IR_COUNT=1) for the CI performance gate
// (docs/PERF_GATE.md). Under valgrind --tool=callgrind --collect-atstart=no,
// callgrind collects only inside timed regions named by SLG_IR_KEYS
// (comma-separated; an entry matches a key exactly, or as a prefix when it ends
// with '*', e.g. "A.*"), and not in cycles below SLG_IR_SKIP_CYCLES (default 0;
// the gate runs 8 cycles and passes 4: at S scale with SLG_REUSE=1 cycles 1-3
// are still warming up). The default build has none of
// this: timed() is the timing path unchanged.
inline int& ir_cycle() { static int c = 0; return c; }  // set by the bench loop
inline bool ir_selected(const char* key) {
    static const int skip = [] { const char* e = std::getenv("SLG_IR_SKIP_CYCLES"); return e ? std::atoi(e) : 0; }();
    if (ir_cycle() < skip) return false;
    static const std::string keys = [] { const char* e = std::getenv("SLG_IR_KEYS"); return std::string(e ? e : ""); }();
    const std::string_view k(key);
    std::size_t pos = 0;
    while (pos <= keys.size()) {
        std::size_t end = keys.find(',', pos);
        if (end == std::string::npos) end = keys.size();
        const std::string_view item = std::string_view(keys).substr(pos, end - pos);
        if (!item.empty() && item.back() == '*') {
            if (k.starts_with(item.substr(0, item.size() - 1))) return true;
        } else if (!item.empty() && item == k) {
            return true;
        }
        pos = end + 1;
    }
    return false;
}
#endif

template <class F> void timed(CycleStats& st, const char* key, F&& f) {
#if SLG_IR_COUNT
    const bool ir_on = ir_selected(key);
    if (ir_on) CALLGRIND_TOGGLE_COLLECT;
#endif
    const auto t0 = std::chrono::steady_clock::now();
    f();
#if SLG_IR_COUNT
    if (ir_on) CALLGRIND_TOGGLE_COLLECT;
#endif
    st.ns[key] += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
}

struct Runner {
    virtual ~Runner() = default;
    virtual std::string name() const = 0;
    virtual void setup(const Scale& sc, uint64_t seed) = 0;
    virtual void run_cycle(int cycle, CycleStats& st, SampleWires* keep) = 0;
    virtual uint64_t world_digest() = 0;
};

// SLG_REUSE=1 selects the reuse protocol for every format: one long-lived
// encoder/decoder state per message slot (cereal output string, protobuf
// messages + output string, FlatBufferBuilder::Clear(), XOffset
// reset/reload_verified/save_verified_into) and decoded std objects reused
// across messages. Unset: every message builds and frees its own state.
inline bool reuse_mode() { static const bool on = std::getenv("SLG_REUSE") != nullptr; return on; }

// Message slots for the reuse protocol.
enum Slot : int { kReq, kRes, kOut, kBack, kSnap, kSnap2, kSlots };

inline std::string to_string_bytes(std::span<const uint8_t> b) { return {reinterpret_cast<const char*>(b.data()), b.size()}; }

} // namespace slg
