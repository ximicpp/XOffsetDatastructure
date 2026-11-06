// Delta sync from a server to a client over a trusted link
// (docs/DELTA_SYNC.md). The server mutates its buffer in place and keeps one
// XDeltaBaseline per client (or per group of clients that always get the
// same deltas); each tick it sends only the bytes that changed. The client
// applies them with XReceiver<XLink::from_server>, which checks the delta's
// framing (admit) and patches its copy in place.
//
// A delta that does not follow what the client holds (one was lost, the
// client restarted) is rejected; the client then asks for a key delta and the
// server resets that client's baseline. Deltas need the same schema on both
// ends: when the schema handshake (examples/schema_handshake.cpp) does not
// answer `same` for the root type, send whole wires instead
// (docs/SCHEMA_EVOLUTION.md).
#include "../xoffset_receiver.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

using namespace XOffsetDatastructure;

struct Unit {
    std::uint32_t id;
    std::int32_t hp;
    XString name;
};
XOFFSET_RECORD(Unit, id, hp, name)

struct View {
    std::uint64_t tick;
    XVector<Unit> units;
};
XOFFSET_RECORD(View, tick, units)
XOFFSET_REGISTER_SCHEMA_NAME(View, "example.View")

int main() {
    // Server: the world as this client sees it.
    auto world = TypedXBuffer<View>::create();
    for (std::uint32_t i = 0; i < 8; ++i) {
        world.root().units.emplace_back();
        Unit& u = world.root().units.back();
        u.id = i;
        u.hp = 100;
        u.name = ("unit-" + std::to_string(i)).c_str();
    }
    XDeltaBaseline client_base;  // what the client holds: nothing yet

    // Client: its copy, the cursor that says which state the copy holds, and
    // the receiver for data from the server.
    XBuffer held(4096, XBuffer::maximum_capacity(1 << 20));
    XDeltaCursor cursor;
    XReceiver<XLink::from_server> rx(XBuffer::maximum_capacity(1 << 20));

    std::string delta;
    for (int tick = 1; tick <= 6; ++tick) {
        // Server: change a few fields in place, then write the delta.
        View& v = world.root();
        v.tick = std::uint64_t(tick);
        v.units[std::size_t(tick)].hp -= 7 * tick;
        if (tick == 3) {
            v.units.emplace_back();
            v.units.back().id = 8;
            v.units.back().hp = 50;
            v.units.back().name = "reinforcement";
        }
        const XDeltaStats stats = world.save_delta_into(delta, client_base);
        if (stats.empty) continue;  // nothing changed: nothing to send
        if (tick == 4) {
            std::printf("tick %d: delta of %zu bytes lost on the way\n", tick, delta.size());
            continue;
        }

        // Client: apply; on rejection ask for a key delta.
        const XStatus r = rx.apply_delta<View>(held, delta, cursor);
        if (!r) {
            std::printf("tick %d: delta rejected (%s); asking for a key delta\n", tick, r.error().message.c_str());
            client_base.reset();  // server: the next delta for this client is a key delta
            world.save_delta_into(delta, client_base);
            if (!rx.apply_delta<View>(held, delta, cursor)) return 1;
            std::printf("tick %d: key delta of %zu bytes applied\n", tick, delta.size());
            continue;
        }
        std::printf("tick %d: %s delta of %zu bytes applied (whole buffer %zu bytes)\n", tick,
                    stats.key ? "key" : "small", delta.size(), world.save_compact().size());
    }

    // The client's copy is the server's, read in place.
    const View& seen = held.root<View>();
    const bool same = seen.tick == world.root().tick && seen.units.size() == world.root().units.size() &&
                      seen.units[5].hp == world.root().units[5].hp;
    std::printf("client holds tick %llu, %zu units, unit 5 hp %d: %s\n",
                static_cast<unsigned long long>(seen.tick), std::size_t(seen.units.size()), seen.units[5].hp,
                same ? "same as the server" : "DIFFERENT");
    return same ? 0 : 1;
}
