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
#include "slg/login.hpp"
#include "slg/view.hpp"
#include "slg/uplink.hpp"

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
        if (demo_cases()) {
            const ViewScale& vs = *find_view_scale(sc.name);
            for (int i = 0; i < ViewCases::count; ++i) {
                bs_[i].setup(seed, vs, i);
                ops_[i].setup(vs.clients);
            }
            lops_.srv.clear();
            for (int i = 0; i < sc.migrants; ++i) {
                cm::PlayerState h;
                copy_player(h, make_player(mix_seed(seed, 0x1061), i + 1));
                lops_.srv.emplace_back().receive(save(tmp, h));
            }
            lops_.hold = std::vector<Held<cm::PlayerState>>(lops_.srv.size());
            sops_.r = this;
        }
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
            untimed(st, "A.simulate", [&] { simulate_battle(*m); });
            timed(st, "A.res_encode", [&] { save(res, *m); });
            timed(st, "A.res_decode", [&] { prefetch_army_for_apply(att); prefetch_army_for_apply(def); });
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
        // Demo case set, B: one hop between two scene servers; the received buffer
        // becomes the receiver's resident player and is played on in place.
        if (demo_cases()) for (std::size_t k = 0; k < migrants.size(); ++k) {
            if (!reuse) throw std::runtime_error("SLG_CASES=demo needs SLG_REUSE=1");
            auto& slot = players_[std::size_t(migrants[k])];
            if (xport_on()) {   // W3 step 0: the transport model; cista reads the socket into its held buffer
                timed(st, "B.send+T", [&] { save(out_, *slot); xport_put(out_); });
                timed(st, "B.recv+T", [&] { b_m_.receive(xport_view()); });
            } else {
            timed(st, "B.out_encode", [&] { save(out_, *slot); });
            recv(st, "B.in_decode", b_m_, out_);
            }
            untimed(st, "B.scene_edits", [&] { season_edits(*b_m_, mix_seed(seed_, uint64_t(cycle) * 1000003u + k), sc.season_ops); });
            timed(st, "B.install", [&] { swap(slot, b_m_); });
            st.bytes["B.out"] += out_.size();
            if (keep) keep->player = to_string_bytes(out_);
        }
        else for (std::size_t k = 0; k < migrants.size(); ++k) {
            auto& slot = players_[std::size_t(migrants[k])];
            Bytes out_local, back_local;
            Held<cm::PlayerState> m_local, r_local;
            Bytes& out = reuse ? out_ : out_local;
            Bytes& back = reuse ? back_ : back_local;
            Held<cm::PlayerState>& m = reuse ? b_m_ : m_local;
            Held<cm::PlayerState>& r = reuse ? b_r_ : r_local;
            timed(st, "B.out_encode", [&] { save(out, *slot); });
            recv(st, "B.in_decode", m, out);
            untimed(st, "B.season_edits", [&] { season_edits(*m, mix_seed(seed_, uint64_t(cycle) * 1000003u + k), sc.season_ops); });
            timed(st, "B.back_encode", [&] { save(back, *m); });
            recv(st, "B.back_decode", r, back);
            timed(st, "B.writeback", [&] { swap(slot, r); });
            st.bytes["B.out"] += out.size();
            st.bytes["B.back"] += back.size();
            if (keep) keep->player = to_string_bytes(back);
            if (reuse) timed(st, "B.cleanup", [] {});
            else timed(st, "B.cleanup", [&] { m.drop(); r.drop(); Bytes().swap(m.buf); Bytes().swap(r.buf); Bytes().swap(out); Bytes().swap(back); });
        }

        // Demo case set, S: scene <-> storage for players (see std_runner.hpp).
        if (demo_cases()) for (int p : pick_storage(seed_, cycle, sc.players, sc.migrants)) {
            auto& slot = players_[std::size_t(p)];
            if (xport_on()) {   // W3 step 0: the transport model; the storage's resend of its bytes is untimed
                timed(st, "S.send+T", [&] { save(back_, *slot); xport_put(back_); });
                timed(st, "S.recv+T", [&] { b_r_.receive(xport_view()); });
                store_ = back_;
                xport_put(store_);
                timed(st, "S.recv+T", [&] { b_m_.receive(xport_view()); });
            } else {
            timed(st, "S.save_encode", [&] { save(back_, *slot); });
            recv(st, "S.store_recv", b_r_, back_);
            store_ = back_;
            recv(st, "S.load_recv", b_m_, store_);
            }
            timed(st, "S.load_install", [&] { swap(slot, b_m_); });
            st.bytes["S.player"] += back_.size();
        }
        if (demo_cases()) run_dirty_save(sops_, pick_storage(seed_, cycle, sc.players, sc.migrants), cycle, st);

        // C: map shard snapshot, restore, ticks, snapshot again (not in the demo case set).
        if (!demo_cases()) {
            Bytes snap_local, snap2_local;
            Held<cm::ShardState> r_local;
            Bytes& snap = reuse ? snap_ : snap_local;
            Bytes& snap2 = reuse ? snap2_ : snap2_local;
            Held<cm::ShardState>& r = reuse ? c_r_ : r_local;
            timed(st, "C.snapshot", [&] { save(snap, *shard_); });
            recv(st, "C.restore", r, snap);
            untimed(st, "C.ticks", [&] { shard_ticks(*r, mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
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
                untimed(st, "S.ticks_middle", [&] { shard_ticks_middle_erase(*m, mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            }
        }

        // D: read-only battle reports (setup untimed). D.read receives like every other
        // message (copy into an owned buffer + checked deserialize); D.read_direct
        // deserializes the wire itself as const (same checks, no copy), like
        // FlatBuffers' Verifier + in-place read.
        if (demo_cases()) { run_view(cycle, st, keep); return; }
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
                // One timer around receive + query (see xoffset_runner.hpp); Direct
                // times the two parts separately around the untimed copy.
                if constexpr (!Direct) {
                    timed(st, "D.read", [&] {
                        d_.receive({w.data(), w.size()});
                        st.report_sum += query_report(*d_);
                    });
                } else {
                    recv(st, "D.read", d_, w);
                    timed(st, "D.read", [&] { st.report_sum += query_report(*d_); });
                }
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

    // Demo case set, server -> client map data (view.hpp, run_view_case): the server
    // fills its reused message object and serializes it into the send
    // buffer; a client copies the wire into its held view's buffer, deserializes
    // it there with the runner's checks, reads it in place and keeps it, changing
    // it in place when deltas arrive.
    void run_view(int cycle, CycleStats& st, SampleWires* keep) {
        for (int i = 0; i < ViewCases::count; ++i)
            run_view_case(bs_[i], ops_[i], ViewCases::keys(i), cycle, st, keep ? &keep->view : nullptr, keep ? &keep->delta : nullptr);
        run_login(lops_, mix_seed(seed_, 0x1062), cycle, 4, st, lseq_);
        run_uplink(lops_, mix_seed(seed_, 0x1063), cycle, 4, st);
    }

    // L / LD (login.hpp): players live in cista buffers; a client copies the wire
    // into its held player's buffer, deserializes it there with the runner's checks
    // and changes it in place.
    struct LoginOps {
        std::vector<Held<cm::PlayerState>> srv, hold;
        std::vector<Held<cm::PlayerState>> hw = std::vector<Held<cm::PlayerState>>(kWarmDevices);   // warm devices' players (LDW)
        cm::PlayerPatch pm;
        Held<cm::PlayerPatch> pr;
        Bytes lw, pw;
        static std::string_view sv(const Bytes& b) { return {reinterpret_cast<const char*>(b.data()), b.size()}; }
        std::string_view login_wire() const { return sv(lw); }
        std::string_view patch_wire() const { return sv(pw); }
        void login_recv_w(int d, std::string_view w) { hw[std::size_t(d)].receive({reinterpret_cast<const uint8_t*>(w.data()), w.size()}); }
        cm::PlayerState& held_w(int d) { return *hw[std::size_t(d)]; }
        void patch_recv_from(const std::string& w) { pr.receive({reinterpret_cast<const uint8_t*>(w.data()), w.size()}); }
        int online() const { return int(srv.size()); }
        cm::PlayerState& server(int p) { return *srv[std::size_t(p)]; }
        std::size_t login_encode(int p) { return save(lw, *srv[std::size_t(p)]).size(); }
        void login_recv(int p) { hold[std::size_t(p)].receive({lw.data(), lw.size()}); }
        std::size_t login_send_x(int p) { save(lw, *srv[std::size_t(p)]); xport_put(lw); return lw.size(); }
        void login_recv_x(int p) { hold[std::size_t(p)].receive(xport_view()); }
        cm::PlayerState& held(int p) { return *hold[std::size_t(p)]; }
        template <class Fill> std::size_t patch_encode(Fill&& fill) { fill(pm); return save(pw, pm).size(); }
        void patch_recv() { pr.receive({pw.data(), pw.size()}); }
        const cm::PlayerPatch& patch() const { return *pr; }
        // UL (uplink.hpp): the client saves a march order, the server receives it (the format's mode).
        cm::March um;
        Held<cm::March> ur;
        Bytes uw;
        template <class Fill> std::size_t up_encode(Fill&& fill) { fill(um); return save(uw, um).size(); }
        uint64_t up_recv() { ur.receive({uw.data(), uw.size()}); return query_order(*ur); }
        std::string_view up_wire() const { return sv(uw); }
    };
    struct StoreOps {
        CistaRunner* r = nullptr;
        cm::PlayerPatch pm;
        Held<cm::PlayerPatch> pr;
        Bytes w;
        cm::PlayerState& player(int p) { return *r->players_[std::size_t(p)]; }
        template <class Fill> std::size_t modules_encode(Fill&& fill) { fill(pm); return save(w, pm).size(); }
        void modules_recv() { pr.receive({w.data(), w.size()}); }
        template <class Fill> std::size_t modules_send_x(Fill&& fill) { fill(pm); save(w, pm); xport_put(w); return w.size(); }
        void modules_recv_x() { pr.receive(xport_view()); }
    };

    struct BlockOps {
        std::vector<Bytes> snaps;
        std::vector<Held<cm::ViewSnapshot>> held_views;
        cm::ViewSnapshot ev;
        cm::ViewDelta ds;
        Held<cm::ViewDelta> dd;
        Bytes dw;
        void setup(int clients) {
            snaps.assign(1, {});
            held_views = std::vector<Held<cm::ViewSnapshot>>(std::size_t(clients + kWarmDevices));   // [clients + d]: warm device d (EBW, U*W)
        }
        template <class Fill> std::size_t snap_encode(int b, Fill&& fill) { fill(ev); return save(snaps[std::size_t(b)], ev).size(); }
        std::string_view snap_wire(int b) const {
            const auto& v = snaps[std::size_t(b)];
            return {reinterpret_cast<const char*>(v.data()), v.size()};
        }
        void snap_recv(int c, int k, int b) {
            const auto& v = snaps[std::size_t(b)];
            held_views[std::size_t(c) + std::size_t(k)].receive({v.data(), v.size()});
        }
        cm::ViewSnapshot& held(int c, int k) { return *held_views[std::size_t(c) + std::size_t(k)]; }
        template <class Fill> std::size_t delta_encode(Fill&& fill) { fill(ds); return save(dw, ds).size(); }
        std::string_view delta_wire() const { return {reinterpret_cast<const char*>(dw.data()), dw.size()}; }
        void delta_recv() { dd.receive({dw.data(), dw.size()}); }
        void delta_recv_from(const std::string& w) { dd.receive({reinterpret_cast<const uint8_t*>(w.data()), w.size()}); }
        const cm::ViewDelta& delta() const { return *dd; }
    };

    // A cista player is its deserialized buffer (plus what a mutation moved to the heap).
    void memory(CycleStats& st) override {
        std::size_t b = 0;
        for (const auto& p : players_) b += p.buf.capacity() + sizeof(p);
        if (!players_.empty()) st.bytes["MEM.player"] = b / players_.size();
        for (int i = 0; i < ViewCases::count; ++i) view_memory(bs_[i], ViewCases::keys(i), st);
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
        msg.attacker = att;  // T4 adapter (c): cista's own copy assignment
        msg.defender = def;
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
    Bytes store_;
    ViewSession bs_[ViewCases::count];
    BlockOps ops_[ViewCases::count];
    LoginOps lops_;
    StoreOps sops_;
    std::vector<int64_t> lseq_;
};

} // namespace slg
