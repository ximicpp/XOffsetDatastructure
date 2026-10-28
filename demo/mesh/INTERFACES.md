# Device mesh demo: interfaces for parallel implementation

Frozen contract for the device mesh demo ("跨设备数据互传 demo"). The design,
the options and the decision live in the proposal document
<https://claude.ai/artifact/PUqGPMQvcr2JjnqN4ocxYq>. Decided 10-28: option A,
and the first version is **pure relay** (devices register at a central
hub and every message goes through it; direct device-to-device links come
later and keep this frame format).

What the demo shows: a set of everyday data types (not a game). Any device
(Linux, Windows, Android, iOS, macOS) joins the mesh, and an object of any
type travels device to device as its XOffset wire: no serialization on the
sender, no parsing on the receiver. Each receiver verifies it, checks it
against a reference model, changes it in place and sends it on. The hub
replays every job and checks that each device sent byte-identical data.

## 1. Branch, ownership and rules

All threads work on branch `claude/xmesh-demo-20261028` (from next_cpp26
f7ea833). `git pull --rebase` before every push, never force-push, touch
only the files you own, and keep the build green at every commit (replace
stubs, do not delete them). Merge class 0 (demo and docs only); no GitHub CI.

| Path | Owner |
| --- | --- |
| `include/xmesh/schema.hpp`, `types.hpp`, `protocol.hpp`, `hop.hpp`, `result.hpp`, this file | design thread (frozen, section 8) |
| `src/types/` (names.cpp, objects.cpp and any new files), `src/hop.cpp`, `src/selftest.cpp`, `src/schema_freeze.cpp`, `schema.lock`, `corpus/` | types thread (T1) |
| `include/xmesh/net.hpp`, `src/net.cpp`, `src/hub_main.cpp`, `src/peer_main.cpp`, `src/hub/`, `src/peer/`, `tools/`, `CMakeLists.txt`, `README.md`, `RESULT.md` | network thread (T2) |

T1 may add source files to the `xmesh_core` list in `CMakeLists.txt`; T2
owns everything else in it. Root `CMakeLists.txt`: option
`XOFFSET_BUILD_MESH_DEMO` (already added) and nothing else.

Every device target is 64-bit little-endian. Device columns: Linux x86-64
and arm64 (GCC, Clang), Windows (MSVC, clang-cl, MinGW-w64), Android arm64
and x86-64 (NDK), iOS Simulator, macOS (Apple Clang); each also as a build
without exceptions.

## 2. Build

- `-DXOFFSET_BUILD_MESH_DEMO=ON -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF`.
  Binaries land in `<build>/bin/<Config>/`.
- Device targets: add `-DXMESH_PEER_ONLY=ON -DXOFFSET_CXX_STANDARD=20` and a
  toolchain file from `cmake/toolchains/` (or a native compiler): builds
  `xo_peer`, `xo_peer_noexc` and `xmesh_selftest` with the
  `XOFFSET_RECORD` field lists.
- Hub: Linux, either backend (C++26 reflection with clang-p2996 or GCC 16,
  or C++20 field lists). Both backends give the same fingerprints (section 3).
- Everything is built with `-ffp-contract=off` (set on `xmesh_core` for
  GCC-style front ends).
- `xoffset_test_host_modules()` at the end of `CMakeLists.txt`: with
  `-DXOFFSET_TEST_HOST=ON`, `xo_peer` and `xmesh_selftest` also build as
  `libxoh_<program>` for the iOS test host app (tools/test_host), the way
  the map demo's client runs on the iOS Simulator.
- The skeleton (stubs) builds with g++-13 and clang++-18 at C++20, with and
  without exceptions; the programs exit 1 with "not implemented yet".

## 3. Schema (schema.hpp)

Six data roots and five control roots. Portable scalars only
(USER_GUIDE 2.2), no implicit padding (`XOFFSET_REQUIRE_NO_PADDING` on each
root), optional = `XVector` of length 0 or 1, variant = tag plus one field
per kind (`KvConfig::Value`, 4-aligned so `XMap<XString, Value>` has no
padding between key and value). Every data root keeps a `trail` of
`HopMark{hop, device}`.

Fingerprints with C++20 field lists (g++-13 and clang++-18 agree);
`xmesh_schema_freeze` writes them to `schema.lock` and every build checks
them, C++26 reflection builds included:

| Root | sizeof | alignof | wire_schema_hash | schema_name_digest |
| --- | --- | --- | --- | --- |
| Document | 80 | 8 | `87787a9f1767d50c` | `65e21ba4d06b5172` |
| Order | 160 | 8 | `39adf9e08cc953b8` | `0173d7de2c384d78` |
| SensorSeries | 104 | 8 | `465895f95425a12f` | `316b9a0cc3d526df` |
| KvConfig | 64 | 8 | `ec54cd6bd56be812` | `a8d471d4e8efa750` |
| SceneTree | 56 | 8 | `087476c9f00da0d7` | `433d8f17c9332544` |
| ChatRoom | 88 | 8 | `9c8bd9ae5c45da97` | `6c00d8b7c16cde24` |
| PeerInfo | 32 | 4 | `6b91e1726bb9acc8` | `4442a4259c239a62` |
| Welcome | 32 | 4 | `8267cae10f66a5e5` | `4b8c42b30eb9a7a1` |
| Roster | 24 | 8 | `e3261a05c9ff8a97` | `e0095f48fdcff559` |
| HopMeta | 56 | 8 | `564e0bf1979abbcc` | `4911e6cf9a2e8e79` |
| HopReport | 88 | 8 | `3572ca1a55546563` | `7fccbae2e9b0458e` |

## 4. Protocol (protocol.hpp)

One TCP connection per device, to the hub; 32-byte `FrameHeader`, magic
`XMS1`, per-type limits checked before the payload is read. The sequence
is drawn at the top of `protocol.hpp`.

Hub rules:

- Hello: `accept_hello`; the device is admitted only when `hello_complete`
  and `accepted()` (every root `same`). Otherwise HelloAck, then Error, then
  close.
- Register: the hub gives the next device id (1, 2, ...; never reused in a
  run), answers Welcome and sends Roster (version + 1) to every device.
  A device that leaves (EOF, error, or Bye answered) is dropped from the
  roster the same way; its jobs in flight fail with "device left".
- Hop: the hub overwrites `from` with the connection's device id, checks
  that `to` is in the roster, and forwards the frame without reading the
  payload. Unknown `to`: the job fails ("no such device"). Every forwarded
  payload is also written to the archive (section 6).
- Report: the hub reads it (`read_report`) and scores the job (section 5).
- Correlation of Job, Hop and Report frames is the job id; Roster's is the
  roster version.

Device rules: the device answers a Job with `start_job` and a Hop with
`handle_hop`; it sends `next_payload` (when not empty) as a Hop frame to
`next_device`, then the Report. It handles one message at a time, in
arrival order.

## 5. Hops and jobs (types.hpp, hop.hpp)

The per-hop checks are listed at the top of `hop.hpp`; their order is fixed,
and a device reports the first failure. Mutations, digests and the model
are deterministic on every device (`types.hpp` header).

Job kinds the hub plans (`--plan`):

- `pairs`: every ordered pair (a, b) of different devices, for each chosen
  type, scale and save mode: route `[a, b, a]` (a makes it, b checks,
  changes and sends it back, a checks it).
- `chain`: for each type and scale, one seeded permutation of all devices
  p1..pn: route `[p1, ..., pn, p1]`.
- `evil`: every ordered pair (a, b) with `kPeerEvil` on a, each EvilCase
  once (types rotated by job id): route `[a, b]`.

A normal job passes when every route position reported `Ok`, every
reported `content_digest` and `sent_wire_digest` equals the hub's `replay`
of the job (byte-identical sends), and the last stop reported. An evil job
passes when route[1] reports `expected_verdict(evil)` with
`expected_reject = 1` and nothing goes further. A job that has not
finished after `--job-timeout-s` fails with "timeout".

## 6. Programs, files and the join link

`xo_hub` (network thread):

```
xo_hub --run LABEL --out DIR [--bind 0.0.0.0] [--port 47801] [--http-port 47802]
       [--expect N] [--wait-s 120] [--plan pairs,chain,evil]
       [--types all|Document,Order,...] [--scales small,medium] [--save-modes verified,compact]
       [--seed 1] [--max-inflight 8] [--job-timeout-s 60] [--linger] [--no-archive]
```

Starts jobs when `--expect` devices registered (or `--wait-s` passed with at
least 2), sends Bye to everyone after the last job (unless `--linger`), and
exits 0 only when every job passed. Writes into `DIR`:

- `devices.csv`: `id,name,build,flags,joined_ms,left_ms`
- `jobs.csv`: `job,kind,type,scale,save_mode,evil,route,result,reason,ms`
  (route as device names joined by `>`; result `pass` or `fail`)
- `hops.csv`: `job,hop,device,verdict,expected_reject,data_bytes,sent_bytes,grown_bytes,content_ok,wire_ok,verify_ns,check_ns,mutate_ns,save_ns,error`
- `matrix.csv`: `sender,receiver,jobs,passed` (from pair jobs)
- `summary.txt`: one line per device, totals, and `RESULT PASS` or `RESULT FAIL`
- `archive/<job>-<hop>.xmhop`: each forwarded Hop payload (pack_hop format)

HTTP on `--http-port`: `/` a dashboard (roster, jobs in flight, matrix,
the last summaries), `/join` the join page (QR code of the join link, the
`xo_peer` command line), `/status.json` the same data for scripts. Join
link: `http://HOST:47802/join?hub=HOST:47801&run=LABEL`.

`xo_peer` and `xo_peer_noexc` (network thread):

```
xo_peer (--hub HOST:PORT | --join URL) [--name NAME] [--evil] [--out DIR] [--timeout-s 600]
xo_peer --mailbox IN_DIR OUT_DIR --name NAME --id N
```

`--name` defaults to `<os>-<arch>-<compiler>[-noexc]` from the build line.
`--evil` sets `kPeerEvil`. `--out DIR` writes `peer.log` and the build
line. Exit 0 after Bye; 1 on a handshake or protocol failure; 2 on usage.
Mailbox mode (phase 4, cross-site): reads `.xmhop` files addressed to
`--id`, runs `handle_hop`, writes the next hop and the report as files.
Output on stdout: one line per hop handled: job, type, verdict, summary.

`xmesh_selftest` (types thread): in memory, no network. For every type,
scale and save mode: make, save, receive, digest, check, a five-device
chain of mutations, `replay` equal to the hops run by hand, both save modes
digesting the same; every EvilCase through `handle_hop` giving
`expected_verdict`. `--corpus-write DIR` writes a fixed set of jobs (wires
and `HopExpect`s); `--corpus-check DIR` recomputes them and fails on any
byte difference. Exit 0 when everything passed.

`xmesh_schema_freeze` (types thread): `write LOCK` and `check LOCK` for the
section 3 table.

## 7. Threads and done criteria

**T1, types and self-test** (`互传 demo：类型和自测`): `src/types/` (six types:
state 0 per scale, mutations, content digest, the plain-std reference model
and field-by-field comparison, `replay`, `make_evil`), `src/hop.cpp`,
`xmesh_selftest`, `xmesh_schema_freeze` and `schema.lock`, the corpus.
Done when: the self-test passes on every cloud device column (Linux x86-64
and arm64, Windows MSVC / clang-cl / MinGW, Android NDK, each with and
without exceptions), the corpus written on Linux checks byte-identical on
all of them, C++26 reflection builds give the section 3 fingerprints, and
`RESULT.md` lists the state 0 sizes per type and scale.

**T2, hub and device program** (`互传 demo：中心节点和设备程序`): `net.hpp` /
`net.cpp` (adapted from demo/mapstate: POSIX and Winsock, `try_*` calls for
builds without exceptions), `xo_hub` (registration, roster, relay, job
planning, scoring, CSV files, archive, HTTP dashboard and join page),
`xo_peer` (`--hub`, `--join`; mailbox mode may stub until phase 4), and
`tools/run_mesh.sh` (the cloud mesh: the hub plus one device per column,
Windows through wine, Android through qemu-user, both exception modes).
T2 builds against T1's stubs and then T1's code as it lands. Done when: a
cloud run with all columns passes every pairs job for the six types (small
and medium, both save modes), one chain per type passes, every evil job is
refused with its expected verdict, and `RESULT.md` holds the matrix.

**Later phases** (design thread schedules them): 3, the Mac mesh (macOS,
iOS Simulator through the test host, Android emulator through `10.0.2.2`,
colima Linux) and the dashboard; 4, cross-site jobs through the mailbox
and the full N x N report. Real devices are postponed (no paid Apple
developer account: iOS stays on the Simulator; scan-to-install only for
Android and desktop).

## 8. Changing a frozen file

Ask in the design thread first, with the reason and the exact change. A
schema change changes every fingerprint and both threads' corpora; a
protocol or hop change changes both programs. Bug fixes inside a frozen
header that change no behaviour (a comment, an include) need no approval;
say so in the commit message.
