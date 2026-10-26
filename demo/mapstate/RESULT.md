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

## 5. Scope

One arena per batch, per snapshot and for the catalog (kept and refilled in
`reuse`, new per message in `fresh`); no `XBufferPool`, compression,
incremental sync, LOD or gameplay; validation never disabled. Structural
validation is not authentication or tamper-proofing. The full-batch round trip
every step is a validation workload, not a production architecture, and these
results do not predict incremental sync, real migration or mobile
performance.
