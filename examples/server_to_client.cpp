// Use case 1: a server sends whole states to a client (docs/GETTING_STARTED.md).
//
// The whole-state path: what a client gets when it joins or after a
// reconnect. Once the client holds a copy, the server sends only what
// changed: examples/delta_sync.cpp for a state the client owns,
// examples/shared_delta.cpp for a viewport onto a shared world.
//
// Best practices shown here:
//   connect - the schema handshake is the first frame each way; a client of
//             another generation is refused and asked to update, versions of
//             one add-only family are accepted (docs/SCHEMA_EVOLUTION.md);
//   server  - mutate the state in place (proxy, or edit() for loops), write
//             the wire into a reused string (save_verified_into) with the
//             default emission, which sends a churned buffer compact;
//   link    - frame each wire with its length and cap the frame size;
//   client  - one XReceiver<XLink::from_server> per link (trusted tier: the
//             header and image bounds are checked, the graph is not walked),
//             one held buffer reloaded for every wire (no new mapping per
//             message), XStatus instead of exceptions (the client may be
//             built with -fno-exceptions or MSVC /EHs-c-), read in place.
#include "game_protocol.hpp"
#include "../xoffset_handshake.hpp"
#include "../xoffset_receiver.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

constexpr std::size_t kViewLimit = 1 << 20;  // the largest WorldView either end accepts

// ---- server -----------------------------------------------------------------

struct Server {
    TypedXBuffer<WorldView> world =
        TypedXBuffer<WorldView>::create(4096, XBuffer::maximum_capacity(kViewLimit));
    std::string wire;  // reused for every send: no host allocation once it has its size

    Server() {
        auto units = world.root().units();
        for (std::uint32_t i = 0; i < 16; ++i) {
            auto u = units.emplace_back();  // proxies stay valid if the buffer grows
            u.id() = i;
            u.owner() = i % 2 ? 7 : 9;
            u.x() = std::int32_t(i) * 10;
            u.y() = 0;
            u.hp() = 100;
            u.name() = "unit-" + std::to_string(i);
        }
    }

    void tick(std::uint64_t t) {
        // A pin scope gives plain references at plain speed for the loop.
        world.edit([&](WorldView& v) {
            v.tick = t;
            for (Unit& u : v.units) {
                u.y += 1;
                if (u.id == t) u.hp -= 25;
            }
        });
    }

    // The arena bytes are the message; nothing is encoded per field.
    const std::string& snapshot() {
        world.save_verified_into(wire);
        return wire;
    }
};

// ---- link: u32 length + wire --------------------------------------------------

std::string frame(const std::string& wire) {
    std::string out(4, '\0');
    const auto n = static_cast<std::uint32_t>(wire.size());
    for (int i = 0; i < 4; ++i) out[std::size_t(i)] = char((n >> (8 * i)) & 0xFF);
    return out + wire;
}

// The payload of one frame, or false when it is malformed or too large.
bool unframe(const std::string& in, std::string& wire) {
    if (in.size() < 4) return false;
    std::uint32_t n = 0;
    for (int i = 0; i < 4; ++i) n |= std::uint32_t(static_cast<unsigned char>(in[std::size_t(i)])) << (8 * i);
    if (n > kViewLimit + 4096 || in.size() - 4 != n) return false;  // cap before copying
    wire.assign(in, 4, n);
    return true;
}

// ---- client -------------------------------------------------------------------

struct Client {
    XReceiver<XLink::from_server> rx{XBuffer::maximum_capacity(kViewLimit)};
    TypedXBuffer<WorldView> held = TypedXBuffer<WorldView>::create(4096, XBuffer::maximum_capacity(kViewLimit));
    std::string wire;
    bool has_view = false;

    bool receive(const std::string& bytes) {
        if (!unframe(bytes, wire)) return false;
        // reload: the trusted tier on this link; on failure `held` is left
        // empty, so the client asks for the next whole state.
        if (XStatus s = rx.reload(held, wire); !s) {
            std::printf("client: rejected (%s): %s\n", to_string(s.code()), s.error().message.c_str());
            has_view = false;
            return false;
        }
        has_view = true;
        return true;
    }

    void draw() {
        held.read([](const WorldView& v) {
            int mine = 0;
            for (const Unit& u : v.units) mine += u.owner == 7;
            std::printf("client: tick %llu, %zu units (%d mine), unit 3 at (%d, %d) hp %d \"%s\"\n",
                        static_cast<unsigned long long>(v.tick), std::size_t(v.units.size()), mine, v.units[3].x,
                        v.units[3].y, v.units[3].hp, std::string(v.units[3].name.view()).c_str());
        });
    }
};

}  // namespace

int main() {
    Server server;
    Client client;

    // Connect. The server answers every valid hello, a refusal included (the
    // ack names the refused root types), and closes a refused connection; the
    // client sends nothing else before an ack that accepts it. Neither side
    // throws, so the client builds without exceptions.
    std::string hello, ack, ack_in;
    if (!unframe(frame(make_schema_hello<WorldView>("client-1.0")), hello)) return 1;
    const XSchemaHelloResult peer = accept_schema_hello<WorldView>(hello, ack, "server-1.0");
    if (!peer.accepted()) return 1;
    if (!unframe(frame(ack), ack_in)) return 1;
    if (XSchemaAckResult r = read_schema_ack<WorldView>(ack_in); !r) {
        std::printf("client: update needed: %s\n", r.message.c_str());
        return 1;
    }
    std::printf("handshake: %s accepted\n", peer.label.c_str());

    for (std::uint64_t t = 1; t <= 3; ++t) {
        server.tick(t);
        const std::string bytes = frame(server.snapshot());
        std::printf("server: tick %llu, wire %zu bytes\n", static_cast<unsigned long long>(t), server.wire.size());
        if (!client.receive(bytes)) return 1;
        client.draw();
    }

    // A frame cut short on the way is rejected before anything is copied;
    // the client keeps its last view and asks again.
    std::string cut = frame(server.snapshot());
    cut.resize(cut.size() / 2);
    if (client.receive(cut)) return 1;
    std::printf("client: truncated frame dropped, still showing tick %llu\n",
                static_cast<unsigned long long>(client.held.root().tick()));

    const bool same = client.has_view && client.held.root().tick() == server.world.root().tick() &&
                      client.held.root().units()[3].hp() == server.world.root().units()[3].hp();
    std::printf("%s\n", same ? "OK" : "MISMATCH");
    return same ? 0 : 1;
}
