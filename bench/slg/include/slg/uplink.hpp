// SLG benchmark, demo case set: client uplink (3.5 W4, report only).
//
// UL (client -> server): every online client sends march orders to the
// server. A client builds an order from the player it holds (login.hpp): one
// of its armies, a target tile, an arrival time and the troops it sends (each
// of the army's troop types, part of its count); 4 rounds per cycle, every
// online client one order per round. UL.send is the client's fill + encode,
// UL.recv the server's receive with the checks it runs on data from a client
// plus the read of every field of the order (the order is then handled by game
// logic, not timed). Bytes UL.order (wire), UL.order_zstd (zstd-3 per message,
// ULZ.send timed after the rounds), count UL.orders. Every format reads the
// same orders (report_sum).
//
// Checks per format: the rivals decode as everywhere else in this bench
// (FlatBuffers runs the Verifier, cista DEEP_CHECK / CAST as its format,
// protobuf and the std formats parse); xoffset reloads the wire with the full
// check (XReceiver<from_client> default for a mutable result, the same as
// reload_verified); xoffset_trusted views it in place with the bounds check
// (XReceiver<from_client>::view, XCheck::bounds) and reads the view. For the
// bounds ÷ full ratio in one process, xoffset_trusted also times ULF.recv:
// the full-check reload of the same wire plus the same read, the order of the
// two alternating by message.
//
// SLG_UL_OFF=1 drops the case. New keys only: existing keys and stage ranges
// are unchanged (the case runs after the login case).
//
// Ops (provided by each runner's LoginOps):
//   std::size_t up_encode(auto fill)   fill(March&) then encode into the uplink wire
//   uint64_t up_recv()                 the server receives the wire and returns query_order
//   std::string_view up_wire()
// Optional: uint64_t up_recv_full()   (ULF.recv, xoffset_trusted)
#pragma once
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include "generic.hpp"
#include "login.hpp"

namespace slg {

inline bool ul_on() { static const bool on = std::getenv("SLG_UL_OFF") == nullptr; return on; }

// A march order from the player a client holds.
template <class M, class P> void fill_order(M& m, const P& p, Rng& rng, int64_t seq) {
    m.march_id = seq;
    m.owner = p.player_id;
    m.to_x = int32_t(rng.below(4096));
    m.to_y = int32_t(rng.below(4096));
    m.eta = int32_t(rng.range(30, 600));
    m.troops.clear();
    if (p.armies.size() == 0) { m.from_x = 0; m.from_y = 0; return; }
    const auto& a = p.armies[std::size_t(rng.below(int32_t(p.armies.size())))];
    m.from_x = a.x;
    m.from_y = a.y;
    m.troops.reserve(a.troops.size());
    for (const auto& t : a.troops) {
        auto& d = m.troops.emplace_back();
        d.type = t.type;
        d.count = 1 + int32_t(rng.below(t.count > 1 ? t.count : 1));
        d.wounded = 0;
    }
}

// What the server reads from an order (every field).
template <class M> uint64_t query_order(const M& m) {
    uint64_t h = uint64_t(m.march_id) * 31u + uint64_t(m.owner) * 7u;
    h += uint64_t(uint32_t(m.from_x)) * 1315423911u + uint64_t(uint32_t(m.from_y)) * 2654435761u;
    h += uint64_t(uint32_t(m.to_x)) * 40503u + uint64_t(uint32_t(m.to_y)) * 69069u + uint64_t(uint32_t(m.eta));
    for (const auto& t : m.troops) h = h * 3u + uint64_t(uint32_t(t.type)) * 5u + uint64_t(uint32_t(t.count)) + uint64_t(uint32_t(t.wounded));
    return h;
}

template <class Ops> void run_uplink(Ops& ops, uint64_t seed, int cycle, int rounds, CycleStats& st) {
    if (!ul_on()) return;
    constexpr bool full_too = requires { ops.up_recv_full(); };
    const int online = ops.online();
    static thread_local WireLog uz;
    uz.clear();
    for (int r = 0; r < rounds; ++r)
        for (int p = 0; p < online; ++p) {
            Rng rng(mix_seed(seed, (uint64_t(cycle) * 64 + uint64_t(r)) * 100019 + uint64_t(p)));
            const int64_t seq = (int64_t(cycle) * 64 + r) * 1000003 + p;
            std::size_t n = 0;
            timed(st, "UL.send", [&] { n = ops.up_encode([&](auto& m) { fill_order(m, ops.held(p), rng, seq); }); });
            uint64_t q = 0, qf = 0;
            const bool full_first = full_too && ((r * online + p) & 1) != 0;
            if constexpr (full_too) if (full_first) timed(st, "ULF.recv", [&] { qf = ops.up_recv_full(); });
            timed(st, "UL.recv", [&] { q = ops.up_recv(); });
            if constexpr (full_too) {
                if (!full_first) timed(st, "ULF.recv", [&] { qf = ops.up_recv_full(); });
                if (qf != q) throw std::runtime_error("uplink: the full check read another order");
            }
            st.report_sum += q;
            st.bytes["UL.order"] += n;
            st.bytes["UL.orders"] += 1;
            uz.add(ops.up_wire());
        }
    zstd_timed(st, "ULZ.send", "UL.order_zstd", uz);
}

} // namespace slg
