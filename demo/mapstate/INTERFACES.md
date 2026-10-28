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

| Owner thread | Files |
| --- | --- |
| Common library | `include/mapdemo/rules_impl.hpp`, `src/net.cpp`, `src/metrics.cpp`, `src/checks.cpp` (+ optional `include/mapdemo/checks_impl.hpp`), `src/codec.cpp`, `src/selftest.cpp` |
| Servers | `src/server_a.cpp`, `src/server_b.cpp` (+ optional `src/server_*.hpp`) |
| Client | `src/client_main.cpp`, `src/client_core.cpp`, `src/client_core.hpp`, `src/client_offline.cpp`, `src/client_gui.cpp` |
| Integration | `CMakeLists.txt`, top-level `CMakeLists.txt` hook, `README.md`, `tools/*`, `.github/workflows/*` demo job, `RESULT.md`, Mac step files |

Frozen headers (`schema.hpp`, `std_model.hpp`, `rules.hpp`, `net.hpp`,
`metrics.hpp`, `checks.hpp`, `codec.hpp`, `result.hpp`) change only with a note to every demo thread
(through the coordinator); additive helpers are fine.

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
  reflection. The Hello schema hash is the same in both builds.
- zstd 1.5.7 is built from its release tarball (SHA-256 pinned;
  `-DMAPDEMO_ZSTD_URL=<local copy>` builds offline; `-DMAPDEMO_WITH_ZSTD=OFF`
  leaves dense and compact only).
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
               [--corpus-dir DIR [--corpus-max 60]]
mapdemo_client [--host 127.0.0.1] [--port 47701] [--headless] [--check]
               [--buffers reuse|fresh] [--encoding zstd|compact|dense|cycle]
               [--csv-dir DIR] [--auto-drag] [--screenshot-at F1,F2,...] [--evil]
mapdemo_client --self-check [--csv-dir DIR]
mapdemo_client --replay CORPUS_DIR [--buffers reuse|fresh] [--csv-dir DIR]
mapdemo_selftest [--csv-dir DIR]
```

- A `--host`: the address A listens on for clients (`0.0.0.0` when the client
  is in an emulator or another machine; B stays on 127.0.0.1).
- A `--corpus-dir`: keeps the first `--corpus-max` snapshot payloads exactly
  as sent (`snap_<request_id>.<encoding>`) and `corpus.csv` with A's digests.
- Client `--encoding` (default `zstd`, or `compact` without zstd): the
  snapshot encoding asked for in every `ViewReq`; `cycle` takes
  `request_id % 3` (dense, compact, zstd), so one run covers all three.
- Client `--self-check`: no network; builds sample snapshots and checks that
  every encoding verifies and that the refusal cases are refused (truncated
  and bit-flipped wires, wrong schema, malformed zstd frames, over-cap
  sizes). Exit 0 when all pass.
- Client `--replay DIR`: no network; verifies every corpus snapshot and
  compares wire bytes, wire digest, content digest, request id and visible
  count with A's row, so every platform shows it reads the same bytes the
  same way. Exit 0 when all match.
- Every client run prints `client: build <compiler, os, arch, C++ standard,
  schema backend, exceptions on|off, zstd>` and writes it to
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
| A connects to B | A `Hello(hello_payload())` → B `HelloOk` (mismatch: `Error`, exit 2) |
| Each step | A `StepReq(corr = tick T, UnitStateBatch wire)` → B `StepRes(corr = T+1, wire)`; at most one in flight |
| Client connects to A | client `Hello` → A `HelloOk`, then A `Catalog(MapCatalog wire)` |
| View | client `ViewReq(corr = request_id, flags = codec::view_flags(category, encoding), ViewRequest wire)` → A `ViewSnapshot(corr = request_id, flags echoed, ViewSnapshot payload in that encoding)`; at most one outstanding per client |
| Illegal / malformed request | A answers `Error(text)` (bad wire or illegal rect) or closes the connection (bad header, oversize length), logs `rejected: ...`, keeps running |
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

Every schema struct is declared with `XOFFSET_RECORD(Type, fields...)` in
declaration order: C++20 builds take the layout from these lists, C++26
builds check them against reflection. The V2.1 wire (12-byte container heads)
left padding in the old layouts, so the filler fields went: `UnitState` is
56 bytes, `VisibleUnit` 32, `ViewSnapshot` 48, all without padding. Step and
snapshot wire bytes differ from runs before 10-27.

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
  `Error` or a close for each bad request; exit 0 when all six were rejected
  and A answered every valid request. A logs six `rejected:` lines.

## 6. Metrics (metrics.hpp names) and CSV files

Units: `_us` microseconds (process-local steady clock), `_bytes`, `_count`.

| Process | Metric names |
| --- | --- |
| A | `a.units_count`, `a.ticks_per_s`, `a.ab_rtt_us` (send start → StepRes complete), `a.step_req_bytes`, `a.step_res_bytes`, `a.save_us`, `a.load_us` (load or reload), `a.destroy_us`, `a.req_load_us`, `a.filter_us`, `a.snap_build_us`, `a.snap_save_us`, `a.snap_destroy_us`, `a.snap_bytes` (XOffset wire), `a.snap_net_bytes` (payload sent), `a.snap_compress_us` (zstd only), `a.visible_count`, `a.check_us` |
| B | `b.load_us`, `b.modify_us`, `b.save_us`, `b.send_us`, `b.destroy_us`, `b.in_bytes`, `b.out_bytes` |
| Client | `c.view_rtt_us` (ViewReq sent → ViewSnapshot complete), `c.load_us`, `c.read_present_us` (field read + presentation data, MERGED), `c.draw_us`, `c.present_us`, `c.destroy_us`, `c.snap_bytes` (XOffset wire), `c.snap_net_bytes` (payload received), `c.decompress_us` (zstd only), `c.visible_count`, `c.catalog_load_us` |

The four destroy metrics (`a.destroy_us`, `a.snap_destroy_us`, `b.destroy_us`,
`c.destroy_us`) exist only with `--buffers fresh`; `reuse` destroys nothing per
message, and the `destroy_us` cells of the per-row files below stay empty.
`c.draw_us` and `c.present_us` come from the window client only.

Files in `--csv-dir` (comma separated, header row first):

| File | Columns |
| --- | --- |
| `a_metrics.csv`, `b_metrics.csv`, `client_metrics.csv` | `role,metric,count,mean,p50,p95,max` |
| `a_steps.csv` | `tick,req_bytes,res_bytes,ab_rtt_us,save_us,load_us,destroy_us,free_blocks,free_bytes,check_ok,req_digest,res_digest` |
| `a_snapshots.csv` | `request_id,tick,category,min_x,min_y,max_x,max_y,visible,wire_bytes,digest,req_load_us,filter_us,build_us,save_us,destroy_us,check_ok,encoding,net_bytes,wire_digest,compress_us` |
| `client_snapshots.csv` | `request_id,tick,category,visible,wire_bytes,digest,rtt_us,load_us,read_present_us,destroy_us,check_ok,encoding,net_bytes,wire_digest,decompress_us` |
| `corpus.csv` (A `--corpus-dir`) | `request_id,flags,category,encoding,file,net_bytes,wire_bytes,wire_digest,digest,visible` |
| `client_replay.csv` (client `--replay`) | `request_id,category,encoding,net_bytes,wire_bytes,wire_digest,digest,decompress_us,load_us,result` |
| `client_build.txt` | one line: how the client was built |
| `a_checks.csv`, `client_checks.csv`, `selftest_checks.csv` | `check,result,detail` (`result` is `pass` or `fail`; coverage counts go in `detail`) |

`a_checks.csv` rows, in this order: `batch_roundtrip`, `reference_model`,
`modification_coverage`, `view_snapshot`, `snapshot_paths`, `rejected_requests`.

`category` is `normal`, `full`, or `empty`; `digest` is 16 lowercase hex digits
of `checks::snapshot_digest`; `check_ok` is `1` or `0`. `req_digest` and
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
| End to end (10-27) | `tools/run_e2e.sh` passes for every client target (Linux, Windows MinGW and MSVC, Android emulator, iOS Simulator): self-check, replay of one corpus, the three tiers with `--encoding cycle`, evil; `tools/e2e_report.py` writes `E2E_RESULT.md` |
