// Game server demo: frame types and limits of every link.
//
//   client <-> gateway   (untrusted: the gateway caps and rate-limits)
//   gateway <-> logic    (server link: every body starts with the player id)
//   logic  <-> logic     (server link)
//
// Bodies are a few fixed-width little-endian fields followed by XOffset
// bytes (a schema hello or ack, a command wire, a delta, a save). The
// XOffset bytes travel unchanged from the process that wrote them to the one
// that reads them.
#ifndef GS_PROTOCOL_HPP
#define GS_PROTOCOL_HPP

#include <cstddef>
#include <cstdint>

namespace gs {

enum Frame : std::uint8_t {
    // client -> gateway
    c_hello = 1,       // schema hello (xoffset_handshake.hpp)
    c_login = 2,       // u64 player id, u64 resume token (0: new session)
    c_command = 3,     // game::Command wire
    c_ping = 4,        // u64 client time
    c_logout = 5,      // (empty)
    c_gm_migrate = 6,  // u64 player id, u32 target node (an operator action; a real deployment uses an admin port)
    // gateway -> client
    g_ack = 20,       // schema ack
    g_login_ok = 21,  // u64 resume token, u32 logic node
    g_error = 22,     // text
    g_pong = 23,      // u64 client time
    g_downlink = 24,  // u8 stream, u8 key, delta
    // gateway -> logic (each body starts with u64 player id)
    l_hello = 40,     // u32 gateway id, then a schema hello (server role)
    l_login = 41,     // u64 player
    l_command = 42,   // u64 player, command wire
    l_logout = 43,    // u64 player
    l_resync = 44,    // u64 player
    l_migrate = 45,   // u64 player, u32 target node
    // logic -> gateway
    l_ack = 60,       // u32 node id, then a schema ack
    l_downlink = 61,  // u64 player, u8 stream, u8 key, delta
    l_moved = 62,     // u64 player, u32 node now owning it
    // logic -> logic
    n_hello = 80,      // u32 node id, then a schema hello (server role)
    n_ack = 81,        // u32 node id, then a schema ack
    n_join = 82,       // u64 alliance, u64 player, u32 reply node
    n_joined = 83,     // u64 player, u64 alliance (0: refused)
    n_migrate = 84,    // u64 player, u32 gateway id, u8 dirty, u32 player wire size, player wire, city wire
    n_command = 85,    // u64 player, u32 gateway id, command wire (forwarded after a migration)
};

enum class Stream : std::uint8_t { player = 0, city = 1 };

// Limits: a frame longer than its link's limit closes the connection before
// its body is buffered.
constexpr std::size_t kClientFrameLimit = 4096;     // client -> gateway
constexpr std::size_t kCommandLimit = 4096;         // a command wire
constexpr std::size_t kServerFrameLimit = 1 << 20;  // server links
constexpr std::size_t kPlayerLimit = 256 << 10;
constexpr std::size_t kCityLimit = 64 << 10;
constexpr std::size_t kAllianceLimit = 1 << 20;
constexpr std::size_t kConfigLimit = 64 << 10;

// Game rules shared by the logic nodes and the bots (the bots choose valid
// commands from their replica).
constexpr std::int32_t kFarmMaxLevel = 20;
constexpr std::int64_t kFarmGoldPerLevel = 100;
constexpr std::uint64_t kAllianceCount = 16;  // alliances 1..16, each owned by node (id % nodes)

}  // namespace gs

#endif
