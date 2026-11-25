// Use case 3: servers hand state to each other (docs/GETTING_STARTED.md).
//
// A player moves from zone server A to zone server B; when the player leaves
// B, B sends the changed state to the storage server. Every hop is the same
// three steps: save the arena bytes, receive them into a held buffer, change
// them in place. Nothing is encoded or decoded per field, and the free space
// a hop leaves travels with the wire, so the next hop keeps changing the
// state without it degrading.
//
// Best practices shown here:
//   - The schema handshake first on every server link
//     (make_schema_hello<...>(label, XSchemaRole::server)), so servers of
//     another generation never exchange state (docs/SCHEMA_EVOLUTION.md).
//   - XReceiver<XLink::server_peer> between servers of one generation: the
//     trusted tier (header and image bounds checked, graph walk skipped). It
//     assumes a link the servers control (TLS / mTLS or an internal network);
//     never use it for client data. Debug and sanitizer builds run the full
//     check anyway.
//   - One held buffer per slot, reloaded for every incoming player; Automatic
//     growth so in-place changes never run out of room below the limit.
//   - edit() with an expected growth for a batch of changes that allocate.
#include "game_protocol.hpp"
#include "../xoffset_handshake.hpp"
#include "../xoffset_receiver.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

constexpr std::size_t kPlayerLimit = 256 << 10;

XBuffer::MaximumCapacity player_cap() {
    return XBuffer::maximum_capacity(kPlayerLimit).with_growth(XGrowPolicy::Automatic);
}

struct ZoneServer {
    const char* name;
    XReceiver<XLink::server_peer> rx{player_cap()};
    TypedXBuffer<PlayerState> player = TypedXBuffer<PlayerState>::create(4096, player_cap());
    std::string out;

    // Connect to `to`: the side that connects sends the hello as a server.
    bool connect(ZoneServer& to) {
        std::string ack;
        const XSchemaHelloResult peer =
            accept_schema_hello<PlayerState>(make_schema_hello<PlayerState>(name, XSchemaRole::server), ack, to.name);
        if (!peer.accepted() || !read_schema_ack<PlayerState>(ack)) {
            std::printf("%s: %s refused the link\n", name, to.name);
            return false;
        }
        return true;
    }

    bool admit(const std::string& wire) {
        if (XStatus s = rx.reload(player, wire); !s) {
            std::printf("%s: refused player (%s)\n", name, to_string(s.code()));
            return false;
        }
        std::printf("%s: player %llu \"%s\" arrived, %zu bytes\n", name,
                    static_cast<unsigned long long>(player.root().player_id()),
                    player.root().name().str().c_str(), wire.size());
        return true;
    }

    const std::string& leave() {
        player.save_verified_into(out, XSaveOptions{.emission = XWireEmission::zero_runs});
        return out;
    }
};

}  // namespace

int main() {
    // Zone A creates the player.
    ZoneServer a{"zone A"};
    {
        auto p = a.player.root();
        p.player_id() = 1001;
        p.level() = 12;
        p.gold() = 500;
        p.name() = "Alice";
        for (std::uint32_t item : {7u, 8u, 9u}) p.items().push_back(item);
    }
    a.player.edit([](PlayerState& p) { p.quests.emplace(100u, 3); }, 256);

    // A -> B.
    ZoneServer b{"zone B"};
    if (!a.connect(b) || !b.admit(a.leave())) return 1;

    // B plays: scalars through the proxy, a batch of allocating changes in
    // one pin scope (expected_growth reserves room so it cannot fail midway).
    b.player.root().gold() += 250;
    const bool ok = b.player.edit(
        [](PlayerState& p) {
            p.level += 1;
            for (std::uint32_t item = 20; item < 40; ++item) p.items.push_back(item);
            p.quests[100u] = 4;
            p.quests.emplace(101u, 1);
        },
        1024);
    if (!ok || !b.player.ok()) {
        std::printf("zone B: change failed: %s\n", to_string(b.player.error()));
        return 1;
    }

    // B -> storage server, the same way.
    ZoneServer storage{"storage"};
    if (!b.connect(storage) || !storage.admit(b.leave())) return 1;

    const bool same = storage.player.read([](const PlayerState& p) {
        std::printf("storage: level %d, gold %lld, %zu items, quest 100 at %d, %zu quests\n", p.level,
                    static_cast<long long>(p.gold), std::size_t(p.items.size()), p.quests.find(100u)->second,
                    std::size_t(p.quests.size()));
        return p.level == 13 && p.gold == 750 && p.items.size() == 23 && p.quests.size() == 2;
    });
    std::printf("%s\n", same ? "OK" : "MISMATCH");
    return same ? 0 : 1;
}
