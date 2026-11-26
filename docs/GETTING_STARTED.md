# Getting started: core use cases and recommended patterns (2.0.0-preview.4)

This document walks new users through the use cases: server to server, server to client, and client
uplink. Each use case has a runnable example, and the example is the recommended pattern. First get
`helloworld` running with the [README's C++20 quick start](../README.md#quick-start-c20); the full API
description is in the [user guide](USER_GUIDE.md).

## 1. What problem this library solves

Servers (Linux) and clients (Windows, iOS, Android, all 64-bit little-endian) exchange data using the
same C++ type definitions:

- Objects are built directly in one contiguous arena and read and modified in place (including
  variable-length changes).
- The arena bytes are the message: sending copies the whole block, receiving validates and then copies,
  with no field-by-field encoding or decoding. Each end copies once; it is not zero-copy.
- The receiver keeps modifying in place and sends again; free space travels with the message, and
  further edits do not degrade.
- When the peer already has a copy, only the changed bytes are sent: deltas for whole state, shared
  deltas for viewports (the library includes a publisher).

## 2. Run the examples first

In the build directory of the README quick start (a C++20 configuration is enough; the C++26 reflection
build works as well):

```sh
cmake --build build --config Release --target server_to_client client_uplink server_to_server
./build/bin/Release/server_to_client
./build/bin/Release/client_uplink
./build/bin/Release/server_to_server
```

The three examples also have no-exception builds (`server_to_client_noexc` and so on); after a full
build, `ctest --test-dir build -C Release -L examples` runs all six.

| Use case | Sender → receiver | Receiver (link) | Default check tier | Example |
| --- | --- | --- | --- | --- |
| Whole state: join, reconnect | Server → client | `XReceiver<XLink::from_server>` | Trusted | [`server_to_client.cpp`](../examples/server_to_client.cpp) |
| Deltas: the client holds its own player and unit data | Server → client | `XReceiver<XLink::from_server>::apply_delta` | Trusted | [`delta_sync.cpp`](../examples/delta_sync.cpp) |
| Viewport: each client sees part of a shared world | Server → client | `XReceiver<XLink::from_server>::apply_sync` | Trusted | [`shared_delta.cpp`](../examples/shared_delta.cpp) |
| Uplink commands | Client → server | `XReceiver<XLink::from_client>::view` | Bounds (read-only) | [`client_uplink.cpp`](../examples/client_uplink.cpp) |
| Keeping, modifying or forwarding client data | Client → server | `XReceiver<XLink::from_client>::reload` | Full | [`client_uplink.cpp`](../examples/client_uplink.cpp) |
| Player migration, saving | Server ↔ server | `XReceiver<XLink::server_peer>` | Trusted | [`server_to_server.cpp`](../examples/server_to_server.cpp) |
| Matching schema versions on connect | Both directions | `xoffset_handshake.hpp` | — | [`schema_handshake.cpp`](../examples/schema_handshake.cpp) |

The check tier follows the link and is decided by the receiver's type; there is no global switch
([user guide section 8](USER_GUIDE.md#8-core-guarantees-and-api-stability)):

- **Trusted tier**: the default for server to server and server to client. It checks only the wire header
  (schema fingerprint, limit, sizes) and that the root fits, without walking the object graph. It
  assumes the sender is your own service from the same build and the link has TLS or mTLS or is on an
  internal network. Debug and sanitizer builds still validate fully.
- **Bounds tier**: the default for client uplink. It guarantees that no read goes outside the message,
  and the data is used read-only.
- **Full tier**: for keeping, modifying in place or forwarding data sent by clients.

Every tier checks structure only, not content: a message with a valid structure and arbitrary field
values is accepted. Field values sent by clients are checked by the server against its own state and the
game rules (`on_order` in `client_uplink.cpp`).

## 3. First step: one protocol header shared by both ends

[`examples/game_protocol.hpp`](../examples/game_protocol.hpp) holds the message types shared by the three
examples. The server build (C++26 reflection) and every client build (C++20) include the same header:

```cpp
struct Unit {
    std::uint32_t id;
    std::uint32_t owner;
    std::int32_t x, y, hp;          // written separately in the example, one field per line
    XString name;
    XSpare<8> spare{};              // reserved for later versions, see below
};
XOFFSET_RECORD(Unit, id, owner, x, y, hp, name, spare) // global scope, right after the type

struct WorldView {
    std::uint64_t tick;
    XVector<Unit> units;
    XSpare<20> spare{};
};
XOFFSET_RECORD(WorldView, tick, units, spare)
XOFFSET_REGISTER_SCHEMA_NAME(WorldView, "game.WorldView")   // one stable name per root type

XOFFSET_PIN_SCHEMA(WorldView, "record[s:40,a:8]{...}");     // pinned layout, see below
```

- Plain aggregates, with the library's containers `XString`, `XVector`, `XSet`, `XMap`; no pointers or
  `std::` containers, and fixed-width scalar types
  ([user guide section 2.2](USER_GUIDE.md#22-portable-scalars)).
- One `XOFFSET_RECORD` line per record, at global scope (it cannot go inside a namespace; for a type
  declared in a namespace write the qualified name, `XOFFSET_RECORD(game::Unit, ...)`). C++20 builds
  get the fields from it and C++26 builds check it with reflection; both builds write the same bytes.
- Register a schema name for each root type (the root of a message), then pin its layout with
  `XOFFSET_PIN_SCHEMA`: when any compiler computes a layout different from the pinned text, compilation
  fails, so differences between client toolchains show up at build time instead of as rejections at run
  time. The pinned text is generated by `schema_pin<T>("name")`; regenerate and commit it when the type
  changes ([SCHEMA_PINS.md](SCHEMA_PINS.md)).
- **Reserve bytes in the first release.** End every record with an `XSpare<N>` member (N zero bytes; 8
  in a repeated element and at least 16 in a root type are common choices, rounded up so the record keeps
  no tail padding). A later version takes new fields from these bytes and keeps the fingerprint, so
  servers and clients of the old and the new version keep exchanging wires and deltas with no conversion
  and update in any order. Only the first release can reserve them: a type released without spare bytes
  changes only by a new generation, with servers and clients updated together
  ([SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md) sections 1 to 3).
- **Handshake on every connection.** The first frame each way is the schema handshake: the client (or the
  connecting server, with `XSchemaRole::server`) sends `make_schema_hello<Roots...>(label)`, the server
  answers with `accept_schema_hello`, and the client checks the answer with `read_schema_ack` before it
  sends anything else. A peer of another generation is refused at connect time instead of having its
  wires rejected one by one. None of the three calls throws; the three use-case examples start this way
  ([`schema_handshake.cpp`](../examples/schema_handshake.cpp) adds the framing).

## 4. Server to client

### 4.1 Whole state (`server_to_client.cpp`)

When a client has just joined or reconnected, the server sends the whole state.

```cpp
// Server: modify in place, write into a reused string.
world.edit([&](WorldView& v) { v.tick = t; for (Unit& u : v.units) u.y += 1; });
world.save_verified_into(wire);

// Client: one receiver per link, one long-lived buffer reloaded again and again.
XReceiver<XLink::from_server> rx{XBuffer::maximum_capacity(kViewLimit)};
if (XStatus s = rx.reload(held, wire); !s) ask_for_whole_state();
held.read([](const WorldView& v) { draw(v); });       // read plain members in place
```

- The server modifies data: scattered fields through the `root()` proxy (still valid after the buffer
  moves), loops and bulk changes with `edit()` (plain references while pinned, as fast as a plain struct).
- Sending: `save_verified_into` reuses the same string and stops allocating once it is large enough.
  Keep the default emission: a buffer that churn has left with much free space or spare capacity goes
  out compact, any other buffer as its image. `XWireEmission::zero_runs` keeps the churned image and
  scans it, so on a churned buffer it is slower than the default
  ([PERFORMANCE.md section 2](PERFORMANCE.md#2-where-the-lead-holds)).
- Each message gets its own length frame, and frames that are too long are rejected by the limit before
  copying (the library does not define a frame format).
- Client: `reload` reuses `held`'s mapping, so no new mapping per message; on failure `held` becomes
  empty and the client asks the server for another copy. Client code uses only entry points that return
  `XStatus`, so no-exception builds work as well (section 7).

### 4.2 Sending only the changed bytes (`delta_sync.cpp`, `shared_delta.cpp`)

Once the client has a copy, the server stops sending the whole state:

- **The client's own state** (player, inventory, own units): deltas. The server has one
  `XDeltaBaseline` per client, and each tick `save_delta_into` writes only the changed bytes; the client
  patches its copy in place with `rx.apply_delta<T>(held, delta, cursor)`. After loss or a restart it
  returns `delta_base_mismatch`; the server `reset()`s the baseline and sends a key delta to recover. See
  [DELTA_SYNC.md](DELTA_SYNC.md) and
  [user guide section 4.5](USER_GUIDE.md#45-delta-sync-send-only-the-changed-bytes).
- **Viewports of a shared world** (units and marches on the map): shared deltas. The server uses the
  library's publisher `XSyncPublisher`; each changed entity is encoded once per tick, and every client
  that can see it receives the same bytes; the client writes them into its own records with
  `rx.apply_sync(msg, cursor, target)`. See [SHARED_DELTA.md](SHARED_DELTA.md) and
  [user guide section 4.6](USER_GUIDE.md#46-shared-deltas-viewport-sync).

Both kinds of delta require one generation on both ends: the handshake answers `same` (the same
version, or versions of one add-only family) before anything is sent, and refuses a client of another
generation. The copy the client receives is read-only; to send changes back, the client uses its own buffer and goes through uplink (section 5).

## 5. Client uplink (`client_uplink.cpp`)

Data sent by clients is untrusted input.

```cpp
// Client: one send buffer, reused.
auto o = out.reset();
o.unit_id() = unit; o.to_x() = x; o.to_y() = y;
if (!out.try_save_verified_into(wire, XSaveOptions{.emission = XWireEmission::dense})) return;  // out.error()

// Server: bounds tier, read-only in place, no copy; then check the content against the game rules.
XReceiver<XLink::from_client> up{XBuffer::maximum_capacity(4096)};
if (auto v = up.view<MoveOrder>(wire)) {
    const MoveOrder& o = v->root();                    // const access only
    if (owns(conn_player, o.unit_id) && in_map(o.to_x, o.to_y)) move(o);
}
```

- Uplink messages are small, so the server sets a small limit for this link (4 KiB in the example) and
  rejects by length before parsing.
- `view` needs a dense wire (always dense under 4 KiB; writing `dense` explicitly on the client is
  safer), bytes starting on an 8-byte boundary, and bytes not changed or freed until the view is done.
- Read the fields you need into the server's own state; structural checks say nothing about content, so
  the server checks player id, unit ownership and coordinate ranges.
- To keep and modify a client's data, or forward it to another server: `up.reload<MoveOrder>(kept, wire)`
  (only the full tier on this link), then rebuild with `save_compact` after modifying before sending, so
  the bytes the client wrote into holes and slack are not carried to the next hop
  ([DEPLOYMENT.md section 4.2](DEPLOYMENT.md#42-forwarding-client-uplink-data-rebuild-with-save_compact)).

## 6. Server to server (`server_to_server.cpp`)

A player migrates from zone server A to zone server B, and when leaving B the state is handed to the save
server. Every hop is the same three steps: save the arena bytes, receive into a long-lived buffer, modify
in place.

```cpp
XReceiver<XLink::server_peer> rx{player_cap()};       // trusted tier
if (XStatus s = rx.reload(player, wire); !s) refuse();
player.root().gold() += 250;                           // scattered fields: the proxy
player.edit([](PlayerState& p) {                       // a batch of allocating changes: one pinned scope
    for (std::uint32_t item = 20; item < 40; ++item) p.items.push_back(item);
    p.quests[100u] = 4;
}, 1024);                                              // expected_growth: reserves space so it cannot fail midway
player.save_verified_into(out);
```

- `server_peer` is only for servers you control, never for client data.
- Buffers that grow in place use `XGrowPolicy::Automatic` and an explicit `maximum_capacity`.
- When making allocating bulk changes in `edit()`, pass `expected_growth` and check `ok()` afterwards;
  without it the buffer cannot move while pinned, and running out of space stops the changes midway.
- Do the schema handshake first on every server link (`make_schema_hello<...>(label,
  XSchemaRole::server)`): during a rolling update, servers of one add-only family keep exchanging
  state and servers of another generation are refused (`server_to_server.cpp`,
  [SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)).

## 7. Client builds: C++20, native compilers, no exceptions

- Clients (Windows MSVC/clang-cl, Android NDK Clang, iOS Apple clang) use C++20 and the field-list
  build: the same library and the same wire as the server ([CLIENT_TARGETS.md](CLIENT_TARGETS.md)). The
  CMake target `XOffset::xoffset` adds `/Zc:preprocessor /Zc:__cplusplus /permissive-` for MSVC
  automatically.
- When the engine disables exceptions (`-fno-exceptions`, MSVC `/EHs-c-`) the whole library still works,
  with no RTTI needed. Receive with the entry points that return `XStatus` / `XResult` (the methods of
  `XReceiver`, `try_load_verified`, `try_reload_verified`); failures with no caller to return to, such as
  running out of space while modifying, call the handler set with `set_fatal_handler` and then terminate
  ([user guide section 6.2](USER_GUIDE.md#62-no-exception-builds--fno-exceptions-msvc-ehs-c-)). All three
  use-case examples are also compiled without exceptions as `*_noexc` and run in CTest.
- Only 64-bit little-endian is supported; Apple deployment targets below macOS 14 / iOS 17
  automatically disable `std::pmr` (`XOFFSET_USE_PMR=0`) and use header-only `XMemoryResource`
  ([user guide section 5](USER_GUIDE.md#5-constraints-you-must-follow)).

## 8. Recommended pattern checklist

| Do | Don't |
| --- | --- |
| Share one protocol header between both ends, one `XOFFSET_RECORD` line per record, register a name for each root type and `XOFFSET_PIN_SCHEMA` it | Write separate type definitions on each end; put the record macros inside a namespace |
| End every record with `XSpare<N>` from the first release on, and start every connection with the schema handshake | Release a type without spare bytes (it then changes only with servers and clients updated together); send messages before the peer's ack |
| Build receivers per link: `server_peer`, `from_server`, `from_client` | Use the server-to-server trusted tier to receive client data |
| Set `maximum_capacity` on every buffer and receiver; reject overlong messages by length first | Receive without a limit (even a single valid wire can fill memory) |
| `reload` into a long-lived buffer again and again, `save_verified_into` the same send string again and again | `load` a new buffer for every message on hot paths |
| Use the `root()` proxy for scattered fields and `edit()` / `read()` for loops and bulk changes | Hold `plain_root()` references long-term (they dangle when the buffer moves) |
| Once the peer has a copy, send deltas or shared deltas; send whole state only on join and reconnect | Send the whole state to every client every tick |
| Use `view` read-only for client uplink and check content against the server's state | Treat structural checks as content checks |
| Keeping or forwarding client data: full-tier `reload`, and `save_compact` before forwarding | `save_verified` received client data and pass it on unchanged |
| Clients use the `XStatus` / `XResult` entry points, so no-exception builds compile and run as well | Rely on exception handling for receive failures on clients |

## 9. What to read next

- [User guide](USER_GUIDE.md): all the rules for defining types, modifying, saving and loading, when
  references are invalidated, and error handling.
- [Deployment guide](DEPLOYMENT.md): same-version releases, untrusted input, capacity limits, networking
  and compression, and the integration checklist.
- [DELTA_SYNC.md](DELTA_SYNC.md), [SHARED_DELTA.md](SHARED_DELTA.md): format, checks and guarantees of
  the two kinds of delta.
- [SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md): the handshake, adding fields in reserved bytes so old and
  new versions keep talking, and loading stored data of an older generation.
