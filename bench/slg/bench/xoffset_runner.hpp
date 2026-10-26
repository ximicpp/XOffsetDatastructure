// SLG prototype: runner for XOffset V2. Each player and the map shard live in their own
// XBuffer; messages are built in, saved from and loaded into arenas, and the game logic
// runs in place on the received arena. Requires P2996 Clang.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
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
        shard_.emplace(XBuffer::create<ShardState>(kShardInitial, cap()));
        copy_shard(shard_->root<ShardState>(), src);
        if (reuse_mode()) {
            a_req_.emplace(XBuffer::create<BattleMsg>(kBattleInitial, cap()));
            a_m_.emplace(XBuffer::create<BattleMsg>(kBattleInitial, cap()));
            a_r_.emplace(XBuffer::create<BattleMsg>(kBattleInitial, cap()));
            b_m_.emplace(XBuffer::create<PlayerState>(kPlayerInitial, cap()));
            b_r_.emplace(XBuffer::create<PlayerState>(kPlayerInitial, cap()));
            c_r_.emplace(XBuffer::create<ShardState>(kShardInitial, cap()));
            d_.emplace(XBuffer::create<Report>(kReportInitial, cap()));
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
            timed(st, "A.res_decode", [&] { r.emplace(XBuffer::load_verified<BattleMsg>(res, cap())); });
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
            timed(st, "C.restore", [&] { r.emplace(XBuffer::load_verified<ShardState>(snap, cap())); });
            timed(st, "C.ticks", [&] { shard_ticks(r->root<ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            timed(st, "C.snapshot2", [&] { snap2 = r->save_verified<ShardState>(); });
            timed(st, "C.replace", [&] { *shard_ = std::move(*r); });
            st.bytes["C.snapshot"] += snap.size();
            st.bytes["C.snapshot2"] += snap2.size();
            if (keep) keep->shard = snap2;
            timed(st, "C.cleanup", [&] { r.reset(); std::string().swap(snap); std::string().swap(snap2); });
            {
                // Untimed copy of the current shard for the middle-erase stress item.
                auto m = XOffsetDatastructure::XBuffer::load_verified<ShardState>(shard_->save_verified<ShardState>(), cap());
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
        for (std::size_t k = 0; k < migrants.size(); ++k) {
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

        {
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
                auto m = XOffsetDatastructure::XBuffer::load_verified<ShardState>(shard_->save_verified<ShardState>(), cap());
                timed(st, "S.ticks_middle", [&] { shard_ticks_middle_erase(m.template root<ShardState>(), mix_seed(seed_, 0xc0 + uint64_t(cycle)), sc.shard_ticks); });
            }
        }

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

    uint64_t world_digest() override {
        Digest g;
        for (auto& b : players_) dig_player(g, b.root<PlayerState>());
        dig_shard(g, shard_->root<ShardState>());
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
    PlayerState& player(int i) { return players_[std::size_t(i)].root<PlayerState>(); }

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
            if (extents == 0) take(body.data(), payload);
            else
                for (std::size_t e = 0; e < extents; ++e) {
                    uint32_t ext[2];
                    std::memcpy(ext, table.data() + e * 8, 8);
                    if (std::size_t(ext[0]) + ext[1] > body.size()) throw std::runtime_error("direct receive: extent bounds");
                    take(body.data() + ext[0], ext[1]);
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
};

using XOffsetRunner = XOffsetRunnerT<XoModel, false>;

} // namespace slg
