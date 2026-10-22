// SLG prototype: runner for formats that decode into the std model (cereal, protobuf, FlatBuffers).
#pragma once
#include <optional>
#include <span>
#include <utility>
#include <vector>
#include "runner.hpp"
#include "slg/generic.hpp"
#include "slg/model_std.hpp"
#include "slg/trace.hpp"

namespace slg {

template <class F> class StdRunner final : public Runner {
public:
    std::string name() const override { return F::name; }

    void setup(const Scale& sc, uint64_t seed) override {
        sc_ = &sc; seed_ = seed;
        players_.clear();
        for (int i = 0; i < sc.players; ++i) players_.push_back(make_player(seed, i + 1));
        armies_.clear();
        for (const auto& p : players_) armies_.push_back(int(p.armies.size()));
        shard_ = make_shard(seed, sc);
    }

    void run_cycle(int cycle, CycleStats& st, SampleWires* keep) override {
        if (reuse_mode()) return run_cycle_reuse(cycle, st, keep);
        using W = typename F::Wire;
        const Scale& sc = *sc_;
        std::vector<stdm::Report> reports;
        reports.reserve(std::size_t(sc.battles));

        // A: battle server round trip.
        for (const Pairing& pr : make_pairings(seed_, cycle, sc, armies_)) {
            auto& att = players_[std::size_t(pr.ap)].armies[std::size_t(pr.aa)];
            auto& def = players_[std::size_t(pr.dp)].armies[std::size_t(pr.da)];
            W req, res;
            std::optional<stdm::BattleMsg> m, r;
            timed(st, "A.req_encode", [&] { req = F::encode_battle_request(pr.battle_id, pr.seed, att, def); });
            timed(st, "A.req_decode", [&] { m.emplace(); F::decode(F::bytes(req), *m); });
            timed(st, "A.simulate", [&] { simulate_battle(*m); });
            timed(st, "A.res_encode", [&] { res = F::encode(*m); });
            timed(st, "A.res_decode", [&] { r.emplace(); F::decode(F::bytes(res), *r); });
            timed(st, "A.apply", [&] { apply_battle_army(att, r->attacker); apply_battle_army(def, r->defender); });
            st.bytes["A.req"] += F::bytes(req).size();
            st.bytes["A.res"] += F::bytes(res).size();
            reports.push_back(r->report);
            if (keep) { keep->req = to_string_bytes(F::bytes(req)); keep->res = to_string_bytes(F::bytes(res)); }
            timed(st, "A.cleanup", [&] { m.reset(); r.reset(); req = W(); res = W(); });
        }

        // B: cross-server season migration and write-back.
        const auto migrants = pick_migrants(seed_, cycle, sc);
        for (std::size_t k = 0; k < migrants.size(); ++k) {
            auto& slot = players_[std::size_t(migrants[k])];
            W out, back;
            std::optional<stdm::PlayerState> m, r;
            timed(st, "B.out_encode", [&] { out = F::encode(slot); });
            timed(st, "B.in_decode", [&] { m.emplace(); F::decode(F::bytes(out), *m); });
            timed(st, "B.season_edits", [&] { season_edits(*m, mix_seed(seed_, uint64_t(cycle) * 1000003u + k), sc.season_ops); });
            timed(st, "B.back_encode", [&] { back = F::encode(*m); });
            timed(st, "B.back_decode", [&] { r.emplace(); F::decode(F::bytes(back), *r); });
            timed(st, "B.writeback", [&] { slot = std::move(*r); });
            st.bytes["B.out"] += F::bytes(out).size();
            st.bytes["B.back"] += F::bytes(back).size();
            if (keep) keep->player = to_string_bytes(F::bytes(back));
            timed(st, "B.cleanup", [&] { m.reset(); r.reset(); out = W(); back = W(); });
        }

        // C: map shard snapshot, restore, ticks, snapshot again.
        {
            W snap, snap2;
            std::optional<stdm::ShardState> r;
            timed(st, "C.snapshot", [&] { snap = F::encode(shard_); });
            timed(st, "C.restore", [&] { r.emplace(); F::decode(F::bytes(snap), *r); });
            timed(st, "C.ticks", [&] { shard_ticks(*r, mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            timed(st, "C.snapshot2", [&] { snap2 = F::encode(*r); });
            timed(st, "C.replace", [&] { shard_ = std::move(*r); });
            st.bytes["C.snapshot"] += F::bytes(snap).size();
            st.bytes["C.snapshot2"] += F::bytes(snap2).size();
            if (keep) keep->shard = to_string_bytes(F::bytes(snap2));
            timed(st, "C.cleanup", [&] { r.reset(); snap = W(); snap2 = W(); });
            {
                stdm::ShardState m = shard_;  // untimed copy of the current shard
                timed(st, "S.ticks_middle", [&] { shard_ticks_middle_erase(m, mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            }
        }

        // D: read-only battle reports (setup untimed).
        std::vector<W> wires;
        wires.reserve(reports.size());
        for (const auto& rep : reports) wires.push_back(F::encode(rep));
        for (const auto& w : wires) st.bytes["D.report"] += F::bytes(w).size();
        if (keep && !wires.empty()) keep->report = to_string_bytes(F::bytes(wires.back()));
        for (int q = 0; q < sc.report_reads && !wires.empty(); ++q) {
            const W& w = wires[std::size_t(q) % wires.size()];
            timed(st, "D.read", [&] { stdm::Report r; F::decode(F::bytes(w), r); st.report_sum += query_report(r); });
            if constexpr (requires { F::read_report_direct(F::bytes(w)); })
                timed(st, "D.read_direct", [&] { st.report_direct_sum += F::read_report_direct(F::bytes(w)); });
        }
    }

    // Reuse protocol: encoder state per slot (F::Reuse) and decode targets
    // live across messages; write-back and replace swap the decoded object in.
    void run_cycle_reuse(int cycle, CycleStats& st, SampleWires* keep) {
        const Scale& sc = *sc_;
        std::vector<stdm::Report> reports;
        reports.reserve(std::size_t(sc.battles));

        for (const Pairing& pr : make_pairings(seed_, cycle, sc, armies_)) {
            auto& att = players_[std::size_t(pr.ap)].armies[std::size_t(pr.aa)];
            auto& def = players_[std::size_t(pr.dp)].armies[std::size_t(pr.da)];
            std::span<const uint8_t> req, res;
            timed(st, "A.req_encode", [&] { req = rz_.encode_battle_request(kReq, pr.battle_id, pr.seed, att, def); });
            timed(st, "A.req_decode", [&] { rz_.decode(req, am_); });
            timed(st, "A.simulate", [&] { simulate_battle(am_); });
            timed(st, "A.res_encode", [&] { res = rz_.encode(kRes, am_); });
            timed(st, "A.res_decode", [&] { rz_.decode(res, ar_); });
            timed(st, "A.apply", [&] { apply_battle_army(att, ar_.attacker); apply_battle_army(def, ar_.defender); });
            st.bytes["A.req"] += req.size();
            st.bytes["A.res"] += res.size();
            reports.push_back(ar_.report);
            if (keep) { keep->req = to_string_bytes(req); keep->res = to_string_bytes(res); }
            timed(st, "A.cleanup", [] {});
        }

        const auto migrants = pick_migrants(seed_, cycle, sc);
        for (std::size_t k = 0; k < migrants.size(); ++k) {
            auto& slot = players_[std::size_t(migrants[k])];
            std::span<const uint8_t> out, back;
            timed(st, "B.out_encode", [&] { out = rz_.encode(kOut, slot); });
            timed(st, "B.in_decode", [&] { rz_.decode(out, bm_); });
            timed(st, "B.season_edits", [&] { season_edits(bm_, mix_seed(seed_, uint64_t(cycle) * 1000003u + k), sc.season_ops); });
            timed(st, "B.back_encode", [&] { back = rz_.encode(kBack, bm_); });
            timed(st, "B.back_decode", [&] { rz_.decode(back, br_); });
            timed(st, "B.writeback", [&] { std::swap(slot, br_); });
            st.bytes["B.out"] += out.size();
            st.bytes["B.back"] += back.size();
            if (keep) keep->player = to_string_bytes(back);
            timed(st, "B.cleanup", [] {});
        }

        {
            std::span<const uint8_t> snap, snap2;
            timed(st, "C.snapshot", [&] { snap = rz_.encode(kSnap, shard_); });
            timed(st, "C.restore", [&] { rz_.decode(snap, cr_); });
            timed(st, "C.ticks", [&] { shard_ticks(cr_, mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            timed(st, "C.snapshot2", [&] { snap2 = rz_.encode(kSnap2, cr_); });
            timed(st, "C.replace", [&] { std::swap(shard_, cr_); });
            st.bytes["C.snapshot"] += snap.size();
            st.bytes["C.snapshot2"] += snap2.size();
            if (keep) keep->shard = to_string_bytes(snap2);
            timed(st, "C.cleanup", [] {});
            {
                stdm::ShardState m = shard_;  // untimed copy of the current shard
                timed(st, "S.ticks_middle", [&] { shard_ticks_middle_erase(m, mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            }
        }

        using W = typename F::Wire;
        std::vector<W> wires;
        wires.reserve(reports.size());
        for (const auto& rep : reports) wires.push_back(F::encode(rep));
        for (const auto& w : wires) st.bytes["D.report"] += F::bytes(w).size();
        if (keep && !wires.empty()) keep->report = to_string_bytes(F::bytes(wires.back()));
        for (int q = 0; q < sc.report_reads && !wires.empty(); ++q) {
            const W& w = wires[std::size_t(q) % wires.size()];
            timed(st, "D.read", [&] { rz_.decode(F::bytes(w), dr_); st.report_sum += query_report(dr_); });
            if constexpr (requires { F::read_report_direct(F::bytes(w)); })
                timed(st, "D.read_direct", [&] { st.report_direct_sum += F::read_report_direct(F::bytes(w)); });
        }
    }

    uint64_t world_digest() override {
        Digest g;
        for (const auto& p : players_) dig_player(g, p);
        dig_shard(g, shard_);
        return g.h;
    }

private:
    const Scale* sc_ = nullptr;
    uint64_t seed_ = 0;
    std::vector<stdm::PlayerState> players_;
    std::vector<int> armies_;
    stdm::ShardState shard_;
    // Reuse protocol state.
    typename F::Reuse rz_;
    stdm::BattleMsg am_, ar_;
    stdm::PlayerState bm_, br_;
    stdm::ShardState cr_;
    stdm::Report dr_;
};

} // namespace slg
