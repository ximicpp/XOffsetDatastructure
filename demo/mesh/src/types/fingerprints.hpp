// Device mesh demo: the schema fingerprints of the eleven roots
// (INTERFACES.md section 3), as this build computes them and as frozen.
// OWNER: types thread. Used by xmesh_schema_freeze (schema.lock) and the
// self-test, so every build, device builds included, checks them.
#pragma once
#include <array>
#include <cstdint>
#include "xmesh/schema.hpp"
#include "xoffset_handshake.hpp"

namespace xmesh::fp {

struct Fingerprint {
    const char* name;
    std::uint32_t size;
    std::uint32_t align;
    std::uint64_t schema_hash;   // wire_schema_hash_v
    std::uint64_t name_digest;   // schema_name_digest_v
};

template <class T>
Fingerprint of(const char* name) {
    return {name, std::uint32_t(sizeof(T)), std::uint32_t(alignof(T)), XOffsetDatastructure::wire_schema_hash_v<T>(),
            XOffsetDatastructure::schema_name_digest_v<T>};
}

inline std::array<Fingerprint, 11> current() {
    return {of<Document>("Document"),   of<Order>("Order"),         of<SensorSeries>("SensorSeries"),
            of<KvConfig>("KvConfig"),   of<SceneTree>("SceneTree"), of<ChatRoom>("ChatRoom"),
            of<PeerInfo>("PeerInfo"),   of<Welcome>("Welcome"),     of<Roster>("Roster"),
            of<HopMeta>("HopMeta"),     of<HopReport>("HopReport")};
}

// INTERFACES.md section 3 (schema version 1).
inline constexpr std::array<Fingerprint, 11> kFrozen{{
    {"Document", 80, 8, 0x87787a9f1767d50cull, 0x65e21ba4d06b5172ull},
    {"Order", 160, 8, 0x39adf9e08cc953b8ull, 0x0173d7de2c384d78ull},
    {"SensorSeries", 104, 8, 0x465895f95425a12full, 0x316b9a0cc3d526dfull},
    {"KvConfig", 64, 8, 0xec54cd6bd56be812ull, 0xa8d471d4e8efa750ull},
    {"SceneTree", 56, 8, 0x087476c9f00da0d7ull, 0x433d8f17c9332544ull},
    {"ChatRoom", 88, 8, 0x9c8bd9ae5c45da97ull, 0x6c00d8b7c16cde24ull},
    {"PeerInfo", 32, 4, 0x6b91e1726bb9acc8ull, 0x4442a4259c239a62ull},
    {"Welcome", 32, 4, 0x8267cae10f66a5e5ull, 0x4b8c42b30eb9a7a1ull},
    {"Roster", 24, 8, 0xe3261a05c9ff8a97ull, 0xe0095f48fdcff559ull},
    {"HopMeta", 56, 8, 0x564e0bf1979abbccull, 0x4911e6cf9a2e8e79ull},
    {"HopReport", 88, 8, 0x3572ca1a55546563ull, 0x7fccbae2e9b0458eull},
}};

inline bool same(const Fingerprint& a, const Fingerprint& b) noexcept {
    return a.size == b.size && a.align == b.align && a.schema_hash == b.schema_hash && a.name_digest == b.name_digest;
}

} // namespace xmesh::fp
