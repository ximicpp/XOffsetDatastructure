// SLG prototype: runner for XOffset V2. Each player and the map shard live in their own
// XBuffer; messages are built in, saved from and loaded into arenas, and the game logic
// runs in place on the received arena. Requires P2996 Clang.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "runner.hpp"
#include "slg/generic.hpp"
#include "slg/model_xoffset.hpp"
#include "slg/model_xoffset_optin.hpp"
#include "slg/trace.hpp"
#include "slg/login.hpp"
#include "slg/view.hpp"
#include "xoffset_receiver.hpp"

namespace slg {

struct XoModel {
    static constexpr const char* name = "xoffset";
    using PlayerState = xm::PlayerState; using BattleMsg = xm::BattleMsg;
    using ShardState = xm::ShardState; using Report = xm::Report;
};
// Trust tier (3.4 P1, #228; susu 07:17Z 11-04): the same model, every reuse-protocol
// receive of a whole wire through XReceiver with XCheck::admit (server <-> server:
// XLink::server_peer; server -> client: XLink::from_server) instead of
// reload_verified. Delta and native receives are unchanged until P2.
struct XoTrustedModel : XoModel {
    static constexpr const char* name = "xoffset_trusted";
    static constexpr bool trusted = true;
};
struct XoOptinModel {
    static constexpr const char* name = "xoffset_optin";
    using PlayerState = xmr::PlayerState; using BattleMsg = xmr::BattleMsg;
    using ShardState = xmr::ShardState; using Report = xmr::Report;
};

// M: the model (XoModel = model_xoffset.hpp, XoOptinModel = model_xoffset_optin.hpp).
// Direct: receive with begin_verified_receive / finish_verified (reuse protocol only).
template <class M, bool Direct> class XOffsetRunnerT final : public Runner {
    using PlayerState = typename M::PlayerState;
    using BattleMsg = typename M::BattleMsg;
    using ShardState = typename M::ShardState;
    using Report = typename M::Report;

    static constexpr bool Trusted = requires { M::trusted; };
    // A whole-wire receive into a reused buffer: reload_verified, or (Trusted) the
    // receiver for link L with its default check (admit for server_peer / from_server).
    template <class T, XOffsetDatastructure::XLink L, class W>
    static void reload_wire(XOffsetDatastructure::XBuffer& b, const W& w) {
        using namespace XOffsetDatastructure;
        if constexpr (Trusted) {
            static XReceiver<L> rx(XBuffer::maximum_capacity(XBuffer::DEFAULT_MAXIMUM_CAPACITY));
            const XStatus r = rx.template reload<T>(b, w);
            if (!r) throw std::runtime_error("trusted receive rejected: " + std::string(r.error().message));
        } else {
            b.template reload_verified<T>(w);
        }
    }
    static constexpr auto kPeer = XOffsetDatastructure::XLink::server_peer;
    static constexpr auto kClient = XOffsetDatastructure::XLink::from_server;

public:
    std::string name() const override { return Direct ? "xoffset_direct" : M::name; }

    void setup(const Scale& sc, uint64_t seed) override {
        using namespace XOffsetDatastructure;
        sc_ = &sc; seed_ = seed;
        players_.clear();
        armies_.clear();
        for (int i = 0; i < sc.players; ++i) {
            const stdm::PlayerState src = make_player(seed, i + 1);
            XBuffer b = XBuffer::create<PlayerState>(kPlayerInitial, cap());
            copy_player(b.root<PlayerState>(), src);
            armies_.push_back(int(src.armies.size()));
            players_.push_back(std::move(b));
        }
        const stdm::ShardState src = make_shard(seed, sc);
        shard_.emplace(XBuffer::create<ShardState>(kShardInitial, cap(true)));
        copy_shard(shard_->root<ShardState>(), src);
        if (reuse_mode()) {
            a_req_.emplace(XBuffer::create<BattleMsg>(kBattleInitial, cap()));
            a_m_.emplace(XBuffer::create<BattleMsg>(kBattleInitial, cap()));
            a_r_.emplace(XBuffer::create<BattleMsg>(kBattleInitial, cap()));
            b_m_.emplace(XBuffer::create<PlayerState>(kPlayerInitial, cap()));
            b_r_.emplace(XBuffer::create<PlayerState>(kPlayerInitial, cap()));
            c_r_.emplace(XBuffer::create<ShardState>(kShardInitial, cap(true)));
            d_.emplace(XBuffer::create<Report>(kReportInitial, cap()));
            if (demo_cases()) {
                const ViewScale& vs = *find_view_scale(sc.name);
                for (int k = 0; k < ViewCases::count; ++k) {
                    bs_[k].setup(seed, vs, k);
                    ops_[k].setup(vs.clients, vs.units, vs.marches);
                }
                lops_.srv.clear();
                for (int i = 0; i < sc.migrants; ++i) {
                    XBuffer b = XBuffer::create<PlayerState>(kPlayerInitial, cap());
                    copy_player(b.root<PlayerState>(), make_player(mix_seed(seed, 0x1061), i + 1));
                    lops_.srv.push_back(std::move(b));
                }
                lops_.setup();
                sops_.r = this;
            }
        }
    }

    void run_cycle(int cycle, CycleStats& st, SampleWires* keep) override {
        if (reuse_mode()) return run_cycle_reuse(cycle, st, keep);
        if (Direct) throw std::runtime_error("xoffset_direct runs only with SLG_REUSE=1");
        using namespace XOffsetDatastructure;
        const Scale& sc = *sc_;
        std::vector<stdm::Report> reports;
        reports.reserve(std::size_t(sc.battles));

        // A: battle server round trip. The battle server changes the received arena in place.
        for (const Pairing& pr : make_pairings(seed_, cycle, sc, armies_)) {
            auto& att = player(pr.ap).armies[std::size_t(pr.aa)];
            auto& def = player(pr.dp).armies[std::size_t(pr.da)];
            std::string req, res;
            std::optional<XBuffer> m, r;
            timed(st, "A.req_encode", [&] {
                XBuffer b = XBuffer::create<BattleMsg>(kBattleInitial, cap());
                auto& msg = b.root<BattleMsg>();
                msg.battle_id = pr.battle_id; msg.seed = pr.seed; msg.winner = 0;
#if defined(XOFFSET_SAVE_COMPOSED_API)
                // Adapter v3: the request is composed from the two armies straight
                // into the wire (tight save_compact layout); the draft holds only the root.
                b.save_compact_into<BattleMsg>(req, XCompactOptions{.slack_div = 0},
                                               xassign(msg.attacker, att), xassign(msg.defender, def));
#else
                msg.attacker = att;  // T4 adapter (c): public copy assignment
                msg.defender = def;
                req = b.save_verified<BattleMsg>();
#endif
            });
            timed(st, "A.req_decode", [&] { m.emplace(XBuffer::load_verified<BattleMsg>(req, cap())); });
            untimed(st, "A.simulate", [&] { simulate_battle(m->root<BattleMsg>()); });
            timed(st, "A.res_encode", [&] { res = m->save_verified<BattleMsg>(); });
            timed(st, "A.res_decode", [&] {
                prefetch_army_for_apply(att); prefetch_army_for_apply(def);
                r.emplace(XBuffer::load_verified<BattleMsg>(res, cap()));
            });
            timed(st, "A.apply", [&] {
                const auto& out = r->root<BattleMsg>();
                apply_battle_army(att, out.attacker);
                apply_battle_army(def, out.defender);
            });
            st.bytes["A.req"] += req.size();
            st.bytes["A.res"] += res.size();
            copy_report(reports.emplace_back(), r->root<BattleMsg>().report);
            if (keep) { keep->req = req; keep->res = res; }
            timed(st, "A.cleanup", [&] { m.reset(); r.reset(); std::string().swap(req); std::string().swap(res); });
        }

        // B: cross-server season migration and write-back.
        const auto migrants = pick_migrants(seed_, cycle, sc);
        for (std::size_t k = 0; k < migrants.size(); ++k) {
            XBuffer& slot = players_[std::size_t(migrants[k])];
            std::string out, back;
            std::optional<XBuffer> m, r;
            timed(st, "B.out_encode", [&] { out = slot.save_verified<PlayerState>(); });
            timed(st, "B.in_decode", [&] { m.emplace(XBuffer::load_verified<PlayerState>(out, cap())); });
            untimed(st, "B.season_edits", [&] {
                season_edits(m->root<PlayerState>(), mix_seed(seed_, uint64_t(cycle) * 1000003u + k), sc.season_ops);
            });
            timed(st, "B.back_encode", [&] { back = m->save_verified<PlayerState>(); });
            timed(st, "B.back_decode", [&] { r.emplace(XBuffer::load_verified<PlayerState>(back, cap())); });
            timed(st, "B.writeback", [&] { slot = std::move(*r); });
            st.bytes["B.out"] += out.size();
            st.bytes["B.back"] += back.size();
            if (keep) keep->player = back;
            timed(st, "B.cleanup", [&] { m.reset(); r.reset(); std::string().swap(out); std::string().swap(back); });
        }

        // C: map shard snapshot, restore, ticks, snapshot again.
        {
            std::string snap, snap2;
            std::optional<XBuffer> r;
            timed(st, "C.snapshot", [&] { snap = shard_->save_verified<ShardState>(); });
            timed(st, "C.restore", [&] { r.emplace(XBuffer::load_verified<ShardState>(snap, cap(true))); });
            untimed(st, "C.ticks", [&] { shard_ticks(r->root<ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            timed(st, "C.snapshot2", [&] { snap2 = r->save_verified<ShardState>(); });
            timed(st, "C.replace", [&] { *shard_ = std::move(*r); });
            st.bytes["C.snapshot"] += snap.size();
            st.bytes["C.snapshot2"] += snap2.size();
            if (keep) keep->shard = snap2;
            timed(st, "C.cleanup", [&] { r.reset(); std::string().swap(snap); std::string().swap(snap2); });
            {
                // Untimed copy of the current shard for the middle-erase stress item.
                auto m = XOffsetDatastructure::XBuffer::load_verified<ShardState>(shard_->save_verified<ShardState>(), cap(true));
                untimed(st, "S.ticks_middle", [&] { shard_ticks_middle_erase(m.template root<ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            }
        }

        // D: read-only battle reports (setup untimed).
        std::vector<std::string> wires;
        wires.reserve(reports.size());
        for (const auto& rep : reports) {
            XBuffer b = XBuffer::create<Report>(kReportInitial, cap());
            copy_report(b.root<Report>(), rep);
            wires.push_back(b.save_verified<Report>());
        }
        for (const auto& w : wires) st.bytes["D.report"] += w.size();
        if (keep && !wires.empty()) keep->report = wires.back();
        for (int q = 0; q < sc.report_reads && !wires.empty(); ++q) {
            const std::string& w = wires[std::size_t(q) % wires.size()];
            timed(st, "D.read", [&] {
                XBuffer b = use_pool()
                    ? XBuffer::load_verified<Report>(w, XBuffer::maximum_capacity(XBuffer::DEFAULT_MAXIMUM_CAPACITY).with_pool(pool()))
                    : XBuffer::load_verified<Report>(w);
                st.report_sum += query_report(b.root<Report>());
            });
        }
    }

    // Reuse protocol: long-lived buffers per message slot. Senders reset<T>()
    // and save_verified_into a reused string; receivers reload_verified (the
    // same admission and verification as load_verified); write-back and
    // replace swap the received buffer in.
    void run_cycle_reuse(int cycle, CycleStats& st, SampleWires* keep) {
        using namespace XOffsetDatastructure;
        const Scale& sc = *sc_;
        std::vector<stdm::Report> reports;
        reports.reserve(std::size_t(sc.battles));

        for (const Pairing& pr : make_pairings(seed_, cycle, sc, armies_)) {
            auto& att = player(pr.ap).armies[std::size_t(pr.aa)];
            auto& def = player(pr.dp).armies[std::size_t(pr.da)];
            timed(st, "A.req_encode", [&] {
#if defined(XOFFSET_SAVE_COMPOSED_API)
                // Adapter v3 (see above): a_req_'s armies stay empty, nothing is reset.
                auto& msg = a_req_->root<BattleMsg>();
                msg.battle_id = pr.battle_id; msg.seed = pr.seed; msg.winner = 0;
                a_req_->save_compact_into<BattleMsg>(req_, XCompactOptions{.slack_div = 0},
                                                     xassign(msg.attacker, att), xassign(msg.defender, def));
#else
                auto& msg = a_req_->reset<BattleMsg>();
                msg.battle_id = pr.battle_id; msg.seed = pr.seed; msg.winner = 0;
                msg.attacker = att;  // T4 adapter (c): public copy assignment
                msg.defender = def;
                a_req_->save_verified_into<BattleMsg>(req_);
#endif
            });
            recv<BattleMsg>(st, "A.req_decode", req_, a_m_);
            untimed(st, "A.simulate", [&] { simulate_battle(a_m_->root<BattleMsg>()); });
            timed(st, "A.res_encode", [&] { a_m_->save_verified_into<BattleMsg>(res_); });
            timed(st, "A.res_decode", [&] { prefetch_army_for_apply(att); prefetch_army_for_apply(def); });
            recv<BattleMsg>(st, "A.res_decode", res_, a_r_);
            timed(st, "A.apply", [&] {
                const auto& out = a_r_->root<BattleMsg>();
                apply_battle_army(att, out.attacker);
                apply_battle_army(def, out.defender);
            });
            st.bytes["A.req"] += req_.size();
            st.bytes["A.res"] += res_.size();
            copy_report(reports.emplace_back(), a_r_->root<BattleMsg>().report);
            if (keep) { keep->req = req_; keep->res = res_; }
            timed(st, "A.cleanup", [] {});
        }

        const auto migrants = pick_migrants(seed_, cycle, sc);
        // Demo case set, B: one hop between two scene servers. The received buffer
        // becomes the receiver's resident player and is played on in place; the next
        // hop sends it again as it is (no return trip).
        if (demo_cases()) for (std::size_t k = 0; k < migrants.size(); ++k) {
            XBuffer& slot = players_[std::size_t(migrants[k])];
            timed(st, "B.out_encode", [&] { slot.save_verified_into<PlayerState>(out_); });
            recv<PlayerState>(st, "B.in_decode", out_, b_m_);
            untimed(st, "B.scene_edits", [&] {
                season_edits(b_m_->root<PlayerState>(), mix_seed(seed_, uint64_t(cycle) * 1000003u + k), sc.season_ops);
            });
            timed(st, "B.install", [&] { slot.swap(*b_m_); });
            st.bytes["B.out"] += out_.size();
            if (keep) keep->player = out_;
        }
        else for (std::size_t k = 0; k < migrants.size(); ++k) {
            XBuffer& slot = players_[std::size_t(migrants[k])];
            timed(st, "B.out_encode", [&] { slot.save_verified_into<PlayerState>(out_); });
            recv<PlayerState>(st, "B.in_decode", out_, b_m_);
            untimed(st, "B.season_edits", [&] {
                season_edits(b_m_->root<PlayerState>(), mix_seed(seed_, uint64_t(cycle) * 1000003u + k), sc.season_ops);
            });
            timed(st, "B.back_encode", [&] { b_m_->save_verified_into<PlayerState>(back_); });
            recv<PlayerState>(st, "B.back_decode", back_, b_r_);
            timed(st, "B.writeback", [&] { std::swap(slot, *b_r_); });
            st.bytes["B.out"] += out_.size();
            st.bytes["B.back"] += back_.size();
            if (keep) keep->player = back_;
            timed(st, "B.cleanup", [] {});
        }

        // Demo case set, S: scene <-> storage for players (see std_runner.hpp).
        if (demo_cases()) for (int p : pick_storage(seed_, cycle, sc.players, sc.migrants)) {
            XBuffer& slot = players_[std::size_t(p)];
            timed(st, "S.save_encode", [&] { slot.save_verified_into<PlayerState>(back_); });
            recv<PlayerState>(st, "S.store_recv", back_, b_r_);
            store_ = back_;
            recv<PlayerState>(st, "S.load_recv", store_, b_m_);
            timed(st, "S.load_install", [&] { slot.swap(*b_m_); });
            st.bytes["S.player"] += back_.size();
        }
        if (demo_cases()) run_dirty_save(sops_, pick_storage(seed_, cycle, sc.players, sc.migrants), cycle, st);

        if (!demo_cases()) {  // the demo case set has no shard snapshot (susu 2026-11-02)
            timed(st, "C.snapshot", [&] { shard_->save_verified_into<ShardState>(snap_); });
            recv<ShardState>(st, "C.restore", snap_, c_r_);
            untimed(st, "C.ticks", [&] { shard_ticks(c_r_->root<ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            timed(st, "C.snapshot2", [&] { c_r_->save_verified_into<ShardState>(snap2_); });
            timed(st, "C.replace", [&] { std::swap(*shard_, *c_r_); });
            st.bytes["C.snapshot"] += snap_.size();
            st.bytes["C.snapshot2"] += snap2_.size();
            if (keep) keep->shard = snap2_;
            timed(st, "C.cleanup", [] {});
            {
                // Untimed copy of the current shard for the middle-erase stress item.
                auto m = XOffsetDatastructure::XBuffer::load_verified<ShardState>(shard_->save_verified<ShardState>(), cap(true));
                untimed(st, "S.ticks_middle", [&] { shard_ticks_middle_erase(m.template root<ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            }
        }

        if (demo_cases()) { run_view(cycle, st, keep); return; }
        std::vector<std::string> wires;
        wires.reserve(reports.size());
        for (const auto& rep : reports) {
            XBuffer b = XBuffer::create<Report>(kReportInitial, cap());
            copy_report(b.root<Report>(), rep);
            wires.push_back(b.save_verified<Report>());
        }
        for (const auto& w : wires) st.bytes["D.report"] += w.size();
        if (keep && !wires.empty()) keep->report = wires.back();
        for (int q = 0; q < sc.report_reads && !wires.empty(); ++q) {
            const std::string& w = wires[std::size_t(q) % wires.size()];
            // One timer around receive + query, like the std formats (a second timer
            // per read costs about 10% of D). Direct needs the untimed transport write
            // in between, so it times the two parts separately.
            if constexpr (!Direct) {
                timed(st, "D.read", [&] {
                    reload_wire<Report, kClient>(*d_, w);
                    st.report_sum += query_report(d_->root<Report>());
                });
            } else {
                recv<Report>(st, "D.read", w, d_);
                timed(st, "D.read", [&] { st.report_sum += query_report(d_->root<Report>()); });
            }
        }
    }

    // Demo case set, server -> client map data (view.hpp, run_view_case). The server
    // resets its reused message buffer, fills it from the world and saves it into
    // the send buffer; a client reloads it (full verification) into its
    // held view's buffer, reads it in place and keeps it; deltas change it in
    // place.
    void run_view(int cycle, CycleStats& st, SampleWires* keep) {
        for (int k = 0; k < ViewCases::count; ++k)
            run_view_case(bs_[k], ops_[k], ViewCases::keys(k), cycle, st, keep ? &keep->view : nullptr, keep ? &keep->delta : nullptr);
        run_login(lops_, mix_seed(seed_, 0x1062), cycle, 4, st, lseq_);
    }

    // L / LD (login.hpp): the server's players are resident XBuffers; a client
    // reloads the login wire into the XBuffer it keeps and changes it in place.
    // Native (LK / LN): one baseline per online client over the server's player.
    struct LoginOps {
        using XBuffer = XOffsetDatastructure::XBuffer;
        std::vector<XBuffer> srv;
        std::vector<std::optional<XBuffer>> hold;
        std::optional<XBuffer> pm, pr;
        std::string lw, pw;
        void setup() {
            hold.clear(); hold.resize(srv.size());
            pm.emplace(XBuffer::create<xm::PlayerPatch>(kViewInitial, cap()));
            pr.emplace(XBuffer::create<xm::PlayerPatch>(kViewInitial, cap()));
            for (auto& h : hw) h.reset();
#ifdef XOFFSET_DELTA_SYNC_API
            for (auto& h : nhw) h.reset();
            ncw.assign(kWarmDevices, {});
            nb.assign(srv.size(), {});
            nc.assign(srv.size(), {});
            nh.clear(); nh.resize(srv.size());
#endif
        }
        int online() const { return int(srv.size()); }
        PlayerState& server(int p) { return srv[std::size_t(p)].template root<PlayerState>(); }
        std::size_t login_encode(int p) { srv[std::size_t(p)].template save_verified_into<PlayerState>(lw); return lw.size(); }
        void login_recv(int p) {
            auto& h = hold[std::size_t(p)];
            if (!h) h.emplace(XBuffer::create<PlayerState>(kPlayerInitial, cap()));
            reload_wire<PlayerState, kClient>(*h, lw);
        }
        PlayerState& held(int p) { return hold[std::size_t(p)]->template root<PlayerState>(); }
        template <class Fill> std::size_t patch_encode(Fill&& fill) {
            fill(pm->template reset<xm::PlayerPatch>());
            pm->template save_verified_into<xm::PlayerPatch>(pw);
            return pw.size();
        }
        void patch_recv() { reload_wire<xm::PlayerPatch, kClient>(*pr, pw); }
        // Scoring V3: wires for the zstd keys; the warm single device's player (LDW, LNW).
        std::vector<std::optional<XBuffer>> hw = std::vector<std::optional<XBuffer>>(kWarmDevices);
        std::string_view login_wire() const { return lw; }
        std::string_view patch_wire() const { return pw; }
        void login_recv_w(int d, std::string_view w) {
            auto& h = hw[std::size_t(d)];
            if (!h) h.emplace(XBuffer::create<PlayerState>(kPlayerInitial, cap()));
            reload_wire<PlayerState, kClient>(*h, std::as_bytes(std::span<const char>(w.data(), w.size())));
        }
        PlayerState& held_w(int d) { return hw[std::size_t(d)]->template root<PlayerState>(); }
        void patch_recv_from(const std::string& w) { reload_wire<xm::PlayerPatch, kClient>(*pr, w); }
        const xm::PlayerPatch& patch() { return pr->template root<xm::PlayerPatch>(); }
#ifdef XOFFSET_DELTA_SYNC_API
        std::vector<XOffsetDatastructure::XDeltaBaseline> nb;
        std::vector<XOffsetDatastructure::XDeltaCursor> nc;
        std::vector<std::optional<XBuffer>> nh;
        std::string nd;
        std::size_t n_login_key(int p) {
            nb[std::size_t(p)].reset();
            srv[std::size_t(p)].template save_delta_into<PlayerState>(nd, nb[std::size_t(p)]);
            return nd.size();
        }
        void n_login_recv(int p) {
            auto& h = nh[std::size_t(p)];
            if (!h) h.emplace(XBuffer::create<PlayerState>(kPlayerInitial, cap()));
            const XOffsetDatastructure::XStatus r = h->template try_apply_delta_verified<PlayerState>(nd, nc[std::size_t(p)]);
            if (!r) throw std::runtime_error("native login rejected: " + std::string(r.error().message));
        }
        std::size_t n_diff(int p) { srv[std::size_t(p)].template save_delta_into<PlayerState>(nd, nb[std::size_t(p)]); return nd.size(); }
        // The edit's dirty marks as touches; the delta compares only what was touched
        // (SLG_NO_TOUCH=1: compare everything, as before).
        static bool touch_on() { static const bool on = std::getenv("SLG_NO_TOUCH") == nullptr; return on; }
        template <class T> void n_touch(int p, const T& obj) { if (touch_on()) srv[std::size_t(p)].touch(obj); }
        template <class D> std::size_t n_diff_dirty(int p, const D&) {
            if (!touch_on()) return n_diff(p);
            auto& b = srv[std::size_t(p)];
            XOffsetDatastructure::XDeltaOptions o;
            o.touched_only = true;
            b.template save_delta_into<PlayerState>(nd, nb[std::size_t(p)], o);
            return nd.size();
        }
        void n_recv(int p) {
            if (nd.empty()) return;
            const XOffsetDatastructure::XStatus r = nh[std::size_t(p)]->template try_apply_delta_verified<PlayerState>(nd, nc[std::size_t(p)]);
            if (!r) throw std::runtime_error("native self delta rejected: " + std::string(r.error().message));
        }
        PlayerState& n_held(int p) { return nh[std::size_t(p)]->template root<PlayerState>(); }
        std::vector<std::optional<XBuffer>> nhw = std::vector<std::optional<XBuffer>>(kWarmDevices);
        std::vector<XOffsetDatastructure::XDeltaCursor> ncw = std::vector<XOffsetDatastructure::XDeltaCursor>(kWarmDevices);
        std::string_view n_wire() const { return nd; }
        void n_login_recv_w(int d, std::string_view w) {
            auto& h = nhw[std::size_t(d)];
            if (!h) h.emplace(XBuffer::create<PlayerState>(kPlayerInitial, cap()));
            const XOffsetDatastructure::XStatus r = h->template try_apply_delta_verified<PlayerState>(std::as_bytes(std::span<const char>(w.data(), w.size())), ncw[std::size_t(d)]);
            if (!r) throw std::runtime_error("native login (warm) rejected: " + std::string(r.error().message));
        }
        void n_recv_from_w(int d, const std::string& w) {
            if (w.empty()) return;
            const XOffsetDatastructure::XStatus r = nhw[std::size_t(d)]->template try_apply_delta_verified<PlayerState>(w, ncw[std::size_t(d)]);
            if (!r) throw std::runtime_error("native self delta (warm) rejected: " + std::string(r.error().message));
        }
        PlayerState& n_held_w(int d) { return nhw[std::size_t(d)]->template root<PlayerState>(); }
#endif
    };
    // SD (login.hpp): dirty modules to storage. Native SN: a delta against what
    // storage holds of that player (one baseline per player, kept across cycles).
    struct StoreOps {
        using XBuffer = XOffsetDatastructure::XBuffer;
        XOffsetRunnerT* r = nullptr;
        std::optional<XBuffer> pm, pr;
        std::string w;
        PlayerState& player(int p) { return r->players_[std::size_t(p)].template root<PlayerState>(); }
        template <class Fill> std::size_t modules_encode(Fill&& fill) {
            if (!pm) { pm.emplace(XBuffer::create<xm::PlayerPatch>(kPlayerInitial, cap())); pr.emplace(XBuffer::create<xm::PlayerPatch>(kPlayerInitial, cap())); }
            fill(pm->template reset<xm::PlayerPatch>());
            pm->template save_verified_into<xm::PlayerPatch>(w);
            return w.size();
        }
        void modules_recv() { reload_wire<xm::PlayerPatch, kPeer>(*pr, w); }
#ifdef XOFFSET_DELTA_SYNC_API
        std::unordered_map<int, XOffsetDatastructure::XDeltaBaseline> nb;
        std::unordered_map<int, XOffsetDatastructure::XDeltaCursor> nc;
        std::unordered_map<int, XBuffer> nh;
        std::string nd;
        std::size_t n_store_diff(int p) { r->players_[std::size_t(p)].template save_delta_into<PlayerState>(nd, nb[p]); return nd.size(); }
        void n_store_recv(int p) {
            if (nd.empty()) return;
            auto it = nh.find(p);
            if (it == nh.end()) it = nh.emplace(p, XBuffer::create<PlayerState>(kPlayerInitial, cap())).first;
            const XOffsetDatastructure::XStatus st = it->second.template try_apply_delta_verified<PlayerState>(nd, nc[p]);
            if (!st) throw std::runtime_error("native storage delta rejected: " + std::string(st.error().message));
        }
        PlayerState& n_store_held(int p) { return nh.at(p).template root<PlayerState>(); }
        // Which check the last storage apply ran: 0 key, 1 none, 2 in place, 3 full; 4 = nothing changed.
        int n_store_check(int p) { return nd.empty() ? 4 : int(nc[p].last_check()); }
        void n_store_prime_all() {
            for (int p = 0; p < int(r->players_.size()); ++p) { n_store_diff(p); n_store_recv(p); }
        }
#endif
    };

    // Held views are small: slot storage (no VM reservation each), 32 KiB at most.
    static XOffsetDatastructure::XBuffer::MaximumCapacity held_cap() {
        using XOffsetDatastructure::XBuffer;
        return XBuffer::maximum_capacity(XBuffer::SLOT_MAX).with_growth(XOffsetDatastructure::XGrowPolicy::Automatic);
    }

    struct BlockOps {
        using XBuffer = XOffsetDatastructure::XBuffer;
        std::optional<XBuffer> es, ds, dr;
        std::vector<std::string> snaps;
        std::string dw, comp;
        std::vector<std::optional<XBuffer>> held_views;
        // The client keeps its view's root reference, as a real client holds the
        // object it reads (the zpp_bits/std and cista adapters hold the decoded
        // object or its pointer). The root moves only when the view is reloaded,
        // so snap_recv, the only epoch change point, refreshes it; U*.apply then
        // starts at the root instead of looking it up in the arena on every delta.
        // -DSLG_RP_CHECK checks every use against root<ViewSnapshot>() (off by default).
        std::vector<xm::ViewSnapshot*> held_roots;
        void setup(int clients, int units, int marches) {
            es.emplace(XBuffer::create<xm::ViewSnapshot>(kViewInitial, cap()));
            ds.emplace(XBuffer::create<xm::ViewDelta>(kViewInitial, cap()));
            dr.emplace(XBuffer::create<xm::ViewDelta>(kViewInitial, cap()));
            snaps.assign(1, {});
            held_views.clear();
            held_views.resize(std::size_t(clients + kWarmDevices));   // [clients + d]: warm single device d (EBW, U*W)
            held_roots.assign(std::size_t(clients + kWarmDevices), nullptr);
#ifdef XOFFSET_DELTA_SYNC_API
            n_setup(clients, units, marches);
#else
            (void)units; (void)marches;
#endif
        }
        template <class Fill> std::size_t snap_encode(int b, Fill&& fill) {
            fill(es->reset<xm::ViewSnapshot>());
            es->save_verified_into<xm::ViewSnapshot>(snaps[std::size_t(b)]);
            return snaps[std::size_t(b)].size();
        }
        std::string_view snap_wire(int b) const { return snaps[std::size_t(b)]; }
        // Bytes only: the same snapshot as save_compact (default slack: the client edits it).
        std::string_view snap_compact(int) { es->save_compact_into<xm::ViewSnapshot>(comp); return comp; }
        void snap_recv(int c, int k, int b) {
            auto& h = held_views[std::size_t(c) + std::size_t(k)];
            if (!h) h.emplace(XBuffer::create<xm::ViewSnapshot>(4096, held_cap()));
            reload_wire<xm::ViewSnapshot, kClient>(*h, snaps[std::size_t(b)]);
            held_roots[std::size_t(c) + std::size_t(k)] = &h->root<xm::ViewSnapshot>();
        }
        xm::ViewSnapshot& held(int c, int k) {
            const std::size_t i = std::size_t(c) + std::size_t(k);
#ifdef SLG_RP_CHECK
            if (held_roots[i] != &held_views[i]->root<xm::ViewSnapshot>()) {
                std::fprintf(stderr, "SLG_RP_CHECK: held root of view %zu is stale\n", i);
                std::abort();
            }
#endif
            return *held_roots[i];
        }
        template <class Fill> std::size_t delta_encode(Fill&& fill) {
            fill(ds->reset<xm::ViewDelta>());
            ds->save_verified_into<xm::ViewDelta>(dw);
            return dw.size();
        }
        std::string_view delta_wire() const { return dw; }
        void delta_recv() { reload_wire<xm::ViewDelta, kClient>(*dr, dw); }
        void delta_recv_from(const std::string& w) { reload_wire<xm::ViewDelta, kClient>(*dr, w); }
        const xm::ViewDelta& delta() { return dr->root<xm::ViewDelta>(); }

#ifdef XOFFSET_DELTA_SYNC_API
        // Native delta (N*): the server keeps each static client's view in an
        // XBuffer with stable slots (a record that leaves zeroes its slot, a new
        // one takes the lowest free slot or is appended; tiles stay in place),
        // mutates it in place, and writes one delta against that client's
        // baseline. The client patches + verifies and reads in place.
        // A view's record id -> slot map: open addressing with linear probing
        // and backward-shift erase, at most half full, so an id that enters or
        // leaves the view allocates and frees nothing (std::unordered_map does
        // one node per insert). The slots it maps to are chosen as before.
        struct SlotMap {
            struct Cell { int32_t key, val; };
            static constexpr int32_t kEmpty = INT32_MIN;
            std::vector<Cell> cells;
            std::size_t count = 0;
            unsigned shift = 32;
            std::size_t home(int32_t id) const { return std::size_t((uint32_t(id) * 0x9E3779B1u) >> shift); }
            std::size_t mask() const { return cells.size() - 1; }
            // The cell of id, or of the empty cell where it would go.
            std::size_t probe(int32_t id) const {
                std::size_t i = home(id);
                while (cells[i].key != id && cells[i].key != kEmpty) i = (i + 1) & mask();
                return i;
            }
            int32_t* find(int32_t id) {
                if (cells.empty()) return nullptr;
                const std::size_t i = probe(id);
                return cells[i].key == id ? &cells[i].val : nullptr;
            }
            void grow() {
                std::vector<Cell> old(cells.empty() ? 32 : cells.size() * 2, Cell{kEmpty, 0});
                old.swap(cells);
                shift = 32;
                for (std::size_t n = cells.size(); n > 1; n >>= 1) --shift;
                for (const Cell& c : old) if (c.key != kEmpty) cells[probe(c.key)] = c;
            }
            // The value of id and whether it was added (then 0).
            std::pair<int32_t*, bool> try_emplace(int32_t id) {
                if (2 * (count + 1) > cells.size()) grow();
                const std::size_t i = probe(id);
                if (cells[i].key == id) return {&cells[i].val, false};
                cells[i] = Cell{id, 0};
                ++count;
                return {&cells[i].val, true};
            }
            // Removes id (present): later cells of its run move back, so no
            // tombstones are left.
            void erase(int32_t id) {
                std::size_t i = probe(id);
                for (std::size_t j = (i + 1) & mask(); cells[j].key != kEmpty; j = (j + 1) & mask()) {
                    const std::size_t h = home(cells[j].key);
                    // cells[j] may fill the hole at i unless its home lies in (i, j].
                    if (((j - h) & mask()) >= ((j - i) & mask())) { cells[i] = cells[j]; i = j; }
                }
                cells[i].key = kEmpty;
                --count;
            }
        };
        struct NView {
            std::optional<XBuffer> buf;
            XOffsetDatastructure::XDeltaBaseline base;
            SlotMap uslot, mslot;
            std::vector<int32_t> ufree, mfree;
            int32_t usize = 0, msize = 0;
            std::string key;
            std::vector<int32_t> tring;   // browse (NB): the tile each ring slot holds (ty * kTiles + tx), -1 none
            bool tring_empty = true;      // browse: no tile placed yet (n_bview)
        };
        std::vector<NView> nb;
        // This delivery's plan (n_plan, then n_update of the same client): one
        // set of vectors for every client, warm, like the shared delta buffer.
        struct NPlan {
            std::vector<int32_t> urm, mrm;
            std::vector<std::pair<int32_t, int32_t>> uup, mup;
            std::vector<uint8_t> unew;   // uup[k] took a slot this tick: the unit is new to the view
        } np;
        // Every client's delta is written into one buffer, as the application-
        // level path writes every message into one, and read by that client's
        // apply right after.
        std::string ndelta;
        std::vector<std::optional<XBuffer>> nheld;
        std::vector<XOffsetDatastructure::XDeltaCursor> ncur;
        int32_t nucap = 0, nmcap = 0;
        // Capacity per view is reserved once (about 1.5x the average units in a
        // viewport, a quarter of all marches, plus margin), so the layout does
        // not move from tick to tick; the vectors are as long as the most records
        // the view has held (free slots are reused lowest first).
        void n_setup(int clients, int units, int marches) {
            nb.clear();
            nb.resize(std::size_t(clients));
            nucap = int32_t(int64_t(3) * units * kViewW * kViewH / (int64_t(2) * kMapSize * kMapSize)) + 32;
            nmcap = marches / 4 + 32;
            nheld.clear();
            nheld.resize(std::size_t(clients + kWarmDevices));   // [clients + d]: warm replica d (N*W)
            ncur.assign(std::size_t(clients + kWarmDevices), {});
        }
        void n_view(int c, const MapWorld& w, const ViewRect& r) {
            auto& n = nb[std::size_t(c)];
            // One small buffer per client: the largest slot class, not a VM reservation.
            n.buf.emplace(XBuffer::create<xm::ViewSnapshot>(kViewInitial, XBuffer::maximum_capacity(XBuffer::SLOT_MAX).with_growth(XOffsetDatastructure::XGrowPolicy::Automatic)));
            auto& v = n.buf->template root<xm::ViewSnapshot>();
            v.x0 = r.x0; v.y0 = r.y0; v.x1 = r.x1; v.y1 = r.y1;
            v.units.reserve(std::size_t(nucap));
            v.marches.reserve(std::size_t(nmcap));
            const TileSpan sp = tile_span(r.x0, r.y0, r.x1, r.y1);
            v.tiles.resize(std::size_t(sp.cols * sp.rows));
            for (int32_t ty = 0; ty < sp.rows; ++ty)
                for (int32_t tx = 0; tx < sp.cols; ++tx) copy_vtile(v.tiles[std::size_t(ty * sp.cols + tx)], w, sp.tx0 + tx, sp.ty0 + ty);
        }
        // Browse (NB, report only): the view moves, so its tiles live in a ring of
        // kRingC x kRingR slots, tile (tx, ty) at slot (tx mod kRingC, ty mod kRingR):
        // a viewport covers at most 17 x 13 tiles, so the tiles of one span never
        // share a slot, and a tile that stays in view keeps its slot while the view
        // pans. A slot outside the span is zero (adds nothing to query_view).
        static constexpr int32_t kRingC = kViewW / kTile + 1, kRingR = kViewH / kTile + 1;
        void n_bview(int c) {
            auto& n = nb[std::size_t(c)];
            n.buf.emplace(XBuffer::create<xm::ViewSnapshot>(kViewInitial, XBuffer::maximum_capacity(XBuffer::SLOT_MAX).with_growth(XOffsetDatastructure::XGrowPolicy::Automatic)));
            auto& v = n.buf->template root<xm::ViewSnapshot>();
            v.units.reserve(std::size_t(nucap));
            v.marches.reserve(std::size_t(nmcap));
            v.tiles.resize(std::size_t(kRingC * kRingR));
            n.tring.assign(std::size_t(kRingC * kRingR), -1);
            n.tring_empty = true;
        }
        // The view's new rectangle and the tiles under it: a tile that entered the
        // span is copied into its slot, a slot whose tile left (and that no
        // entering tile took) is cleared, a tile that stayed and whose owner changed
        // this tick (changed: the tracker's p.tiles, untimed as for N*) gets its owner;
        // each written slot is touched. Only the span's difference is visited: a pan
        // of at most 48 px moves two columns and two rows.
        static std::size_t ring_slot(int32_t tx, int32_t ty) { return std::size_t((ty % kRingR) * kRingC + tx % kRingC); }
        void n_move(int c, const MapWorld& w, const ViewRect& r, const std::vector<int32_t>& changed) {
            auto& n = nb[std::size_t(c)];
            auto& v = n.buf->template root<xm::ViewSnapshot>();
            XBuffer& b = *n.buf;
            const TileSpan os = n.tring_empty ? TileSpan{0, 0, 0, 0} : tile_span(v.x0, v.y0, v.x1, v.y1);
            const TileSpan ns = tile_span(r.x0, r.y0, r.x1, r.y1);
            b.touch_set(v.x0, r.x0); b.touch_set(v.y0, r.y0); b.touch_set(v.x1, r.x1); b.touch_set(v.y1, r.y1);
            n.tring_empty = false;
            // Every tile of span a that is not in span b: rows outside b's whole, the
            // rest the columns left and right of b's.
            const auto outside = [](const TileSpan& a, const TileSpan& o, auto&& f) {
                const int32_t ox1 = o.tx0 + o.cols, oy1 = o.ty0 + o.rows;
                for (int32_t ty = a.ty0; ty < a.ty0 + a.rows; ++ty) {
                    if (ty < o.ty0 || ty >= oy1) { for (int32_t tx = a.tx0; tx < a.tx0 + a.cols; ++tx) f(tx, ty); continue; }
                    for (int32_t tx = a.tx0; tx < std::min(a.tx0 + a.cols, o.tx0); ++tx) f(tx, ty);
                    for (int32_t tx = std::max(a.tx0, ox1); tx < a.tx0 + a.cols; ++tx) f(tx, ty);
                }
            };
            outside(ns, os, [&](int32_t tx, int32_t ty) {
                const std::size_t s = ring_slot(tx, ty);
                copy_vtile(v.tiles[s], w, tx, ty); b.touch(v.tiles[s]);
                n.tring[s] = ty * kTiles + tx;
            });
            outside(os, ns, [&](int32_t tx, int32_t ty) {
                const std::size_t s = ring_slot(tx, ty);
                if (n.tring[s] != ty * kTiles + tx) return;   // an entering tile took the slot
                v.tiles[s] = xm::Tile{}; b.touch(v.tiles[s]);
                n.tring[s] = -1;
            });
            for (int32_t t : changed) {
                const int32_t tx = t % kTiles, ty = t / kTiles;
                auto& d = v.tiles[ring_slot(tx, ty)];
                const WorldTile& s = w.tile(tx, ty);
                if (d.owner != s.owner) { d.owner = s.owner; b.touch(d); }
            }
        }
        void n_plan(int c, const MapWorld& w, const DeltaPlan& p) {
            auto& n = nb[std::size_t(c)];
            np.urm.clear(); np.mrm.clear(); np.uup.clear(); np.mup.clear(); np.unew.clear();
            // The slot of id, taking the lowest free one (or a new one) for an
            // id the view does not hold yet: one lookup either way.
            auto take = [](SlotMap& at, std::vector<int32_t>& fr, int32_t& size, int32_t id, bool& fresh) {
                const auto [val, added] = at.try_emplace(id);
                fresh = added;
                if (!added) return *val;
                int32_t pos;
                if (fr.empty()) pos = size++;
                else { std::pop_heap(fr.begin(), fr.end(), std::greater<>{}); pos = fr.back(); fr.pop_back(); }
                *val = pos;
                return pos;
            };
            for (int32_t id : p.removed) {
                const int32_t* at = n.uslot.find(id);
                if (!at) continue;
                const int32_t pos = *at;
                np.urm.push_back(pos); n.ufree.push_back(pos); n.uslot.erase(id);
                std::push_heap(n.ufree.begin(), n.ufree.end(), std::greater<>{});
            }
            for (int32_t id : p.removed_marches) {
                const int32_t* at = n.mslot.find(id);
                if (!at) continue;
                const int32_t pos = *at;
                np.mrm.push_back(pos); n.mfree.push_back(pos); n.mslot.erase(id);
                std::push_heap(n.mfree.begin(), n.mfree.end(), std::greater<>{});
            }
            bool fresh = false;
            for (int32_t i : p.units) {
                const int32_t pos = take(n.uslot, n.ufree, n.usize, w.unit(i).id, fresh);
                np.unew.push_back(fresh ? 1 : 0);
                np.uup.emplace_back(i, pos);
            }
            for (int32_t i : p.marches) np.mup.emplace_back(i, take(n.mslot, n.mfree, n.msize, w.march(i).id, fresh));
        }
        void n_update(int c, const MapWorld& w, const DeltaPlan& p, int64_t id) {
            auto& n = nb[std::size_t(c)];
            auto& v = n.buf->template root<xm::ViewSnapshot>();
            // Every change is touched after it is made, so the delta compares
            // the touched blocks only (XDeltaOptions::touched_only). A vector
            // that gains slots has its head touched; the new slots are filled
            // and touched below.
            XBuffer& b = *n.buf;
            v.request_id = id; v.tick = w.tick_no();
            b.touch(v.request_id); b.touch(v.tick);
            if (v.units.size() < std::size_t(n.usize)) { v.units.resize(std::size_t(n.usize)); b.touch(&v.units, sizeof(v.units)); }
            if (v.marches.size() < std::size_t(n.msize)) { v.marches.resize(std::size_t(n.msize)); b.touch(&v.marches, sizeof(v.marches)); }
            for (int32_t pos : np.urm) {
                static const WorldUnit empty{};
                auto& u = v.units[std::size_t(pos)];
                copy_unit(u, empty);
                b.touch(u);
            }
            for (int32_t pos : np.mrm) { v.marches[std::size_t(pos)] = xm::ViewMarch{}; b.touch(v.marches[std::size_t(pos)]); }
            // A unit new to the view is copied whole; one the view holds gets
            // the fields the world changed this tick (its dirty flags, as a
            // game server keeps them), each written and touched where its
            // value differs (touch_set: the bytes that changed).
            for (std::size_t k = 0; k < np.uup.size(); ++k) {
                const auto [i, pos] = np.uup[k];
                auto& d = v.units[std::size_t(pos)];
                const WorldUnit& s = w.unit(i);
                if (np.unew[k]) { copy_unit(d, s); b.touch(d); continue; }
                const uint64_t m = w.dirty(i);
#define SLG_X(f) if (m & SLG_D(f)) b.touch_set(d.f, s.f);
                SLG_VUNIT_SCALARS(SLG_X)
#undef SLG_X
                if (m & SLG_D(name)) { set_str(d.name, s.name); b.touch(d.name); }
                if (m & SLG_D(alliance_tag)) { set_str(d.alliance_tag, s.alliance_tag); b.touch(d.alliance_tag); }
                if (m & SLG_D(owner_name)) { set_str(d.owner_name, s.owner_name); b.touch(d.owner_name); }
                const auto hero = [&](auto& a, const auto& e) { b.touch_set(a.id, e.id); b.touch_set(a.level, e.level); b.touch_set(a.star, e.star); b.touch_set(a.skin, e.skin); };
                // A list of the same length is written element by element; one
                // whose length changed is copied whole and touched.
                const auto list = [&](auto& dl, const auto& sl, auto&& each, auto&& assign) {
                    if (dl.size() == sl.size()) { for (std::size_t q = 0; q < sl.size(); ++q) each(dl[q], sl[q]); return; }
                    copy_list(dl, sl, assign);
                    b.touch(dl);
                };
                if (m & SLG_D(commander)) hero(d.commander, s.commander);
                if (m & SLG_D(heroes)) list(d.heroes, s.heroes, hero, [](auto& a, const auto& e) { copy_uhero(a, e); });
                if (m & SLG_D(troops)) list(d.troops, s.troops, [&](auto& a, const auto& e) { b.touch_set(a.type, e.type); b.touch_set(a.count, e.count); b.touch_set(a.wounded, e.wounded); },
                                            [](auto& a, const auto& e) { a.type = e.type; a.count = e.count; a.wounded = e.wounded; });
                if (m & SLG_D(buffs)) list(d.buffs, s.buffs, [&](auto& a, const auto& e) { b.touch_set(a.id, e.id); b.touch_set(a.stacks, e.stacks); b.touch_set(a.expire_ms, e.expire_ms); },
                                           [](auto& a, const auto& e) { a.id = e.id; a.stacks = e.stacks; a.expire_ms = e.expire_ms; });
            }
            for (const auto& [i, pos] : np.mup) { copy_vmarch(v.marches[std::size_t(pos)], w.march(i)); b.touch(v.marches[std::size_t(pos)]); }
            for (int32_t t : p.tiles) {
                const int32_t tx = t % kTiles, ty = t / kTiles;
                auto& d = v.tiles[tile_pos(v, tx, ty)];
                const WorldTile& s = w.tile(tx, ty);
                if (d.owner != s.owner) { d.owner = s.owner; b.touch(d); }
            }
        }
        std::size_t n_diff(int c) {
            auto& n = nb[std::size_t(c)];
            XOffsetDatastructure::XDeltaOptions o;
            o.touched_only = true;
            n.buf->template save_delta_into<xm::ViewSnapshot>(ndelta, n.base, o);
            return ndelta.size();
        }
        std::size_t n_key(int c) {
            auto& n = nb[std::size_t(c)];
            if (n.base.empty()) n_diff(c);
            n.base.key_delta_into(n.key);
            return n.key.size();
        }
        std::optional<XBuffer>& n_slot(int c) {
            auto& h = nheld[std::size_t(c)];
            // The replica holds the server's non-compact view layout (free slots,
            // growth slack), which can pass 32 KiB: the largest slot class.
            if (!h) h.emplace(XBuffer::create<xm::ViewSnapshot>(4096, XBuffer::maximum_capacity(XBuffer::SLOT_MAX).with_growth(XOffsetDatastructure::XGrowPolicy::Automatic)));
            return h;
        }
        void n_key_recv(int c, int, int b) {
            const XOffsetDatastructure::XStatus r = n_slot(c)->template try_apply_delta_verified<xm::ViewSnapshot>(nb[std::size_t(b)].key, ncur[std::size_t(c)]);
            if (!r) throw std::runtime_error("native key delta rejected: " + std::string(r.error().message));
        }
        void n_apply(int c, int) {
            const XOffsetDatastructure::XStatus r = nheld[std::size_t(c)]->template try_apply_delta_verified<xm::ViewSnapshot>(ndelta, ncur[std::size_t(c)]);
            if (!r) throw std::runtime_error("native delta rejected: " + std::string(r.error().message));
        }
        xm::ViewSnapshot& n_held(int c, int) { return nheld[std::size_t(c)]->template root<xm::ViewSnapshot>(); }
        // How the client's last apply was checked (report only): 0 key, 1 none, 2 in place, 3 full.
        int n_check(int c) const { return int(ncur[std::size_t(c)].last_check()); }
        std::string_view n_wire() const { return ndelta; }
        void n_apply_from(int c, const std::string& d) {
            const XOffsetDatastructure::XStatus r = nheld[std::size_t(c)]->template try_apply_delta_verified<xm::ViewSnapshot>(d, ncur[std::size_t(c)]);
            if (!r) throw std::runtime_error("native delta rejected: " + std::string(r.error().message));
        }
#endif
    };

    // XOffset: a player is its buffer; used = arena size - free, arena = get_size,
    // reserved = the address space its storage reserves.
    void memory(CycleStats& st) override {
        std::size_t used = 0, arena = 0, res = 0;
        for (auto& b : players_) { used += b.get_size() - b.get_free_memory(); arena += b.get_size(); res += b.reserved_bytes(); }
        if (const std::size_t n = players_.size()) {
            st.bytes["MEM.player"] = used / n; st.bytes["MEM.player_arena"] = arena / n; st.bytes["MEM.player_reserved"] = res / n;
        }
        for (int i = 0; i < ViewCases::count; ++i) {
            view_memory(bs_[i], ViewCases::keys(i), st);
#ifdef XOFFSET_DELTA_SYNC_API
            if (const std::size_t n = ops_[i].nb.size(); n && ops_[i].nb[0].buf) {
                std::size_t v = 0, bl = 0, slots = 0;
                for (auto& c : ops_[i].nb) {
                    if (!c.buf) continue;
                    v += c.buf->get_size(); bl += c.base.recorded_bytes();
                    slots += (c.uslot.cells.capacity() + c.mslot.cells.capacity()) * sizeof(typename BlockOps::SlotMap::Cell)
                           + (c.ufree.capacity() + c.mfree.capacity()) * sizeof(int32_t);
                }
                const std::string N = "MEM." + ViewCases::keys(i).nname;
                st.bytes[N + ".view"] = v / n; st.bytes[N + ".baseline"] = bl / n; st.bytes[N + ".slots"] = slots / n;
            }
#endif
        }
#ifdef XOFFSET_DELTA_SYNC_API
        if (const std::size_t n = lops_.nb.size()) {
            std::size_t bl = 0;
            for (const auto& b : lops_.nb) bl += b.recorded_bytes();
            st.bytes["MEM.LN.baseline"] = bl / n;
        }
#endif
    }

    uint64_t world_digest() override {
        Digest g;
        for (auto& b : players_) dig_player(g, b.template root<PlayerState>());
        dig_shard(g, shard_->root<ShardState>());
        return g.h;
    }

private:
    static constexpr std::size_t kPlayerInitial = 64 * 1024;
    static constexpr std::size_t kBattleInitial = 16 * 1024;
    static constexpr std::size_t kShardInitial = 4 * 1024 * 1024;
    static constexpr std::size_t kReportInitial = 4 * 1024;
    static constexpr std::size_t kViewInitial = 16 * 1024;

    // Every buffer reserves 256 MiB of address space. SLG_XOFFSET_CAP_MIB=N reserves N MiB
    // for each player, battle and report buffer instead (the shard keeps 256 MiB): an
    // iPhone gives a process about 3.4 GiB of address space, less than the 64 players of
    // scale S reserve at 256 MiB each (docs/DEPLOYMENT.md, SC-12).
    static XOffsetDatastructure::XBuffer::MaximumCapacity cap(bool shard = false) {
        using XOffsetDatastructure::XBuffer;
        static const std::size_t small = [] {
            const char* v = std::getenv("SLG_XOFFSET_CAP_MIB");
            const long mib = v ? std::strtol(v, nullptr, 10) : 0;
            return mib > 0 ? std::size_t(mib) << 20 : std::size_t(256) << 20;
        }();
        auto c = XBuffer::maximum_capacity(shard ? std::size_t(256) << 20 : small)
                     .with_growth(XOffsetDatastructure::XGrowPolicy::Automatic);
        return use_pool() || reuse_mode() ? c.with_pool(pool()) : c;
    }
    // Diagnostic only (small-message probe): SLG_XOFFSET_POOL=1 reuses idle VM
    // owners through the public XBufferPool. Every load is still load_verified.
    static bool use_pool() { static const bool on = std::getenv("SLG_XOFFSET_POOL") != nullptr; return on; }
    static XOffsetDatastructure::XBufferPool& pool() {
        static XOffsetDatastructure::XBufferPool p(XOffsetDatastructure::XBufferPool::Limits{
            16, 16 * (std::size_t(256) << 20), std::size_t(256) << 20});
        return p;
    }
    PlayerState& player(int i) { return players_[std::size_t(i)].template root<PlayerState>(); }
    // Receive in the reuse protocol. Default: reload_verified copies the wire into the
    // slot's long-lived buffer and verifies it there. Direct ("xoffset_direct", the
    // "direct receive" column): begin_verified_receive reserves the final arena (from the
    // pool), the transport writes header / extent table / payload / hole table
    // straight into it (untimed: the network read every format pays outside the
    // timer; the other formats decode from the wire buffer in place), and
    // finish_verified runs the same verification as load_verified. Replacing the
    // slot's previous buffer (back to the pool) is timed.
    template <class T>
    void recv(CycleStats& st, const char* key, const std::string& wire, std::optional<XOffsetDatastructure::XBuffer>& dst) {
        using namespace XOffsetDatastructure;
        if constexpr (!Direct) { timed(st, key, [&] { reload_wire<T, kPeer>(*dst, wire); }); return; }
        else {
        XWireHeaderV2 h{};
        if (wire.size() < sizeof(h)) throw std::runtime_error("direct receive: short wire");
        std::memcpy(&h, wire.data(), sizeof(h));
        const std::size_t payload = std::size_t(h.used_bytes), holes = std::size_t(h.hole_count), extents = std::size_t(h.extent_count);
        std::optional<VerifiedReceiveToken<T>> tok;
        timed(st, key, [&] { tok.emplace(begin_verified_receive<T>(payload, cap(), holes, extents)); });
        {
            auto op = tok->acquire_operation();
            auto head = op.header_bytes(), body = op.payload_bytes(), hole = op.hole_bytes(), table = op.extent_bytes();
            std::size_t at = 0;
            auto take = [&](void* to, std::size_t n) {
                if (at + n > wire.size()) throw std::runtime_error("direct receive: wire framing");
                if (n) std::memcpy(to, wire.data() + at, n);
                at += n;
            };
            take(head.data(), head.size());
            take(table.data(), table.size());
            // payload_bytes() is payload [begin, used): begin is 48 for wire
            // format 2.1 (the payload header is not sent) and 0 before it; an
            // extent at payload offset o lands at o - begin.
            const std::size_t begin = payload - body.size();
            if (extents == 0) take(body.data(), body.size());
            else
                for (std::size_t e = 0; e < extents; ++e) {
                    uint32_t ext[2];
                    std::memcpy(ext, table.data() + e * 8, 8);
                    if (ext[0] < begin || std::size_t(ext[0]) - begin + ext[1] > body.size())
                        throw std::runtime_error("direct receive: extent bounds");
                    take(body.data() + (ext[0] - begin), ext[1]);
                }
            take(hole.data(), hole.size());
            if (at != wire.size()) throw std::runtime_error("direct receive: trailing bytes");
            op.complete(head.size(), body.size(), hole.size(), table.size());
        }
        timed(st, key, [&] { dst.emplace(finish_verified(*tok)); });
        }
    }

    const Scale* sc_ = nullptr;
    uint64_t seed_ = 0;
    std::vector<XOffsetDatastructure::XBuffer> players_;
    std::vector<int> armies_;
    std::optional<XOffsetDatastructure::XBuffer> shard_;
    // Reuse protocol state.
    std::optional<XOffsetDatastructure::XBuffer> a_req_, a_m_, a_r_, b_m_, b_r_, c_r_, d_;
    std::string req_, res_, out_, back_, snap_, snap2_;
    std::string store_;
    ViewSession bs_[ViewCases::count];
    BlockOps ops_[ViewCases::count];
    LoginOps lops_;
    StoreOps sops_;
    std::vector<int64_t> lseq_;
};

using XOffsetRunner = XOffsetRunnerT<XoModel, false>;
using XOffsetTrustedRunner = XOffsetRunnerT<XoTrustedModel, false>;

} // namespace slg
