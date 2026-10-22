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

| Owner thread | Files |
| --- | --- |
| Common library | `include/mapdemo/rules_impl.hpp`, `src/net.cpp`, `src/metrics.cpp`, `src/checks.cpp` (+ optional `include/mapdemo/checks_impl.hpp`), `src/selftest.cpp` |
| Servers | `src/server_a.cpp`, `src/server_b.cpp` (+ optional `src/server_*.hpp`) |
| Client | `src/client_main.cpp`, `src/client_core.cpp`, `src/client_core.hpp`, `src/client_gui.cpp` |
| Integration | `CMakeLists.txt`, top-level `CMakeLists.txt` hook, `README.md`, `tools/*`, `.github/workflows/*` demo job, `RESULT.md`, Mac step files |

Frozen headers (`schema.hpp`, `std_model.hpp`, `rules.hpp`, `net.hpp`,
`metrics.hpp`, `checks.hpp`) change only with a note to every demo thread
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
- All demo code is built with `-ffp-contract=off` (set on `mapdemo_common`).
- Write conservative C++ for clang-p2996 d77eff1: explicit includes (`<span>`,
  `<optional>`, ...), no deduced return types used before their definition.

## 3. Processes and command lines

```
mapdemo_b      [--port 47702] [--csv-dir DIR]
mapdemo_a      [--units 10000] [--seed 1] [--steps 0] [--duration-s 0]
               [--port 47701] [--b-host 127.0.0.1] [--b-port 47702]
               [--check | --no-check] [--csv-dir DIR]
mapdemo_client [--host 127.0.0.1] [--port 47701] [--headless] [--check]
               [--csv-dir DIR] [--auto-drag] [--screenshot-at F1,F2,...] [--evil]
mapdemo_selftest [--csv-dir DIR]
```

- Start order B, A, client; connections retry for 5 s.
- `--steps 0` and `--duration-s 0` mean unlimited. A stops after whichever limit
  is reached, sends `Bye` to B and the client, writes its CSV files, exits.
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
| View | client `ViewReq(corr = request_id, flags = ViewCategory, ViewRequest wire)` → A `ViewSnapshot(corr = request_id, flags echoed, ViewSnapshot wire)`; at most one outstanding per client |
| Illegal / malformed request | A answers `Error(text)` (bad wire or illegal rect) or closes the connection (bad header, oversize length), logs `rejected: ...`, keeps running |
| End | A `Bye` to B and client |

Receivers call `load_verified<T>(payload, cap_*())` with the caps in `schema.hpp`.
Every snapshot and every received batch is its own `XBuffer` (no pool, no reuse).

## 5. Behaviour fixed for all threads

- Rules (generation, movement, events, rect rule): design section 3, implemented
  once in `rules_impl.hpp`. A builds the initial batch and the reference model with
  `init_batch`; B calls `step_batch` on the loaded batch; A's reference model calls
  `step_batch` on the std model.
- A is single-threaded (`poll`): sends a step when none is in flight and the 20 Hz
  deadline has passed; serves view requests from the last completed batch while
  waiting for B; swaps in B's result after `load_verified` and destroys the old one.
- A classifies nothing itself: the category comes in the `ViewReq` flags.
- Headless client sampling: 10 Hz; each group of 10 requests = 8 `Normal`
  (1024×768 rect moving along a fixed route over the scene), 1 `FullScene`
  (`scene_bounds()`), 1 `Empty` (`empty_probe_rect()`).
- Window client: 1024×768, 1 px = 1 scene unit, left-drag pans, camera clamped to
  the scene; snapshot replaced only at the start of a frame; reads fields directly
  from the live snapshot buffer (design section 5).
- `--evil` (client): three separate connections to A sending (1) a truncated
  `ViewRequest` wire, (2) a `ViewSnapshot` wire as `ViewReq` (wrong schema),
  (3) a header declaring a 1 GiB `ViewReq`; expects `Error` or a close for each;
  exit 0 when all three were rejected and A still answers a normal request.

## 6. Metrics (metrics.hpp names) and CSV files

Units: `_us` microseconds (process-local steady clock), `_bytes`, `_count`.

| Process | Metric names |
| --- | --- |
| A | `a.units_count`, `a.ticks_per_s`, `a.ab_rtt_us` (send start → StepRes complete), `a.step_req_bytes`, `a.step_res_bytes`, `a.save_us`, `a.load_us`, `a.destroy_us`, `a.req_load_us`, `a.filter_us`, `a.snap_build_us`, `a.snap_save_us`, `a.snap_destroy_us`, `a.snap_bytes`, `a.visible_count`, `a.check_us` |
| B | `b.load_us`, `b.modify_us`, `b.save_us`, `b.send_us`, `b.destroy_us`, `b.in_bytes`, `b.out_bytes` |
| Client | `c.view_rtt_us` (ViewReq sent → ViewSnapshot complete), `c.load_us`, `c.read_present_us` (field read + presentation data, MERGED), `c.draw_us`, `c.present_us`, `c.destroy_us`, `c.snap_bytes`, `c.visible_count`, `c.catalog_load_us` |

Files in `--csv-dir` (comma separated, header row first):

| File | Columns |
| --- | --- |
| `a_metrics.csv`, `b_metrics.csv`, `client_metrics.csv` | `role,metric,count,mean,p50,p95,max` |
| `a_steps.csv` | `tick,req_bytes,res_bytes,ab_rtt_us,save_us,load_us,destroy_us,free_blocks,free_bytes,check_ok` |
| `a_snapshots.csv` | `request_id,tick,category,min_x,min_y,max_x,max_y,visible,wire_bytes,digest,req_load_us,filter_us,build_us,save_us,destroy_us,check_ok` |
| `client_snapshots.csv` | `request_id,tick,category,visible,wire_bytes,digest,rtt_us,load_us,read_present_us,destroy_us,check_ok` |
| `a_checks.csv`, `client_checks.csv`, `selftest_checks.csv` | `check,result,detail` (`result` is `pass` or `fail`; coverage counts go in `detail`) |

`category` is `normal`, `full`, or `empty`; `digest` is 16 lowercase hex digits
of `checks::snapshot_digest`; `check_ok` is `1` or `0`.

## 7. Done criteria per thread

| Thread | Done when |
| --- | --- |
| Common library | `mapdemo_selftest` passes in Release and ASan+UBSan (GCC 16); net round trip works between two processes |
| Servers | A and B run 500 steps at 1 / 100 / 10000 units with `--check` all passing, with the headless client attached |
| Client | headless sampling and `--evil` pass against A; window client builds on the Mac and shows units moving, state changes and other areas after a drag (screenshots) |
| Integration | `tools/run_all_tiers.sh` runs the three tiers + `--evil` and `tools/summarize.py` writes the result tables; CI job; Mac step file; `RESULT.md`; PR to next_cpp26 |
