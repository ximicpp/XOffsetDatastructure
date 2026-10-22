// SLG prototype: runner for cista (v0.16) in offset mode, used the way XOffset is
// used: the receiver copies the wire into a buffer it owns, deserializes it in place
// (cista::deserialize with the runner's mode; the default "cista" format uses
// mode::DEEP_CHECK), runs the game logic on the deserialized object, and serializes
// it again. Containers that grow past their deserialized size move to the heap (cista's
// own behaviour); dropping the object walks it once to free those (cista_free.hpp,
// timed where the drop happens), and the buffer is freed or reused.
// Each player and the map shard are such a deserialized object, like the XOffset runner
// keeps one XBuffer each. "cista_unchecked" (mode::CAST: cista's trusted-input
// path, a reinterpret_cast with no checks and no traversal) is a
// diagnostic format for the "opponent does not validate" column, not in the default
// format list.
#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <cista/mode.h>
#include <cista/serialization.h>
#include <cista/targets/buf.h>
#include "runner.hpp"
#include "slg/cista_free.hpp"
#include "slg/generic.hpp"
#include "slg/model_cista.hpp"
#include "slg/model_std.hpp"
#include "slg/trace.hpp"

namespace slg {

// Direct ("cista_direct", reuse protocol only): the "direct receive" column, see recv().
template <cista::mode Mode, bool Direct = false> class CistaRunner final : public Runner {
    using Bytes = std::vector<uint8_t>;

    // A deserialized object living in a buffer this side owns.
    template <class T> struct Held {
        Bytes buf;
        T* p = nullptr;
        Held() = default;
        Held(Held&& o) noexcept : buf(std::move(o.buf)), p(std::exchange(o.p, nullptr)) {}
        Held& operator=(Held&& o) noexcept { if (this != &o) { drop(); buf = std::move(o.buf); p = std::exchange(o.p, nullptr); } return *this; }
        ~Held() { drop(); }
        friend void swap(Held& a, Held& b) noexcept { std::swap(a.buf, b.buf); std::swap(a.p, b.p); }
        // Frees what the object moved to the heap (see cista_free.hpp); the buffer
        // itself is kept for reuse.
        void drop() { if (p) { cista_free::free_self_allocated(p); p = nullptr; } }
        // Receive: copy the wire into the owned buffer and deserialize (checked) in place.
        void receive(std::span<const uint8_t> in) {
            drop();
            buf.assign(in.begin(), in.end());
            p = cista::deserialize<T, Mode>(buf);
        }
        T& operator*() const { return *p; }
    };

    template <class T> static std::span<const uint8_t> save(Bytes& out, T& v) {
        out.clear();
        cista::buf<Bytes&> b{out};
        cista::serialize<Mode>(b, v);
        return {out.data(), out.size()};
    }

public:
    std::string name() const override {
        return Direct ? "cista_direct" : cista::is_mode_enabled(Mode, cista::mode::DEEP_CHECK) ? "cista" : "cista_unchecked";
    }

    void setup(const Scale& sc, uint64_t seed) override {
        sc_ = &sc; seed_ = seed;
        players_.clear();
        armies_.clear();
        Bytes tmp;
        for (int i = 0; i < sc.players; ++i) {
            const stdm::PlayerState src = make_player(seed, i + 1);
            cm::PlayerState h;
            copy_player(h, src);
            players_.emplace_back().receive(save(tmp, h));
            armies_.push_back(int(src.armies.size()));
        }
        cm::ShardState s;
        copy_shard(s, make_shard(seed, sc));
        shard_.receive(save(tmp, s));
    }

    void run_cycle(int cycle, CycleStats& st, SampleWires* keep) override {
        const bool reuse = reuse_mode();
        if (Direct && !reuse) throw std::runtime_error("cista_direct runs only with SLG_REUSE=1");
        const Scale& sc = *sc_;
        std::vector<stdm::Report> reports;
        reports.reserve(std::size_t(sc.battles));

        // A: battle server round trip. The battle server changes the received object in place.
        for (const Pairing& pr : make_pairings(seed_, cycle, sc, armies_)) {
            auto& att = player(pr.ap).armies[std::size_t(pr.aa)];
            auto& def = player(pr.dp).armies[std::size_t(pr.da)];
            Bytes req_local, res_local;
            Held<cm::BattleMsg> m_local, r_local;
            Bytes& req = reuse ? req_ : req_local;
            Bytes& res = reuse ? res_ : res_local;
            Held<cm::BattleMsg>& m = reuse ? a_m_ : m_local;
            Held<cm::BattleMsg>& r = reuse ? a_r_ : r_local;
            timed(st, "A.req_encode", [&] {
                if (reuse) {
                    build_request(a_req_, pr, att, def);
                    save(req, a_req_);
                } else {
                    cm::BattleMsg msg;
                    build_request(msg, pr, att, def);
                    save(req, msg);
                }
            });
            recv(st, "A.req_decode", m, req);
            timed(st, "A.simulate", [&] { simulate_battle(*m); });
            timed(st, "A.res_encode", [&] { save(res, *m); });
            recv(st, "A.res_decode", r, res);
            timed(st, "A.apply", [&] {
                const auto& out = *r;
                apply_battle_army(att, out.attacker);
                apply_battle_army(def, out.defender);
            });
            st.bytes["A.req"] += req.size();
            st.bytes["A.res"] += res.size();
            copy_report(reports.emplace_back(), (*r).report);
            if (keep) { keep->req = to_string_bytes(req); keep->res = to_string_bytes(res); }
            if (reuse) timed(st, "A.cleanup", [] {});
            else timed(st, "A.cleanup", [&] { m.drop(); r.drop(); Bytes().swap(m.buf); Bytes().swap(r.buf); Bytes().swap(req); Bytes().swap(res); });
        }

        // B: cross-server season migration and write-back.
        const auto migrants = pick_migrants(seed_, cycle, sc);
        for (std::size_t k = 0; k < migrants.size(); ++k) {
            auto& slot = players_[std::size_t(migrants[k])];
            Bytes out_local, back_local;
            Held<cm::PlayerState> m_local, r_local;
            Bytes& out = reuse ? out_ : out_local;
            Bytes& back = reuse ? back_ : back_local;
            Held<cm::PlayerState>& m = reuse ? b_m_ : m_local;
            Held<cm::PlayerState>& r = reuse ? b_r_ : r_local;
            timed(st, "B.out_encode", [&] { save(out, *slot); });
            recv(st, "B.in_decode", m, out);
            timed(st, "B.season_edits", [&] { season_edits(*m, mix_seed(seed_, uint64_t(cycle) * 1000003u + k), sc.season_ops); });
            timed(st, "B.back_encode", [&] { save(back, *m); });
            recv(st, "B.back_decode", r, back);
            timed(st, "B.writeback", [&] { swap(slot, r); });
            st.bytes["B.out"] += out.size();
            st.bytes["B.back"] += back.size();
            if (keep) keep->player = to_string_bytes(back);
            if (reuse) timed(st, "B.cleanup", [] {});
            else timed(st, "B.cleanup", [&] { m.drop(); r.drop(); Bytes().swap(m.buf); Bytes().swap(r.buf); Bytes().swap(out); Bytes().swap(back); });
        }

        // C: map shard snapshot, restore, ticks, snapshot again.
        {
            Bytes snap_local, snap2_local;
            Held<cm::ShardState> r_local;
            Bytes& snap = reuse ? snap_ : snap_local;
            Bytes& snap2 = reuse ? snap2_ : snap2_local;
            Held<cm::ShardState>& r = reuse ? c_r_ : r_local;
            timed(st, "C.snapshot", [&] { save(snap, *shard_); });
            recv(st, "C.restore", r, snap);
            timed(st, "C.ticks", [&] { shard_ticks(*r, mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            timed(st, "C.snapshot2", [&] { save(snap2, *r); });
            timed(st, "C.replace", [&] { swap(shard_, r); });
            st.bytes["C.snapshot"] += snap.size();
            st.bytes["C.snapshot2"] += snap2.size();
            if (keep) keep->shard = to_string_bytes(snap2);
            if (reuse) timed(st, "C.cleanup", [] {});
            else timed(st, "C.cleanup", [&] { r.drop(); Bytes().swap(r.buf); Bytes().swap(snap); Bytes().swap(snap2); });
            {
                // Untimed copy of the current shard for the middle-erase stress item.
                Bytes tmp;
                Held<cm::ShardState> m;
                m.receive(save(tmp, *shard_));
                timed(st, "S.ticks_middle", [&] { shard_ticks_middle_erase(*m, mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            }
        }

        // D: read-only battle reports (setup untimed). D.read receives like every other
        // message (copy into an owned buffer + checked deserialize); D.read_direct
        // deserializes the wire itself as const (same checks, no copy), like
        // FlatBuffers' Verifier + in-place read.
        std::vector<Bytes> wires;
        wires.reserve(reports.size());
        for (const auto& rep : reports) {
            cm::Report r;
            copy_report(r, rep);
            save(wires.emplace_back(), r);
        }
        for (const auto& w : wires) st.bytes["D.report"] += w.size();
        if (keep && !wires.empty()) keep->report = to_string_bytes(wires.back());
        for (int q = 0; q < sc.report_reads && !wires.empty(); ++q) {
            const Bytes& w = wires[std::size_t(q) % wires.size()];
            if (reuse) {
                recv(st, "D.read", d_, w);
                timed(st, "D.read", [&] { st.report_sum += query_report(*d_); });
            } else {
                timed(st, "D.read", [&] {
                    Held<cm::Report> d;
                    d.receive({w.data(), w.size()});
                    st.report_sum += query_report(*d);
                });
            }
            timed(st, "D.read_direct", [&] {
                const cm::Report* r = cista::deserialize<cm::Report, Mode>(w.data(), w.data() + w.size());
                st.report_direct_sum += query_report(*r);
            });
        }
    }

    uint64_t world_digest() override {
        Digest g;
        for (auto& p : players_) dig_player(g, *p);
        dig_shard(g, *shard_);
        return g.h;
    }

private:
    static void build_request(cm::BattleMsg& msg, const Pairing& pr, const cm::Army& att, const cm::Army& def) {
        msg.battle_id = pr.battle_id; msg.seed = pr.seed; msg.winner = 0;
        copy_army(msg.attacker, att);
        copy_army(msg.defender, def);
        msg.report.battle_id = 0;
        msg.report.summary = std::string_view{};
        msg.report.events.clear();
    }
    cm::PlayerState& player(int i) { return *players_[std::size_t(i)]; }

    // Receive. Default: copy the wire into the owned buffer and deserialize, all timed.
    // Direct: the transport writes into the owned buffer (untimed: the network read,
    // which every format pays outside the timer), then the checked deserialize runs in
    // place (timed), like xoffset_direct's begin_verified_receive / finish_verified.
    template <class T> static void recv(CycleStats& st, const char* key, Held<T>& h, const Bytes& in) {
        if constexpr (!Direct) { timed(st, key, [&] { h.receive({in.data(), in.size()}); }); return; }
        timed(st, key, [&] { h.drop(); });
        h.buf.assign(in.begin(), in.end());
        timed(st, key, [&] { h.p = cista::deserialize<T, Mode>(h.buf); });
    }

    const Scale* sc_ = nullptr;
    uint64_t seed_ = 0;
    std::vector<Held<cm::PlayerState>> players_;
    std::vector<int> armies_;
    Held<cm::ShardState> shard_;
    // Reuse protocol state: one request object kept on the heap (its vectors keep their
    // capacity), one receive buffer per slot, one output buffer per slot.
    cm::BattleMsg a_req_;
    Held<cm::BattleMsg> a_m_, a_r_;
    Held<cm::PlayerState> b_m_, b_r_;
    Held<cm::ShardState> c_r_;
    Held<cm::Report> d_;
    Bytes req_, res_, out_, back_, snap_, snap2_;
};

} // namespace slg
