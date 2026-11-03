// SLG prototype: runner for XOffset V2. Each player and the map shard live in their own
// XBuffer; messages are built in, saved from and loaded into arenas, and the game logic
// runs in place on the received arena. Requires P2996 Clang.
#pragma once
#include <algorithm>
#include <cstdint>
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

namespace slg {

struct XoModel {
    static constexpr const char* name = "xoffset";
    using PlayerState = xm::PlayerState; using BattleMsg = xm::BattleMsg;
    using ShardState = xm::ShardState; using Report = xm::Report;
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
            timed(st, "A.simulate", [&] { simulate_battle(m->root<BattleMsg>()); });
            timed(st, "A.res_encode", [&] { res = m->save_verified<BattleMsg>(); });
            timed(st, "A.res_decode", [&] {
                prefetch_for_apply(pr);
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
            timed(st, "B.season_edits", [&] {
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
            timed(st, "C.ticks", [&] { shard_ticks(r->root<ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            timed(st, "C.snapshot2", [&] { snap2 = r->save_verified<ShardState>(); });
            timed(st, "C.replace", [&] { *shard_ = std::move(*r); });
            st.bytes["C.snapshot"] += snap.size();
            st.bytes["C.snapshot2"] += snap2.size();
            if (keep) keep->shard = snap2;
            timed(st, "C.cleanup", [&] { r.reset(); std::string().swap(snap); std::string().swap(snap2); });
            {
                // Untimed copy of the current shard for the middle-erase stress item.
                auto m = XOffsetDatastructure::XBuffer::load_verified<ShardState>(shard_->save_verified<ShardState>(), cap(true));
                timed(st, "S.ticks_middle", [&] { shard_ticks_middle_erase(m.template root<ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
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
            timed(st, "A.simulate", [&] { simulate_battle(a_m_->root<BattleMsg>()); });
            timed(st, "A.res_encode", [&] { a_m_->save_verified_into<BattleMsg>(res_); });
            timed(st, "A.res_decode", [&] { prefetch_for_apply(pr); });
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
            timed(st, "B.scene_edits", [&] {
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
            timed(st, "B.season_edits", [&] {
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
            timed(st, "C.ticks", [&] { shard_ticks(c_r_->root<ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            timed(st, "C.snapshot2", [&] { c_r_->save_verified_into<ShardState>(snap2_); });
            timed(st, "C.replace", [&] { std::swap(*shard_, *c_r_); });
            st.bytes["C.snapshot"] += snap_.size();
            st.bytes["C.snapshot2"] += snap2_.size();
            if (keep) keep->shard = snap2_;
            timed(st, "C.cleanup", [] {});
            {
                // Untimed copy of the current shard for the middle-erase stress item.
                auto m = XOffsetDatastructure::XBuffer::load_verified<ShardState>(shard_->save_verified<ShardState>(), cap(true));
                timed(st, "S.ticks_middle", [&] { shard_ticks_middle_erase(m.template root<ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
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
                    d_->reload_verified<Report>(w);
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
#ifdef XOFFSET_DELTA_SYNC_API
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
            h->template reload_verified<PlayerState>(lw);
        }
        PlayerState& held(int p) { return hold[std::size_t(p)]->template root<PlayerState>(); }
        template <class Fill> std::size_t patch_encode(Fill&& fill) {
            fill(pm->template reset<xm::PlayerPatch>());
            pm->template save_verified_into<xm::PlayerPatch>(pw);
            return pw.size();
        }
        void patch_recv() { pr->template reload_verified<xm::PlayerPatch>(pw); }
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
        void modules_recv() { pr->template reload_verified<xm::PlayerPatch>(w); }
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
        return XBuffer::maximum_capacity(32 * 1024).with_growth(XOffsetDatastructure::XGrowPolicy::Automatic);
    }

    struct BlockOps {
        using XBuffer = XOffsetDatastructure::XBuffer;
        std::optional<XBuffer> es, ds, dr;
        std::vector<std::string> snaps;
        std::string dw, comp;
        std::vector<std::optional<XBuffer>> held_views;
        void setup(int clients, int units, int marches) {
            es.emplace(XBuffer::create<xm::ViewSnapshot>(kViewInitial, cap()));
            ds.emplace(XBuffer::create<xm::ViewDelta>(kViewInitial, cap()));
            dr.emplace(XBuffer::create<xm::ViewDelta>(kViewInitial, cap()));
            snaps.assign(1, {});
            held_views.clear();
            held_views.resize(std::size_t(clients));
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
            h->reload_verified<xm::ViewSnapshot>(snaps[std::size_t(b)]);
        }
        xm::ViewSnapshot& held(int c, int k) { return held_views[std::size_t(c) + std::size_t(k)]->root<xm::ViewSnapshot>(); }
        template <class Fill> std::size_t delta_encode(Fill&& fill) {
            fill(ds->reset<xm::ViewDelta>());
            ds->save_verified_into<xm::ViewDelta>(dw);
            return dw.size();
        }
        std::string_view delta_wire() const { return dw; }
        void delta_recv() { dr->reload_verified<xm::ViewDelta>(dw); }
        const xm::ViewDelta& delta() { return dr->root<xm::ViewDelta>(); }

#ifdef XOFFSET_DELTA_SYNC_API
        // Native delta (N*): the server keeps each static client's view in an
        // XBuffer with stable slots (a record that leaves zeroes its slot, a new
        // one takes the lowest free slot or is appended; tiles stay in place),
        // mutates it in place, and writes one delta against that client's
        // baseline. The client patches + verifies and reads in place.
        struct NView {
            std::optional<XBuffer> buf;
            XOffsetDatastructure::XDeltaBaseline base;
            std::unordered_map<int32_t, int32_t> uslot, mslot;
            std::vector<int32_t> ufree, mfree;
            int32_t usize = 0, msize = 0;
            std::vector<int32_t> urm, mrm;
            std::vector<std::pair<int32_t, int32_t>> uup, mup;
            std::string delta, key;
        };
        std::vector<NView> nb;
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
            nheld.resize(std::size_t(clients));
            ncur.assign(std::size_t(clients), {});
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
        void n_plan(int c, const MapWorld& w, const DeltaPlan& p) {
            auto& n = nb[std::size_t(c)];
            n.urm.clear(); n.mrm.clear(); n.uup.clear(); n.mup.clear();
            auto take = [](std::unordered_map<int32_t, int32_t>& at, std::vector<int32_t>& fr, int32_t& size, int32_t id) {
                const auto it = at.find(id);
                if (it != at.end()) return it->second;
                int32_t pos;
                if (fr.empty()) pos = size++;
                else { std::pop_heap(fr.begin(), fr.end(), std::greater<>{}); pos = fr.back(); fr.pop_back(); }
                at.emplace(id, pos);
                return pos;
            };
            for (int32_t id : p.removed) {
                const auto it = n.uslot.find(id);
                if (it == n.uslot.end()) continue;
                n.urm.push_back(it->second); n.ufree.push_back(it->second); n.uslot.erase(it);
                std::push_heap(n.ufree.begin(), n.ufree.end(), std::greater<>{});
            }
            for (int32_t id : p.removed_marches) {
                const auto it = n.mslot.find(id);
                if (it == n.mslot.end()) continue;
                n.mrm.push_back(it->second); n.mfree.push_back(it->second); n.mslot.erase(it);
                std::push_heap(n.mfree.begin(), n.mfree.end(), std::greater<>{});
            }
            for (int32_t i : p.units) n.uup.emplace_back(i, take(n.uslot, n.ufree, n.usize, w.unit(i).id));
            for (int32_t i : p.marches) n.mup.emplace_back(i, take(n.mslot, n.mfree, n.msize, w.march(i).id));
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
            for (int32_t pos : n.urm) {
                auto& u = v.units[std::size_t(pos)];
                u.id = 0; u.owner = 0; u.x = 0; u.y = 0; u.hp = 0; u.state = 0;
                u.name = std::string_view{};
                b.touch(u);
            }
            for (int32_t pos : n.mrm) { v.marches[std::size_t(pos)] = xm::ViewMarch{}; b.touch(v.marches[std::size_t(pos)]); }
            for (const auto& [i, pos] : n.uup) {
                const WorldUnit& s = w.unit(i);
                auto& u = v.units[std::size_t(pos)];
                u.id = s.id; u.owner = s.owner; u.x = s.x; u.y = s.y; u.hp = s.hp; u.state = s.state;
                if (sv(u.name) != std::string_view(s.name)) u.name = std::string_view(s.name);
                b.touch(u);
            }
            for (const auto& [i, pos] : n.mup) { copy_vmarch(v.marches[std::size_t(pos)], w.march(i)); b.touch(v.marches[std::size_t(pos)]); }
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
            n.buf->template save_delta_into<xm::ViewSnapshot>(n.delta, n.base, o);
            return n.delta.size();
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
            const XOffsetDatastructure::XStatus r = nheld[std::size_t(c)]->template try_apply_delta_verified<xm::ViewSnapshot>(nb[std::size_t(c)].delta, ncur[std::size_t(c)]);
            if (!r) throw std::runtime_error("native delta rejected: " + std::string(r.error().message));
        }
        xm::ViewSnapshot& n_held(int c, int) { return nheld[std::size_t(c)]->template root<xm::ViewSnapshot>(); }
#endif
    };

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
    // Adapter (2026-11-02): the battle result for these two players is
    // arriving, so the scene server starts loading their buffers' allocator
    // lines (public cache hint, changes nothing) and A.apply does not wait on
    // those misses one after another. Timed as part of A.res_decode; a no-op
    // with libraries that do not have the hint.
    void prefetch_for_apply([[maybe_unused]] const Pairing& pr) const {
#if defined(XOFFSET_PREFETCH_FOR_UPDATE_API)
        players_[std::size_t(pr.ap)].prefetch_for_update();
        players_[std::size_t(pr.dp)].prefetch_for_update();
#endif
    }

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
        if constexpr (!Direct) { timed(st, key, [&] { dst->reload_verified<T>(wire); }); return; }
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

} // namespace slg
