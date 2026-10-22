// SLG prototype: runner for XOffset V2. Each player and the map shard live in their own
// XBuffer; messages are built in, saved from and loaded into arenas, and the game logic
// runs in place on the received arena. Requires P2996 Clang.
#pragma once
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "runner.hpp"
#include "slg/generic.hpp"
#include "slg/model_xoffset.hpp"
#include "slg/trace.hpp"

namespace slg {

class XOffsetRunner final : public Runner {
public:
    std::string name() const override { return "xoffset"; }

    void setup(const Scale& sc, uint64_t seed) override {
        using namespace XOffsetDatastructure;
        sc_ = &sc; seed_ = seed;
        players_.clear();
        armies_.clear();
        for (int i = 0; i < sc.players; ++i) {
            const stdm::PlayerState src = make_player(seed, i + 1);
            XBuffer b = XBuffer::create<xm::PlayerState>(kPlayerInitial, cap());
            copy_player(b.root<xm::PlayerState>(), src);
            armies_.push_back(int(src.armies.size()));
            players_.push_back(std::move(b));
        }
        const stdm::ShardState src = make_shard(seed, sc);
        shard_.emplace(XBuffer::create<xm::ShardState>(kShardInitial, cap()));
        copy_shard(shard_->root<xm::ShardState>(), src);
        if (reuse_mode()) {
            a_req_.emplace(XBuffer::create<xm::BattleMsg>(kBattleInitial, cap()));
            a_m_.emplace(XBuffer::create<xm::BattleMsg>(kBattleInitial, cap()));
            a_r_.emplace(XBuffer::create<xm::BattleMsg>(kBattleInitial, cap()));
            b_m_.emplace(XBuffer::create<xm::PlayerState>(kPlayerInitial, cap()));
            b_r_.emplace(XBuffer::create<xm::PlayerState>(kPlayerInitial, cap()));
            c_r_.emplace(XBuffer::create<xm::ShardState>(kShardInitial, cap()));
            d_.emplace(XBuffer::create<xm::Report>(kReportInitial, cap()));
        }
    }

    void run_cycle(int cycle, CycleStats& st, SampleWires* keep) override {
        if (reuse_mode()) return run_cycle_reuse(cycle, st, keep);
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
                XBuffer b = XBuffer::create<xm::BattleMsg>(kBattleInitial, cap());
                auto& msg = b.root<xm::BattleMsg>();
                msg.battle_id = pr.battle_id; msg.seed = pr.seed; msg.winner = 0;
                copy_army(msg.attacker, att);
                copy_army(msg.defender, def);
                req = b.save_verified<xm::BattleMsg>();
            });
            timed(st, "A.req_decode", [&] { m.emplace(XBuffer::load_verified<xm::BattleMsg>(req, cap())); });
            timed(st, "A.simulate", [&] { simulate_battle(m->root<xm::BattleMsg>()); });
            timed(st, "A.res_encode", [&] { res = m->save_verified<xm::BattleMsg>(); });
            timed(st, "A.res_decode", [&] { r.emplace(XBuffer::load_verified<xm::BattleMsg>(res, cap())); });
            timed(st, "A.apply", [&] {
                const auto& out = r->root<xm::BattleMsg>();
                apply_battle_army(att, out.attacker);
                apply_battle_army(def, out.defender);
            });
            st.bytes["A.req"] += req.size();
            st.bytes["A.res"] += res.size();
            copy_report(reports.emplace_back(), r->root<xm::BattleMsg>().report);
            if (keep) { keep->req = req; keep->res = res; }
            timed(st, "A.cleanup", [&] { m.reset(); r.reset(); std::string().swap(req); std::string().swap(res); });
        }

        // B: cross-server season migration and write-back.
        const auto migrants = pick_migrants(seed_, cycle, sc);
        for (std::size_t k = 0; k < migrants.size(); ++k) {
            XBuffer& slot = players_[std::size_t(migrants[k])];
            std::string out, back;
            std::optional<XBuffer> m, r;
            timed(st, "B.out_encode", [&] { out = slot.save_verified<xm::PlayerState>(); });
            timed(st, "B.in_decode", [&] { m.emplace(XBuffer::load_verified<xm::PlayerState>(out, cap())); });
            timed(st, "B.season_edits", [&] {
                season_edits(m->root<xm::PlayerState>(), mix_seed(seed_, uint64_t(cycle) * 1000003u + k), sc.season_ops);
            });
            timed(st, "B.back_encode", [&] { back = m->save_verified<xm::PlayerState>(); });
            timed(st, "B.back_decode", [&] { r.emplace(XBuffer::load_verified<xm::PlayerState>(back, cap())); });
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
            timed(st, "C.snapshot", [&] { snap = shard_->save_verified<xm::ShardState>(); });
            timed(st, "C.restore", [&] { r.emplace(XBuffer::load_verified<xm::ShardState>(snap, cap())); });
            timed(st, "C.ticks", [&] { shard_ticks(r->root<xm::ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            timed(st, "C.snapshot2", [&] { snap2 = r->save_verified<xm::ShardState>(); });
            timed(st, "C.replace", [&] { *shard_ = std::move(*r); });
            st.bytes["C.snapshot"] += snap.size();
            st.bytes["C.snapshot2"] += snap2.size();
            if (keep) keep->shard = snap2;
            timed(st, "C.cleanup", [&] { r.reset(); std::string().swap(snap); std::string().swap(snap2); });
            {
                // Untimed copy of the current shard for the middle-erase stress item.
                auto m = XOffsetDatastructure::XBuffer::load_verified<xm::ShardState>(shard_->save_verified<xm::ShardState>(), cap());
                timed(st, "S.ticks_middle", [&] { shard_ticks_middle_erase(m.root<xm::ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            }
        }

        // D: read-only battle reports (setup untimed).
        std::vector<std::string> wires;
        wires.reserve(reports.size());
        for (const auto& rep : reports) {
            XBuffer b = XBuffer::create<xm::Report>(kReportInitial, cap());
            copy_report(b.root<xm::Report>(), rep);
            wires.push_back(b.save_verified<xm::Report>());
        }
        for (const auto& w : wires) st.bytes["D.report"] += w.size();
        if (keep && !wires.empty()) keep->report = wires.back();
        for (int q = 0; q < sc.report_reads && !wires.empty(); ++q) {
            const std::string& w = wires[std::size_t(q) % wires.size()];
            timed(st, "D.read", [&] {
                XBuffer b = use_pool()
                    ? XBuffer::load_verified<xm::Report>(w, XBuffer::maximum_capacity(XBuffer::DEFAULT_MAXIMUM_CAPACITY).with_pool(pool()))
                    : XBuffer::load_verified<xm::Report>(w);
                st.report_sum += query_report(b.root<xm::Report>());
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
                auto& msg = a_req_->reset<xm::BattleMsg>();
                msg.battle_id = pr.battle_id; msg.seed = pr.seed; msg.winner = 0;
                copy_army(msg.attacker, att);
                copy_army(msg.defender, def);
                a_req_->save_verified_into<xm::BattleMsg>(req_);
            });
            timed(st, "A.req_decode", [&] { a_m_->reload_verified<xm::BattleMsg>(req_); });
            timed(st, "A.simulate", [&] { simulate_battle(a_m_->root<xm::BattleMsg>()); });
            timed(st, "A.res_encode", [&] { a_m_->save_verified_into<xm::BattleMsg>(res_); });
            timed(st, "A.res_decode", [&] { a_r_->reload_verified<xm::BattleMsg>(res_); });
            timed(st, "A.apply", [&] {
                const auto& out = a_r_->root<xm::BattleMsg>();
                apply_battle_army(att, out.attacker);
                apply_battle_army(def, out.defender);
            });
            st.bytes["A.req"] += req_.size();
            st.bytes["A.res"] += res_.size();
            copy_report(reports.emplace_back(), a_r_->root<xm::BattleMsg>().report);
            if (keep) { keep->req = req_; keep->res = res_; }
            timed(st, "A.cleanup", [] {});
        }

        const auto migrants = pick_migrants(seed_, cycle, sc);
        for (std::size_t k = 0; k < migrants.size(); ++k) {
            XBuffer& slot = players_[std::size_t(migrants[k])];
            timed(st, "B.out_encode", [&] { slot.save_verified_into<xm::PlayerState>(out_); });
            timed(st, "B.in_decode", [&] { b_m_->reload_verified<xm::PlayerState>(out_); });
            timed(st, "B.season_edits", [&] {
                season_edits(b_m_->root<xm::PlayerState>(), mix_seed(seed_, uint64_t(cycle) * 1000003u + k), sc.season_ops);
            });
            timed(st, "B.back_encode", [&] { b_m_->save_verified_into<xm::PlayerState>(back_); });
            timed(st, "B.back_decode", [&] { b_r_->reload_verified<xm::PlayerState>(back_); });
            timed(st, "B.writeback", [&] { std::swap(slot, *b_r_); });
            st.bytes["B.out"] += out_.size();
            st.bytes["B.back"] += back_.size();
            if (keep) keep->player = back_;
            timed(st, "B.cleanup", [] {});
        }

        {
            timed(st, "C.snapshot", [&] { shard_->save_verified_into<xm::ShardState>(snap_); });
            timed(st, "C.restore", [&] { c_r_->reload_verified<xm::ShardState>(snap_); });
            timed(st, "C.ticks", [&] { shard_ticks(c_r_->root<xm::ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            timed(st, "C.snapshot2", [&] { c_r_->save_verified_into<xm::ShardState>(snap2_); });
            timed(st, "C.replace", [&] { std::swap(*shard_, *c_r_); });
            st.bytes["C.snapshot"] += snap_.size();
            st.bytes["C.snapshot2"] += snap2_.size();
            if (keep) keep->shard = snap2_;
            timed(st, "C.cleanup", [] {});
            {
                // Untimed copy of the current shard for the middle-erase stress item.
                auto m = XOffsetDatastructure::XBuffer::load_verified<xm::ShardState>(shard_->save_verified<xm::ShardState>(), cap());
                timed(st, "S.ticks_middle", [&] { shard_ticks_middle_erase(m.root<xm::ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            }
        }

        std::vector<std::string> wires;
        wires.reserve(reports.size());
        for (const auto& rep : reports) {
            XBuffer b = XBuffer::create<xm::Report>(kReportInitial, cap());
            copy_report(b.root<xm::Report>(), rep);
            wires.push_back(b.save_verified<xm::Report>());
        }
        for (const auto& w : wires) st.bytes["D.report"] += w.size();
        if (keep && !wires.empty()) keep->report = wires.back();
        for (int q = 0; q < sc.report_reads && !wires.empty(); ++q) {
            const std::string& w = wires[std::size_t(q) % wires.size()];
            timed(st, "D.read", [&] {
                d_->reload_verified<xm::Report>(w);
                st.report_sum += query_report(d_->root<xm::Report>());
            });
        }
    }

    uint64_t world_digest() override {
        Digest g;
        for (auto& b : players_) dig_player(g, b.root<xm::PlayerState>());
        dig_shard(g, shard_->root<xm::ShardState>());
        return g.h;
    }

private:
    static constexpr std::size_t kPlayerInitial = 64 * 1024;
    static constexpr std::size_t kBattleInitial = 16 * 1024;
    static constexpr std::size_t kShardInitial = 4 * 1024 * 1024;
    static constexpr std::size_t kReportInitial = 4 * 1024;

    static XOffsetDatastructure::XBuffer::MaximumCapacity cap() {
        using XOffsetDatastructure::XBuffer;
        auto c = XBuffer::maximum_capacity(std::size_t(256) << 20).with_growth(XOffsetDatastructure::XGrowPolicy::Automatic);
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
    xm::PlayerState& player(int i) { return players_[std::size_t(i)].root<xm::PlayerState>(); }

    const Scale* sc_ = nullptr;
    uint64_t seed_ = 0;
    std::vector<XOffsetDatastructure::XBuffer> players_;
    std::vector<int> armies_;
    std::optional<XOffsetDatastructure::XBuffer> shard_;
    // Reuse protocol state.
    std::optional<XOffsetDatastructure::XBuffer> a_req_, a_m_, a_r_, b_m_, b_r_, c_r_, d_;
    std::string req_, res_, out_, back_, snap_, snap2_;
};

} // namespace slg
