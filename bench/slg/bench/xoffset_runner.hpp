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
#include <tuple>
#include <utility>
#include <vector>
#include "runner.hpp"
#include "slg/generic.hpp"
#include "slg/model_xoffset.hpp"
#include "slg/model_xoffset_optin.hpp"
#include "slg/trace.hpp"
#include "slg/login.hpp"
#include "slg/view.hpp"
#include "slg/uplink.hpp"
#include "xoffset_receiver.hpp"
#include "xoffset_shared_delta.hpp"

namespace slg {

struct XoModel {
    static constexpr const char* name = "xoffset";
    using PlayerState = xm::PlayerState; using BattleMsg = xm::BattleMsg;
    using ShardState = xm::ShardState; using Report = xm::Report;
};
// Trust tier (3.4 P1, #228; susu 07:17Z 11-04): the same model, every reuse-protocol
// receive of a whole wire through XReceiver with XCheck::admit (server <-> server:
// XLink::server_peer; server -> client: XLink::from_server) instead of
// reload_verified; every native delta receive through the same receiver's
// apply_delta (trust tier P2: admit, no map of checked bytes).
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
#ifdef XOFFSET_DELTA_SYNC_API
    // A delta receive: try_apply_delta_verified, or (Trusted) the receiver for link L
    // with its default check (admit for server_peer / from_server: trust tier P2).
    template <class T, XOffsetDatastructure::XLink L, class W>
    static XOffsetDatastructure::XStatus apply_delta_wire(XOffsetDatastructure::XBuffer& b, const W& w,
                                                         XOffsetDatastructure::XDeltaCursor& c) {
        using namespace XOffsetDatastructure;
        if constexpr (Trusted) {
            static XReceiver<L> rx(XBuffer::maximum_capacity(XBuffer::DEFAULT_MAXIMUM_CAPACITY));
            return rx.template apply_delta<T>(b, w, c);
        } else {
            return b.template try_apply_delta_verified<T>(w, c);
        }
    }
#endif
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
            a_req_.emplace(XBuffer::create<BattleMsg>(kBattleInitial, build_cap()));
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
                XBuffer b = XBuffer::create<BattleMsg>(kBattleInitial, build_cap());
                build_in<BattleMsg>(b, [&](BattleMsg& msg) {
                    msg.battle_id = pr.battle_id; msg.seed = pr.seed; msg.winner = 0;
#if defined(XOFFSET_SAVE_COMPOSED_API)
                    // Adapter v3: the request is composed from the two armies straight
                    // into the wire (tight save_compact layout); the draft holds only the root.
                    b.save_compact_into<BattleMsg>(req, XCompactOptions{.slack_div = 0},
                                                   xassign(msg.attacker, att), xassign(msg.defender, def));
#else
                    msg.attacker = att;  // T4 adapter (c): public copy assignment
                    msg.defender = def;
#endif
                });
#if !defined(XOFFSET_SAVE_COMPOSED_API)
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
            XBuffer b = XBuffer::create<Report>(kReportInitial, build_cap());
            build_in<Report>(b, [&](Report& out) { copy_report(out, rep); });
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
                auto compose = [&](BattleMsg& msg) {
                    msg.battle_id = pr.battle_id; msg.seed = pr.seed; msg.winner = 0;
                    a_req_->save_compact_into<BattleMsg>(req_, XCompactOptions{.slack_div = 0},
                                                         xassign(msg.attacker, att), xassign(msg.defender, def));
                };
                // Scalar stores only, nothing allocates: the payload cannot
                // move, so the plain root reference needs no pin scope.
                compose(a_req_->root<BattleMsg>());
#else
                build_in<BattleMsg>(*a_req_, [&](BattleMsg& msg) {
                    msg.battle_id = pr.battle_id; msg.seed = pr.seed; msg.winner = 0;
                    msg.attacker = att;  // T4 adapter (c): public copy assignment
                    msg.defender = def;
                });
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
            XBuffer b = XBuffer::create<Report>(kReportInitial, build_cap());
            build_in<Report>(b, [&](Report& out) { copy_report(out, rep); });
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
#ifdef SLG_D3_STATS
        for (int k = 0; k < ViewCases::count; ++k) {
            const auto& o = ops_[k];
            if (ViewCases::browse(k) || o.dstat_msgs == 0) continue;
            std::fprintf(stderr, "d3bytes %s cycle %d: %.1f B/msg", ViewCases::keys(k).dname.c_str(), cycle, o.dstat_total / o.dstat_msgs);
            static const char* tn[4] = {"root", "unit", "march", "tile"};
            static const char* kn[3] = {"enter", "change", "leave"};
            for (int q = 0; q < 12; ++q)
                if (o.dstat_ops[q] > 0)
                    std::fprintf(stderr, "; %s.%s %.2f ops %.1f B (body %.1f)", tn[q / 3], kn[q % 3], o.dstat_ops[q] / o.dstat_msgs,
                                 o.dstat_bytes[q] / o.dstat_msgs, o.dstat_body[q] / o.dstat_msgs);
            std::fprintf(stderr, "\n");
        }
#endif
        run_login(lops_, mix_seed(seed_, 0x1062), cycle, 4, st, lseq_);
        run_uplink(lops_, mix_seed(seed_, 0x1063), cycle, 4, st);
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
            pm.emplace(XBuffer::create<xm::PlayerPatch>(kViewInitial, build_cap()));
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
            build_in<xm::PlayerPatch>(*pm, fill);
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
        // UL (uplink.hpp): the client fills a march order in its reused buffer and saves
        // it; the server receives it through XReceiver<from_client>: xoffset reloads it
        // with the full check into a reused buffer, xoffset_trusted views it in place
        // with the bounds check (and times the full check as ULF.recv).
        std::optional<XBuffer> um, ur;
        std::string uw;
        template <class Fill> std::size_t up_encode(Fill&& fill) {
            // The client keeps one order and refills it in place, as the std
            // formats keep one March and clear() it: the first order builds
            // the buffer, every later one edits it (troops keeps its capacity).
            if (!um) {
                um.emplace(XBuffer::create<xm::March>(kUplinkInitial, build_cap()));
                build_in<xm::March>(*um, fill);
            } else {
                edit_in<xm::March>(*um, fill);
            }
            um->template save_verified_into<xm::March>(uw);
            return uw.size();
        }
        static XOffsetDatastructure::XReceiver<XOffsetDatastructure::XLink::from_client>& uplink_rx() {
            static XOffsetDatastructure::XReceiver<XOffsetDatastructure::XLink::from_client> rx(
                XBuffer::maximum_capacity(XBuffer::DEFAULT_MAXIMUM_CAPACITY));
            return rx;
        }
        uint64_t up_recv_checked() {
            if (!ur) ur.emplace(XBuffer::create<xm::March>(kUplinkInitial, cap()));
            const XOffsetDatastructure::XStatus r = uplink_rx().template reload<xm::March>(*ur, uw);
            if (!r) throw std::runtime_error("uplink receive rejected: " + std::string(r.error().message));
            return query_order(ur->template root<xm::March>());
        }
        uint64_t up_recv() {
            if constexpr (Trusted) {
                const auto v = uplink_rx().template view<xm::March>(uw);
                if (!v) throw std::runtime_error("uplink view rejected: " + std::string(v.error().message));
                return query_order(v->root());
            } else {
                return up_recv_checked();
            }
        }
        uint64_t up_recv_full() requires Trusted { return up_recv_checked(); }
        std::string_view up_wire() const { return uw; }
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
            const XOffsetDatastructure::XStatus r = apply_delta_wire<PlayerState, kClient>(*h, nd, nc[std::size_t(p)]);
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
            const XOffsetDatastructure::XStatus r = apply_delta_wire<PlayerState, kClient>(*nh[std::size_t(p)], nd, nc[std::size_t(p)]);
            if (!r) throw std::runtime_error("native self delta rejected: " + std::string(r.error().message));
        }
        PlayerState& n_held(int p) { return nh[std::size_t(p)]->template root<PlayerState>(); }
        std::vector<std::optional<XBuffer>> nhw = std::vector<std::optional<XBuffer>>(kWarmDevices);
        std::vector<XOffsetDatastructure::XDeltaCursor> ncw = std::vector<XOffsetDatastructure::XDeltaCursor>(kWarmDevices);
        std::string_view n_wire() const { return nd; }
        void n_login_recv_w(int d, std::string_view w) {
            auto& h = nhw[std::size_t(d)];
            if (!h) h.emplace(XBuffer::create<PlayerState>(kPlayerInitial, cap()));
            const XOffsetDatastructure::XStatus r = apply_delta_wire<PlayerState, kClient>(*h, std::as_bytes(std::span<const char>(w.data(), w.size())), ncw[std::size_t(d)]);
            if (!r) throw std::runtime_error("native login (warm) rejected: " + std::string(r.error().message));
        }
        void n_recv_from_w(int d, const std::string& w) {
            if (w.empty()) return;
            const XOffsetDatastructure::XStatus r = apply_delta_wire<PlayerState, kClient>(*nhw[std::size_t(d)], w, ncw[std::size_t(d)]);
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
            if (!pm) { pm.emplace(XBuffer::create<xm::PlayerPatch>(kPlayerInitial, build_cap())); pr.emplace(XBuffer::create<xm::PlayerPatch>(kPlayerInitial, cap())); }
            build_in<xm::PlayerPatch>(*pm, fill);
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
            const XOffsetDatastructure::XStatus st = apply_delta_wire<PlayerState, kPeer>(it->second, nd, nc[p]);
            if (!st) throw std::runtime_error("native storage delta rejected: " + std::string(st.error().message));
        }
        PlayerState& n_store_held(int p) { return nh.at(p).template root<PlayerState>(); }
        // Which check the last storage apply ran: 0 key, 1 none, 2 in place, 3 full; 4 = nothing
        // changed; 5 admit (xoffset_trusted).
        int n_store_check(int p) {
            if (nd.empty()) return 4;
            const auto c = nc[p].last_check();
            return c == XOffsetDatastructure::XDeltaCheck::admit ? 5 : int(c);
        }
        void n_store_prime_all() {
            for (int p = 0; p < int(r->players_.size()); ++p) { n_store_diff(p); n_store_recv(p); }
        }
#endif
    };

    // Held views (client replicas, server view buffers): a fixed 128 KiB slot
    // each, no VM reservation (3.6 memory gate, 2026-11-11). The largest view at
    // scale M uses 93 KiB (NC replica, server layout with its free blocks); a
    // view that outgrows the slot fails its receive or edit (an allocation
    // failure) rather than moving. 128 KiB instead of SLOT_MAX
    // (256 KiB) halves the address space of 6000 views, which is most of the
    // page tables left once the players are relocating.
    static constexpr std::size_t kHeldMax = (std::size_t(128) << 10) - 64;   // the 128 KiB slot's payload
    static XOffsetDatastructure::XBuffer::MaximumCapacity held_cap() {
        using XOffsetDatastructure::XBuffer;
        return XBuffer::maximum_capacity(kHeldMax).with_growth(XOffsetDatastructure::XGrowPolicy::Automatic);
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
            es.emplace(XBuffer::create<xm::ViewSnapshot>(kViewInitial, build_cap()));
            ds.emplace(XBuffer::create<xm::ViewDelta>(kViewInitial, build_cap()));
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
            build_in<xm::ViewSnapshot>(*es, fill);
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
            held_roots[std::size_t(c) + std::size_t(k)] = &h->template root<xm::ViewSnapshot>();
        }
        xm::ViewSnapshot& held(int c, int k) {
            const std::size_t i = std::size_t(c) + std::size_t(k);
#ifdef SLG_RP_CHECK
            if (held_roots[i] != &held_views[i]->template root<xm::ViewSnapshot>()) {
                std::fprintf(stderr, "SLG_RP_CHECK: held root of view %zu is stale\n", i);
                std::abort();
            }
#endif
            return *held_roots[i];
        }
        template <class Fill> std::size_t delta_encode(Fill&& fill) {
            build_in<xm::ViewDelta>(*ds, fill);
            ds->save_verified_into<xm::ViewDelta>(dw);
            return dw.size();
        }
        std::string_view delta_wire() const { return dw; }
        // xoffset reloads a delta into the reused buffer with the full check; xoffset_trusted
        // reads it in place (XReceiver<from_server>::view, admit: framing and root in the
        // payload) and copies nothing. The wire stays alive while the delta is applied.
        const xm::ViewDelta* dview = nullptr;
        void delta_in(const std::string& w) {
            if constexpr (Trusted) {
                static XOffsetDatastructure::XReceiver<kClient> rx(XBuffer::maximum_capacity(XBuffer::DEFAULT_MAXIMUM_CAPACITY));
                const auto v = rx.template view<xm::ViewDelta>(w);
                if (!v) throw std::runtime_error("trusted delta view rejected: " + std::string(v.error().message));
                dview = &v->root();
            } else {
                reload_wire<xm::ViewDelta, kClient>(*dr, w);
            }
        }
        void delta_recv() { delta_in(dw); }
        void delta_recv_from(const std::string& w) { delta_in(w); }
        const xm::ViewDelta& delta() {
            if constexpr (Trusted) return *dview;
            else return dr->root<xm::ViewDelta>();
        }

#ifdef XOFFSET_DELTA_SYNC_API
        // Native delta (N*): the server keeps each static client's view in an
        // XBuffer with stable slots (a record that leaves zeroes its slot, a new
        // one takes the lowest free slot or is appended; tiles stay in place),
        // mutates it in place, and writes one delta against that client's
        // baseline. The client patches + verifies and reads in place. The slots
        // xsync::XBasicSyncSlots maps to are chosen as before.
        struct NView {
            std::optional<XBuffer> buf;
            XOffsetDatastructure::XDeltaBaseline base;
            // Lowest free slot first (keeps the view's vectors packed).
            XOffsetDatastructure::xsync::XBasicSyncSlots<XOffsetDatastructure::xsync::XSlotReuse::lowest> uslot, mslot;
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
            n.buf.emplace(XBuffer::create<xm::ViewSnapshot>(kViewInitial, held_cap()));
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
            n.buf.emplace(XBuffer::create<xm::ViewSnapshot>(kViewInitial, held_cap()));
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
            // id the view does not hold yet: one lookup either way. A new slot
            // is sized into the view by n_update.
            constexpr auto none = [](int64_t) {};
            for (int32_t id : p.removed) {
                const int64_t pos = n.uslot.find(uint32_t(id));
                if (pos < 0) continue;
                np.urm.push_back(int32_t(pos)); n.uslot.leave(uint32_t(id), pos);
            }
            for (int32_t id : p.removed_marches) {
                const int64_t pos = n.mslot.find(uint32_t(id));
                if (pos < 0) continue;
                np.mrm.push_back(int32_t(pos)); n.mslot.leave(uint32_t(id), pos);
            }
            bool fresh = false;
            for (int32_t i : p.units) {
                const int32_t pos = int32_t(n.uslot.take(uint32_t(w.unit(i).id), none, fresh));
                np.unew.push_back(fresh ? 1 : 0);
                np.uup.emplace_back(i, pos);
            }
            for (int32_t i : p.marches) np.mup.emplace_back(i, int32_t(n.mslot.take(uint32_t(w.march(i).id), none, fresh)));
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
            if (v.units.size() < n.uslot.size()) { v.units.resize(n.uslot.size()); b.touch(&v.units, sizeof(v.units)); }
            if (v.marches.size() < n.mslot.size()) { v.marches.resize(n.mslot.size()); b.touch(&v.marches, sizeof(v.marches)); }
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
            if (!h) h.emplace(XBuffer::create<xm::ViewSnapshot>(4096, held_cap()));
            return h;
        }
        void n_key_recv(int c, int, int b) {
            const XOffsetDatastructure::XStatus r = apply_delta_wire<xm::ViewSnapshot, kClient>(*n_slot(c), nb[std::size_t(b)].key, ncur[std::size_t(c)]);
            if (!r) throw std::runtime_error("native key delta rejected: " + std::string(r.error().message));
        }
        void n_apply(int c, int) {
            const XOffsetDatastructure::XStatus r = apply_delta_wire<xm::ViewSnapshot, kClient>(*nheld[std::size_t(c)], ndelta, ncur[std::size_t(c)]);
            if (!r) throw std::runtime_error("native delta rejected: " + std::string(r.error().message));
        }
        xm::ViewSnapshot& n_held(int c, int) { return nheld[std::size_t(c)]->template root<xm::ViewSnapshot>(); }
        // How the client's last apply was checked (report only): 0 key, 1 none, 2 in place, 3 full.
        int n_check(int c) const { return int(ncur[std::size_t(c)].last_check()); }
        std::string_view n_wire() const { return ndelta; }
        void n_apply_from(int c, const std::string& d) {
            const XOffsetDatastructure::XStatus r = apply_delta_wire<xm::ViewSnapshot, kClient>(*nheld[std::size_t(c)], d, ncur[std::size_t(c)]);
            if (!r) throw std::runtime_error("native delta rejected: " + std::string(r.error().message));
        }
#endif

        // ---- D3: shared deltas (xoffset_shared_delta.hpp, XS 2.0), keys D* ----
        // Server: one image of every entity as its holders have it (units in
        // dstore by world index, tiles and marches next to their entries),
        // shared by all connections. Every changed unit some view holds is
        // compared and encoded once per tick (d_publish); other units, marches
        // and tiles on their first use in the tick; key bodies (an entity
        // entering a view) are built once per tick on first use. A delivery
        // copies those bodies into its message under the client's slot
        // (d_send). Per connection the server keeps the message version and
        // the client's free slots (two cache lines), and cold, the stream and
        // free slots past them; per entity, the client's slot in the tracker
        // every format shares. Client: its own view buffer, indexed by slot
        // (no id index); d_recv checks the whole message, then applies it.
        // A shared body of this tick and the key body of this tick (ticks fit 32 bits).
        struct D3Ent { int32_t tick = -1, ktick = -1; uint32_t off = 0, len = 0, koff = 0, klen = 0; };
        // Units (d_unit): dirty flags of tick ptick (now) and of earlier ticks
        // no delivery used (old); rtick, the tick d_unit last ran.
        // hold: connections whose view holds the unit (enters sent minus leaves).
        struct D3UEnt : D3Ent { uint64_t now = 0, old = 0; int32_t ptick = -1, rtick = -1, hold = 0; };
        // Marches and tiles keep the image every holder has next to the entry:
        // one cache line per entity.
        struct alignas(64) D3MEnt : D3Ent { xm::ViewMarch prev{}; };
        struct D3TEnt : D3Ent { xm::Tile prev{}; };
        // The server gives each client's records their slots (the client
        // indexes its view by them, no id index). A slot that leaves is free
        // from the next message on (an entry appears at most once in a
        // message); an entering record takes the last freed slot, else a new
        // one.
        // Free slots below 256 are bytes in the connection's two cache lines
        // (the lines d_send reads for the version anyway; at scale M a client
        // has at most 36 unit and 48 march slots free); the rest go to the
        // connection's cold spill list.
        static constexpr unsigned kD3UFree = 48, kD3MFree = 64;
        // Per connection, hot: the message version and both tables' free slots.
        struct alignas(128) D3Conn {
            uint32_t version = 0;
            uint32_t uused = 0, mused = 0;     // slots given out so far
            uint8_t un = 0, mn = 0;            // uf[0, un) / mf[0, mn) free, the last taken first
            uint8_t uspill = 0, mspill = 0;    // the spill list is not empty
            uint8_t uf[kD3UFree]{}, mf[kD3MFree]{};
        };
        static_assert(sizeof(D3Conn) == 128);
        // Per connection, cold: the stream (key messages only) and the spill lists.
        struct D3Cold { uint64_t stream = 0; std::vector<int32_t> us, ms; };
        // Takes slots with its counters in registers (the message stores may
        // alias the connection): construct, take() per entering record, then done().
        struct D3Taker {
            const uint8_t* const f; uint8_t& n_at; uint32_t& used_at; uint8_t& spill_at; std::vector<int32_t>& sp;
            uint32_t n, used; bool spill;
            D3Taker(const uint8_t* fr, uint8_t& n0, uint32_t& u, uint8_t& s, std::vector<int32_t>& v)
                : f(fr), n_at(n0), used_at(u), spill_at(s), sp(v), n(n0), used(u), spill(s != 0) {}
            [[gnu::always_inline]] int32_t take() {
                if (n) return int32_t(f[--n]);
                if (spill) [[unlikely]] return from_spill();
                return int32_t(used++);
            }
            XOFFSET_DETAIL_NOINLINE int32_t from_spill() noexcept {
                const int32_t v = sp.back();
                sp.pop_back();
                spill = !sp.empty();
                return v;
            }
            void done() noexcept { n_at = uint8_t(n); used_at = used; spill_at = uint8_t(spill); }
        };
        static D3Taker d_utaker(D3Conn& k, D3Cold& kc) { return {k.uf, k.un, k.uused, k.uspill, kc.us}; }
        static D3Taker d_mtaker(D3Conn& k, D3Cold& kc) { return {k.mf, k.mn, k.mused, k.mspill, kc.ms}; }
        // Gives back the k slots at x.
        template <unsigned Cap>
        [[gnu::always_inline]] static void d_give(uint8_t* f, uint8_t& n_at, uint8_t& spill, std::vector<int32_t>& sp, const int32_t* x, std::size_t k) {
            uint32_t n = n_at;
            for (std::size_t i = 0; i < k; ++i) {
                const uint32_t v = uint32_t(x[i]);
                if (n < Cap && v < 256) f[n++] = uint8_t(v);
                else [[unlikely]] { d_spill(sp, x[i]); spill = 1; }
            }
            n_at = uint8_t(n);
        }
        XOFFSET_DETAIL_NOINLINE static void d_spill(std::vector<int32_t>& sp, int32_t v) { sp.push_back(v); }
        // A client: its view (units and marches at the slots the server gave
        // them, no id index), its cursor, and for the full check which slots
        // it holds.
        struct D3Client {
            std::optional<XBuffer> view;
            XOffsetDatastructure::xsync::XSyncCursor cursor;
            std::vector<uint8_t> uheld, mheld;   // full check only: slot held
            TileSpan span{};
        };
        enum : unsigned { kTRoot = 0, kTUnit = 1, kTMarch = 2, kTTile = 3 };
        template <class T> using RD = XOffsetDatastructure::xsync::XRecordBody<T>;
        // A client's view as the records XReceiver::apply_sync writes: the
        // root fields (through a D3Root copy), units and marches at the slots
        // the server gave them (an op's id is the slot), tiles by tile id
        // inside the view's span. Full: find() also answers whether the slot
        // is held (the full check rejects an op on a slot not held and an
        // enter of one held); the trust tier takes the sender's word. Adding a
        // slot may move the view; its root is re-read if the buffer moved.
        // The target's calls on the receive path are forced inline: this
        // translation unit (every format, every case) reaches GCC's
        // inline-unit-growth limit, past which whether they inline depends on
        // unrelated code.
#define SLG_D3_HOT [[gnu::always_inline]]
#define SLG_D3_FLAT [[gnu::always_inline, gnu::flatten]]
        template <bool Full>
        struct D3Target {
            using tables = std::tuple<xm::D3Root, xm::ViewUnit, xm::ViewMarch, xm::Tile>;
            template <unsigned K> static constexpr bool keyed = K == kTUnit || K == kTMarch;
            D3Client* L;
            xm::ViewSnapshot* v;
            const void* base;
            xm::D3Root root{};
            explicit D3Target(D3Client& c) : L(&c), v(&c.view->template root<xm::ViewSnapshot>()), base(c.view->get_address()) {}
            template <unsigned K> SLG_D3_FLAT int64_t find(uint32_t id) {
                if constexpr (K == kTRoot) {
                    if (id != 0) return -1;
                    root = xm::D3Root{v->request_id, v->tick};
                    return 0;
                } else if constexpr (K == kTTile) {
                    const uint32_t tx = id % uint32_t(kTiles) - uint32_t(L->span.tx0), ty = id / uint32_t(kTiles) - uint32_t(L->span.ty0);
                    if (tx >= uint32_t(L->span.cols) || ty >= uint32_t(L->span.rows)) return -1;
                    return int64_t(ty) * L->span.cols + tx;
                } else if constexpr (Full) {
                    const std::vector<uint8_t>& h = K == kTUnit ? L->uheld : L->mheld;
                    return id < h.size() && h[id] ? int64_t(id) : -1;
                } else {
                    if constexpr (K == kTUnit) return id < v->units.size() ? int64_t(id) : -1;
                    else return id < v->marches.size() ? int64_t(id) : -1;
                }
            }
            template <unsigned K> SLG_D3_HOT auto& record(int64_t h) {
                if constexpr (K == kTRoot) return root;
                else if constexpr (K == kTUnit) return v->units[std::size_t(h)];
                else if constexpr (K == kTMarch) return v->marches[std::size_t(h)];
                else return v->tiles[std::size_t(h)];
            }
            template <unsigned K> SLG_D3_HOT int64_t enter(uint32_t id) {
                // A slot past the end grows the view's vector.
                const std::size_t n = K == kTUnit ? v->units.size() : v->marches.size();
                if (id >= n) [[unlikely]] {
                    if constexpr (K == kTUnit) v->units.resize(std::size_t(id) + 1);
                    else v->marches.resize(std::size_t(id) + 1);
                    if (L->view->get_address() != base) { base = L->view->get_address(); v = &L->view->template root<xm::ViewSnapshot>(); }
                }
                if constexpr (Full) {
                    std::vector<uint8_t>& h = K == kTUnit ? L->uheld : L->mheld;
                    if (id >= h.size()) h.resize(std::size_t(id) + 1);
                    h[id] = 1;
                }
                return int64_t(id);
            }
            template <unsigned K> SLG_D3_HOT void leave(uint32_t id, int64_t h) {
                RD<std::tuple_element_t<K, tables>>::clear(record<K>(h));   // free slots stay zeroed (query_view)
                if constexpr (Full) (K == kTUnit ? L->uheld : L->mheld)[id] = 0;
                else (void)id;
            }
            template <unsigned K> SLG_D3_HOT void applied(uint32_t, int64_t) {
                if constexpr (K == kTRoot) { v->request_id = root.request_id; v->tick = root.tick; }
            }
        };
#undef SLG_D3_FLAT
#undef SLG_D3_HOT
        static XOffsetDatastructure::XReceiver<XOffsetDatastructure::XLink::from_server>& d_rx() {
            static XOffsetDatastructure::XReceiver<XOffsetDatastructure::XLink::from_server> rx(
                XBuffer::maximum_capacity(XBuffer::DEFAULT_MAXIMUM_CAPACITY));
            return rx;
        }
        static constexpr std::size_t kD3UnitBytes = 1024;
        // Section room per unit op: a body up to kD3OpBody bytes plus slack.
        static constexpr std::size_t kD3OpBody = 224, kD3OpRoom = kD3OpBody + XOffsetDatastructure::xsync::kSyncSlack;
        // A march change op (id, member mask, members) fits in this.
        static constexpr std::size_t kD3MarchBody = 48;
        static_assert(4 + RD<xm::ViewMarch>::diff_room <= kD3MarchBody);
        std::optional<XBuffer> dstore;   // every unit as its holders have it (the shared image)
        std::vector<D3UEnt> due;
        std::vector<D3TEnt> dte;
        // Marches by id in a ring of a power-of-two size (slot id & (size - 1);
        // march ids only grow, so an id-indexed array would grow forever): a
        // slot whose march has left the world is reused, a live one makes the
        // ring double.
        std::vector<D3MEnt> dme;
        XOffsetDatastructure::xsync::XSyncOut darena, dmsg;   // this tick's bodies; the message being built
        int64_t dtick = -1;   // the world's tick (entries keep it as int32_t)
        int32_t ducap = 0, dmcap = 0;   // view capacity, as NP's (n_setup)
        std::vector<D3Conn> dconn;
        std::vector<D3Cold> dcold;
        std::vector<D3Client> dcli;
        std::vector<D3Client> dclt;   // the same clients in the trust tier (d_recv_trusted)
        // A dirty flag's bit is its ViewUnit member's index (d_publish).
#define SLG_X(f) static_assert(XOffsetDatastructure::schema::member_t<xm::ViewUnit, dbit::f>::offset == offsetof(xm::ViewUnit, f));
        SLG_VUNIT_SCALARS(SLG_X)
        SLG_X(name) SLG_X(alliance_tag) SLG_X(owner_name) SLG_X(commander) SLG_X(heroes) SLG_X(troops) SLG_X(buffs)
#undef SLG_X
        static_assert(RD<xm::ViewUnit>::members::N == std::size_t(dbit::buffs) + 1);

#if defined(__GNUC__) || defined(__clang__)
#define SLG_PREFETCH(p) __builtin_prefetch(p)
#else
#define SLG_PREFETCH(p) ((void)(p))
#endif
        // Clients 0..clients-1, then the warm devices' views (clients + d), as held().
        void d_init(const MapWorld& w) {
            const ViewScale& vs = w.scale();
            ducap = int32_t(int64_t(3) * vs.units * kViewW * kViewH / (int64_t(2) * kMapSize * kMapSize)) + 32;
            dmcap = vs.marches / 4 + 32;
            const std::size_t views = std::size_t(vs.clients + warm_devices(vs.clients));
            dstore.emplace(XBuffer::create<xm::ViewSnapshot>(std::size_t(4) << 20, cap(true)));
            auto& s = dstore->template root<xm::ViewSnapshot>();
            s.units.resize(std::size_t(vs.units));
            for (int32_t i = 0; i < vs.units; ++i) copy_unit(s.units[std::size_t(i)], w.unit(i));
            dte.assign(std::size_t(kTiles) * kTiles, {});
            for (int32_t t = 0; t < kTiles * kTiles; ++t) copy_vtile(dte[std::size_t(t)].prev, w, t % kTiles, t / kTiles);
            due.assign(std::size_t(vs.units), {});
            dme.assign(std::bit_ceil(std::size_t(vs.marches) * 4 + 64), {});
            dnext = 0;
            for (int32_t j = 0; j < w.march_count(); ++j) dnext = std::max(dnext, w.march(j).id + 1);
            dconn.assign(std::size_t(vs.clients), {});
            dcold.clear();
            dcold.resize(std::size_t(vs.clients));
            dcli.clear();
            dcli.resize(views);
            dclt.clear();
            dclt.resize(views);
            dtick = w.tick_no();
            darena.clear();
            darena.reserve(4096);
        }
        // The client's view: its rect, reserved slots, the tiles under it
        // (zero until its key message fills them). Untimed, as n_view.
        void d_view(int c, const ViewRect& r) {
            for (D3Client* L : {&dcli[std::size_t(c)], &dclt[std::size_t(c)]}) {
                // The view sized for its records and their containers up front
                // (the rival's views reserve their vectors; a view that grows
                // later relocates inside a timed receive).
                L->view.emplace(XBuffer::create<xm::ViewSnapshot>(std::min(std::size_t(ducap) * kD3UnitBytes + kViewInitial, kHeldMax), held_cap()));
                auto& v = L->view->template root<xm::ViewSnapshot>();
                v.x0 = r.x0; v.y0 = r.y0; v.x1 = r.x1; v.y1 = r.y1;
                v.units.reserve(std::size_t(ducap));
                v.marches.reserve(std::size_t(dmcap));
                L->uheld.assign(std::size_t(ducap), 0);
                L->mheld.assign(std::size_t(dmcap), 0);
                L->span = tile_span(r.x0, r.y0, r.x1, r.y1);
                v.tiles.resize(std::size_t(L->span.cols * L->span.rows));
            }
        }
        xm::ViewSnapshot& d_held(int c) { return dcli[std::size_t(c)].view->template root<xm::ViewSnapshot>(); }
        xm::ViewSnapshot& d_held_trusted(int c) { return dclt[std::size_t(c)].view->template root<xm::ViewSnapshot>(); }
        std::string_view d_wire() const { return dmsg.view(); }

        // Server memory (--mode mem, MEM.D*): per connection, and the image,
        // entries and arenas every connection shares.
        std::size_t d_conn_bytes() const {
            std::size_t spill = 0;
            for (const D3Cold& k : dcold) spill += (k.us.capacity() + k.ms.capacity()) * sizeof(int32_t);
            return sizeof(D3Conn) + sizeof(D3Cold) + (dcold.empty() ? 0 : spill / dcold.size());
        }
        std::size_t d_shared_bytes() const {
            if (!dstore) return 0;
            return dstore->get_size() + due.capacity() * sizeof(D3UEnt) + dte.capacity() * sizeof(D3TEnt) + dme.capacity() * sizeof(D3MEnt) +
                   darena.capacity() + dmsg.capacity();
        }


        // Once per tick: the dirty flags of every unit the world changed are
        // noted, and every changed unit some connection holds is copied,
        // compared and encoded (each once, for all its holders). A unit no
        // one holds waits for its first use (a key: an entering view), so a
        // small server whose viewports cover a fraction of the world does not
        // encode changes no one receives.
        void d_publish(const MapWorld& w) {
            dtick = w.tick_no();
            darena.clear();
            const auto& cu = w.changed_units();
            // Three passes, so the cache misses of different units overlap:
            // the entries, then the images of held units, then the encoding.
            for (int32_t i : cu) SLG_PREFETCH(&due[std::size_t(i)]);
            auto& units = dstore->template root<xm::ViewSnapshot>().units;
            for (int32_t i : cu) {
                D3UEnt& e = due[std::size_t(i)];
                if (e.ptick != dtick) { e.old |= e.now; e.now = 0; e.ptick = int32_t(dtick); }
                e.now |= w.dirty(i);
                if (e.hold > 0) {
                    const char* r = reinterpret_cast<const char*>(&units[std::size_t(i)]);
                    for (std::size_t o = 0; o < sizeof(xm::ViewUnit); o += 64) SLG_PREFETCH(r + o);
                }
            }
            for (int32_t i : cu) if (due[std::size_t(i)].hold > 0) d_unit(w, i);
            // Tiles whose owner changed: their entries are fetched now (one
            // line each, all at once); they are encoded on first use.
            for (int32_t t : w.changed_tiles()) SLG_PREFETCH(&dte[std::size_t(t)]);
            // The same for the marches that started this tick (the world
            // appends them: the ids from dnext on, at the end).
            for (int32_t j = w.march_count() - 1; j >= 0 && w.march(j).id >= dnext; --j)
                SLG_PREFETCH(&dme[std::size_t(w.march(j).id) & (dme.size() - 1)]);
            if (const int32_t n = w.march_count()) dnext = std::max(dnext, w.march(n - 1).id + 1);
        }
        int32_t dnext = 0;   // the lowest march id d_publish has not seen
        // Unit i on its first use in the tick: the members changed since the
        // store last saw it are copied in from the world. Changes of earlier
        // ticks that no delivery used (old) had no holder, so none holds the
        // unit now except through a key of this tick: the image takes them
        // and no change op goes out. Otherwise this tick's change is compared
        // with the image, over the changed members only, and encoded: the
        // same body as comparing whole records. Returns the entry; e.tick ==
        // dtick when the unit has a change body this tick.
        D3UEnt& d_unit(const MapWorld& w, int32_t i) {
            D3UEnt& e = due[std::size_t(i)];
            if (e.rtick == dtick) return e;
            return d_unit_fill(w, i);
        }
        // The first use in the tick, out of line: a delivery's loop stays
        // small (it runs between other work that evicts the I-cache).
        XOFFSET_DETAIL_NOINLINE D3UEnt& d_unit_fill(const MapWorld& w, int32_t i) {
            D3UEnt& e = due[std::size_t(i)];
            e.rtick = dtick;
            const uint64_t now = e.ptick == dtick ? e.now : 0;
            const uint64_t old = e.old | (e.ptick == dtick ? 0 : e.now);
            const uint64_t m = now | old;
            e.now = e.old = 0;
            if (!m) return e;
            auto& d = dstore->template root<xm::ViewSnapshot>().units[std::size_t(i)];
            const WorldUnit& u = w.unit(i);
            const auto set = [&](xm::ViewUnit& r) {
#define SLG_X(f) if (m & SLG_D(f)) r.f = u.f;
                SLG_VUNIT_SCALARS(SLG_X)
#undef SLG_X
                if (m & SLG_D(name)) set_str(r.name, u.name);
                if (m & SLG_D(alliance_tag)) set_str(r.alliance_tag, u.alliance_tag);
                if (m & SLG_D(owner_name)) set_str(r.owner_name, u.owner_name);
                if (m & SLG_D(commander)) copy_uhero(r.commander, u.commander);
                if (m & SLG_D(heroes)) copy_list(r.heroes, u.heroes, [](auto& a, const auto& x) { copy_uhero(a, x); });
                if (m & SLG_D(troops)) copy_list(r.troops, u.troops, [](auto& a, const auto& x) { a.type = x.type; a.count = x.count; a.wounded = x.wounded; });
                if (m & SLG_D(buffs)) copy_list(r.buffs, u.buffs, [](auto& a, const auto& x) { a.id = x.id; a.stacks = x.stacks; a.expire_ms = x.expire_ms; });
            };
            if (old) { set(d); return e; }   // no holder: the image catches up, nothing is sent
            // This tick's change, compared over the changed members only (the
            // rest of the image is unchanged): the body a whole-record
            // comparison with last tick's image writes.
            // The change body straight from the dirty flags (a dirty flag's
            // bit is its member's): no compare.
            set(d);
            e.off = uint32_t(darena.size());
            e.len = uint32_t(XOffsetDatastructure::xsync::put_change_op(darena, uint32_t(u.id), d, now));
            e.tick = dtick;
            return e;
        }
        // A march's body for this tick (compared with the image every holder
        // has, on first use in the tick) and its key body.
        D3MEnt& d_march(const MapWorld& w, int32_t i) {
            const WorldMarch& m = w.march(i);
            D3MEnt& e = dme[std::size_t(m.id) & (dme.size() - 1)];
            if (e.prev.id == m.id && e.tick == dtick) return e;
            return d_march_fill(w, i);
        }
        XOFFSET_DETAIL_NOINLINE D3MEnt& d_march_fill(const MapWorld& w, int32_t i) {
            const WorldMarch& m = w.march(i);
            D3MEnt* e = &dme[std::size_t(m.id) & (dme.size() - 1)];
            if (e->prev.id != m.id) {
                if (e->prev.id != 0 && e->prev.id >= d_min_live(w)) e = &d_march_grow(w, m.id);
                else *e = D3MEnt{};
            }
            if (e->tick != dtick) {
                xm::ViewMarch img;
                copy_vmarch(img, m);
                if (e->prev.id == 0) { e->prev = img; e->len = 0; }
                else d_diff(*e, uint32_t(m.id), img, e->prev);
                e->tick = int32_t(dtick);
            }
            return *e;
        }
        // The lowest id of a march in the world this tick (found on the first
        // slot taken in the tick): a march with a lower id has left it.
        int32_t d_min_live(const MapWorld& w) {
            if (dmin_tick != dtick) {
                dmin_tick = dtick;
                dmin_live = std::numeric_limits<int32_t>::max();
                for (int32_t j = 0; j < w.march_count(); ++j) dmin_live = std::min(dmin_live, w.march(j).id);
            }
            return dmin_live;
        }
        int32_t dmin_live = 0;
        int64_t dmin_tick = -1;
        // A live march holds id's slot: double the ring until every live march
        // has a slot of its own. Returns id's (empty) entry.
        XOFFSET_DETAIL_NOINLINE D3MEnt& d_march_grow(const MapWorld& w, int32_t id) {
            for (std::size_t n = dme.size() * 2;; n *= 2) {
                std::vector<D3MEnt> r(n);
                bool ok = true;
                for (const D3MEnt& e : dme) {
                    if (e.prev.id == 0 || e.prev.id < d_min_live(w)) continue;
                    D3MEnt& to = r[std::size_t(e.prev.id) & (n - 1)];
                    if (to.prev.id != 0) { ok = false; break; }
                    to = e;
                }
                if (!ok || r[std::size_t(id) & (n - 1)].prev.id != 0) continue;
                dme.swap(r);
                return dme[std::size_t(id) & (n - 1)];
            }
        }
        XOFFSET_DETAIL_NOINLINE void d_march_refind(const MapWorld& w, const int32_t* pm, const int32_t* ent, D3MEnt** entp, std::size_t ne) {
            for (std::size_t r = 0; r < ne; ++r) entp[r] = &d_march(w, pm[ent[r]]);
        }
        D3TEnt& d_tile(const MapWorld& w, int32_t t) {
            D3TEnt& e = dte[std::size_t(t)];
            if (e.tick != dtick) d_tile_fill(w, t);
            return e;
        }
        XOFFSET_DETAIL_NOINLINE void d_tile_fill(const MapWorld& w, int32_t t) {
            D3TEnt& e = dte[std::size_t(t)];
            {
                xm::Tile img;
                copy_vtile(img, w, t % kTiles, t / kTiles);
                d_diff(e, uint32_t(t), img, e.prev);
                e.tick = int32_t(dtick);
            }
        }
        XOFFSET_DETAIL_NOINLINE void d_unit_key(const MapWorld& w, int32_t i) {
            D3UEnt& e = d_unit(w, i);
            if (e.ktick == dtick) return;
            const xm::ViewUnit& r = dstore->template root<xm::ViewSnapshot>().units[std::size_t(i)];
            e.koff = uint32_t(darena.size());
            e.klen = uint32_t(XOffsetDatastructure::xsync::put_whole_op(darena, uint32_t(w.unit(i).id), r));
            e.ktick = int32_t(dtick);
        }
        // A plain record's change against the image every holder has (the
        // members that differ; the image takes them).
        template <class R> void d_diff(D3Ent& e, uint32_t id, const R& now, R& prev) {
            e.off = uint32_t(darena.size());
            e.len = uint32_t(XOffsetDatastructure::xsync::put_diff_op(darena, id, now, prev));
        }
        using D3Writer = XOffsetDatastructure::xsync::XSyncWriter;
        static constexpr auto kEnter = XOffsetDatastructure::xsync::enter, kChange = XOffsetDatastructure::xsync::change;
        std::vector<int32_t> dscr;
        std::vector<void*> dpscr;
        int32_t* d_scratch(std::size_t n) { if (dscr.size() < n) dscr.resize(n * 2); return dscr.data(); }
        void** d_pscratch(std::size_t n) { if (dpscr.size() < n) dpscr.resize(n * 2); return dpscr.data(); }
        void d_put(D3Writer& m, unsigned table, XOffsetDatastructure::xsync::XSyncKind kind, uint32_t off, uint32_t len) {
            m.op(darena.data() + off, len);
#ifdef SLG_D3_STATS
            dstat_bytes[table * 3 + kind] += double(len);
            dstat_body[table * 3 + kind] += double(len);
            dstat_ops[table * 3 + kind] += 1;
#endif
        }
#ifdef SLG_D3_STATS
        // Diagnostic build only (-DSLG_D3_STATS): message bytes by table and kind.
        double dstat_bytes[12] = {}, dstat_body[12] = {}, dstat_ops[12] = {}, dstat_msgs = 0, dstat_total = 0;
#endif
        // The key message of client c's first pull (untimed): every unit,
        // march and tile in its view.
        void d_key(int c, const MapWorld& w, const DeltaPlan& p) {
            D3Conn& k = dconn[std::size_t(c)];
            D3Cold& kc = dcold[std::size_t(c)];
            k = D3Conn{};
            kc.us.clear();
            kc.ms.clear();
            kc.stream = 0x5d3000000ull + uint64_t(c);
            D3Writer m(dmsg, xm::D3Root{0, w.tick_no()}, ++k.version, kc.stream);
            m.begin(kTUnit, kEnter);
            auto tu = d_utaker(k, kc);
            for (std::size_t q = 0; q < p.units.size(); ++q) {
                const int32_t i = p.units[q]; d_unit_key(w, i); D3UEnt& e = due[std::size_t(i)]; ++e.hold;
                const int32_t s = tu.take(); *p.uref[q] = s;
                m.op(darena.data() + e.koff, e.klen, uint32_t(s));
            }
            tu.done();
            m.end();
            m.begin(kTMarch, kEnter);
            auto tm = d_mtaker(k, kc);
            for (std::size_t q = 0; q < p.marches.size(); ++q) {
                D3MEnt& e = d_march(w, p.marches[q]);
                const int32_t s = tm.take(); *p.mref[q] = s;
                m.whole(uint32_t(s), e.prev);
            }
            tm.done();
            m.end();
            m.begin(kTTile, kEnter);
            const TileSpan sp = dcli[std::size_t(c)].span;
            for (int32_t ty = sp.ty0; ty < sp.ty0 + sp.rows; ++ty)
                for (int32_t tx = sp.tx0; tx < sp.tx0 + sp.cols; ++tx) {
                    const int32_t t = ty * kTiles + tx;
                    m.whole(uint32_t(t), dte[std::size_t(t)].prev);
                }
            m.end();
            m.finish();
        }
        // One delivery: leaves, the changes of units, marches and tiles, then
        // the units and marches entering the view, each a copy of the
        // entity's body of this tick. The connection's lines are fetched at
        // the start and first used by the enters (and the version, written
        // at the end).
        std::size_t d_send(int c, const MapWorld& w, const DeltaPlan& p, int64_t id) {
            D3Conn& k = dconn[std::size_t(c)];
            __builtin_prefetch(&k, 1);
            __builtin_prefetch(reinterpret_cast<const char*>(&k) + 64, 1);
#ifdef SLG_D3_STATS
            if (dstat_msgs == 0) for (int q = 0; q < 12; ++q) dstat_bytes[q] = dstat_body[q] = dstat_ops[q] = 0;   // drop the key messages
#endif
            D3Writer m(dmsg, xm::D3Root{id, w.tick_no()}, 0);   // the version: at finish
            if (!p.rslot.empty()) {
                m.leaves(kTUnit, p.rslot.data(), p.rslot.size());
                for (int32_t uid : p.removed) --due[std::size_t(uid - 1)].hold;   // unit id = index + 1
            }
            if (!p.rmslot.empty()) m.leaves(kTMarch, p.rmslot.data(), p.rmslot.size());
            // Plan arrays as locals: the message stores may alias anything.
            const int32_t* const pu = p.units.data();
            const uint8_t* const pun = p.unew.data();
            const int32_t* const pus = p.uslot.data();
            int32_t* const* const pur = p.uref.data();
            const int32_t* const pm = p.marches.data();
            const uint8_t* const pmn = p.mnew.data();
            const int32_t* const pms = p.mslot.data();
            int32_t* const* const pmr = p.mref.data();
            const std::size_t nu = p.units.size(), nm = p.marches.size();
            // Entering units and marches: their places in the plan.
            int32_t* const uent = d_scratch(nu + nm);
            int32_t* const ment = uent + nu;
            std::size_t une = 0, mne = 0;
            if (nu) {
                // Room for the section up front (kD3OpRoom per op); an op past
                // kD3OpBody re-reserves for itself and the rest.
                m.begin(kTUnit, kChange);
                m.reserve(nu * kD3OpRoom);
                for (std::size_t q = 0; q < nu; ++q) {
                    const int32_t i = pu[q];
                    if (pun[q]) { uent[une++] = int32_t(q); continue; }
                    if (const D3Ent& e = d_unit(w, i); e.tick == dtick) {
                        if (e.len > kD3OpBody) [[unlikely]] m.reserve((nu - q) * kD3OpRoom + e.len);
                        m.op_in_room(darena.data() + e.off, e.len, uint32_t(pus[q]));
                    }
                }
                m.end();
            }
            D3MEnt** const mentp = reinterpret_cast<D3MEnt**>(d_pscratch(nm));
            bool moved = false;   // a fill grew the ring: the noted entries moved
            if (nm) {
                // The ring and the world's marches as locals (message stores
                // may alias them); a fill may grow the ring: reload then.
                D3MEnt* const dm0 = dme.data();
                D3MEnt* dm = dm0;
                std::size_t mk = dme.size() - 1;
                const WorldMarch* const wm = &w.march(0);
                const int32_t t32 = int32_t(dtick);
                m.begin(kTMarch, kChange);
                for (std::size_t q = 0; q < nm; ++q) {
                    const int32_t i = pm[q];
                    const int32_t mid = wm[i].id;
                    D3MEnt* e = &dm[std::size_t(mid) & mk];
                    if (e->prev.id != mid || e->tick != t32) [[unlikely]] { e = &d_march_fill(w, i); dm = dme.data(); mk = dme.size() - 1; }
                    // No branch on the kind: an entering march is noted (mne
                    // advances), a changed one's body copied (length 0 when new
                    // or unchanged); a march change body is at most 4 + 1 + 40 bytes.
                    const bool isnew = pmn[q] != 0;
                    ment[mne] = int32_t(q); mentp[mne] = e; mne += isnew;
                    m.op_upto<kD3MarchBody>(darena.data() + e->off, isnew ? 0 : e->len, uint32_t(pms[q]));
                }
                m.end();
                moved = dm != dm0;
            }
            if (!p.tiles.empty()) {
                m.begin(kTTile, kChange);
                for (int32_t t : p.tiles) {
                    const D3Ent& e = d_tile(w, t);
                    if (e.len) d_put(m, kTTile, kChange, e.off, e.len);
                }
                m.end();
            }
            if (une) {
                m.begin(kTUnit, kEnter);
                m.reserve(une * kD3OpRoom);
                auto tk = d_utaker(k, dcold[std::size_t(c)]);
                for (std::size_t r = 0; r < une; ++r) {
                    const std::size_t q = std::size_t(uent[r]); const int32_t i = pu[q];
                    d_unit_key(w, i); D3UEnt& e = due[std::size_t(i)]; ++e.hold;
                    if (e.klen > kD3OpBody) [[unlikely]] m.reserve((une - r) * kD3OpRoom + e.klen);
                    const int32_t s = tk.take(); *pur[q] = s;
                    m.op_in_room(darena.data() + e.koff, e.klen, uint32_t(s));
                }
                tk.done();
                m.end();
            }
            if (mne) {
                if (moved) [[unlikely]] d_march_refind(w, pm, ment, mentp, mne);
                m.begin(kTMarch, kEnter);
                auto tk = d_mtaker(k, dcold[std::size_t(c)]);
                for (std::size_t r = 0; r < mne; ++r) { const int32_t s = tk.take(); *pmr[ment[r]] = s; m.whole(uint32_t(s), mentp[r]->prev); }
                tk.done();
                m.end();
            }
            // The slots that left are free from the next message on (an entry
            // appears at most once in a message).
            d_give<kD3UFree>(k.uf, k.un, k.uspill, dcold[std::size_t(c)].us, p.rslot.data(), p.rslot.size());
            d_give<kD3MFree>(k.mf, k.mn, k.mspill, dcold[std::size_t(c)].ms, p.rmslot.data(), p.rmslot.size());
            const std::size_t n = m.finish(++k.version);
#ifdef SLG_D3_STATS
            dstat_msgs += 1; dstat_total += double(n);
#endif
            return n;
        }
        // Trust tier (susu 2026-11-04: server -> client over a trusted
        // transport): XReceiver<from_server>::apply_sync with its default
        // check, admit (head, stream and version, every section's table and
        // kind, every op's id; bodies are the sender's). Same view layout as
        // d_recv.
        void d_recv_trusted(int c) { d_recv_trusted_from(c, dmsg.view()); }
        void d_recv_trusted_from(int c, std::string_view msg) {
            D3Client& L = dclt[std::size_t(c)];
            D3Target<false> t(L);
            const XOffsetDatastructure::XStatus r = d_rx().apply_sync(msg, L.cursor, t);
            if (!r) throw std::runtime_error("D3 (trust tier): " + r.error().message);
        }
        // The client: XReceiver<from_server>::apply_sync with XCheck::full,
        // every op checked against its view (slots held or not, bodies against
        // their record types), then applied. A rejected message changes nothing.
        void d_recv(int c) { d_recv_from(c, dmsg.view()); }
        void d_recv_from(int c, std::string_view msg) {
            D3Client& L = dcli[std::size_t(c)];
            D3Target<true> t(L);
            const XOffsetDatastructure::XStatus r = d_rx().template apply_sync<XOffsetDatastructure::XCheck::full>(msg, L.cursor, t);
            if (!r) throw std::runtime_error("D3: " + r.error().message);
        }
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
                    slots += c.uslot.capacity_bytes() + c.mslot.capacity_bytes();
                }
                const std::string N = "MEM." + ViewCases::keys(i).nname;
                st.bytes[N + ".view"] = v / n; st.bytes[N + ".baseline"] = bl / n; st.bytes[N + ".slots"] = slots / n;
            }
#endif
            // D3 (keys D*): per connection, and what every connection shares
            // (whole server, and divided by the connections for the per-client table).
            if (const std::size_t n = ops_[i].dconn.size()) {
                const std::string D = "MEM." + ViewCases::keys(i).dname;
                st.bytes[D + ".conn"] = ops_[i].d_conn_bytes();
                st.bytes[D + ".shared"] = ops_[i].d_shared_bytes();
                st.bytes[D + ".shared_per_conn"] = ops_[i].d_shared_bytes() / n;
            }
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
    static constexpr std::size_t kUplinkInitial = 4 * 1024;   // UL orders (uplink.hpp)

    // Player, battle and report buffers are Relocating (growth policy, 3.6
    // memory gate 2026-11-11): a slot first, a move when one outgrows it, so
    // players no longer reserve a 256 MiB VM region each (page tables
    // 13.1 -> 5.6 MiB at scale M with kHeldMax). Their maximum is
    // 256 MiB, or SLG_XOFFSET_CAP_MIB=N MiB (an iPhone gives a process about
    // 3.4 GiB of address space, docs/DEPLOYMENT.md, SC-12). The shard keeps a
    // 256 MiB reservation of its own (Automatic, pooled under reuse).
    // Verify passes in an XOFFSET_STRESS_RELOCATE build (a moved-from payload
    // is poisoned and unmapped): no path verify runs reads a stale root.
    static XOffsetDatastructure::XBuffer::MaximumCapacity cap(bool shard = false) {
        using XOffsetDatastructure::XBuffer;
        static const std::size_t small = [] {
            const char* v = std::getenv("SLG_XOFFSET_CAP_MIB");
            const long mib = v ? std::strtol(v, nullptr, 10) : 0;
            return mib > 0 ? std::size_t(mib) << 20 : std::size_t(256) << 20;
        }();
        if (!shard) return XBuffer::relocating(small);
        auto c = XBuffer::maximum_capacity(std::size_t(256) << 20)
                     .with_growth(XOffsetDatastructure::XGrowPolicy::Automatic);
        return use_pool() || reuse_mode() ? c.with_pool(pool()) : c;
    }
#if defined(XOFFSET_PROXY_BATCH_API)
    // Growth policy (3.6): a buffer a sender builds its messages in is
    // Relocating (1 MiB reserved, moved only when a message outgrows it) and
    // is filled inside a pin scope: plain references at plain speed, and no
    // move can leave one dangling. Received and resident buffers keep cap().
    // The reservation is explicit: build<T> runs a fill again when the
    // reservation is too small, and the case fills draw from the shared Rng,
    // so a second run would change every later message (verify would show
    // it). 1 MiB holds every SLG message; the senders are a few long-lived
    // buffers, so it costs no page tables to speak of.
    static XOffsetDatastructure::XBuffer::MaximumCapacity build_cap() {
        using XOffsetDatastructure::XBuffer;
        return XBuffer::relocating().with_reserve(XBuffer::size_type(1) << 20);   // a 1 MiB slot
    }
    // One message: XBuffer::build<T> (reset<T>(), then fill(T&) in a pin scope).
    template <class T, class Fill>
    [[gnu::always_inline]] static inline void build_in(XOffsetDatastructure::XBuffer& b, Fill&& fill) {
        if (!b.template build<T>([&](T& m) { fill(m); })) [[unlikely]]
            throw std::runtime_error(std::string("message build failed: ") + XOffsetDatastructure::to_string(b.error()));
    }
    // A message refilled in place: fill(T&) in a pin scope on the root.
    template <class T, class Fill>
    [[gnu::always_inline]] static inline void edit_in(XOffsetDatastructure::XBuffer& b, Fill&& fill) {
        if (!b.template edit<T>([&](T& m) { fill(m); })) [[unlikely]]
            throw std::runtime_error(std::string("message edit failed: ") + XOffsetDatastructure::to_string(b.error()));
    }
#else
    static XOffsetDatastructure::XBuffer::MaximumCapacity build_cap() { return cap(); }
    template <class T, class Fill>
    [[gnu::always_inline]] static inline void build_in(XOffsetDatastructure::XBuffer& b, Fill&& fill) { fill(b.template reset<T>()); }
    template <class T, class Fill>
    [[gnu::always_inline]] static inline void edit_in(XOffsetDatastructure::XBuffer& b, Fill&& fill) { fill(b.template root<T>()); }
#endif
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
