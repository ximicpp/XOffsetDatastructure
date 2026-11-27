# Game server demo

A multi-process SLG / MMO back end built on XOffset. It shows how the patterns in
[docs/GAME_SERVER_ARCHITECTURE.md](../../docs/GAME_SERVER_ARCHITECTURE.md) look with real TCP links, several
logic nodes, Redis as the store, and a load generator that measures them:

```
 gs_bot (N clients)          gs_gateway                      gs_logic node 0..M-1                Redis
 ------------------          ----------                      --------------------                -----
 TCP, length-prefixed  --->  handshake, login, token   --->  IO thread (epoll)                   gs:player:<id>
 frames                      heartbeat, rate limit           shard threads (actors) ---------->  gs:city:<id>
 replica per player    <---  route by session          <---  write-behind thread (batched SET)   gs:alliance:<id>
                             directory lookup  <---------------------------------------------    gs:route:<id>
                                                             node <-> node links (join, migrate)
```

Everything that crosses a process boundary is XOffset bytes inside a small frame: a client command is the
command's wire, a downlink is a delta, a save is the arena image, a migration is the player's and city's
images. Nothing is encoded or decoded per field anywhere.

The schema is the one of [`examples/game_server/game_schema.hpp`](../../examples/game_server/game_schema.hpp):
one root type per entity (`Player`, `City`, `Alliance`), the client `Command`, and the `ConfigTable`.

## 1. What production game servers do, and where this demo stands

The common shape of SLG and MMO back ends (cloudwu's Skynet, Pitaya, Nakama, Orleans and Akka based
services, and the in-house stacks they inspired):

| Concern | Common practice | This demo | XOffset's part |
| --- | --- | --- | --- |
| Client connections | Gateway processes hold long-lived TCP (or WebSocket / KCP) connections, length-prefixed frames, never run game logic | `gs_gateway`, one event loop, frames of at most 4 KiB from clients | The gateway forwards the client's command bytes unchanged |
| Version checks | A protocol version at login; mismatched clients are told to update | The schema handshake (`accept_schema_hello`) is the first frame | Add-only families pass, other generations are refused |
| Login and reconnect | A session token; a dropped connection may resume within a grace period, then the client gets a full state | Token in `g_login_ok`; reconnect within 15 s resumes and triggers key deltas | A key delta is the full state; the client keeps one receive path |
| Liveness, abuse | Heartbeats, per-session rate limits, frame size caps | Ping every 2 s, closed after 10 s of silence; token bucket per session; length checked before buffering | `XReceiver<XLink::from_client>` (bounds tier, default `XOFFSET_CHECK_UPLINK=1`) checks the structure, game rules the values |
| Game logic | Entities sharded by id over nodes; inside a node, actors or single-threaded services own entities (no locks) | `gs_logic`: an IO thread, N shard threads with mailboxes; one `TypedXBuffer` per entity, owned by one shard | Changes in place (proxies, `edit()`), no object model to keep in sync with a wire format |
| Entity directory | A routing table (Redis, etcd, ZooKeeper) for entities that moved off their home node | `gs:route:<id>` in Redis, written by the node that receives a migrated player | — |
| State to clients | Snapshot on login, then deltas or events; full resync on gaps | Key delta at login and resume, small deltas after every change | `save_delta_into` / `apply_delta`: changed bytes only (about 21 B per command here) |
| Persistence | Redis as the hot store, MySQL / MongoDB as cold storage; one blob per player (often a protobuf blob); dirty flags, periodic write-back, a write on logout | Write-behind thread, one pipelined batch per round, dirty entities saved every second and on logout; a load first looks at saves not yet written | The blob is the arena image (`save_verified_into`), loaded with `reload` at the trusted tier |
| Cross-server moves | Serialize the player, send it to the target, switch the route, resync the client | `l_migrate` -> `n_migrate` (both images) -> route in Redis -> `l_moved` to the gateway -> key deltas | The images are sent as they are |
| Cross-entity requests | Messages between actors by id (alliance, mail, trade) | Joining an alliance: player's node -> alliance's node -> back, by id | — |
| Scenes and AOI | Grid or cross-linked-list AOI on scene servers, viewport updates | Not repeated here: see shared deltas ([SHARED_DELTA.md](../../docs/SHARED_DELTA.md)) and [`demo/mapstate`](../mapstate/README.md) | Shared deltas per viewport |
| Hot update | Scripts or config tables reloaded without a restart | Config table as an XOffset image each shard loads; reloading means loading new bytes | Add-only schema evolution lets clients and servers update in any order |
| Monitoring, load tests | Metrics per process, bots that replay player behavior | Metrics line every 5 s per process; `gs_bot` with latency percentiles, byte counts, CPU per process | — |

## 2. Processes and threads

**`gs_gateway`** (one event loop thread)

- Accepts clients. Per connection: `c_hello` (schema hello) -> `g_ack`; `c_login` (player id, resume token) ->
  `g_login_ok` (token, node); then `c_command`, `c_ping`, `c_logout`. A frame out of order closes the
  connection.
- Sessions by player: one connection per player (a newer login closes the older one), a token for
  resuming, a grace period after a drop, then `l_logout` to the node.
- Routing: `gs:route:<id>` in Redis, else the home node (`id % nodes`). `l_moved` from a node updates the
  session's route.
- Never parses a command; the bytes go to the node as the client sent them.

**`gs_logic`** (an IO thread, N shard threads, a write-behind thread)

- Links: one from each gateway (`l_hello` with the gateway id and a server-role schema hello), one to each
  other node (`n_hello`). A link that has not passed the handshake gets no other frame through.
- The IO thread decodes the frame header and posts the job to the shard that owns the entity
  (`(id / nodes) % shards`). Shards post outgoing frames back to the IO thread. A job carries ids and
  bytes, never a reference into a buffer.
- A shard owns players (with their cities) and alliances: one `TypedXBuffer` each, plus a delta baseline
  per client stream. Commands: `XReceiver<from_client>::view`, then the game rules, then the change in
  place and `save_delta_into` for the client.
- Saves: every second for dirty entities, on logout, and for every loaded entity at shutdown (`SIGTERM`). The shard
  hands the image to the write-behind thread and goes on; the thread writes all pending saves in one
  pipelined Redis round. Loads check the pending saves first, so a player who logs out and straight back
  in sees the save not yet written.
- Migration: the source shard saves both images, erases the player, remembers where it went (frames that
  were already routed to it are forwarded) and sends `n_migrate`. The target reloads the images at the
  trusted tier, writes the directory, tells the gateway (`l_moved`), and sends key deltas.

**`gs_bot`** (one event loop thread)

- N clients, each with a replica of its player and city (`XReceiver<from_server>::apply_delta`). Each
  picks a command its replica says the rules accept (collect, upgrade the farm when it has the gold,
  rename, join a random alliance once) and keeps one command outstanding at the target rate. Latency is
  from sending a command to receiving the player delta it caused.
- During the run: session resumes (drop the connection, reconnect with the token), migrations of players
  to another node (an operator frame), and before the run a set of hostile-client checks.
- At the end every bot logs out, and the bot reads a sample of saves from Redis and compares them with
  the replicas.

## 3. Frames

Every link uses `u32 length | u8 type | body` (little-endian). The full list is in
[`include/gs/protocol.hpp`](include/gs/protocol.hpp). Limits close a connection before its body is
buffered: 4 KiB on client connections, 1 MiB on server links.

| Flow | Frames |
| --- | --- |
| Connect | `c_hello` -> `g_ack` |
| Login | `c_login` -> `l_login` -> (load or create) -> `l_downlink` x2 (key deltas) -> `g_downlink` x2 |
| Command | `c_command` -> `l_command` -> (view, rules, change) -> `l_downlink` -> `g_downlink` |
| Resume | new connection, `c_hello`, `c_login` with the token -> `l_resync` -> key deltas |
| Join an alliance | `c_command` -> player's node `n_join` -> alliance's node -> `n_joined` -> player's node -> delta |
| Migrate | `c_gm_migrate` -> `l_migrate` -> `n_migrate` (images) -> Redis route -> `l_moved` -> key deltas |
| Logout | `c_logout` -> `l_logout` -> save (write-behind) |

## 4. Build and run

Linux or macOS, with Redis (`redis-server`, `redis-cli`) installed:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DXOFFSET_CXX_STANDARD=20 -DXOFFSET_BUILD_GAMESERVER_DEMO=ON
cmake --build build --target gs_logic gs_gateway gs_bot
# bots, seconds, commands per second per bot, logic nodes, shards per node
demo/gameserver/tools/run_demo.sh build/bin/Release 1000 30 10 2 4
```

The script starts Redis (port 6390, no persistence), the logic nodes and a gateway, runs the bots, and
prints their report, the CPU time each server used during the run, and each server's last metrics line.
`RECONNECTS` and `MIGRATIONS` (20 each by default) set the events during the run. With `redis-server`
found at configure time, CTest runs a short version (`ctest -L demo -R gameserver`).

## 5. Results

One 4-core cloud container runs everything: Redis, two logic nodes with 4 shards each, the gateway and the
bots, over loopback. The bots and the gateway compete with the logic nodes for the same 4 cores, so these
are numbers for this setup, not for a server fleet. GCC 13, Release, C++20 build, 2026-11-27.

| | 1,000 bots x 10 cmd/s, 30 s | 2,000 bots x 50 cmd/s, 20 s |
| --- | --- | --- |
| Commands completed | 300,027 (9,998/s) | 2,000,378 (99,954/s) |
| Latency command -> delta, p50 / p99 / p99.9 / max | 0.64 / 1.9 / 4.6 / 14 ms | 4.6 / 12.3 / 20.5 / 29 ms |
| Small deltas, average size | 21.3 B | 21.8 B |
| Key deltas (login, resume, migration), average size | 75.4 B | 75.4 B |
| Client link bytes per command, up / down (frames, pings included) | 93.6 / 30.8 B | 93.1 / 29.5 B |
| Shard time per command (view, rules, change, delta) | 3.7-3.9 us | 2.2-2.3 us |
| CPU per command, logic nodes / gateway | 14 / 9.8 us | 5.4 / 5.6 us |
| Session resumes, resume -> key deltas p50 | 20, 7.2 ms | 20, 14 ms |
| Migrations, request -> new owner's key deltas p50 / max | 20 of 20, 1.0 / 5.0 ms | 20 of 20, 5.1 / 12 ms |
| Saves written to Redis (write-behind) | 62,270 (12.3 MB) | 84,116 (16.6 MB) |
| Timeouts, errors, delta apply failures | 0 | 0 |
| Saves read back and compared with the replicas | 50 of 50 match | 50 of 50 match |
| Hostile-client checks | all passed | all passed |

What the numbers say:

- The game logic itself is a small part of the cost: about 2.3 us of shard time per command at load,
  including the uplink check, the rules, the change in place and writing the delta. The rest is system
  calls, thread hand-offs and the network, as in any server; at 100,000 commands per second the 4 cores
  are saturated, mostly by the gateway and the bots, and latency grows from queueing.
- A command costs about 21 bytes downstream because only the changed bytes travel; the whole player is
  75 bytes as a key delta.
- Login, resume and migration need no extra code paths on the client: each ends in key deltas, which the
  same `apply_delta` call takes.

Checked with ThreadSanitizer and with AddressSanitizer + UndefinedBehaviorSanitizer builds of all three
programs (three logic nodes, 200 bots, resumes and migrations): no reports.

### Hostile-client checks (`gs_bot`, before the load)

| Input | Result |
| --- | --- |
| A login frame before the schema hello | Connection closed |
| A frame header claiming 1 GiB | Connection closed before anything is buffered |
| A malformed schema hello | Refused and closed |
| A truncated command wire | Refused by the bounds check, no delta |
| A command naming another player | Refused by the node (the session is the authority), no delta |
| An unknown command kind | Refused by the rules, no delta |
| 200 bytes of garbage as a command | Refused by the bounds check, no delta |
| A valid command afterwards | Applied on the same connection |

## 6. What this demo leaves out

- **Authentication**: any player id may log in. A login service issues the player id and the gateway checks
  its signature.
- **Transport security**: TLS on client links; mTLS or a private network on server links (the trusted tier
  between servers assumes one).
- **Node failure**: a logic node that dies loses at most the changes since its last save round (1 s by
  default); its players' sessions need a re-login on a live node. Production adds health checks and
  takeover (and often a write-ahead log of commands).
- **Cold storage**: saves stay in Redis; a production system also writes them to a database.
- **Scenes, AOI, battles**: see shared deltas and `demo/mapstate`; a battle service receives entity images
  and returns results the way migration does here.
- **Several gateways**: links carry the gateway id and nodes route downlinks by it, but the demo starts one.
- **Async Redis on the IO paths**: the gateway's directory lookup at login and the shard's loads are
  blocking calls; production uses an async client or a cache.
