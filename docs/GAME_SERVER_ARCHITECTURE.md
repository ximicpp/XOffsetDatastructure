# Game server architecture with XOffset

This document describes how to build the back end of an SLG or MMO game on XOffset: how to cut the
game state into buffers, which service owns which buffer, and how state moves between services and
to clients. Every pattern here runs in
[`examples/game_server/game_server.cpp`](../examples/game_server/game_server.cpp), a single program
that models the services as objects (one thread per logic shard) and passes every message between
them as the bytes a socket would carry. It runs under CTest (`example_game_server`, label
`examples`).

Read [GETTING_STARTED.md](GETTING_STARTED.md) first: this document builds on its three use cases
(server to client, client uplink, server to server) and does not repeat their rules.

## 1. The idea in one paragraph

Each game entity (a player, a city, an alliance, a map region) is one `TypedXBuffer`. The service
that owns an entity changes it in place. Everything else the back end does with an entity is moving
its bytes: login sends them to the client, a save writes them to storage, a load reads them back, a
migration sends them to another server, a GM tool or a bug report takes a copy. None of these steps
encodes or decodes per field, and each of them is the same few calls (`save_verified_into`,
`reload`, `save_delta_into`, `apply_delta`).

```
 clients                gateway                 logic shards (1 thread each)          storage
 -------                -------                 ----------------------------          -------
 hello  ------------->  handshake
 command frame ------>  cap size, route  --->   XReceiver<from_client>::view
                        by session              game rules check the values
                                                change entity buffers in place
 apply_delta  <-------  forward  <---------    save_delta_into (per client baseline)
                                                save_verified_into  ------------->    put(key, bytes)
                                                reload(bytes)  <------------------    get(key)
                                                migrate: bytes ---> another shard / server
```

## 2. Cutting the state into buffers

**One buffer per entity.** The example has three entity kinds
([`game_schema.hpp`](../examples/game_server/game_schema.hpp)):

| Entity | Root type | Owner | Synced to |
| --- | --- | --- | --- |
| Player (resources, heroes, items) | `game::Player` | The shard the player is on | The player's client |
| City (position, buildings) | `game::City` | The same shard as its owner | The owner's client |
| Alliance (members, notice) | `game::Alliance` | A fixed shard chosen by `alliance_id` | Members, on request |
| Static configuration | `game::ConfigTable` | Built offline; every shard loads its own copy | — |

Why per entity and not one buffer per shard or per world:

- Saving, migrating and syncing work on whole buffers or on deltas of one buffer. With one buffer per
  entity, a save writes only the entities that changed, a migration moves exactly one player, and a
  client's baseline covers exactly what that client sees.
- A buffer is used by one thread at a time ([user guide 3.6](USER_GUIDE.md#36-threads-one-thread-per-buffer)).
  Per-entity buffers let each entity belong to the one thread that owns it, with no locks.
- Size limits are per buffer (`maximum_capacity`). Per-entity limits bound what one player can grow to.

**References between entities are ids.** `Player::city_id`, `Player::alliance_id` and
`Alliance::members` hold ids, never an offset or pointer into another buffer. That is what lets each
buffer be saved, sent and loaded on its own, and what lets the alliance live on a different shard (or
a different server) from its members.

**Inside an entity, use the containers.** Heroes are an `XVector<Hero>`, items an
`XMap<std::uint32_t, std::int64_t>`, buildings an `XMap`. Variable-length changes (a new hero, a
rename) happen in place; the buffer grows on demand below its limit
(`with_growth(XGrowPolicy::Automatic)`).

**Large shared state** (the world map that every client views a part of) is not a per-player entity.
Keep it on a scene or map service and send each client its viewport with shared deltas
([SHARED_DELTA.md](SHARED_DELTA.md), [`examples/shared_delta.cpp`](../examples/shared_delta.cpp),
[`demo/mapstate`](../demo/mapstate/README.md)).

## 3. Services

### 3.1 Gateway

The gateway owns the client connections and does three things:

1. **Handshake.** The first frame of a connection is the client's schema hello. The gateway answers
   with `accept_schema_hello<game::Player, game::City, game::Command>`: a client of the same version
   or of another version of the same add-only family is accepted, a client of another generation is
   refused and shows its update prompt ([SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md) section 2).
2. **Frame cap.** A frame larger than the command limit (4 KiB in the example) is dropped before
   anything else looks at it.
3. **Routing.** The session's player id comes from authentication, not from the frame. The gateway
   keeps a table from player id to owning shard and posts the frame there. It never parses a command.

The gateway forwards the client's bytes unchanged, so they are still client data when they arrive:
the shard reads them with `XReceiver<XLink::from_client>`, never with a trusted receiver. The check
tier follows where the bytes came from, not the last hop.

### 3.2 Logic shards

A logic shard is a thread with a mailbox (`Shard::post`). It owns a set of entities and is the only
thread that touches their buffers, proxies, scopes and delta baselines. Everything that reaches a
shard is a job carrying ids and bytes; a shard never hands another shard a reference into its
buffers.

**Commands from clients.** `XReceiver<XLink::from_client>::view<game::Command>` checks the frame at
the bounds tier (no read leaves the frame) and hands out const access to the frame's bytes. With the
default `XOFFSET_CHECK_UPLINK=1`, the trusted tier does not compile on this link. The structure check
says nothing about the values, so the shard checks them against its own state before acting:

- the command's `player_id` matches the session (a client cannot act as another player);
- the command kind exists;
- the game rules hold (enough gold, building below its max level, a name of valid length and
  characters).

The command is never kept or forwarded as a buffer; the shard copies the values it needs into its own
entities. A service that must keep or forward client data uses `reload` (full tier) and `save_compact`
([GETTING_STARTED.md](GETTING_STARTED.md) section 5).

**Changes in place.** Scattered scalar changes go through the root proxy
(`e.player.root().gold() -= cost`); loops and allocating changes go into an `edit()` scope with an
expected growth (`e.city.edit([&](game::City& c) { c.buildings[b] = level + 1; }, 64)`). Reads that
compute a value use `read()`.

**Requests across entities.** Joining an alliance touches two entities on two shards:

1. the player's shard checks the player's side and posts `add_member(alliance_id, player_id)` to the
   alliance's shard;
2. the alliance's shard checks its side, appends the id to `members`, and posts
   `joined(player_id, alliance_id)` back, routed through the gateway's table to wherever the player is
   now;
3. the player's shard sets `alliance_id` and syncs the client.

Each step changes only entities its own shard owns. Between servers the posts become messages on a
server link; nothing else changes.

### 3.3 Storage

Storage is a key-value store of raw saves (`player:1001`, `city:5001`, `alliance:9002`).

- **Save**: `save_verified_into(out)` and `put(key, out)`. The example saves dirty entities
  periodically (`save_dirty`) and on logout. Before saving an entity that has been edited for a long
  time, `relayout_advised()` / `relayout()` shed the holes and spare capacity the edits left behind
  ([user guide 4.7](USER_GUIDE.md#47-a-sender-that-edits-in-place-for-a-long-time-relayout)).
- **Load**: `get(key)` and `XReceiver<XLink::server_peer>::reload(buffer, bytes)`. Storage holds the
  servers' own data, so the trusted tier applies.
- **Older saves**: a save written by an earlier version of an add-only family loads as the current
  version with no step (section 5). A save of an earlier *generation* loads through
  `evolve_load_verified` and a lineage ([SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md) section 6, C++26
  reflection build).
- **Compression** is a storage or transport choice, not part of the format: zstd over the saved bytes
  works as for any blob.

### 3.4 Clients

A client holds a replica of each entity it sees (its player and its city) and one `XDeltaCursor` per
replica. It receives with `XReceiver<XLink::from_server>::apply_delta` and reads the replica in place.
It builds commands in one reused buffer (`out.reset()`, then `try_save_verified_into` with a dense
emission) and uses only the `XStatus` / `XResult` entry points, so the same code builds without
exceptions ([GETTING_STARTED.md](GETTING_STARTED.md) section 7).

## 4. Moving state

| Flow | Sender | Receiver | Example |
| --- | --- | --- | --- |
| Login | Shard: a fresh `XDeltaBaseline` per entity; the first `save_delta_into` is a key delta (the whole state) | Client: `apply_delta` | `Shard::login` |
| Change | Shard: `save_delta_into` with the client's baseline after each command | Client: `apply_delta` | `Shard::sync` |
| Lost delta, client restart | Client: `apply_delta` fails (`delta_base_mismatch`), asks for a resync | Shard: `baseline.reset()`, next delta is a key delta | `Shard::resync` |
| Save | Shard: `save_verified_into` | Storage: `put` | `Shard::save_dirty`, `Shard::logout` |
| Load | Storage: `get` | Shard: `XReceiver<server_peer>::reload` | `Shard::login` |
| Migration (other shard, other server) | Shard: `save_verified_into` per entity, erase, update route | New shard: `XReceiver<server_peer>::reload`, new baselines (the client gets key deltas) | `Shard::migrate_out`, `migrate_in` |
| GM inspection, bug reports | Storage or a live shard: the bytes | A tool: `load_typed` and read | end of `main` |

Notes:

- **Login is a key delta, not a separate snapshot message.** The first delta of a fresh baseline
  carries the whole state, so login, resync and migration all use the one delta path, and the client
  has one receive call. A client that holds nothing yet accepts only a key delta.
- **Deltas carry the changed bytes only.** In the example a collect command produces a 21-byte player
  delta and a rename 44 bytes, against a 264-byte save. A server that tracks what it changed can pass
  `touched_only` so only touched blocks are compared ([DELTA_SYNC.md](DELTA_SYNC.md) section 1).
- **Migration** sends the entity's bytes exactly as storage would get them. The receiving shard owns
  the entity from then on; frames that the gateway routed to the old shard before the route changed are
  forwarded to the new one (`Shard::on_command`). Between servers, start the server link with the
  schema handshake (`make_schema_hello<...>(label, XSchemaRole::server)`), and receive with
  `XReceiver<XLink::server_peer>` over a link the servers control (TLS or mTLS, or an internal
  network).
- **One state, many receivers**: to send the same bytes to many connections, save once and share the
  string; several clients that always get the same deltas can share one baseline
  ([DELTA_SYNC.md](DELTA_SYNC.md) section 1).

## 5. Schema evolution

The schema header is a small library of its own that the servers and every client build include.

- Every record ends with `XSpare<N>`, from the first release on.
- A later release only adds fields inside those bytes. In the example, release 2 took
  `Player::vip_level` from the 16 spare bytes of release 1 and declares
  `XOFFSET_SCHEMA_FAMILY(game::Player, game_v1::Player)`; `game_v1::Player` stays in the code for good.
- What follows (all shown by the example):
  - a release-1 client (`GameClient<game_v1::Player>`) passes the handshake with the release-2
    gateway and applies release-2 deltas, including the one that sets `vip_level`, which it does not
    know about;
  - a release-1 save in storage loads as release 2 with `vip_level` 0;
  - the field written by release 2 survives in the save, and a release-1 tool still reads that save.
- Clients and servers update in any order within a family. Anything else (a removed, moved or retyped
  field) is a new generation: both ends update together, and stored data upgrades once through a
  lineage ([SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)).
- `XOFFSET_PIN_SCHEMA` lines (generated by `schema_pin<T>`) make every compiler that builds the header
  check the layout, so a client toolchain that would lay a type out differently fails to compile
  ([SCHEMA_PINS.md](SCHEMA_PINS.md)).

## 6. Threads

- One entity, one owning thread at a time. The shard's mailbox is the hand-over point: jobs carry
  bytes and ids, and the mutex in the mailbox orders them.
- Different buffers on different threads run in parallel; the library's process-wide structures lock
  internally.
- Shared read-only data (the configuration table) is shared as bytes; each shard loads its own buffer
  from them, because two threads reading one buffer at the same time is not supported in this
  version.
- One `XReceiver` per shard (or one shared by several threads, each loading into its own buffer).

The example passes ThreadSanitizer, AddressSanitizer and UndefinedBehaviorSanitizer builds.

## 7. Limits

Every buffer and every receiver has a limit, and the gateway checks the frame length before anything
else:

| Data | Limit in the example |
| --- | --- |
| Command frame (gateway and `from_client` receiver) | 4 KiB |
| Player | 256 KiB |
| City | 64 KiB |
| Alliance | 1 MiB |
| Configuration table | 64 KiB |

## 8. What the example leaves out

The example keeps the program small and deterministic. [`demo/gameserver`](../demo/gameserver/README.md)
runs the same design as separate processes over TCP, with Redis as the store, session resume, heartbeats,
rate limits, migration between nodes and a load generator, and reports latency and throughput. A
production back end adds:

- **Network**: sockets with length-prefixed frames ([`examples/server_to_client.cpp`](../examples/server_to_client.cpp)
  shows framing), TLS on client links, mTLS or a private network on server links.
- **Authentication**: the gateway gets the session's player id from a login service.
- **Persistence**: a real KV store, write batching, and keeping the previous save until the new one is
  written.
- **Scene and map servers**: shared deltas for viewports ([SHARED_DELTA.md](SHARED_DELTA.md)), and a
  battle service that receives entity snapshots and returns results the same way the shards hand
  players over.
- **Operations**: the GM view and replay are copies of the bytes; `xoffset_dump.hpp` prints a buffer for
  humans.

## 9. Map to the code

| Concept | Code |
| --- | --- |
| Entity types, ids, spare bytes, family, pins | `examples/game_server/game_schema.hpp` |
| Gateway: handshake, frame cap, routing | `Gateway` |
| Logic shard: mailbox thread, owned entities | `Shard`, `Shard::run`, `Shard::post` |
| Uplink check and game rules | `Shard::on_command`, `collect`, `upgrade`, `rename` |
| Cross-shard request by id | `Shard::request_join`, `add_member`, `joined` |
| Key delta at login, small deltas after changes, resync | `Shard::login`, `sync`, `resync` |
| Periodic save, relayout, KV store | `Shard::save_dirty`, `KvStore` |
| Migration | `Shard::migrate_out`, `migrate_in`, `Gateway::set_route` |
| Release-1 client and save | `GameClient<game_v1::Player>`, `write_v1_save` |
| Server link handshake | `check_server_links` |
