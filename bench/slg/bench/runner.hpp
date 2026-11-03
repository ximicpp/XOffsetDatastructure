// SLG prototype: the per-format runner interface and per-cycle statistics.
#pragma once
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
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
struct SampleWires { std::string req, res, player, shard, report, view, delta; };

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

// Game logic between the data steps (battle simulation, scene/season edits,
// shard ticks). In the demo case set (SLG_CASES=demo) it runs untimed: a timed
// stage there holds only data reads and writes, encode/decode, send and
// receive (susu 2026-11-03), and the key is not recorded. With SLG_CASES unset
// it stays timed under its key, so the perf gate's stage keys do not change.
template <class F> void untimed(CycleStats& st, const char* key, F&& f) {
    static const bool demo = [] { const char* e = std::getenv("SLG_CASES"); return e && std::string_view(e) == "demo"; }();
    if (demo) f();
    else timed(st, key, std::forward<F>(f));
}

// XOffset native delta stages (N*, LK, LN, SN) run in the same process as the
// application-level ones, interleaved per delivery, so whichever runs second
// finds the world and the player warm. SLG_NATIVE_OFF=1 skips the native
// stages: the formal runs take XOffset's application-level rows from such
// processes and the native rows from processes with them on.
inline bool native_on() { static const bool on = std::getenv("SLG_NATIVE_OFF") == nullptr; return on; }

// The storage process checks a received player and keeps the bytes. A format
// with a check that needs no application model (FlatBuffers' Verifier,
// protobuf's parse) runs only that; the others decode, which is their check.
template <class R, class T> void store_check(R& rz, std::span<const uint8_t> in, T& scratch) {
    if constexpr (requires { rz.store_check(in, scratch); }) rz.store_check(in, scratch);
    else rz.decode(in, scratch);
}

// The counterpart of XOffset's prefetch_for_update() hint before A.apply for
// the other runners (fairness review 2026-11-03): when the battle result is
// arriving, the scene starts loading the two armies it will write (the army
// and its hero and troop arrays). Timed in A.res_decode, as XOffset's hint.
template <class A> void prefetch_army_for_apply(const A& a) {
    __builtin_prefetch(&a, 1);
    if (!a.heroes.empty()) __builtin_prefetch(&a.heroes[0], 1);
    if (!a.troops.empty()) __builtin_prefetch(&a.troops[0], 1);
    if (!a.buffs.empty()) __builtin_prefetch(&a.buffs[0], 1);
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
enum Slot : int { kReq, kRes, kOut, kBack, kSnap, kSnap2, kView, kDelta, kSlots };

inline std::string to_string_bytes(std::span<const uint8_t> b) { return {reinterpret_cast<const char*>(b.data()), b.size()}; }

} // namespace slg
