// Use case 2: a client sends an order up to the server (docs/GETTING_STARTED.md).
//
// Data from a client is untrusted input. The server reads it with
// XReceiver<XLink::from_client>:
//   - view(): the bounds tier, the default on this link. It walks the
//     container headers once so that no read leaves the wire, then hands out
//     const access to the wire's own bytes; nothing is copied. The values
//     can still be anything: the server checks them against its own state
//     before acting on them (structure checks never vouch for content).
//   - reload(): the full check, the only tier that may keep a client's data
//     as a buffer to modify or forward. Forward it rebuilt with
//     save_compact(), so bytes the client left in holes and slack do not go
//     on to the next hop (docs/DEPLOYMENT.md 4.2).
//
// Client best practices: one send buffer reused for every order (reset()),
// a dense wire (view() needs one; wires below 4 KiB always are), small
// per-link limits on the server.
#include "game_protocol.hpp"
#include "../xoffset_receiver.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

constexpr std::size_t kOrderLimit = 4096;  // an order is small: refuse anything larger

// ---- client -------------------------------------------------------------------

struct Client {
    std::uint64_t player_id;
    TypedXBuffer<MoveOrder> out = TypedXBuffer<MoveOrder>::create(512, XBuffer::maximum_capacity(kOrderLimit));
    std::string wire;

    const std::string& order(std::uint32_t unit, std::int32_t x, std::int32_t y, int hops) {
        auto o = out.reset();  // the same buffer for every order: no new mapping
        o.player_id() = player_id;
        o.unit_id() = unit;
        o.to_x() = x;
        o.to_y() = y;
        auto path = o.waypoints();
        path.reserve(std::size_t(hops) * 2);  // known size: one allocation
        for (int i = 1; i <= hops; ++i) {
            path.push_back(x * i / (hops + 1));
            path.push_back(y * i / (hops + 1));
        }
        out.save_verified_into(wire, XSaveOptions{.emission = XWireEmission::dense});
        return wire;
    }
};

// ---- server -------------------------------------------------------------------

struct UnitState {
    std::uint64_t owner;
    std::int32_t x, y;
};

struct Server {
    XReceiver<XLink::from_client> up{XBuffer::maximum_capacity(kOrderLimit)};
    UnitState units[4] = {{1001, 0, 0}, {1001, 5, 5}, {1002, 9, 9}, {1002, 3, 3}};
    XBuffer kept{512, XBuffer::maximum_capacity(kOrderLimit)};  // for orders the server keeps

    // Read in place; check the content against the server's own state.
    bool on_order(std::uint64_t conn_player, const std::string& wire) {
        if (wire.size() > kOrderLimit + 64) return false;  // before any parsing
        auto v = up.view<MoveOrder>(wire);
        if (!v) {
            std::printf("server: order rejected by the bounds check (%s)\n", to_string(v.code()));
            return false;
        }
        const MoveOrder& o = v->root();  // const only: writes do not compile
        if (o.player_id != conn_player || o.unit_id >= 4 || units[o.unit_id].owner != conn_player ||
            o.waypoints.size() % 2 != 0 || o.waypoints.size() > 64 || o.to_x < 0 || o.to_x > 100 || o.to_y < 0 ||
            o.to_y > 100) {
            std::printf("server: order from %llu refused by game rules\n", static_cast<unsigned long long>(conn_player));
            return false;
        }
        units[o.unit_id].x = o.to_x;  // copy what is needed into the server's own state
        units[o.unit_id].y = o.to_y;
        std::printf("server: unit %u -> (%d, %d) via %zu waypoints\n", o.unit_id, o.to_x, o.to_y,
                    std::size_t(o.waypoints.size() / 2));
        return true;
    }

    // Keep the client's message to change or forward it: full check, then
    // rebuild for the next hop.
    bool forward(const std::string& wire, std::string& next_hop) {
        if (XStatus s = up.reload<MoveOrder>(kept, wire); !s) return false;  // XCheck::full on this link
        kept.edit<MoveOrder>([](MoveOrder& o) { o.waypoints.clear(); });     // the server's own change
        next_hop = kept.save_compact<MoveOrder>();
        return true;
    }
};

}  // namespace

int main() {
    Client alice{1001};
    Server server;
    int failures = 0;

    failures += !server.on_order(1001, alice.order(1, 40, 60, 3));

    // A well-formed order for a unit the player does not own: the structure
    // is fine, the game rules refuse it.
    failures += server.on_order(1001, alice.order(2, 10, 10, 1));

    // Bytes cut on the way, or a forged length: rejected before any field is read.
    std::string cut = alice.order(0, 1, 1, 2);
    cut.resize(cut.size() - 8);
    failures += server.on_order(1001, cut);

    // Forward a kept order to a battle server, which trusts its peer.
    std::string next_hop;
    failures += !server.forward(alice.order(0, 20, 30, 4), next_hop);
    XReceiver<XLink::server_peer> peer(XBuffer::maximum_capacity(kOrderLimit));
    auto got = peer.load<MoveOrder>(next_hop);
    failures += !got;
    if (got) {
        const MoveOrder& o = got->root<MoveOrder>();
        std::printf("battle server: unit %u -> (%d, %d), %zu waypoints, %zu bytes\n", o.unit_id, o.to_x, o.to_y,
                    std::size_t(o.waypoints.size()), next_hop.size());
        failures += o.to_x != 20 || !o.waypoints.empty();
    }

    std::printf("%s\n", failures == 0 ? "OK" : "FAILED");
    return failures == 0 ? 0 : 1;
}
