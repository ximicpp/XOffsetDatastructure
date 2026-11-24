// N*/NB server views: n_update grows a view's units or marches when the slot
// table outgrows them, and a vector that outgrows its reserve moves to a new
// block. The delta is written with XDeltaOptions::touched_only, so the moved
// records must be touched where they are now: the new block may be a hole
// below the client's used size, where only touched blocks are compared.
// Here the reserves are tiny (n_setup with no units or marches: 32 slots
// each) and the map is crowded, so views move their vectors often, and every
// client's replica must be the server's view byte for byte after every delta.
// The run must see a vector move into a hole below the client's used size.
// Before the fix (n_update touched only the vector's head) the replica of
// case 0 differed in its first rounds.
#include "xoffset_runner.hpp"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

using namespace slg;
using Ops = XOffsetRunnerT<XoTrustedModel>::BlockOps;

namespace {
struct Counts {
    int deltas = 0, moves = 0, hole_moves = 0;
};

std::uint32_t used_of(const XOffsetDatastructure::XBuffer& b) { return b.arena()->used_end; }

void same_image(const XOffsetDatastructure::XBuffer& server, const XOffsetDatastructure::XBuffer& client,
                const std::string& where) {
    const std::uint32_t used = used_of(server);
    if (used_of(client) != used) throw std::runtime_error(where + ": used size differs");
    if (std::memcmp(server.data(), client.data(), used) != 0) {
        std::uint32_t at = 0;
        while (server.data()[at] == client.data()[at]) ++at;
        throw std::runtime_error(where + ": the replica differs from the server's view at offset " + std::to_string(at) +
                                 " of " + std::to_string(used));
    }
}

// The view's vectors before n_update: where their blocks were.
struct Blocks {
    const void* units;
    const void* marches;
};

// One delivery: plan, update (browse: and move), delta, apply, compare.
void deliver(Ops& ops, ViewSession& s, int c, int64_t id, Counts& n, const std::string& where) {
    MapWorld& w = s.world;
    static std::vector<int32_t> tiles;
    const ViewRect rect = w.rect(c);
    s.tracker.plan(w, c, rect, s.plan);
    if (s.browse) { s.plan.tiles.swap(tiles); s.plan.tiles.clear(); }
    auto& view = ops.nb[std::size_t(c)];
    auto& v = view.buf->template root<xm::ViewSnapshot>();
    const std::uint32_t client_used = used_of(*view.buf);   // what the client holds (its last delta)
    const Blocks before{v.units.data(), v.marches.data()};
    ops.n_plan(c, w, s.plan);
    ops.n_update(c, w, s.plan, id);
    if (s.browse) ops.n_move(c, w, rect, tiles);
    const auto moved = [&](const void* was, const void* now) {
        if (was == now || now == nullptr) return;
        ++n.moves;
        const auto at = std::size_t(static_cast<const char*>(now) - view.buf->data());
        if (at < client_used) ++n.hole_moves;
    };
    moved(before.units, v.units.data());
    moved(before.marches, v.marches.data());
    ops.n_diff(c);
    ops.n_apply(c, 0);
    ++n.deltas;
    same_image(*view.buf, *ops.nheld[std::size_t(c)], where);
}

Counts run_case(int which, int cycles) {
    // A map twice as dense in units as S and with 15 times its marches, and
    // 16 clients: views outgrow their reserves, and with the holes their
    // moves leave, later moves land below the client's used size (browse and
    // stress: 54 of 175 moves on 2026-11-24; at S scale none does).
    static const ViewScale vs{"test", 4000, 16, 4, 3000};
    ViewSession s;
    s.setup(20261124u, vs, which);
    Ops ops;
    ops.setup(vs.clients, 0, 0);   // reserves of 32 units and 32 marches per view
    MapWorld& w = s.world;
    Counts n;
    // First pull: the view laid out and sent as a key delta.
    for (int c = 0; c < vs.clients; ++c) {
        const ViewRect rect = w.rect(c);
        s.tracker.plan(w, c, rect, s.plan);
        if (s.browse) {
            std::vector<int32_t> tiles;
            s.plan.tiles.swap(tiles); s.plan.tiles.clear();
            ops.n_bview(c);
            ops.n_plan(c, w, s.plan);
            ops.n_update(c, w, s.plan, 0);
            ops.n_move(c, w, rect, tiles);
        } else {
            ops.n_view(c, w, rect);
            ops.n_plan(c, w, s.plan);
            ops.n_update(c, w, s.plan, 0);
        }
        ops.n_key(c);
        ops.n_key_recv(c, 0, c);
        same_image(*ops.nb[std::size_t(c)].buf, *ops.nheld[std::size_t(c)], "key delta, client " + std::to_string(c));
    }
    for (int cycle = 0; cycle < cycles; ++cycle)
        for (int r = 0; r < vs.rounds; ++r) {
            w.tick();
            const int64_t base = (int64_t(cycle) * vs.rounds + r) * (vs.clients + 1) + 1;
            for (int c = 0; c < vs.clients; ++c) {
                if (s.browse) w.move(c);
                deliver(ops, s, c, base + c, n,
                        "case " + std::to_string(which) + " cycle " + std::to_string(cycle) + " round " + std::to_string(r) +
                            " client " + std::to_string(c));
            }
        }
    return n;
}
}  // namespace

int main() {
    try {
        Counts all;
        for (int which = 0; which < ViewCases::count; ++which) {
            const Counts n = run_case(which, 6);
            std::printf("view_resize_delta case %d: %d deltas, %d vector moves, %d into a hole below the client's used size\n",
                        which, n.deltas, n.moves, n.hole_moves);
            all.deltas += n.deltas; all.moves += n.moves; all.hole_moves += n.hole_moves;
        }
        if (all.hole_moves == 0) throw std::runtime_error("no vector moved into a hole: the case does not exercise the path");
        std::puts("view_resize_delta_test passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "view_resize_delta_test FAILED: %s\n", e.what());
        return 1;
    }
}
