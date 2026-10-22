// SLG prototype: the per-format runner interface and per-cycle statistics.
#pragma once
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include "slg/trace.hpp"

namespace slg {

struct CycleStats {
    std::map<std::string, int64_t> ns;      // "A.req_encode" -> nanoseconds summed over the batch
    std::map<std::string, uint64_t> bytes;  // "A.req" -> wire bytes summed over the batch
    uint64_t report_sum = 0;                // scenario D query results (must match across formats)
    uint64_t report_direct_sum = 0;         // FlatBuffers in-place reads (must equal report_sum)
};

// Wires kept from the last cycle in verify mode, for size and compression statistics.
struct SampleWires { std::string req, res, player, shard, report; };

template <class F> void timed(CycleStats& st, const char* key, F&& f) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
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
