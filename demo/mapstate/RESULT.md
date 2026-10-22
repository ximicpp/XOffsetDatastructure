# Map state demo: acceptance record

Acceptance record required by section 9 of the demo requirements
(`slg-demo-20261022/DEMO_REQUIREMENTS_20261022.md`): whether every automatic
check passed, the allocation / free / cross-load reuse evidence, the cost and
bytes of the 1 / 100 / 10000-unit tiers, and where the client reads the
verified snapshot directly and how long each buffer lives.

| Run | Where | Status |
| --- | --- | --- |
| Functional + smoke numbers | cloud x86-64 VM, GCC 16.2 (Docker `gcc:16`), Release | all 64 verdicts pass (commit `ac67703`) |
| Sanitizers | same, Debug, ASan+UBSan: self-test, 100-unit tier, evil | all 26 verdicts pass (commit `ac67703`) |
| CI | job `mapdemo-gcc16` (both of the above on every PR to next_cpp26) | CI_STATUS |
| Formal timing + window client | Apple M4 Mac, clang-p2996 d77eff1, Release | MAC_STATUS |

Cloud numbers are smoke numbers on a shared VM: they show the shape of the
costs, not formal timings. Every timing is a process-local steady-clock
interval; percentiles are nearest rank (`rank = ceil(p/100 * n)`), both in the
processes' `*_metrics.csv` and in the per-category rows of `summarize.py`.

## 1. Automatic checks (requirements section 7)

| Requirement | How it is checked | Result (cloud GCC 16) |
| --- | --- | --- |
| Unit state round trip | A after every step: N units, ids 0..N-1 in order, tick = previous + 1, finite positions inside the scene, path 4..8 points inside the scene, cursor < size (`a_checks.csv` `batch_roundtrip`; `a_steps.csv` `check_ok`) | pass, 500 / 500 steps in each tier |
| Modification after receive | A advances a std reference model with the same rule template and compares B's batch field by field, floats bitwise (`reference_model`); coverage of long <-> short names and path grow -> shrink -> grow (`modification_coverage`) | pass in each tier; 10000 units: 1300 to long, 1200 to short, 1300 grow, 1200 shrink, grow-shrink-grow seen |
| Real allocation, free and reuse | `mapdemo_selftest` rounds 1..3 (section 2 below) | pass |
| Independent output | self-test: change the source after saving, reload the saved copy | pass (saved copy keeps hp=90, name U00007, 1 path point) |
| View snapshot | A compares every snapshot with the reference model filtered by the same scene and rect (id set, public fields, no duplicates; empty rect -> empty array) (`view_snapshot`, `snapshot_paths`); `summarize.py` matches A's and the client's per-request digest | pass; 10000 units: 250 / 250 digests equal, 25 empty snapshots with 0 units, 25 full-scene with 10000 |
| Client in-place read | every unit record and name byte the client reads lies inside the live snapshot buffer (`client_checks.csv` `direct_read`); no intermediate unit collection (section 4 below); ASan+UBSan run | pass; 10000 units: 343,584 records and names checked in buffer |
| Failure inputs | self-test: truncated (-1 byte, header only, empty), wrong schema, `used_bytes` over the cap, forged `used_bytes` at each of the four receive points; frame headers over the type limit rejected before any payload allocation (largest allocation <= 121 B). Online `--evil` against a running A: truncated `ViewRequest`, snapshot wire sent as a request, 1 GiB declared length | pass; A logged three `rejected:` lines, answered the following normal request, and finished its 200 steps |

Coverage per tier (`modification_coverage` detail): the event rule touches
units with `id % 100 == 0` every 20 ticks, so 1 and 100 units both modify unit
0 25 times (13 to long, 12 to short); grow -> shrink -> grow first completes at
tick 180, so runs shorter than about 180 steps cannot pass this check.

## 2. Allocation / free / reuse across save + load

`mapdemo_selftest` (`src/selftest.cpp`), public APIs only
(`for_each_free_block` offsets relative to the arena, `arena()->used_end`):

```
reuse_setup    source used_end=520 free_count=2 free_bytes=56 holes=[360+32)[392+24)
reuse_round_1  wire=632B carried=[360+32)[392+24) name_block=392 in_hole=[392+24) path_block=360 in_hole=[360+32) used_end=520->520 free_count=2->0
reuse_round_2  (same)
reuse_round_3  (same)
```

Each round saves the batch, loads the wire into a different buffer, checks the
carried free list, then adds a unit whose first out-of-line name (24 B block)
and path (32 B block) land exactly in the carried holes while `used_end` does
not grow; the unit then leaves (`pop_back`), freeing both blocks again for the
next round. Offsets are relative to the arena, never absolute addresses.
Logical shrinking (smaller size, shorter string, flat memory) is not counted.

## 3. Three tiers (cloud GCC 16 smoke, 500 steps, seed 1; mean / p95)

| | 1 unit | 100 units | 10000 units |
| --- | ---: | ---: | ---: |
| actual ticks per s (target 20) | 20.04 | 20.04 | 20.04 |
| batch wire A->B / B->A, bytes (mean) | 356 / 356 | 9,860 / 9,860 | 967,103 / 967,117 |
| A<->B round trip, us | 372 / 484 | 394 / 527 | 2,791 / 3,690 |
| A save_verified, us | 5.6 / 6.6 | 8.3 / 10.8 | 145 / 198 |
| A load_verified of B's result, us | 62 / 90 | 70 / 96 | 1,001 / 1,305 |
| A destroy old batch, us | 20 / 33 | 21 / 38 | 91 / 136 |
| B load_verified, us | 66 / 91 | 80 / 105 | 982 / 1,263 |
| B step in place, us | 1.0 / 5.6 | 1.8 / 6.4 | 97 / 161 |
| B save_verified, us | 0.9 / 1.5 | 1.9 / 2.8 | 551 / 683 |
| B destroy, us | 16 / 27 | 20 / 33 | 156 / 236 |
| full-scene snapshot: units / wire bytes | 1 / 252 | 100 / 4,212 | 10,000 / 401,352 |
| full-scene view round trip, us | 247 / 409 | 273 / 499 | 1,079 / 1,293 |
| full-scene A build, us | 15 / 22 | 21 / 60 | 417 / 550 |
| full-scene client load_verified, us | 41 / 67 | 39 / 57 | 312 / 443 |
| full-scene client read + present (merged), us | 0.2 / 0.5 | 0.9 / 1.5 | 43 / 63 |
| 1024x768 view: units (mean) / wire bytes (mean) | 0.04 / 202 | 4.4 / 378 | 468 / 18,968 |
| 1024x768 view round trip, us | 265 / 443 | 299 / 500 | 569 / 725 |
| empty view: units / wire bytes | 0 / 200 | 0 / 200 | 0 / 200 |
| empty view round trip, us | 275 / 499 | 272 / 414 | 625 / 812 |

Each tier sampled 200 normal, 25 full-scene and 25 empty requests (the 1-unit
tier 249 in all; the 1024x768 rect usually misses its single unit, and those
zero-visible snapshots are counted as such). Full tables, including every
metric of INTERFACES.md section 6 and A's per-category filter / build / save /
destroy, come from `tools/summarize.py` (`RESULT_TABLES.md`).

Observations (cloud, smoke): for 1 and 100 units the cost is fixed per-buffer
overhead, not bytes: a 356-byte batch takes 62 us to `load_verified` and 20 us
to destroy, and a 176-byte `ViewRequest` 17 us (p50) to load, while saving the
same batch takes 1-6 us. Per-unit work only shows at 10000 units. A's checks
(reference model, `a.check_us`: 1.3 ms mean at 10000 units) run outside every
timed path.

MAC_TABLE

## 4. Client direct read and buffer lifetime

- `src/client_core.hpp` `SnapshotHolder`: owns the current snapshot `XBuffer`;
  `view()` returns `const ViewSnapshot&` into it; `replace(XBuffer&&)`
  (`src/client_core.cpp`) destroys the old buffer and is the only place a
  snapshot changes.
- Window client `src/client_gui.cpp`: `replace` runs only at the start of a
  frame ("frame start: replace the snapshot only here"); drawing, labels and
  hover read `holder().view().units` and `u.name.view()` directly in the same
  frame; nothing keeps a pointer or `string_view` across frames.
- Presentation data: `DrawItem` (`client_core.hpp`) holds only screen position
  and colour, rebuilt each frame by `build_draw_items`; no id, name, hp or
  behaviour is copied out of the snapshot. Field read and presentation-data
  construction are measured together as `c.read_present_us` (MERGED).
- Evidence at run time: with `--check`, `client_core.cpp` checks every
  `VisibleUnit` record and name byte with `checks::in_buffer` against the live
  buffer (`direct_read` row); the headless client also ran under ASan+UBSan.

## 5. Scope

One arena per batch, per snapshot and for the catalog; no pools, compression,
incremental sync, LOD or gameplay; validation never disabled. Structural
validation is not authentication or tamper-proofing. The full-batch round trip
every step is a validation workload, not a production architecture, and these
results do not predict incremental sync, real migration or mobile
performance.
