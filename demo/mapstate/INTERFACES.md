# Map state demo: interfaces for parallel implementation

Frozen contract for the four demo implementation threads. Requirements:
`DEMO_REQUIREMENTS_20261022.md`; full design: `MAP_DEMO_DESIGN_20261022.md`
(both in the project folder `slg-demo-20261022/`). Where this file and the design
differ, this file wins; where either differs from the requirements, the
requirements win.

## 1. Branch and file ownership

All threads work on branch `claude/map-demo-20261022` (from next_cpp26 f0a8c3a),
touch only the files they own, `git pull --rebase` before every push, never
force-push. The skeleton builds at every commit: replace stubs, do not delete them.

Review iteration (10-24, branch `claude/demo-review-20261024` from next_cpp26
d58781e, one thread): adds `--buffers reuse|fresh` to every process, step wire
digests in `a_steps.csv`, `net::PeerClosed`, payload recycling in `net::Conn`,
a fourth `--evil` case and cross-run digest comparison.

End-to-end iteration (10-27, branch `claude/mapdemo-e2e-20261027` from
next_cpp26 83c21b9, one thread): Linux A and B serve clients built for other
platforms. The schema drops its filler fields for the V2.1 wire and is
declared with `XOFFSET_RECORD` field lists, so the client builds as C++20
(MinGW, MSVC, Android NDK, iOS Simulator) while A and B keep C++26
reflection; the network layer runs on Winsock too; snapshots travel dense,
compact or zstd-compressed (`codec.hpp`); the client gains `--self-check`
and `--replay`; `tools/run_e2e.sh` runs every client target. The sections
below describe the result.

No-exceptions iteration (10-28, branch `claude/mapdemo-noexc-20261028` from
next_cpp26 92404a2, one thread, after the library's non-throwing receive
entries, #148): the client side never throws. `result.hpp` adds `Error`,
`Result<T>` and `Status`; every `net` call gets a non-throwing `try_*` form
(the throwing forms remain for A, B and the self-test, in builds with
exceptions; the old non-blocking `try_recv` is now `recv_nowait`); the
codec returns `Status`; the client verifies with `try_load_verified` /
`try_reload_verified`. CMake builds `mapdemo_client_noexc` from the same
sources without exceptions, and `e2e_report.py` requires the same
self-check outcomes and texts on every target.

Small-message iteration (10-28, branch `claude/small-msg-dict-20261028` from
next_cpp26 af601ea, one thread, agreed with the demo thread): an optional
event stream. A client that subscribes gets the rules' unit events (every
20 ticks, the units with id % 100 == 0) as `UnitEvent` wires, raw, zstd,
zstd with a dictionary, or batched in one dictionary frame. The dictionary
code is `contrib/zstd_dict` (outside the library); the dictionary
(`dict/unit_event_v1.xozd`, id 1, trained on A's events) is compiled into
every binary at configure time. The Subscribe names the client's dictionary
id and A refuses another. New: `MsgType` 10-12, `codec` event encoder and
decoder, A `--events-dict` / `--events-dump`, client `--events`, three evil
cases, 25 self-check cases (48 in all), `a_events.csv` / `client_events.csv`. Without
`--events` (the default `off`) the client never subscribes and runs exactly
as before.

Old-version client iteration (10-28, branch `claude/mapdemo-old-client-20261028`
on the small-message branch, one thread, 3.2 plan M2 item 7): two schema
versions on one A. Version 1 is the schema as of the small-message
iteration, frozen by `mapdemo_schema_freeze` into
`include/mapdemo/schema_v1.hpp` (namespace `mapdemo_v1`, generated, never
edited) and `schema.lock`. Version 2 (`schema.hpp`) adds `scene_units` (and
`reserved0`) to `ViewSnapshot`; A lists the version-1 snapshot in
`XOFFSET_SCHEMA_LINEAGE` (`schema_lineage.hpp`, servers only). Both ends now
use the library's handshake (`xoffset_handshake.hpp`): `Hello` and
`HelloAck` carry an `XSchemaHello` / `XSchemaHelloAck` wire instead of the
fixed schema hashes. `mapdemo_client_v1` is the same client sources built
with `include_v1/` first on the include path, so it sees the frozen
version-1 types; A answers its handshake with `convert`, converts every
snapshot for it (`evolve_into` a version-1 buffer) and saves that one in
the encoding asked for. A client whose types A neither has nor can convert
is refused in the `HelloAck` (`reject_names`: same layout, other field
names; `reject_unknown`: a layout A does not know) and disconnected. New:
client `--first-request-id`, three evil cases, A's `schema_versions` check
and `a.snap_convert_us`, `a_snapshots.csv` columns `schema` and
`convert_us`, `run_tier.sh` `MAPDEMO_CLIENT2` (two clients at once),
`run_e2e.sh` `MAPDEMO_CLIENT2_OF`, and the schema lock check in
`run_all_tiers.sh`. Step wires and the event stream are unchanged.

| Owner thread | Files |
| --- | --- |
| Common library | `include/mapdemo/rules_impl.hpp`, `src/net.cpp`, `src/metrics.cpp`, `src/checks.cpp` (+ optional `include/mapdemo/checks_impl.hpp`), `src/codec.cpp`, `src/selftest.cpp` |
| Servers | `src/server_a.cpp`, `src/server_b.cpp` (+ optional `src/server_*.hpp`) |
| Client | `src/client_main.cpp`, `src/client_core.cpp`, `src/client_core.hpp`, `src/client_offline.cpp`, `src/client_gui.cpp` |
| Integration | `CMakeLists.txt`, top-level `CMakeLists.txt` hook, `README.md`, `tools/*`, `.github/workflows/*` demo job, `RESULT.md`, Mac step files, `dict/*.xozd`, `src/schema_freeze.cpp`, `schema.lock` and `include/mapdemo/schema_v1.hpp` (both written by `mapdemo_schema_freeze freeze`), `include_v1/` |

Frozen headers (`schema.hpp`, `schema_lineage.hpp`, `std_model.hpp`, `rules.hpp`, `net.hpp`,
`handshake.hpp`, `metrics.hpp`, `checks.hpp`, `codec.hpp`, `result.hpp`) change only with a note to every demo thread
(through the coordinator); additive helpers are fine. A change to a root
type in `schema.hpp` is a new schema version: freeze the released one first
(section 2) so `mapdemo_schema_freeze check` keeps passing.

Priority for the common-library thread: `src/net.cpp` first (everything else
needs it to run), then `rules_impl.hpp`, `metrics.cpp`, `checks.cpp`, `selftest.cpp`.

## 2. Build

- Cloud: GCC 16 docker (`mirror.gcr.io/library/gcc:16`, start `dockerd` by hand,
  pinned CMake from `tools/prepare_vm_cmake.py`); configure the repo with
  `-DXOFFSET_BUILD_DEMOS=ON -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF`.
  Binaries land in `<build>/bin/<Config>/` and must run inside the container.
- Mac: clang-p2996 d77eff1, same options, plus `-DCMAKE_PREFIX_PATH=<raylib 5.5 prefix>`
  for the window client (design section 9).
- Client targets: `-DMAPDEMO_CLIENT_ONLY=ON -DXOFFSET_CXX_STANDARD=20` with a
  toolchain file from `cmake/toolchains/` (MinGW-w64, Android arm64, iOS
  Simulator) or a native compiler (MSVC, Apple Clang); the schema then comes
  from the `XOFFSET_RECORD` field lists, which C++26 builds check against
  reflection. The handshake's schema fingerprints are the same in both builds.
- `mapdemo_client_v1` (`-DMAPDEMO_V1_CLIENT=ON`, the default): the client
  sources compiled against `include_v1/mapdemo/schema.hpp`, which takes the
  types from the frozen `schema_v1.hpp` and keeps version 1's constants.
  Code that differs between versions tests `MAPDEMO_SCHEMA_VERSION`.
- `mapdemo_schema_freeze` (built with the servers, C++26): `freeze VERSION
  NAMESPACE HEADER LOCK`, run from `include/` at a release, writes the
  snapshot header and the lock entry; `check LOCK` fails when a root type
  changed without a lineage entry for the last frozen version, or kept its
  layout under other field names.
- zstd 1.5.7 is built from its release tarball (SHA-256 pinned;
  `-DMAPDEMO_ZSTD_URL=<local copy>` builds offline; `-DMAPDEMO_WITH_ZSTD=OFF`
  leaves dense and compact only, and the raw event encoding).
- With zstd, `contrib/zstd_dict/xoffset_zstd_dict.cpp` is compiled into the
  common library, and configure turns `dict/unit_event_v1.xozd` into a C++
  array (`file(READ ... HEX)`, no host tool, so cross builds need nothing
  extra). Without that file the dictionary encodings are unavailable.
- All demo code is built with `-ffp-contract=off` (set on `mapdemo_common`;
  MSVC does not contract by default).
- Write conservative C++ for clang-p2996 d77eff1: explicit includes (`<span>`,
  `<optional>`, ...), no deduced return types used before their definition.

## 3. Processes and command lines

```
mapdemo_b      [--port 47702] [--seed 1] [--buffers reuse|fresh] [--csv-dir DIR]
mapdemo_a      [--units 10000] [--seed 1] [--steps 0] [--duration-s 0]
               [--host 127.0.0.1] [--port 47701] [--b-host 127.0.0.1] [--b-port 47702]
               [--check | --no-check] [--buffers reuse|fresh] [--csv-dir DIR]
               [--corpus-dir DIR [--corpus-max 60]] [--events-dict FILE] [--events-dump DIR]
mapdemo_client [--host 127.0.0.1] [--port 47701] [--headless] [--check]
               [--buffers reuse|fresh] [--encoding zstd|compact|dense|cycle]
               [--events off|raw|zstd|zdict|batch|cycle] [--events-dict FILE]
               [--csv-dir DIR] [--auto-drag] [--screenshot-at F1,F2,...] [--evil]
               [--first-request-id 1] [--edits 0]
mapdemo_client --self-check [--csv-dir DIR]
mapdemo_client --replay CORPUS_DIR [--buffers reuse|fresh] [--csv-dir DIR]
mapdemo_client_v1 (the same options; schema version 1)
mapdemo_selftest [--csv-dir DIR]
mapdemo_schema_freeze freeze VERSION NAMESPACE HEADER LOCK | check LOCK
```

- Client `--first-request-id` (default 1): the first `ViewRequest` id, so a
  second client on the same A logs other ids (`run_tier.sh` gives it
  3000001).
- Client `--edits N` (default 0 = off; headless; added 10-28): write-back.
  After the checks of every Nth `Normal` snapshot (one edit outstanding at
  a time), the client changes the snapshot in its own buffer
  (`apply_client_edit`: the middle unit erased; the first remaining unit's
  hp, kept in 50..100, and behavior changed; a unit added at the rect
  centre with id `0x80000000 + n` and a 20-byte name; the first short,
  inline name made long; an empty view only gets the added unit), checks it with `checks::edit_sanity_of` and sends it to A with
  `save_verified` as an `Edit`. It accepts `EditAck` and `Forward` frames
  (section 4). The buffer grows as needed: a compact snapshot leaves no
  free space. `run_tier.sh` passes `--edits 10` to both clients
  (`MAPDEMO_EDITS`, 0 = off).

- A `--host`: the address A listens on for clients (`0.0.0.0` when the client
  is in an emulator or another machine; B stays on 127.0.0.1).
- A `--corpus-dir`: keeps the first `--corpus-max` snapshot payloads exactly
  as sent (`snap_<request_id>.<encoding>`) and `corpus.csv` with A's digests.
- Client `--encoding` (default `zstd`, or `compact` without zstd): the
  snapshot encoding asked for in every `ViewReq`; `cycle` takes
  `request_id % 3` (dense, compact, zstd), so one run covers all three.
- A `--events-dict FILE` / client `--events-dict FILE`: use this `.xozd`
  file instead of the built-in dictionary (it must be bound to `UnitEvent`).
  A `--events-dump DIR`: appends every event wire A builds to
  `DIR/events.bin` (u64 little-endian length + wire), the training input of
  `xoffset_zstd_dict train ... records:DIR/events.bin`.
- Client `--events` (default `off`): after the `Catalog` the client sends
  `Subscribe` with this mode and its dictionary id, waits for `SubscribeOk`,
  then takes `Events` frames between snapshots. `cycle` lets A rotate the
  encoding per event tick (`tick / 20 % 4`: raw, zstd, zdict, batch). A
  refusal (`Error`) is an error: "A refused the event subscription: ...".
- Client `--self-check`: no network; builds sample snapshots and checks that
  every encoding verifies and that the refusal cases are refused (truncated
  and bit-flipped wires, wrong schema, malformed zstd frames, over-cap
  sizes). `edit_round_trip`: a compact snapshot changed with
  `apply_client_edit`, saved with `save_verified`, verified at the Edit
  limit and rebuilt with `save_compact` keeps its content digest. The event cases: the built-in dictionary parses; every event
  encoding round trips 20 events to the same wires and shrinks them; and
  these are refused: a count of 0, a truncated or trailing event, the wrong
  schema, a truncated wire inside a dictionary frame, a zstd frame without
  a content size, frames made with dictionary version 2 (single and batch),
  dictionary frames without an id or a size, truncated, with a trailing
  byte, in the standard (magic) format or declaring more than the event cap,
  batches of 129 events, a forged length or a message shorter than a wire
  header, a dictionary file with a bad checksum, and a dictionary bound to
  another schema. Exit 0 when all pass.
- Client `--replay DIR`: no network; verifies every corpus snapshot and
  compares wire bytes, wire digest, content digest, request id and visible
  count with A's row, so every platform shows it reads the same bytes the
  same way. Exit 0 when all match.
- Every client run prints `client: build <compiler, os, arch, C++ standard,
  schema backend, schema version, exceptions on|off, zstd>` and writes it to
  `client_build.txt` in `--csv-dir`; A and B print the same line for
  themselves.
- `mapdemo_client_noexc` takes the same options and behaves the same; it is
  built without exceptions. A library failure it cannot get back as a value
  (outside the receive path, e.g. an arena exhausted while building a
  request) goes to the fatal handler: one `error:` line, exit 2.

- `--buffers` (default `reuse`, `include/mapdemo/buffer_mode.hpp`):
  - `fresh`: every received batch, request and snapshot is a new `XBuffer`
    (`load_verified`, or `create` + `save_verified`), destroyed after use.
  - `reuse`: each role keeps its buffers and refills them in place
    (`reload_verified`, `reset`, `save_verified_into`); network payload strings
    go back to their `Conn` for the next frame. Same validator and rejection
    set as `fresh`, byte-identical wires (`BUFFER_REUSE.md`). The modes may be
    mixed across processes; the wire does not say which one produced it.

- Start order B, A, client; connections retry for 5 s.
- The rules depend on the seed, so B must get the same `--seed` as A (added
  03:19Z 10-22); a mismatch shows up as a `reference_model` check failure on A.
- `--steps 0` and `--duration-s 0` mean unlimited. A stops after whichever limit
  is reached, sends `Bye` to B and the client, writes its CSV files, exits.
  After the `Bye` A shuts down its sending side and reads each client
  connection until the client closes (2 s at most) before closing it: a
  request still unread at close would reset the connection, and a Windows
  client then drops the snapshot and `Bye` it has not read yet (10-28).
- `--check` defaults: on for A and the headless client, off for the window client.
- Exit codes: 0 success, 1 a check failed, 2 usage / protocol / IO / capacity error.
  Every error prints one line starting with `error:` to stderr.
- Capacity overflow (an `XBadAlloc` from a cap in `schema.hpp`) is an explicit
  error naming the cap; never drop units.

## 4. Protocol (net.hpp)

| Step | Frames |
| --- | --- |
| A connects to B | A `Hello(make_server_schema_hello<UnitStateBatch>)` → B `HelloAck(accept_schema_hello<UnitStateBatch>)`; a malformed Hello gets `Error`, a rejecting ack is followed by B's exit 2, and A exits 2 on either |
| Each step | A `StepReq(corr = tick T, UnitStateBatch wire)` → B `StepRes(corr = T+1, wire)`; at most one in flight |
| Client connects to A | client `Hello(make_schema_hello<MapCatalog, ViewRequest, ViewSnapshot, UnitEvent>)` → A `HelloAck` (`XPeerSchemas::accept`), then A `Catalog(MapCatalog wire)`; a malformed or second Hello gets `Error`; an ack with a refused root (`reject_names`, `reject_unknown`) is followed by a close |
| View | client `ViewReq(corr = request_id, flags = codec::view_flags(category, encoding), ViewRequest wire)` → A `ViewSnapshot(corr = request_id, flags echoed, ViewSnapshot payload in that encoding)`; at most one outstanding per client |
| Illegal / malformed request | A answers `Error(text)` (bad wire or illegal rect) or closes the connection (bad header, oversize length), logs `rejected: ...`, keeps running |
| Events (optional) | client `Subscribe(flags = event mode, codec::subscribe_payload(dictionary id))` after the `Catalog` → A `SubscribeOk`, or `Error` for a second Subscribe, a bad payload (length, UnitEvent fingerprint, reserved field), an unknown mode, a build without zstd, or a dictionary mode while the ids differ ("subscribe: event dictionary X does not match A's dictionary Y"); then after each step whose tick % 20 == 0, A sends `Events(corr = tick, flags = event encoding, payload)` per 128 events, none after `Bye` |
| Write-back (optional, client `--edits`) | client `Edit(corr = checks::snapshot_digest of the changed snapshot, flags 0, save_verified wire of a ViewSnapshot it received, at most net::kEditLimit = 1 MiB)`; the edit id is the snapshot's own `request_id`. A loads it with `load_verified` and `maximum_capacity(1 MiB)` in the schema version the client works in, computes the digest and compares it with `corr`, requires a snapshot of that id to have been sent to this client, checks the fields (`checks::edit_sanity_of`) and answers `EditAck(corr = edit id)`, or `Error` (connection kept). A client with an accepted edit is a writer: A rebuilds each accepted edit once with `save_compact(XCompactOptions{0})` and sends it as `Forward(corr = digest, flags 0)` to every other writer of the same schema version, which verifies it at 1 MiB, compares the digest and checks the fields |
| End | A `Bye` to B and client |

`net` behaviour (src/net.cpp):
- Every call has a non-throwing form returning `Result<T>` / `Status`
  (`result.hpp`): `Conn::try_connect`, `try_send`, `try_recv`,
  `try_send_raw`, `try_set_nonblocking`, `try_enqueue`, `try_flush`,
  `try_recv_nowait`, `Listener::try_bind`, `try_accept`, `net::try_poll`.
  The client uses only these. In builds with exceptions the throwing forms
  (`connect`, `send`, `recv`, `send_raw`, `set_nonblocking`, `enqueue`,
  `flush`, `recv_nowait`, `bind`, `accept`, `poll`) wrap them and throw
  `ProtocolError` with the same text, or `PeerClosed` where the `Error` has
  `peer_closed` set.
- `recv` / `recv_nowait` return an `Error` frame only when `Error` is in
  `allowed`; otherwise an `Error` frame fails with `"peer error: <text>"`.
- A clean close or a reset between frames is `PeerClosed` (`peer_closed`,
  derived from `ProtocolError`); a close in the middle of a frame stays a
  plain `ProtocolError`.
- `Listener::bind` sets `SO_REUSEADDR`. On a non-blocking listener `accept()`
  returns a `Conn` that is not open when nobody is waiting; check `is_open()`.
- Payload recycling (used by `--buffers reuse`): `recycle_rx(std::move(s))`
  hands a received payload string back, and the next non-empty frame is read
  into its capacity; `take_tx_buffer()` returns the payload string of the last
  fully written `enqueue()`, cleared, to save the next message into. Lengths
  are still checked against the per-type limit before any allocation.

`ViewReq` flags (`codec.hpp`): bits 0-7 the `ViewCategory`, bits 8-11 the
snapshot encoding, bits 12-15 zero. A refuses an unknown category, an
unknown encoding (3 is reserved for lz4), a set reserved bit, or zstd when it
was built without it. Encodings of the `ViewSnapshot` payload:

| Encoding | Payload |
| --- | --- |
| 0 `dense` | `save_verified` wire |
| 1 `compact` | `save_compact` wire (`XCompactOptions{0}`: no spare capacity) |
| 2 `zstd` | the compact wire as one zstd frame, level 3 |

`Events` payload encodings (`codec::EventEncoding`, the frame's flags):

| Encoding | Payload |
| --- | --- |
| 0 `raw` | varint count, then per event varint length + the `UnitEvent` wire (`save_compact`, `XCompactOptions{0}`) |
| 1 `zstd` | varint count, then per event varint length + a zstd level 3 frame of the wire, no dictionary |
| 2 `zdict` | varint count, then per event varint length + a lean dictionary frame (`contrib/zstd_dict`: magicless, dictionary id and content size, no checksum, the 24 bound header bytes left out) |
| 3 `batch` | one lean dictionary frame holding all the events (varint count, then varint length + elided wire each) |

The decoder checks before allocating: count 1..128, every part and every
declared content size within `kCapEvent` (4 KiB per wire; the frame
length limit is 1 MiB), the frame's dictionary id equal to its own,
exactly one frame per part and nothing after the last event. Then the client
verifies every wire with `try_load_verified<UnitEvent>` /
`try_reload_verified` and checks its content (tick, id % 100, ascending ids,
hp 50..100, behavior, name, path and waypoint, position in the scene).

The client decodes a zstd payload only when it is exactly one standard zstd
frame (no skippable frame, no trailing bytes) whose header declares a content
size no larger than the snapshot cap (16 MiB), with a 16 MiB window limit,
and the output has exactly that size; then it verifies the wire like any
other (`load_verified` / `reload_verified`). zstd removes no check.

Receivers verify every wire with the caps in `schema.hpp`: `load_verified<T>(payload,
cap_*())` in `fresh` mode, `reload_verified<T>(payload)` on a buffer created with
the same cap in `reuse` mode. The client uses the non-throwing forms
`try_load_verified` / `try_reload_verified` (same checks, same failure texts).
There is no `XBufferPool`.

Schema versions (`handshake.hpp`, `schema_lineage.hpp`): the client's Hello
lists the four root types it uses, each with its layout fingerprint and
field names; A's ack gives one verdict per root: `same`, `convert` (the
client has a version in A's lineage), `reject_names` or `reject_unknown`.
Only `ViewSnapshot` has two versions; the other roots must be `same`. For a
`convert` client A builds the version-2 snapshot as always, converts it
into a version-1 buffer of its own (fields matched by name, `scene_units`
and `reserved0` dropped; `fresh`: `evolve<mapdemo_v1::ViewSnapshot>`, a new
buffer per snapshot; `reuse`: `evolve_into` the kept buffer after
`reset`) and saves that one
(dense, compact or zstd as asked). The client never converts. A logs
`a: client N '<label>': ViewSnapshot version V (same|converted)` and counts
both kinds in its `schema_versions` check. The Hello and HelloAck limit is
4 KiB.

Every schema struct is declared with `XOFFSET_RECORD(Type, fields...)` in
declaration order: C++20 builds take the layout from these lists, C++26
builds check them against reflection. The V2.1 wire (12-byte container heads)
left padding in the old layouts, so the filler fields went: `UnitState` is
56 bytes, `VisibleUnit` 32, `ViewSnapshot` 48 (56 in version 2), all without
padding. Step and snapshot wire bytes differ from runs before 10-27; from
10-28 a version-2 snapshot is 8 bytes longer than a version-1 one, and step
wires are unchanged.

## 5. Behaviour fixed for all threads

- Rules (generation, movement, events, rect rule): design section 3, implemented
  once in `rules_impl.hpp`. A builds the initial batch and the reference model with
  `init_batch`; B calls `step_batch` on the loaded batch; A's reference model calls
  `step_batch` on the std model.
- A is single-threaded (`poll`): sends a step when none is in flight and the 20 Hz
  deadline has passed; serves view requests from the last completed batch while
  waiting for B; verifies B's result into a second buffer and swaps it in. `fresh`
  destroys the old batch; `reuse` keeps it as the target of the next verify.
- A classifies nothing itself: the category comes in the `ViewReq` flags.
- Headless client sampling: 10 Hz; each group of 10 requests = 8 `Normal`
  (1024×768 rect moving along a fixed route over the scene), 1 `FullScene`
  (`scene_bounds()`), 1 `Empty` (`empty_probe_rect()`).
- Window client: 1024×768, 1 px = 1 scene unit, left-drag pans, camera clamped to
  the scene; snapshot replaced only at the start of a frame; reads fields directly
  from the live snapshot buffer (design section 5).
- `--evil` (client): (1) to (3) on separate connections to A: a truncated
  `ViewRequest` wire, a `ViewSnapshot` wire as `ViewReq` (wrong schema), a header
  declaring a 1 GiB `ViewReq`; (4) on one connection: a valid request, a
  truncated one, another valid one (a rejected wire on a kept receive buffer
  must not break the next request on that connection); (5) a `ViewReq` asking
  for encoding 3; (6) a `ViewReq` with reserved flag bit 12 set. Expects
  `Error` or a close for each bad request; (7) a `Subscribe` holding
  dictionary version 2, (8) a `Subscribe` with another `UnitEvent` schema
  hash, (9) a `Subscribe` asking for event encoding 7, each answered by
  `Error`; (10) a Hello whose `ViewSnapshot` has the version-1 layout with
  `hp` renamed to `health` (`schema_probes.hpp`), answered by a `HelloAck`
  with `reject_names` and a close; (11) a Hello whose `ViewSnapshot` has
  one more field than version 2, answered with `reject_unknown` and a
  close; (12) a Hello payload that is no XOffset wire, answered by `Error`;
  on one connection, a snapshot it requested and changed like `--edits`
  does, sent back as (13) a truncated `Edit` wire, (14) an `Edit` whose
  header digest is not its content's, (15) an `Edit` with a unit moved
  outside its rect (sound wire, matching digest: the field rules refuse
  it), each answered by `Error`, then as it is, answered by `EditAck`.
  Exit 0 when all fifteen were rejected and A answered every valid request.
  A logs fifteen `rejected:` lines.

## 6. Metrics (metrics.hpp names) and CSV files

Units: `_us` microseconds (process-local steady clock), `_bytes`, `_count`.

| Process | Metric names |
| --- | --- |
| A | `a.units_count`, `a.ticks_per_s`, `a.ab_rtt_us` (send start → StepRes complete), `a.step_req_bytes`, `a.step_res_bytes`, `a.save_us`, `a.load_us` (load or reload), `a.destroy_us`, `a.req_load_us`, `a.filter_us`, `a.snap_build_us`, `a.snap_save_us`, `a.snap_destroy_us`, `a.snap_bytes` (XOffset wire), `a.snap_net_bytes` (payload sent), `a.snap_compress_us` (zstd only), `a.snap_convert_us` (version-1 clients only), `a.visible_count`, `a.check_us`; write-back only: `a.edit_load_us`, `a.edit_bytes`, `a.edit_rebuild_us`, `a.forward_bytes` |
| B | `b.load_us`, `b.modify_us`, `b.save_us`, `b.send_us`, `b.destroy_us`, `b.in_bytes`, `b.out_bytes` |
| Client | `c.view_rtt_us` (ViewReq sent → ViewSnapshot complete), `c.load_us`, `c.read_present_us` (field read + presentation data, MERGED), `c.draw_us`, `c.present_us`, `c.destroy_us`, `c.snap_bytes` (XOffset wire), `c.snap_net_bytes` (payload received), `c.decompress_us` (zstd only), `c.visible_count`, `c.catalog_load_us`; `--edits` only: `c.edit_us` (change in place), `c.edit_save_us`, `c.edit_bytes`, `c.edit_rtt_us` (Edit sent → EditAck), `c.forward_load_us` |

The four destroy metrics (`a.destroy_us`, `a.snap_destroy_us`, `b.destroy_us`,
`c.destroy_us`) exist only with `--buffers fresh`; `reuse` destroys nothing per
message, and the `destroy_us` cells of the per-row files below stay empty.
`c.draw_us` and `c.present_us` come from the window client only.

Files in `--csv-dir` (comma separated, header row first):

| File | Columns |
| --- | --- |
| `a_metrics.csv`, `b_metrics.csv`, `client_metrics.csv` | `role,metric,count,mean,p50,p95,max` |
| `a_steps.csv` | `tick,req_bytes,res_bytes,ab_rtt_us,save_us,load_us,destroy_us,free_blocks,free_bytes,check_ok,req_digest,res_digest` |
| `a_snapshots.csv` | `request_id,tick,category,min_x,min_y,max_x,max_y,visible,wire_bytes,digest,req_load_us,filter_us,build_us,save_us,destroy_us,check_ok,encoding,net_bytes,wire_digest,compress_us,schema,convert_us` (`schema`: the snapshot version sent; `convert_us`: version-1 rows only) |
| `client_snapshots.csv` | `request_id,tick,category,visible,wire_bytes,digest,rtt_us,load_us,read_present_us,destroy_us,check_ok,encoding,net_bytes,wire_digest,decompress_us` |
| `corpus.csv` (A `--corpus-dir`) | `request_id,flags,category,encoding,file,net_bytes,wire_bytes,wire_digest,digest,visible` |
| `client_replay.csv` (client `--replay`) | `request_id,category,encoding,net_bytes,wire_bytes,wire_digest,digest,decompress_us,load_us,result` |
| `a_events.csv` | `tick,part,encoding,count,wire_bytes,net_bytes,digest,encode_us` (one row per `Events` frame) |
| `client_events.csv` | `tick,part,encoding,count,wire_bytes,net_bytes,digest,decode_us,verify_us,check_ok` |
| `a_edits.csv` | `edit_id,client,schema,units,wire_bytes,wire_digest,digest,client_digest,load_us,forwards,fwd_bytes,fwd_wire_digest,rebuild_us,check_ok` (one row per accepted `Edit`; `digest` A's, `client_digest` the frame's; `forwards` the number of `Forward` frames, `fwd_*` the rebuilt wire, empty without one) |
| `client_edits.csv` (`--edits`) | `edit_id,units,wire_bytes,digest,wire_digest,edit_us,save_us,rtt_us,acked,erased_id,changed_id,added_id,renamed_id` (one row per `Edit` sent; `acked` 0 only for one sent after A stopped reading; an id cell is empty when the snapshot had no such unit) |
| `client_forwards.csv` (`--edits`) | `edit_id,units,wire_bytes,digest,a_digest,wire_digest,load_us,check_ok` (one row per `Forward`; `digest` the client's, `a_digest` the frame's) |
| `client_build.txt` | one line: how the client was built |
| `a_checks.csv`, `client_checks.csv`, `selftest_checks.csv` | `check,result,detail` (`result` is `pass` or `fail`; coverage counts go in `detail`) |

`a_checks.csv` rows, in this order: `batch_roundtrip`, `reference_model`,
`modification_coverage`, `view_snapshot`, `snapshot_paths`, `rejected_requests`,
`schema_versions` (clients per snapshot version, snapshots converted; with
`--check` every converted wire is loaded back as version 1 and must have
the digest of the snapshot it came from), `client_edits` (accepted edits,
writers, Forward frames; with `--check` every rebuilt wire is loaded back
and must have the edit's digest),
`event_stream` (subscribers, dictionary id, frames per encoding; passes with
no subscriber). A subscribed client adds `events_subscribed`,
`events_received` and `event_content` to `client_checks.csv`; a client
with `--edits` adds `edit_sanity`, `edit_acked`, `edits` (at least one
acknowledged) and, when it received any, `forward_received`.
In the event files `wire_bytes` is the sum of the frame's UnitEvent wires,
`net_bytes` the payload, and `digest` the `checks::wire_digest` of the raw
encoding of the same wires, so A and the client must agree on all of them
per (tick, part, encoding) whatever the encoding.

`category` is `normal`, `full`, or `empty`; `digest` is 16 lowercase hex digits
of `checks::snapshot_digest` over the fields both schema versions have
(`scene_units` is left out, so A's digest of the version-2 snapshot equals a
version-1 client's digest of the converted one); `check_ok` is `1` or `0`. `req_digest` and
`res_digest` are 16 lowercase hex digits of `checks::wire_digest` over the exact
`StepReq` / `StepRes` payload bytes (empty without `--check`): equal digests per
tick across runs mean the same bytes, whatever the buffer mode, compiler or
platform that produced them. In the snapshot files `wire_bytes` and
`wire_digest` describe the XOffset wire after any decompression and
`net_bytes` the payload on the network; A and the client must agree on all of
them per request id.

## 7. Done criteria per thread

| Thread | Done when |
| --- | --- |
| Common library | `mapdemo_selftest` passes in Release and ASan+UBSan (GCC 16); net round trip works between two processes |
| Servers | A and B run 500 steps at 1 / 100 / 10000 units with `--check` all passing, with the headless client attached |
| Client | headless sampling and `--evil` pass against A; window client builds on the Mac and shows units moving, state changes and other areas after a drag (screenshots) |
| Integration | `tools/run_all_tiers.sh` runs the three tiers + `--evil` and `tools/summarize.py` writes the result tables; local validation (GitHub CI is not used); Mac step file; `RESULT.md`; PR to next_cpp26 |
| Write-back (10-28) | with `--edits 10` on both clients of every tier: each client's edits are acknowledged, A's `a_edits.csv` equals the senders' rows (wire bytes, wire digest, content digest), every `Forward` arrives at the other client with A's rebuilt bytes and the sender's content; `--evil` refuses the three bad write-backs; `mapdemo_selftest` `writeback` and client `--self-check` `edit_round_trip` pass |
| Old-version client (10-28) | `mapdemo_client_v1` passes the three tiers on every client target while the version-2 client of the same target is connected to the same A (`MAPDEMO_CLIENT2_OF`), both PASS; it replays a version-1 corpus; its evil run refuses the renamed and unknown schemas; `mapdemo_schema_freeze check` passes |
| End to end (10-27) | `tools/run_e2e.sh` passes for every client target (Linux, Windows MinGW and MSVC, Android emulator, iOS Simulator): self-check, replay of one corpus, the three tiers with `--encoding cycle`, evil; `tools/e2e_report.py` writes `E2E_RESULT.md` |
