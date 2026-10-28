# Map state demo: acceptance record

Acceptance record required by section 9 of the demo requirements
(`slg-demo-20261022/DEMO_REQUIREMENTS_20261022.md`): whether every automatic
check passed, the allocation / free / cross-load reuse evidence, the cost and
bytes of the 1 / 100 / 10000-unit tiers, and where the client reads the
verified snapshot directly and how long each buffer lives.

Since the review iteration of 10-24 every process runs in one of two buffer
modes (`--buffers`, INTERFACES.md section 3): `reuse` (default) keeps one buffer
per role and refills it in place with `reload_verified` / `reset` /
`save_verified_into`; `fresh` makes a new `XBuffer` per message and destroys
it. Both run the same validator and send the same bytes.

| Run | Where | Status |
| --- | --- | --- |
| Functional + smoke numbers | cloud x86-64 VM, GCC 16.2 (Docker `gcc:16`), Release, both modes | all 135 verdicts pass (commit `7dc7f58`) |
| Sanitizers | same, Debug, ASan+UBSan: self-test, 100-unit tier and evil in both modes | all 53 verdicts pass (commit `7dc7f58`) |
| Mixed modes | Release: B `fresh`, A `reuse`, client `fresh`, 100 units x 500 steps | all exit 0; step wires equal those of the all-`fresh` tier at 500 / 500 ticks |
| Same bytes as before the iteration | self-test chain, 100 units x 300 ticks and 10000 x 200, against the same chain built from d58781e's schema | last result digests `ad708058dafcb733` and `6b38b6eb73627adc` in both |
| Library, local validation (the project does not use GitHub CI) | GCC 16.2 Docker: full CTest Debug, full CTest ASan+UBSan, validation differential d58781e vs `7dc7f58` (200,000 cases, seeds 1 and 2) | Debug 88 / 88, ASan+UBSan 88 / 88, differential identical outcomes (no library file changes) |
| Window client, headless display | cloud Xvfb, GCC 16, 10000 units, `--auto-drag --check` (commit `d8328f1`, before the iteration) | screenshots at frames 60 / 180 / 420 show units moving, the event-driven name changes and another area after the drag; `direct_read` 69 / 69 snapshots (32,441 records and names in the live buffer). The reuse-mode window client is only syntax-checked in the cloud (no X11 / GL headers); it is built and run on the Mac |
| Same bytes across compiler and platform | Apple M4 Mac, clang-p2996 d77eff1, arm64, against the cloud GCC 16 x86-64 run (`MAPDEMO_COMPARE`) | deferred (Mac queue, 10-26): Mac step file `slg-demo-20261022/review-20261024/MAC_STEPS_DEMO_REVIEW.md`, not yet queued |
| Formal timing + window client | Apple M4 Mac, clang-p2996 d77eff1, Release, both modes, two rounds | deferred (Mac queue, 10-26): same Mac step file |
| Earlier: GitHub CI | job `mapdemo-gcc16` (GCC 16 + P2996 Clang three tiers + ASan) | green on PR #34 (`e6d72bc`); GitHub CI is no longer used, the job stays in `ci.yml` unrun |
| End to end, cloud (10-27) | Linux A/B GCC 16 C++26; clients Linux C++26 and C++20, Windows MinGW, MSVC 19.44 and clang-cl 19 under wine, Android arm64 under qemu; 1 / 100 / 10000 x 500 steps, `--encoding cycle` | all six targets PASS (section 5) |
| End to end, sanitizers (10-27) | same, GCC 16 Debug ASan+UBSan, both buffer modes | PASS: self-check 23 / 23, replay 60 / 60, 151 / 151 verdicts (with step wires equal to `full1`), no sanitizer report |
| Final local validation (10-27) | rebased on next_cpp26 5af77d3 (field-list backend #149): `full3` (six targets) and `asan2` | both PASS; corpus of `full1` replayed 60 / 60 on every target, step wires equal to `full1` at 500 / 500 ticks in every tier and mode |
| End to end, macOS client (10-27) | Mac M4: Apple clang 21 C++20 client; A/B GCC 16 C++26 in a Linux arm64 VM (colima) on the same Mac | PASS: self-check 23 / 23, replay 60 / 60, 74 / 74 verdicts; step wires equal to the cloud x86-64 run at 500 / 500 ticks in every tier |
| Window client, cloud (10-27) | Xvfb, GCC 13 C++20 client with raylib 5.5, 10000 units, `--auto-drag --check --encoding cycle` | screenshots at frames 60 / 180 / 420 (`e2e-20261027/shots-cloud-xvfb/`), `direct_read` 69 / 69, all three encodings |
| End to end, Android emulator (10-28) | Mac M4: Android 15 emulator (API 35, google_apis arm64-v8a, 16 KB pages), NDK r29 C++20 client; A/B GCC 16 C++26 in a Linux arm64 VM (colima) on the same Mac; next_cpp26 00b5d35 | PASS: self-check 23 / 23, replay 60 / 60, 74 / 74 verdicts; step wires equal to the cloud x86-64 run at 500 / 500 ticks in every tier; library CTest 91 / 91 on the emulator. Emulator numbers, not device numbers |
| End to end, clients without exceptions (10-28) | cloud, next_cpp26 c928334 with #155: A/B GCC 16 C++26; the six client builds of `full3`, each also built without exceptions (`mapdemo_client_noexc`), 12 targets against the corpus and step wires of `full1` (`noexc3`); ASan+UBSan run of both C++26 clients in both buffer modes; window client under Xvfb | PASS: self-check 23 / 23, replay 60 / 60, tiers + evil on all 12 targets, with the same self-check and evil outcomes and refusal texts on all of them; step wires equal to `full1` at 500 / 500 ticks; sanitizers 151 / 151 and 149 / 149 verdicts, no report. Verification is 1.08x to 1.29x slower without exceptions (section 5) |
| Window client, macOS (10-28) | Mac M4, macOS 27.2: Apple clang 21 C++20 client with raylib 5.5, 10000 units, `--auto-drag --check`; A/B GCC 16 C++26 in a Linux arm64 VM (colima); next_cpp26 00b5d35 | client exit 0, screenshots at frames 60 / 180 / 420, `direct_read` 71 / 71, 59 fps |
| End to end, iOS Simulator (10-28) | Mac M4: iOS 27.0 Simulator (iPhone 17, Xcode 27.1), Apple clang 21 C++20 clients with and without exceptions; A/B GCC 16 C++26 in a Linux arm64 VM (colima) on the same Mac; next_cpp26 af601ea plus #160 (tests only) | PASS, both clients: self-check 23 / 23, replay 60 / 60, 74 / 74 and 75 / 75 verdicts, the same self-check and evil outcomes; step wires equal to the cloud x86-64 run at 500 / 500 ticks in every tier; library CTest 129 / 129 in the simulator, corpus identical. Simulator numbers, not device numbers |

Sections 1 to 4 were recorded before 10-27. The 10-27 schema change (V2.1
wire, filler fields removed) changes the step and snapshot bytes, so their
digests and byte counts are historical; section 5 is the current run.

Cloud numbers are smoke numbers on a shared VM: they show the shape of the
costs, not formal timings. Every timing is a process-local steady-clock
interval; percentiles are nearest rank (`rank = ceil(p/100 * n)`), both in the
processes' `*_metrics.csv` and in the per-category rows of `summarize.py`.

## 1. Automatic checks (requirements section 7)

| Requirement | How it is checked | Result (cloud GCC 16, both modes) |
| --- | --- | --- |
| Unit state round trip | A after every step: N units, ids 0..N-1 in order, tick = previous + 1, finite positions inside the scene, path 4..8 points inside the scene, cursor < size (`a_checks.csv` `batch_roundtrip`; `a_steps.csv` `check_ok`) | pass, 500 / 500 steps in each tier |
| Modification after receive | A advances a std reference model with the same rule template and compares B's batch field by field, floats bitwise (`reference_model`); coverage of long <-> short names and path grow -> shrink -> grow (`modification_coverage`) | pass in each tier; 10000 units: 1300 to long, 1200 to short, 1300 grow, 1200 shrink, grow-shrink-grow seen |
| Real allocation, free and reuse | `mapdemo_selftest` rounds 1..3, once loading each wire into a new buffer and once reloading it into the same buffer (section 2 below) | pass |
| Independent output | self-test: change the source after saving, reload the saved copy | pass (saved copy keeps hp=90, name U00007, 1 path point) |
| View snapshot | A compares every snapshot with the reference model filtered by the same scene and rect (id set, public fields, no duplicates; empty rect -> empty array) (`view_snapshot`, `snapshot_paths`); `summarize.py` matches A's and the client's per-request digest | pass; 10000 units: 250 / 250 digests equal, 25 empty snapshots with 0 units, 25 full-scene with 10000 |
| Client in-place read | every unit record and name byte the client reads lies inside the live snapshot buffer (`client_checks.csv` `direct_read`); no intermediate unit collection (section 4 below); ASan+UBSan run | pass in both modes |
| Same bytes in both modes | A logs a digest of every `StepReq` / `StepRes` payload (`a_steps.csv`); `summarize.py` requires the `fresh` and `reuse` tier of each unit count to agree at every tick; the self-test runs the whole A <-> B chain both ways and compares the wires byte for byte | pass; 500 / 500 ticks equal in each tier; self-test 300 ticks at 100 units and 200 at 10000 |
| Failure inputs | self-test: truncated (-1 byte, header only, empty), wrong schema, `used_bytes` over the cap, forged `used_bytes` at each of the four receive points; frame headers over the type limit rejected before any payload allocation (largest allocation <= 121 B). Online `--evil` against a running A: truncated `ViewRequest`, snapshot wire sent as a request, 1 GiB declared length, and on one connection a valid, a truncated and another valid request | pass in both modes; A logged four `rejected:` lines (in `reuse` the fourth comes from `reload_verified` on the kept request buffer), answered every valid request and finished its 200 steps |

Coverage per tier (`modification_coverage` detail): the event rule touches
units with `id % 100 == 0` every 20 ticks, so 1 and 100 units both modify unit
0 25 times (13 to long, 12 to short); grow -> shrink -> grow first completes at
tick 180, so runs shorter than about 180 steps cannot pass this check.

## 2. Allocation / free / reuse across save + load

`mapdemo_selftest` (`src/selftest.cpp`), public APIs only
(`for_each_free_block` offsets relative to the arena, `arena()->used_end`):

```
reuse_setup     source used_end=520 free_count=2 free_bytes=56 holes=[360+32)[392+24)
reuse_round_1   wire=632B carried=[360+32)[392+24) name_block=392 in_hole=[392+24) path_block=360 in_hole=[360+32) used_end=520->520 free_count=2->0
reuse_round_2   (same)
reuse_round_3   (same)
reload_round_1  (same as reuse_round_1)
reload_round_2  (same)
reload_round_3  (same)
```

Each round saves the batch, receives the wire, checks the carried free list,
then adds a unit whose first out-of-line name (24 B block) and path (32 B
block) land exactly in the carried holes while `used_end` does not grow; the
unit then leaves (`pop_back`), freeing both blocks again for the next round.
`reuse_round_*` loads each wire into a different buffer, `reload_round_*`
reloads it into the same buffer every round (what `--buffers reuse` does): the
carried free list and the hole reuse are identical. Offsets are relative to the
arena, never absolute addresses. Logical shrinking (smaller size, shorter
string, flat memory) is not counted.

## 3. Three tiers (cloud GCC 16 smoke, 500 steps, seed 1; p50 / p95)

| p50 / p95, us unless noted | 1 unit, fresh | 1 unit, reuse | 100 units, fresh | 100 units, reuse | 10000 units, fresh | 10000 units, reuse |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| ticks per s (target 20) | 20.03 | 20.04 | 20.03 | 20.04 | 20.04 | 20.04 |
| batch wire A->B / B->A, bytes (mean) | 356 / 356 | 356 / 356 | 9,860 / 9,860 | 9,860 / 9,860 | 967,103 / 967,117 | 967,103 / 967,117 |
| A<->B round trip | 401 / 672 | 361 / 570 | 425 / 699 | 390 / 611 | 3,210 / 4,413 | 1,359 / 2,324 |
| A save | 6.12 / 8.62 | 6.27 / 8.41 | 9.71 / 13.8 | 7.52 / 11.0 | 188 / 299 | 179 / 282 |
| A verify B's result | 55.8 / 92.1 | 9.14 / 16.1 | 61.7 / 100 | 12.2 / 20.0 | 957 / 1,202 | 323 / 469 |
| A destroy old batch | 19.5 / 45.3 | - | 21.0 / 48.6 | - | 122 / 202 | - |
| B verify | 61.0 / 95.0 | 10.6 / 16.0 | 69.2 / 102 | 13.4 / 22.8 | 991 / 1,357 | 300 / 459 |
| B step in place | 0.79 / 6.53 | 0.93 / 7.57 | 1.54 / 8.14 | 1.46 / 8.25 | 111 / 206 | 73.6 / 157 |
| B save | 1.36 / 2.04 | 2.20 / 3.15 | 2.54 / 4.37 | 3.19 / 4.65 | 630 / 939 | 122 / 201 |
| B destroy | 17.1 / 32.6 | - | 17.6 / 34.0 | - | 244 / 375 | - |
| 1024x768 view: units / wire bytes (mean) | 0.04 / 202 | 0.04 / 202 | 4.43 / 378 | 4.43 / 378 | 468 / 18,968 | 468 / 18,968 |
| 1024x768 view round trip | 380 / 668 | 204 / 380 | 324 / 590 | 253 / 561 | 672 / 958 | 571 / 1,052 |
| 1024x768 view A build | 13.9 / 26.8 | 2.39 / 3.77 | 14.8 / 29.9 | 3.86 / 5.90 | 45.3 / 88.8 | 16.7 / 35.3 |
| 1024x768 view A save | 1.43 / 2.36 | 1.76 / 2.70 | 1.49 / 2.23 | 1.93 / 3.07 | 5.91 / 9.74 | 4.86 / 9.85 |
| 1024x768 view client verify | 27.5 / 55.2 | 5.43 / 8.63 | 28.9 / 55.6 | 5.53 / 8.17 | 51.8 / 95.3 | 9.93 / 13.2 |
| full-scene view: units / wire bytes (mean) | 1.00 / 252 | 1.00 / 252 | 100 / 4,212 | 100 / 4,212 | 10,000 / 401,352 | 10,000 / 401,352 |
| full-scene view round trip | 401 / 814 | 191 / 368 | 292 / 571 | 237 / 582 | 1,141 / 1,495 | 878 / 1,286 |
| full-scene view A build | 16.0 / 25.5 | 4.10 / 5.68 | 17.8 / 24.9 | 6.07 / 15.9 | 377 / 550 | 217 / 342 |
| full-scene view A save | 1.78 / 2.40 | 1.80 / 2.25 | 2.09 / 3.23 | 2.41 / 3.98 | 53.2 / 91.0 | 53.9 / 132 |
| full-scene view client verify | 29.4 / 50.1 | 5.06 / 6.54 | 33.2 / 58.8 | 6.21 / 15.5 | 354 / 519 | 108 / 183 |
| full-scene view client read + present (MERGED) | 0.36 / 0.57 | 0.27 / 0.49 | 0.97 / 1.47 | 0.76 / 1.35 | 48.8 / 151 | 51.8 / 132 |
| empty view: units / wire bytes (mean) | 0.00 / 200 | 0.00 / 200 | 0.00 / 200 | 0.00 / 200 | 0.00 / 200 | 0.00 / 200 |
| empty view round trip | 406 / 1,133 | 213 / 463 | 285 / 527 | 287 / 551 | 553 / 701 | 481 / 652 |
| empty view A build | 13.8 / 31.4 | 2.21 / 3.40 | 12.8 / 22.2 | 2.27 / 3.63 | 19.2 / 34.6 | 45.6 / 63.8 |
| empty view A save | 1.50 / 2.49 | 1.58 / 1.99 | 1.36 / 3.82 | 1.94 / 9.29 | 1.83 / 2.96 | 2.59 / 4.23 |
| empty view client verify | 34.8 / 58.9 | 5.33 / 6.71 | 23.4 / 69.8 | 5.46 / 7.66 | 34.9 / 43.1 | 5.50 / 7.63 |

Each tier sampled 200 normal, 25 full-scene and 25 empty requests (a run can
end one request short when A's `Bye` arrives first; the 1024x768 rect usually
misses the single unit of the 1-unit tier, and those zero-visible snapshots are
counted as such). Medians are given because a few
single requests per run take about 50 ms on the shared VM in both modes, which
moves the means (and in a few cells the p95) but not the medians. Full tables,
including every metric of INTERFACES.md section 6, means and A's per-category
filter / build / save / destroy, come from `tools/summarize.py`
(`RESULT_TABLES.md`).

Observations (cloud, smoke):

- `fresh` pays a fixed cost per buffer, not per byte: at 1 and 100 units
  verifying a 356-byte or 9.9 KB batch takes 56 to 69 us and destroying it 17
  to 21 us, while saving it takes 1 to 10 us. `reuse` removes most of it:
  verifying the same batch takes 9 to 13 us (`reload_verified`), and nothing
  is destroyed. At 1 and 100 units the client's snapshot verify drops from 23
  to 35 us to 5 to 6 us, and A's snapshot build from 13 to 18 us to 2 to 6 us.
- At 10000 units (967 KB per batch) `reuse` cuts the A <-> B round trip from
  3.2 ms to 1.4 ms: each side verifies in a third of the time (no page faults
  on a fresh mapping), and B's save drops from 630 to 122 us because the
  output string keeps its capacity.
- One cost goes the other way. `reset<ViewSnapshot>()` zeroes the bytes the
  previous snapshot used (BUFFER_REUSE.md: no old bytes in new content). In
  the 10000-unit tier the empty request always follows a full-scene one
  (401 KB), so it takes about 46 us to build in `reuse` against 19 us in
  `fresh`. The requests after it are cheap again.
- A's checks (reference model, `a.check_us`) run outside every timed path.

Formal Mac timing (Apple M4, clang-p2996 d77eff1): deferred (Mac queue), see the status table above.

## 4. Client direct read and buffer lifetime

- `src/client_core.hpp` `SnapshotHolder`: owns the current snapshot `XBuffer`;
  `view()` returns `const ViewSnapshot&` into it. A snapshot changes only in
  `Session::install_pending` (`src/client_core.cpp`): `reload(wire)` verifies
  the new wire into the held buffer in place (`--buffers reuse`; everything
  read from the old content dies there, and a rejected wire leaves the holder
  empty), `replace(XBuffer&&)` destroys the old buffer and takes a new one
  (`fresh`).
- Window client `src/client_gui.cpp`: `install_pending` runs only at the start
  of a frame ("frame start: replace the snapshot only here"); drawing, labels
  and hover read `holder().view().units` and `u.name.view()` directly in the
  same frame; nothing keeps a pointer or `string_view` across frames.
- Presentation data: `DrawItem` (`client_core.hpp`) holds only screen position
  and colour, rebuilt each frame by `build_draw_items`; no id, name, hp or
  behaviour is copied out of the snapshot. Field read and presentation-data
  construction are measured together as `c.read_present_us` (MERGED).
- Evidence at run time: with `--check`, `client_core.cpp` checks every
  `VisibleUnit` record and name byte with `checks::in_buffer` against the live
  buffer (`direct_read` row); the headless client also ran under ASan+UBSan in
  both modes.

## 5. End to end: Linux servers, clients on other platforms (10-27)

Branch `claude/mapdemo-e2e-20261027`, `tools/run_e2e.sh`. A and B: GCC
16.2, C++26 reflection, statically linked so they run on the VM next to the
clients. Each client target ran `--self-check`, `--replay` of one corpus,
the three tiers with `--encoding cycle` (`reuse`) and `--evil`. Two runs:
`full1` (four targets, on the field-list branch 83c21b9) wrote the corpus;
`full2` (six targets, rebased on c8fc851 with the MSVC toolchains and
next_cpp26 431a7ee) replayed that corpus and compared its step wires with
`full1`: 500 / 500 ticks equal in every tier, so the rebase changed no byte.
`full3` and `asan2` repeat this on next_cpp26 5af77d3 with the same result
(the table below is `full3`). Output in the project folder
`slg-demo-20261022/e2e-20261027/` (`mapdemo_e2e_cloud_full{1,2,3}.tgz`,
`mapdemo_e2e_cloud_asan{1,2}.tgz`, `E2E_RESULT_cloud_full{1,2,3}.md`).

| Client target (`full3`) | Runs under | Client build | Self-check | Replay (60 snapshots) | Tiers + evil | Step wires vs first target |
| --- | --- | --- | --- | --- | --- | --- |
| linux-gcc16-cxx26 | native | GCC 16.2, C++26, reflection | 23 / 23 | 60 / 60 same digests | 77 / 77 verdicts (with server self-test) | 3 / 3 equal to `full1` |
| linux-gcc13-cxx20 | native | GCC 13.3, C++20, field lists | 23 / 23 | 60 / 60 | 75 / 75 | 3 / 3 tiers equal |
| windows-mingw | wine | MinGW GCC 16.2, C++20, field lists | 23 / 23 | 60 / 60 | 75 / 75 | 3 / 3 |
| windows-msvc | wine | MSVC 19.44, `/MT`, C++20, field lists | 23 / 23 | 60 / 60 | 75 / 75 | 3 / 3 |
| windows-clang-cl | wine | clang-cl 19.1, MSVC STL, C++20, field lists | 23 / 23 | 60 / 60 | 75 / 75 | 3 / 3 |
| android-arm64 | qemu-aarch64 user mode | NDK r29 Clang 21, API 28, C++20, field lists | 23 / 23 | 60 / 60 | 75 / 75 | 3 / 3 |
| macos (Mac M4, separate run) | native; A/B in colima, Linux arm64 | Apple clang 21, C++20, field lists | 23 / 23 | 60 / 60 | 74 / 74 | 3 / 3 equal to the cloud x86-64 run |
| android (Mac M4, separate run, 10-28) | Android 15 emulator, arm64-v8a; A/B in colima, Linux arm64 | NDK r29 Clang 21, API 28, C++20, field lists | 23 / 23 | 60 / 60 | 74 / 74 | 3 / 3 equal to the cloud x86-64 run |
| ios (Mac M4, separate run, 10-28) | iOS 27.0 Simulator (iPhone 17), arm64; A/B in colima, Linux arm64 | Apple clang 21, iPhoneSimulator 27.1 SDK, C++20, field lists | 23 / 23 | 60 / 60 | 74 / 74 | 3 / 3 equal to the cloud x86-64 run |
| ios-noexc (same run) | same | same, without exceptions | 23 / 23 | 60 / 60 | 75 / 75 | 3 / 3 equal to the cloud x86-64 run |

What this shows:
- Server to server across architectures: A and B built by GCC 16 for Linux
  arm64 (colima on the Mac) sent the same step wires as the cloud x86-64
  build at every one of 500 ticks in all three tiers.
- C++26 reflection servers and C++20 field-list clients agree on the layout:
  the Hello schema hash matches, and per request A and every client log the
  same encoding, bytes sent, wire digest and content digest.
- The same 60 snapshot payloads (20 per encoding) verify on every platform
  to the same wire and content digests as A recorded; every self-check
  refusal case (truncated or bit-flipped wires, wrong schema, malformed or
  oversized zstd frames) is refused on every platform.
- Bytes per snapshot, 10000-unit tier (client side, means): a 1024x768
  viewport is about 15.1 KB as an XOffset wire and 6.4 KB with zstd (0.42); a
  full-scene snapshot of 10000 units is about 321 KB and 123 KB with zstd
  (0.38). A builds its snapshots with exact reservations, so the compact
  wire is about the size of the dense one here.
- Costs on A (cloud VM, p50, 10000-unit tier): saving a full-scene snapshot
  takes 48 us dense and 235 us compact (`save_compact` walks the arena; it
  gains nothing on these tightly built snapshots), zstd level 3 adds about
  3.6 ms (0.17 ms for a viewport). The client's decompression is about
  0.5 ms per full scene (3.8 ms under qemu) and verification about 0.06 ms
  native. zstd therefore pays off on links slower than roughly 40 to 50 MB/s
  (mobile networks are well below that); over loopback it only adds time.
  Wine and qemu timings are not device timings.

Sanitizers: the same run with GCC 16 Debug ASan+UBSan (A, B and a C++26
client; `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`, leak detection
on), both buffer modes: self-check 23 / 23, replay 60 / 60, three tiers x two
modes + evil x two, no sanitizer report (`asan1` 145 / 145 verdicts;
`asan2` on next_cpp26 151 / 151, step wires equal to `full1`).

Android emulator (10-28, next_cpp26 00b5d35): the client built with NDK r29
ran inside the Android 15 emulator (API 35, google_apis arm64-v8a image
with 16 KB pages) on the Mac M4 and reached A in colima through the
emulator's host address 10.0.2.2. It passed self-check, the replay of the cloud corpus
and the three tiers with evil; the library's CTest passed 91 / 91 on the
same emulator and the 15 compile-failure tests passed with the same NDK in
the cloud. Client side, 10000-unit tier, p50 (emulator numbers, not device
numbers): a viewport is 15,072 B as a wire and 6,372 B with zstd, decoded in
43.5 us and verified in 6.0 us; a full scene is 321,288 B and 122,724 B with
zstd, decoded in 463 us and verified in 34 us. Output:
`mobile_sim_android_out.tar.gz` in the project folder (sha256
`dfa319384cc2b82b2152560a4c67f2c2ca0b9b5cd70ee8bafab20614ac17f3ff`), result
table `out-android/android/demo/E2E_RESULT.md`.

Clients without exceptions (10-28): every client build also makes
`mapdemo_client_noexc` from the same sources (`-fno-exceptions`; MSVC and
clang-cl `/EHs-c- /D_HAS_EXCEPTIONS=0`). Its failures are values
(`include/mapdemo/result.hpp`), and snapshots go through `try_load_verified` /
`try_reload_verified` (SC-8, #148). Each run covered the six targets of
`full3`, each with and without exceptions, against the corpus and step wires
of `full1`. The final run `noexc3` (next_cpp26 c928334 with the MinGW fix
#155, and A's close below) passed on all 12 targets: self-check 23 / 23,
replay 60 / 60, the three tiers and evil, and step wires equal at 500 / 500
ticks. `e2e_report.py` found the same self-check outcomes and refusal texts,
and the same evil outcomes, on all 12 targets. Further runs on the same tree:
- ASan+UBSan, both GCC 16 C++26 clients in both buffer modes (`noexc_asan2`):
  151 / 151 and 149 / 149 verdicts, no sanitizer report.
- The window client without exceptions under Xvfb: 10000 units, all three
  encodings, `direct_read` 68 / 68.
- `noexc1` (next_cpp26 579ca44 with the same MinGW fix) also passed on all 12
  targets.

The Linux client without exceptions references none of `__cxa_throw`,
`__cxa_begin_catch` and `__gxx_personality_v0`.

MinGW needed a library fix. With two translation units that include
`xoffset_error.hpp`, MinGW GCC 16 failed to link. `detail::pending_error`
was a namespace-scope `inline thread_local`; #155 makes it function-local.

End of a session on Windows. In `noexc2` (before A's close changed) the
clang-cl client with exceptions failed at the end of the 10000-unit tier:
"recv: Connection reset by peer", and A's last snapshot never logged. A had
closed the connection with one request still unread, so the close sent a
reset. Windows drops received data that the program has not read yet, the
last snapshot and the `Bye` included; Linux keeps it readable. A two-file
probe shows both behaviours (`reset_probe/`: the Linux client reads `BYE`,
the same client under wine gets WSA 10054). A now shuts down its sending side
after the `Bye` and reads each client until the client closes, 2 s at most,
before closing (INTERFACES.md). With that change `noexc3` passed, and so did
30 short runs of the clang-cl client (all client exits 0).

Cost: verification without exceptions is slower. In that build the
library's receive path returns a status from every check (`XOFFSET_VTRY`).
On `noexc1`'s binaries, the two clients replayed the 60-snapshot corpus in
alternation (28 rounds each on Linux, 18 under wine). Per-snapshot medians,
summed, give this ratio without / with exceptions (full scenes, then
viewports):

| Build | Full scenes | Viewports |
| --- | --- | --- |
| GCC 16 | 1.10x | 1.17x |
| GCC 13 | 1.24x | 1.29x |
| MSVC | 1.08x | 1.08x |
| clang-cl | 1.10x | 1.15x |
| MinGW | 1.16x | 1.09x |

That is at most about 10 us more per full scene and under 1 us per viewport.
Output in the project folder `mapdemo-noexc-20261028/`:
`mapdemo_e2e_cloud_noexc{1,2,3}.tgz`, `mapdemo_e2e_cloud_noexc_asan{1,2}.tgz`,
`mapdemo_noexc_ab_replay.tgz`, `mapdemo_noexc_xvfb_shots{,2}.tgz` (the window
runs with their screenshots), `reset_probe/` and the `E2E_RESULT_*.md`
files, with sha256 in `SHA256SUMS`.

The macOS window run first crashed in raylib 5.5's InitWindow (no monitor:
the Mac's screen was asleep); the client now exits 2 with "cannot open a
window" instead (checked without a display and under Xvfb). With the screen
awake (10-28, next_cpp26 00b5d35) the window client ran on the Mac M4 against
A and B in colima: exit 0, three screenshots, `direct_read` 71 / 71. The frame
420 status bar reads tick 270, 474 units visible, a 15,376 B snapshot sent as
6,666 B of zstd, view round trip 17 ms (frame-paced), `reload_verified` 8.0
us, 59 fps. Output: `mobile_sim_macos_window_out.tar.gz` in the project folder
(sha256 `0f71e100757c95f10ed2b62e04cb956763c75ec3cf208ce0a2e4e9af7dc87043`),
screenshots under `out-macos-window/macos/shots/`.

iOS Simulator (10-28): the pack was next_cpp26 af601ea plus #160, which
changes only tests. The clients built with and without exceptions (Apple
clang 21, iPhoneSimulator 27.1 SDK) ran in the iOS 27.0 Simulator (iPhone 17)
on the Mac M4 against A and B in colima. Both passed self-check, the replay of
the cloud corpus, the three tiers and evil, with the same self-check and evil
outcomes, and their step wires equal the cloud x86-64 run at 500 / 500 ticks
in every tier. The library's CTest passed 129 / 129 in the simulator and its
corpus matched the Linux reference. Client side, 10000-unit tier, p50
(simulator numbers, not device numbers): a viewport is 15,072 B as a wire and
6,372 B with zstd, decompressed in 43.6 us and verified in 6.1 us; a full
scene is 321,288 B and 122,724 B with zstd, decompressed in 500 us and
verified in 55 us. The client without exceptions measured 37.9 / 5.2 us and
491 / 56 us in the same run; one sequential run does not resolve the 1.1x to
1.3x verification difference of the interleaved replays above. Output:
`mobile_sim_ios_out.tar.gz` in the project folder (sha256
`daa917de3122e8a95fb3e84a0aa613d297856531f6112e24bc258c458a004c4b`), result
table `out-ios3/ios/demo/E2E_RESULT.md`.

Every planned run of this section has been done. The Android client without
exceptions ran under qemu in the cloud (`noexc3`) but not in the Android
emulator; real phones come later.

## 6. Scope

One arena per batch, per snapshot and for the catalog (kept and refilled in
`reuse`, new per message in `fresh`); zstd is a transport encoding, the
decompressed wire is verified like any other; no `XBufferPool`,
incremental sync, LOD or gameplay; validation never disabled. Structural
validation is not authentication or tamper-proofing. The full-batch round trip
every step is a validation workload, not a production architecture, and these
results do not predict incremental sync, real migration or mobile
performance.
