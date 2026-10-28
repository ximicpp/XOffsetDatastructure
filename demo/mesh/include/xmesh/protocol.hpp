// Device mesh demo: what goes over the wire between a device and the hub.
// FROZEN INTERFACE (demo/mesh/INTERFACES.md section 4). Everything here is
// inline; the sockets that carry it (net.hpp) belong to the network thread.
//
// First version: pure relay. Every device opens one TCP connection to the
// hub and keeps it; the hub registers it, tells everyone who is in the
// mesh, and forwards each Hop frame to the device named in the header
// without reading the payload. Devices never connect to each other, so an
// emulator or a device behind NAT needs no inbound port.
//
// Frame = 32-byte FrameHeader (little endian) + payload. Receivers check the
// magic, the allowed types and max_payload(type) BEFORE allocating the
// payload buffer. Data parts are XOffset wires, sent as they are: no
// serialization step on either side.
//
//   device                         hub                          device
//     | Hello (schema) ------------->|                              |
//     |<------------------ HelloAck  |                              |
//     | Register (PeerInfo) -------->|                              |
//     |<------------------- Welcome  |                              |
//     |<-------------------- Roster  |-- Roster ------------------->|
//     |<------- Job (HopMeta, hop 0) |                              |
//     | Hop (to = route[1]) -------->|-- Hop (from set by hub) ---->|
//     | Report --------------------->|<-------------------- Report  |
//     |                              |<---- Hop (to = route[2]) ... |
//     |<----------------------- Bye  |-- Bye ---------------------->|
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include "xmesh/result.hpp"
#include "xmesh/schema.hpp"
#include "xmesh/types.hpp"
#include "xoffset_handshake.hpp"

namespace xmesh::protocol {

inline constexpr std::uint32_t kFrameMagic = 0x31534D58u;   // "XMS1"

enum class MsgType : std::uint16_t {
    Hello = 1,      // device->hub: XSchemaHello wire (hello()); label = device name
    HelloAck = 2,   // hub->device: XSchemaHelloAck wire; anything but `same` for every
                    // root type is a rejection, followed by Error and a close
    Register = 3,   // device->hub: PeerInfo wire
    Welcome = 4,    // hub->device: Welcome wire (the device id it got)
    Roster = 5,     // hub->every device: Roster wire after each join and leave;
                    // correlation = Roster::version
    Job = 6,        // hub->route[0]: HopMeta wire with hop 0 and both digests 0;
                    // correlation = job
    Hop = 7,        // device->hub->device: hop payload (pack_hop); to = the next route
                    // device, from = the sender (set by the hub); correlation = job
    Report = 8,     // device->hub: HopReport wire, one per Job or Hop handled;
                    // correlation = job
    Error = 9,      // either way: UTF-8 text
    Bye = 10,       // hub->device: empty; the device finishes and exits 0
};

struct FrameHeader {
    std::uint32_t magic;          // kFrameMagic
    std::uint16_t type;           // MsgType
    std::uint16_t flags;          // 0
    std::uint32_t length;         // payload bytes
    std::uint32_t from;           // sender device id, 0 = the hub; the hub overwrites it on
                                  // every frame it forwards, so a device cannot claim another's id
    std::uint32_t to;             // receiver device id, 0 = the hub
    std::uint32_t reserved;       // 0
    std::uint64_t correlation;    // see MsgType
};
static_assert(sizeof(FrameHeader) == 32);

inline constexpr std::uint32_t kHubId = 0;

// Data parts (one object's wire): at most 16 MiB. A hop payload adds the
// envelope and the HopMeta wire.
inline constexpr std::uint32_t kMaxDataBytes = std::uint32_t(16) << 20;
inline constexpr std::uint32_t kMaxMetaBytes = std::uint32_t(64) << 10;

// Per-type payload limit, checked against FrameHeader::length before the
// payload is read: Hello / HelloAck 64 KiB (XSCHEMA_HELLO_MAX_BYTES),
// Register / Welcome 4 KiB, Roster 256 KiB, Job 64 KiB, Hop 16 MiB + 64 KiB +
// 16, Report 64 KiB, Error 4 KiB, Bye 0; 0 for an unknown type.
inline std::uint32_t max_payload(MsgType type) noexcept {
    switch (type) {
        case MsgType::Hello:
        case MsgType::HelloAck: return std::uint32_t(XSCHEMA_HELLO_MAX_BYTES);
        case MsgType::Register:
        case MsgType::Welcome: return 4u << 10;
        case MsgType::Roster: return 256u << 10;
        case MsgType::Job: return kMaxMetaBytes;
        case MsgType::Hop: return kMaxDataBytes + kMaxMetaBytes + 16u;
        case MsgType::Report: return 64u << 10;
        case MsgType::Error: return 4u << 10;
        case MsgType::Bye: return 0;
    }
    return 0;
}

inline const char* type_name(MsgType type) noexcept {
    switch (type) {
        case MsgType::Hello: return "Hello";
        case MsgType::HelloAck: return "HelloAck";
        case MsgType::Register: return "Register";
        case MsgType::Welcome: return "Welcome";
        case MsgType::Roster: return "Roster";
        case MsgType::Job: return "Job";
        case MsgType::Hop: return "Hop";
        case MsgType::Report: return "Report";
        case MsgType::Error: return "Error";
        case MsgType::Bye: return "Bye";
    }
    return "unknown";
}

// maximum_capacity for loading a control wire (PeerInfo, Welcome, Roster,
// HopMeta, HopReport).
inline constexpr std::uint32_t kControlCap = std::uint32_t(1) << 20;

// PeerInfo::flags and RosterEntry::flags.
inline constexpr std::uint32_t kPeerExceptions = 1u << 0;   // built with exceptions
inline constexpr std::uint32_t kPeerEvil = 1u << 1;         // takes evil jobs as route[0]
inline constexpr std::uint32_t kPeerReflection = 1u << 2;   // C++26 reflection schema backend
inline constexpr std::uint32_t kPeerMailbox = 1u << 3;      // replaying archived hops (INTERFACES.md 6)

// Default ports of the hub: frames, and HTTP (dashboard and join link).
inline constexpr std::uint16_t kDefaultPort = 47801;
inline constexpr std::uint16_t kDefaultHttpPort = 47802;

// ---- digests --------------------------------------------------------------

// FNV-1a 64. content_digest (types.hpp) feeds it a canonical field walk;
// wire_digest feeds it the bytes of a data part.
inline constexpr std::uint64_t kFnvOffset = 0xcbf29ce484222325ull;
inline std::uint64_t fnv1a64(const void* data, std::size_t size, std::uint64_t h = kFnvOffset) noexcept {
    const auto* p = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}
inline std::uint64_t wire_digest(std::string_view bytes) noexcept {
    return fnv1a64(bytes.data(), bytes.size());
}

// ---- hop payload ------------------------------------------------------------

// Hop payload = HopEnvelope + HopMeta wire + zeros to a multiple of 8 +
// data part. The hub forwards it untouched; it is also the archive and
// mailbox file format (INTERFACES.md 6).
inline constexpr std::uint32_t kHopMagic = 0x31484D58u;   // "XMH1"
struct HopEnvelope {
    std::uint32_t magic;        // kHopMagic
    std::uint32_t meta_bytes;   // HopMeta wire, <= kMaxMetaBytes
    std::uint32_t data_bytes;   // data part, <= kMaxDataBytes
    std::uint32_t reserved;     // 0
};
static_assert(sizeof(HopEnvelope) == 16);

inline constexpr std::size_t pad8(std::size_t n) noexcept { return (n + 7) & ~std::size_t(7); }

inline std::string pack_hop(std::string_view meta_wire, std::string_view data_wire) {
    const HopEnvelope env{kHopMagic, std::uint32_t(meta_wire.size()), std::uint32_t(data_wire.size()), 0};
    std::string out(sizeof env + pad8(meta_wire.size()) + data_wire.size(), '\0');
    std::memcpy(out.data(), &env, sizeof env);
    std::memcpy(out.data() + sizeof env, meta_wire.data(), meta_wire.size());
    std::memcpy(out.data() + sizeof env + pad8(meta_wire.size()), data_wire.data(), data_wire.size());
    return out;
}

struct HopParts {
    std::string_view meta;   // HopMeta wire
    std::string_view data;   // data part
};
// Views into `payload`. An error when the envelope is bad, a size is over
// its limit, the padding is not zero or the lengths do not add up exactly.
inline Result<HopParts> unpack_hop(std::string_view payload) {
    HopEnvelope env{};
    if (payload.size() < sizeof env) return Error{"hop payload shorter than its envelope"};
    std::memcpy(&env, payload.data(), sizeof env);
    if (env.magic != kHopMagic) return Error{"hop envelope: bad magic"};
    if (env.reserved != 0) return Error{"hop envelope: reserved field is not 0"};
    if (env.meta_bytes == 0 || env.meta_bytes > kMaxMetaBytes) return Error{"hop envelope: bad meta length"};
    if (env.data_bytes > kMaxDataBytes) return Error{"hop envelope: data part over 16 MiB"};
    const std::size_t data_at = sizeof env + pad8(env.meta_bytes);
    if (payload.size() != data_at + env.data_bytes) return Error{"hop envelope: lengths do not match the payload"};
    for (std::size_t i = sizeof env + env.meta_bytes; i < data_at; ++i)
        if (payload[i] != '\0') return Error{"hop envelope: padding is not zero"};
    return HopParts{payload.substr(sizeof env, env.meta_bytes), payload.substr(data_at, env.data_bytes)};
}

// ---- verdicts ---------------------------------------------------------------

// HopReport::verdict: what a device did with a Job or a Hop, in the order
// it checks (hop.hpp). Wire values: never renumber.
enum class HopVerdict : std::uint8_t {
    Ok = 0,              // checked, mutated (unless it ended the job) and sent on
    BadEnvelope = 1,     // unpack_hop failed
    BadMeta = 2,         // the HopMeta wire failed to load, or a field is out of range
    Misrouted = 3,       // route[hop + 1] is not this device
    TooLarge = 4,        // data part over data_limit(type, scale)
    WireDigest = 5,      // data bytes differ from HopMeta::wire_digest (damaged on the way)
    LoadRejected = 6,    // load_verified with receive_cap refused the data part
    ContentDigest = 7,   // loaded, but the value digest differs from HopMeta::content_digest
    ModelMismatch = 8,   // the reference model differs (the error names the field)
    MutateFailed = 9,
    SaveFailed = 10,
    MakeFailed = 11,     // route[0] could not build state 0 (or the evil part)
};

inline const char* verdict_name(HopVerdict v) noexcept {
    switch (v) {
        case HopVerdict::Ok: return "ok";
        case HopVerdict::BadEnvelope: return "bad_envelope";
        case HopVerdict::BadMeta: return "bad_meta";
        case HopVerdict::Misrouted: return "misrouted";
        case HopVerdict::TooLarge: return "too_large";
        case HopVerdict::WireDigest: return "wire_digest";
        case HopVerdict::LoadRejected: return "load_rejected";
        case HopVerdict::ContentDigest: return "content_digest";
        case HopVerdict::ModelMismatch: return "model_mismatch";
        case HopVerdict::MutateFailed: return "mutate_failed";
        case HopVerdict::SaveFailed: return "save_failed";
        case HopVerdict::MakeFailed: return "make_failed";
    }
    return "unknown";
}

// The verdict the first receiver of an evil hop must give. An evil job
// passes when route[1] reports exactly this and nothing goes further.
inline constexpr HopVerdict expected_verdict(EvilCase evil) noexcept {
    switch (evil) {
        case EvilCase::None: return HopVerdict::Ok;
        case EvilCase::Truncated:
        case EvilCase::BadOffset:
        case EvilCase::SchemaHash:
        case EvilCase::WrongType: return HopVerdict::LoadRejected;
        case EvilCase::TooLarge: return HopVerdict::TooLarge;
        case EvilCase::StaleDigest: return HopVerdict::ContentDigest;
        case EvilCase::ModelDrift: return HopVerdict::ModelMismatch;
    }
    return HopVerdict::Ok;
}

// ---- schema handshake ---------------------------------------------------------

// Device and hub must work in exactly the same version of all eleven roots
// (DEPLOYMENT.md 4.3 rule 3: only `same` for device-to-device data).
#define XMESH_ROOTS Document, Order, SensorSeries, KvConfig, SceneTree, ChatRoom, \
                    PeerInfo, Welcome, Roster, HopMeta, HopReport

// The device's Hello; label = its name (clipped to 256 bytes).
inline std::string hello(std::string_view label) {
    return make_schema_hello<XMESH_ROOTS>(label);
}
// The device's reading of the hub's HelloAck.
inline XSchemaAckResult read_ack(std::string_view ack) {
    return read_schema_ack<XMESH_ROOTS>(ack);
}
// The hub's answer: `same` for each root type whose fingerprint and field
// names equal the hub's, else a rejection. Accepted only when every root
// is in the hello (check with hello_complete) and accepted().
inline XSchemaHelloResult accept_hello(std::string_view hello_wire, std::string& ack_out,
                                       std::string_view hub_label) {
    return accept_schema_hello<XMESH_ROOTS>(hello_wire, ack_out, hub_label);
}
template <class... Roots>
inline bool hello_has_all(const XSchemaHelloResult& r) noexcept {
    return (... && (r.find(wire_root_type_id_v<Roots>()) != nullptr));
}
inline bool hello_complete(const XSchemaHelloResult& r) noexcept {
    return hello_has_all<XMESH_ROOTS>(r);
}

} // namespace xmesh::protocol
