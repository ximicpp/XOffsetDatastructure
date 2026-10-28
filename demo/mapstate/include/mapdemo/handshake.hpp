// Map state demo: the schema handshake of each connection (library
// xoffset_handshake.hpp; docs/SCHEMA_EVOLUTION.md). Additive helper next to
// the frozen headers (10-28); OWNER: common-library thread.
//
// The client's Hello lists the version it works in of every root type it
// sends or receives; A answers with a HelloAck: same, convert (A converts for
// this client) or a rejection, after which A closes the connection. A's Hello
// to B lists UnitStateBatch only; B accepts its own version and nothing else.
// The C++20 client builds compile this header too (it needs no reflection).
#pragma once
#include <string>
#include <string_view>
#include "mapdemo/schema.hpp"
#include "xoffset_handshake.hpp"

namespace mapdemo::handshake {

// The client's Hello: the working versions of MapCatalog, ViewRequest,
// ViewSnapshot and UnitEvent in this build; `label` (at most 256 bytes) is
// for A's log.
inline std::string client_hello(std::string_view label) {
    return make_schema_hello<MapCatalog, ViewRequest, ViewSnapshot, UnitEvent>(label);
}

// The client's reading of A's HelloAck: ok when A accepted every root type.
inline XSchemaAckResult read_client_ack(std::string_view ack) {
    return read_schema_ack<MapCatalog, ViewRequest, ViewSnapshot, UnitEvent>(ack);
}

// The schema name of a root type id of the client's Hello, for messages.
inline const char* client_root_name(std::uint64_t root_type_id) noexcept {
    if (root_type_id == wire_root_type_id_v<MapCatalog>()) return "MapCatalog";
    if (root_type_id == wire_root_type_id_v<ViewRequest>()) return "ViewRequest";
    if (root_type_id == wire_root_type_id_v<ViewSnapshot>()) return "ViewSnapshot";
    if (root_type_id == wire_root_type_id_v<UnitEvent>()) return "UnitEvent";
    return "an unknown root type";
}

} // namespace mapdemo::handshake
