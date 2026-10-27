// Instruction counts of cross-version receive and send (schema evolution, E5),
// on the SLG prototype's messages: A = BattleMsg (battle request), B =
// PlayerState (season migration), D = Report (battle report). Record only; not
// a gate. bench/evolution/run_ir.sh builds it and runs every case under
// callgrind.
//
// The working versions are the prototype's slg::xm types; each has one older
// version in slg_old that a client still on the previous release would use:
//   Hero      without `star`       (A, B)
//   Event     without `target`     (A, D)
//   PlayerState without `items`    (B)
// The older version only lacks fields, so no hooks are needed: an upgrade
// fills the added fields with their initial values, a downgrade drops them.
//
//   evolution_ir <A|B|D> <op> [iterations]
//     op: recv_same   load_verified of the working version (baseline)
//         recv_old    XPeerSchemas::load_verified of the old version's wire
//                     (load_verified as the old version + upgrade)
//         load_old    load_verified as the old version only
//         send_same   save_verified of the working version (baseline)
//         send_old    XPeerSchemas::save for an old peer (downgrade + save_compact)
// Collection is on only around the iterations (run under
// valgrind --tool=callgrind --collect-atstart=no).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <valgrind/callgrind.h>

#include "bench/xoffset_runner.hpp"
#include "xoffset_evolution.hpp"

namespace slg_old {
using namespace XOffsetDatastructure;
using slg::xm::Buff;
using slg::xm::Skill;
using slg::xm::Troop;
using slg::xm::City;

struct alignas(8) Hero {
    int32_t id{}; int32_t level{};
    XString name;
    XVector<Skill> skills;
    XVector<Buff> buffs;
};
struct alignas(8) Army {
    int64_t army_id{}; int64_t owner_id{}; int32_t x{}; int32_t y{};
    XVector<Hero> heroes;
    XVector<Troop> troops;
    XVector<Buff> buffs;
};
struct alignas(8) PlayerState {
    int64_t player_id{};
    XString name;
    City city;
    XVector<Army> armies;
    XVector<Hero> heroes;
    XMap<int32_t, int32_t> research;
};
struct alignas(8) Event { int32_t round{}; int32_t hero_id{}; int32_t skill_id{}; int32_t damage{}; };
struct alignas(8) Report {
    int64_t battle_id{};
    XString summary;
    XVector<Event> events;
};
struct alignas(8) BattleMsg {
    int64_t battle_id{}; int64_t seed{}; int32_t winner{};
    Army attacker;
    Army defender;
    Report report;
};
} // namespace slg_old

XOFFSET_REGISTER_SCHEMA_NAME(slg_old::PlayerState, "slg.PlayerState.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg_old::BattleMsg, "slg.BattleMsg.v1")
XOFFSET_REGISTER_SCHEMA_NAME(slg_old::Report, "slg.Report.v1")
XOFFSET_SCHEMA_LINEAGE(slg::xm::BattleMsg, slg_old::BattleMsg)
XOFFSET_SCHEMA_LINEAGE(slg::xm::PlayerState, slg_old::PlayerState)
XOFFSET_SCHEMA_LINEAGE(slg::xm::Report, slg_old::Report)

namespace {
using namespace XOffsetDatastructure;
constexpr std::size_t kCap = std::size_t(256) << 20;
XBuffer::MaximumCapacity cap() { return XBuffer::maximum_capacity(kCap).with_growth(XGrowPolicy::Automatic); }

template <typename Old>
XPeerSchemas old_peer() {
    std::string ack;
    return XPeerSchemas::accept<slg::xm::BattleMsg, slg::xm::PlayerState, slg::xm::Report>(
        make_schema_hello<Old>("old-client"), ack, "server");
}

template <typename W, typename Old>
int run(const std::string& wire, std::string_view op, int iterations) {
    const XPeerSchemas peer = old_peer<Old>();
    if (!peer.accepted() || peer.template sends_working_version<W>()) {
        std::fprintf(stderr, "old peer not accepted as a conversion\n");
        return 1;
    }
    auto object = TypedXBuffer<W>::load_verified(wire, cap());
    const std::string old_wire = peer.save(object);
    std::size_t sink = 0;
    auto measure = [&](auto&& body) {
        CALLGRIND_TOGGLE_COLLECT;
        for (int i = 0; i < iterations; ++i) body();
        CALLGRIND_TOGGLE_COLLECT;
    };
    if (op == "recv_same") {
        measure([&] { auto b = TypedXBuffer<W>::load_verified(wire, cap()); sink += memory_stats(b).used_size; });
    } else if (op == "recv_old") {
        measure([&] { auto b = peer.load_verified<W>(old_wire, cap()); sink += memory_stats(b).used_size; });
    } else if (op == "load_old") {
        measure([&] { auto b = TypedXBuffer<Old>::load_verified(old_wire, cap()); sink += memory_stats(b).used_size; });
    } else if (op == "send_same") {
        measure([&] { sink += object.save_verified().size(); });
    } else if (op == "send_old") {
        measure([&] { sink += peer.save(object).size(); });
    } else {
        std::fprintf(stderr, "unknown op %.*s\n", int(op.size()), op.data());
        return 2;
    }
    std::printf("wire %zu B, old wire %zu B, iterations %d, sink %zu\n", wire.size(), old_wire.size(), iterations,
                sink);
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: evolution_ir <A|B|D> <recv_same|recv_old|load_old|send_same|send_old> [iterations]\n");
        return 2;
    }
    const std::string_view scenario = argv[1], op = argv[2];
    const int iterations = argc > 3 ? std::atoi(argv[3]) : 10;
    // Wires of the Ir gate's scale (S, seed 20261021) from cycle 4, the first
    // cycle the gate counts (setup, not counted). Scale M reserves more
    // address space than valgrind allows.
    const slg::Scale* sc = slg::find_scale("S");
    slg::XOffsetRunnerT<slg::XoModel, false> runner;
    runner.setup(*sc, 20261021);
    slg::SampleWires keep;
    for (int c = 0; c < 5; ++c) {
        slg::CycleStats st;
        runner.run_cycle(c, st, c == 4 ? &keep : nullptr);
    }
    if (scenario == "A") return run<slg::xm::BattleMsg, slg_old::BattleMsg>(keep.req, op, iterations);
    if (scenario == "B") return run<slg::xm::PlayerState, slg_old::PlayerState>(keep.player, op, iterations);
    if (scenario == "D") return run<slg::xm::Report, slg_old::Report>(keep.report, op, iterations);
    std::fprintf(stderr, "unknown scenario\n");
    return 2;
}
