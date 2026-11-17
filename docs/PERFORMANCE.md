# Performance numbers (4.0.0)

This page records, for each release, the numbers measured at the Mac checkpoint, how they were
measured, and their limits. All numbers come from the same Apple M4 and the same game server
workload (SLG); do not use them to infer other machines, other workloads or other adapter styles.
There are no formal timing conclusions on Linux or Windows, and on phones there is only a same-device
comparison on one iPhone (section 4); cloud instruction counts are only used to catch regressions
([PERF_GATE.md](PERF_GATE.md)) and are not evidence of speed. Since 3.5.0 there is also a set of
cloud x86-64 timings (section 6) on a machine that is not quiet; 3.5.0 was report-only, 3.6.0 gates
on point estimates (section 6a, [RELEASING.md](RELEASING.md) section 2a), and since 4.0.0 releases are
judged on a scorecard reproduced by one command (section 6b).

## 1. Method

| Item | Setting |
| --- | --- |
| Machine | Mac mini M4, macOS 26.3, P2996 Clang `d77eff1`, libc++, Release |
| Workload | `bench/slg` (frozen tree, see its `MANIFEST`): A battle server round trip, B cross-server migration, C zone snapshot, D read-only battle report; M scale, `SLG_REUSE=1` (protocol c, buffer reuse) |
| Rivals | zpp_bits, cista, cereal, protobuf, FlatBuffers; the six run in turn in one process, and every cycle checks that all six produce the same result |
| Samples | 12 processes of 41 cycles each; between processes the harness waits for quiet conditions (screen off, WindowServer < 20%, remote < 5%, for 60 seconds) |
| Statistics | Each cell is the geometric mean of XOffset ÷ rival and the process-level one-sided 95% upper bound (Bonferroni k=20, df=11). Smaller is better; only an upper bound ≤ 1.0 counts as "not slower" |
| Relocation tax (G9) | `bench/reloc`: XOffset container ÷ the std container with the same function (set against `flat_set`), 8 ABBA rounds, k=19, upper bound ≤ 1.05 passes |

Verdicts and raw data are in the project files under `perf-gate/mac/<date>-<batch>/VERDICT.md`; each
section below names its batch.

## 2. 3.0.0

The code of `v3.0.0` is identical to `30af216` (only documentation and `experiments/` were removed
afterwards).

### SLG scenarios (G8 checkpoint `3549272`, 2026-10-27)

| Scenario | ÷zpp_bits | ÷cista | ÷cereal | ÷protobuf | ÷FlatBuffers |
| --- | --- | --- | --- | --- | --- |
| A battle server round trip | 0.771 / 0.778 | 0.294 / 0.297 | 0.244 / 0.245 | 0.088 / 0.089 | 0.313 / 0.317 |
| B cross-server migration | 0.260 / 0.261 | 0.274 / 0.275 | 0.138 / 0.139 | 0.071 / 0.071 | 0.175 / 0.176 |
| C zone snapshot | 0.361 / 0.365 | 0.229 / 0.232 | 0.108 / 0.109 | 0.064 / 0.064 | 0.239 / 0.242 |
| D read-only battle report | **1.570 / 1.589** | 0.351 / 0.354 | 0.341 / 0.345 | 0.518 / 0.524 | **1.355 / 1.383** |

Geometric mean / upper bound. `30af216` showed no regression against `3549272` at CKPT3 (A 0.979,
B 0.970, C 0.990, upper bound 1.009; formal harness upper bound 1.0074), so this table is
conservative for 3.0.0.

Stages still behind (geometric mean, report only, no multiple-comparison correction):

| Stage | XOffset µs | Rival µs | Ratio |
| --- | ---: | ---: | ---: |
| A.apply | 92.7 | zpp_bits 48.6 / cista 45.6 | 2.03 / 2.18 |
| A.simulate | 116.4 | zpp_bits 87.2 | 1.32 |
| B.writeback | 21.9 | zpp_bits 10.2 / cista 3.8 | 2.11 / 5.58 |
| C.ticks | 134.7 | cista 118.6 | 1.13 |
| D.read | 99.3 | zpp_bits 63.2 | 1.57 |

All encode/decode stages lead (A.req_decode ÷zpp_bits 0.36, B.in_decode 0.137, C.restore 0.125).
The remaining A and B gaps are in the stages that apply business changes to decoded data; the
reasons are in [USER_GUIDE.md](USER_GUIDE.md) 4.3 "Design conventions". The D gap is in in-place
reading (D.read).

### Relocation tax (G9R, `30af216`, 2026-10-27): 14 of 19 items pass

| Item (÷std) | Geometric mean / upper bound | Verdict |
| --- | --- | --- |
| vec.push_back | 1.29 / 1.30 | Fail |
| vec.mid_insert | 1.10 / 1.11 | Fail |
| vec.mid_erase | 1.06 / 1.07 | Fail |
| set.insert.fresh (÷flat_set) | 1.11 / 1.12 | Fail |
| set.erase.fresh (÷flat_set) | 1.12 / 1.13 | Fail |
| vec.push_back_grow | 0.79 | Pass |
| hero.mid_erase.ord / hero.mid_insert.ord | 0.75 / 0.80 | Pass |
| str.assign_short.view / str.assign_long / str.append | 0.36 / 0.86 / 0.46 | Pass |
| reset | 0.21 | Pass |

The other items (fresh find on map and set, vec.iterate, nested.build.ord) all pass. `hero.*.plain`
(the form without the ordinary declaration) is about 4.6x, report only.

## 3. 3.1.0

INT4 checkpoint: RC head `e913d22` against `v3.0.0` (`e383aef`), 2026-10-27, judged pass.
VERDICT: `perf-gate/mac/2026-10-27-int4-e913d22/VERDICT.md`.

### SLG scenarios

This SLG batch ran only four formats (XOffset, cereal, protobuf, FlatBuffers), 6 head processes,
statistics with k=12 (section 2 used 12 processes and k=20), so the upper bounds cannot be compared
cell by cell with section 2. zpp_bits and cista were not measured; the G8 conclusion for them is an
inference (3.0.0 passed, and A/B/C did not regress against 3.0.0), not a measurement.

| Scenario | ÷cereal | ÷protobuf | ÷FlatBuffers |
| --- | --- | --- | --- |
| A battle server round trip | 0.240 / 0.243 | 0.088 / 0.090 | 0.312 / 0.318 |
| B cross-server migration | 0.126 / 0.127 | 0.065 / 0.065 | 0.161 / 0.164 |
| C zone snapshot | 0.101 / 0.102 | 0.060 / 0.061 | 0.227 / 0.232 |
| D read-only battle report | 0.284 / 0.290 | 0.433 / 0.443 | **1.117 / 1.156** |

### Against 3.0.0

head÷anchor measured with alternating head and anchor processes (ABBA×3), geometric mean and 95%
interval:

| Item | 3.1.0 ÷ 3.0.0 |
| --- | --- |
| SLG A | 0.991 [0.978, 1.005] |
| SLG B | 0.951 [0.942, 0.961] |
| SLG C | 0.950 [0.942, 0.959] |
| SLG D | 0.846 [0.832, 0.859] |
| Formal harness read_sum / mutate / emit_after (two batches pooled) | 0.950 / 0.970 / 0.869, largest upper bound 0.981 |
| Formal harness emit_after ÷cereal (two batches, medians) | 3.1.0 0.164 / 0.166, 3.0.0 in the same batches 0.188 / 0.194; 3.1.0 upper bounds 0.168 / 0.170 |

### Relocation tax (G9R3, `e383aef` against `e913d22` in one batch): 17 of 19 items pass

3.0.0 in the same batch was 15/19. No item is slower than 3.0.0 because of code.

| Item (÷std) | 3.0.0 | 3.1.0 geometric mean / upper bound | Verdict |
| --- | --- | --- | --- |
| vec.push_back | 1.287 | 1.128 / 1.136 | Fail (every stored element recomputes the data pointer from `data_delta`) |
| set.erase.fresh (÷flat_set) | 1.130 | 1.092 / 1.102 | Fail (bounds re-check, empty check and slot clearing in the erase step) |
| vec.mid_insert | 1.092 | 0.954 | Pass (newly passing) |
| set.insert.fresh (÷flat_set) | 1.111 | 0.995 | Pass (newly passing) |

The other 15 items pass in both versions.

### Wire bytes (cloud GCC 16, byte-identical to Mac)

SLG M scale, average bytes per message in cycle 20 (the measure used when the O1 threshold was set):

| Message | 3.0.0 bytes | 3.1.0 bytes | ÷3.0.0 |
| --- | ---: | ---: | ---: |
| A.req | 1508 | 1253 | 0.831 |
| A.res | 1743 | 1488 | 0.854 |
| B | 15657 | 13667 | 0.873 |
| D | 309 | 197 | 0.638 |

Last cycle of the default two-cycle run: A.req 1422 → 1166 (0.820), A.res 2614 → 2358 (0.902),
B 15119 → 12964 (0.857), D 824 → 712 (0.864). D is 112 bytes smaller per message under both
measures; the ratios differ because the messages differ in size (309 versus 824 bytes).

Formal harness (M): final-state wire 12,627,496 → 11,182,856 bytes (0.886), input wire
9,435,904 → 8,649,368 bytes (0.917).

## 4. 3.2.0

CKPT5 checkpoint: `next_cpp26` `70a07a05` against `v3.1.0` (`9ef5ce75`), 2026-10-28. The release head
adds class-0 PRs after it (demo, tools, docs) and class-S #183 (library headers change only under the
new optional macro `XOFFSET_OWNER_MAP_SHARED`; the default build has the same Ir and machine code).
VERDICT: `perf-gate/mac/2026-10-28-ckpt5-70a07a0/VERDICT.md`. The CKPT5 L2 SLG was judged fail by the
rules because two outlier processes made the interval too wide; the same binaries re-ran L2 (CKPT5b,
`perf-gate/mac/2026-10-28-ckpt5b-70a07a0/VERDICT.md`) with all point estimates ≤ 1.003, this time with
the outlier on the anchor side, and C's interval still wider than 1.03. Under the merge rule written
before the run (`perf-gate/mac/src-ckpt5c/POOL_RULE.md`), the same binaries ran one more SLG batch
(CKPT5c, `perf-gate/mac/2026-10-28-ckpt5c-70a07a0/VERDICT.md`), and all processes of the three
batches were pooled with no outliers removed: A / B / C 0.9988 / 1.0005 / 1.0051, C upper bound
1.0216, slightly above 1.02, which the rule judges as "stop and let susu decide"; susu decided to
release as planned (2026-10-29). C in the three batches alone was 1.014, 0.995 and 1.007, none
significantly above 1; the GATED C of the same binaries was 1.004 (upper bound 1.014).

3.2.1 is a patch release: against `v3.2.0` it changes only the iOS test host, `demo/mesh` and
documentation; the library headers and sources are byte-identical, no separate checkpoint was run,
and the numbers in this section apply unchanged.

### Against 3.1.0

| Item | 3.2.0 ÷ 3.1.0 |
| --- | --- |
| SLG A / B / C (six-format GATED measure, 12+12 processes) | 0.994 [0.986, 1.002] / 1.003 [0.992, 1.014] / 1.004 [0.994, 1.014] |
| SLG A / B / C (four-format L2 measure, CKPT5, CKPT5b, CKPT5c pooled, 18+18 processes) | 0.999 / 1.001 / 1.005 [0.989, 1.022], largest upper bound 1.0216 |
| Formal harness read_sum / mutate / emit_after (two batches pooled) | 1.002 / 1.005 / 1.001, largest upper bound 1.013 |
| Formal harness load (two-sided) | 0.944 [0.909, 0.980] |

### D against FlatBuffers (GATE_D) and G8

- D÷FlatBuffers 0.933, upper bound 0.947 (3.1.0 was 1.117 / 1.156): reaches ≤ 1.0. D.read
  82.4 → 67.5 µs, mainly from #147 (root pointer cache for D reloads) and #153.
- G8: ÷zpp_bits upper bounds A 0.771, B 0.255, C 0.378; ÷cista upper bounds 0.295, 0.254, 0.220;
  passes.

### Relocation tax (G9R3, `v3.1.0` against `70a07a05` in one batch): 17 of 19 items pass

The two failing items are the same as in 3.1.0, and 3.2.0 is slightly better on both: vec.push_back
1.143 → 1.118, set.erase.fresh 1.087 → 1.085. `map.erase.fresh` takes 2 more instructions because
container iterators became classes (a public API change in rc.1); by CKPT4's machine-code reading the
effect is ≤ 0.5%. The other items are equal to or better than 3.1.0 in the aligned build
(`-falign-functions=64`); per-item machine-code conclusions are in VERDICT section 4.

### Phone (iPhone 15 Pro Max, against FlatBuffers 25.12.19 on the same device)

SLG M scale, xoffset÷FlatBuffers: A 0.305, B 0.179, C 0.252, D 0.858. Compared only with FlatBuffers
on the same phone, not with the Mac numbers; Android phones have no timing. Method and wire bytes are
in [DEVICE_TESTING.md](DEVICE_TESTING.md) section 9.

## 5. 3.3.0

Release checkpoint REL330: `next_cpp26` `059b6a5f` (candidate C) against `v3.2.1` (`23c6e206`, library
same as the 3.2.0 checkpoint), 2026-11-04, also serving as a stage review (bisection points for #213,
#191, #219 and #214 were prepared; the verdict passed and they were not needed). All points were built
with C's `bench/slg` (frozen tree `23e86624`). The Mac was on macOS 27.2 at this point. The verdict is
in the project files under `perf-gate/mac/2026-11-04-rel330-059b6a5/VERDICT.md` (raw data in
`087-rel330` on the `mac-results` branch).

### Against 3.2.1

| Item | 3.3.0 ÷ 3.2.1 |
| --- | --- |
| SLG A / B / C (four-format L2 measure, 6+6 processes) | 0.959 [0.941, 0.977] / 0.922 [0.908, 0.937] / 0.959 [0.935, 0.984] |
| SLG D (report only) | 1.007 [0.976, 1.039] |
| SLG A / B / C (six-format GATED measure, 12+12 processes), largest upper bound | 0.975 / 0.931 / 0.981, 0.9813 |
| Formal harness read_sum / mutate / emit_after (two batches pooled) | 1.001 / 0.974 / 0.977, largest upper bound 1.010 |

A stages listed separately (L2 SLG, 3.3.0 ÷ 3.2.1, µs per cycle): A.req_decode 1.014 [1.001, 1.027]
(+0.8 µs), A.res_decode 0.967, A.apply 0.928, A.req_encode 0.997. A.req_decode is suspected code
placement: the gate when #191 merged measured 1.024, and the aligned build (`-falign-functions=64`)
measures 1.005 [0.998, 1.013]. In the demo scenario (report only) sending got faster and the
receiver's apply slower: UP.send 0.864, UP.apply 1.053, UC.apply 1.073, A.apply 1.038; send + recv +
apply per delivery totals UP +2.0%, UC +1.1%; in the aligned build A.apply is 0.971, also suspected
placement.

### D against FlatBuffers (GATE_D) and G8

- D÷FlatBuffers 0.949, upper bound 0.960 (3.2.1 in the same batch 0.915 / 0.939): still ≤ 1.0.
  D.read 67.8 → 68.9 µs.
- G8: ÷zpp_bits upper bounds A 0.778, B 0.506, C 0.787; ÷cista upper bounds 0.289, 0.234, 0.223;
  passes.

### Relocation tax (G9R3, `v3.2.1` against C in one batch): 17 of 19 items pass

The two failing items are the same as in 3.2.x: vec.push_back and set.erase.fresh
(`perf-gate/mac/2026-11-04-rel330-059b6a5/g9r/`). A few XOffset items are slower than 3.2.1 (G9 still
passes): map.insert.reused 1.023 and set.insert.reused 1.022, from the fast path #214 added for
ascending appends (inserting in random order adds one comparison with the last element per insert,
3–5% more instructions in the cloud, while ascending inserts get faster); map.find.fresh and
map.find.reused 1.028, the same in the default and aligned builds, with no increase in instruction
count; the cause was not found, report only (`perf-gate/cloud/g9r-map-ir-20261105/REPORT.md`).

### Clients

Apple clang 21.0.0, C++20: macOS arm64 and the iOS simulator (iPhone 17, iOS 27.0) pass full CTest
148/148 (including 24 no-exception programs), read the 24 reference corpora written by Linux GCC 16,
and write identical bytes (`perf-gate/mac/2026-11-04-apple330-059b6a5/`). Real phones were not
re-run; the iPhone numbers in section 4 are from 3.2.x.

## 6. 3.5.0: cloud x86-64 (report only)

Servers are x86-64 Linux, so 3.5 added a set of timings with the same measures on cloud x86 (3.5 W1,
2026-11-10). The cloud machine is not quiet and other jobs may run at the same time, so results are
report-only and are not release verdicts; the release verdict is still the two cloud Ir gates
([RELEASING.md](RELEASING.md) section 2).

| Item | Setting |
| --- | --- |
| Machine | Cloud Intel Xeon 2.1 GHz, 4 cores, Linux, GCC 16 `-O3` (gcc:16 container), libstdc++ |
| Workload | `bench/slg`, M scale, the same scenarios and adapter styles as the Mac SLG scorecard |
| Samples | One process per format (not taking turns in one process), 8 batches × 21 cycles, interleaved, medians, pooled by `pool_v7.py` / `pool_v8.py` |
| Measure | The 3.4 trust model: XOffset uses the `xoffset_trusted` row, rivals `cista_unchecked`, `flatbuffers_noverify` and zpp_bits; compression is not counted in time; bytes are per-message zstd-3 |

Raw data and summaries are in the project files under `x86-scorecard-20261110/` (`pooled_v7.md`,
`X86_LOSS_LIST.md`, `w4-2bd8873/`).

### Main columns (3.4.0 library, `e71d4ed3`; GM / 95% upper bound, XOffset ÷ rival, < 1 means XOffset is faster)

| Main column | zpp_bits | cista_unchecked | flatbuffers_noverify |
| --- | --- | --- | --- |
| Server, full login | 0.411 / 0.435 | 0.340 / 0.389 | 0.193 / 0.208 |
| Server, browse viewport | 0.737 / 0.815 | 0.561 / 0.612 | 0.361 / 0.384 |
| Server, own delta | 0.699 / 0.777 | 0.486 / 0.523 | 0.500 / 0.548 |
| Server, idle peace (UP.send) | 0.977 / 1.073 | 0.656 / 0.719 | 0.478 / 0.510 |
| Server, idle conflict | 0.584 / 0.658 | 0.480 / 0.562 | 0.348 / 0.386 |
| Server, D3 peace / conflict | 0.899 / 0.956; 0.252 / 0.282 | 0.604; 0.207 | 0.440; 0.150 |
| Client, idle peace (UPW.recv) | 0.991 / 1.120 | 1.219 / 1.363 | 0.883 / 0.922 |
| Client, idle conflict (UCW.recv) | 0.404 / 0.480 | 1.028 / 1.178 | 0.302 / 0.344 |
| Client, D3 peace (DPTW.recv) | 0.819 / 0.906 | 1.007 / 1.119 | 0.730 / 0.823 |
| Client, browse viewport | 0.313 / 0.332 | 0.803 / 0.858 | 0.195 / 0.206 |
| Class 1, both ends combined | 0.225–0.497 | 0.424–0.509 | 0.162–0.353 |
| Egress bytes (zstd-3) | 1.107–1.329 | 1.004–1.135 | 0.938–0.987 |

### After the 3.5 changes (`2bd8873` = one tree with #248 + #249 + #247, against `e71d4ed3` on the same machine, 4 interleaved batches × 21 cycles)

| Key (µs per message) | 3.4.0 library | 3.5 | cista_unchecked | zpp_bits | 3.5 ÷ rival |
| --- | ---: | ---: | ---: | ---: | --- |
| UP.send (server idle peace) | 0.394 | 0.378 | 0.612 | 0.414 | 0.91 ÷ zpp_bits |
| UCW.recv (client conflict hot view) | 3.346 | 3.011 | 3.414 | 9.209 | 0.88 ÷ cista |
| USW.recv (client stress hot view) | 7.605 | 6.808 | 7.737 | 14.245 | 0.88 ÷ cista |
| UPW.recv (client peace hot view) | 1.055 | 1.090 | 0.889 | 1.056 | 1.23 ÷ cista |
| UL.recv (server receiving uplink, bounds tier) | – | 0.041 | 0.045 | 0.045 | 0.91 ÷ zpp_bits |
| UL.send (client sending uplink) | – | 0.251 | 0.230 | 0.181 | 1.39 ÷ zpp_bits |

- The UP.send upper bound did not clear on 3.4.0 (1.07); after #248 the point estimate is 0.91.
- Client hot views: since #247 the trusted tier reads in place through `XReceiver<from_server>::view`
  instead of copying the whole message into a receive buffer, and the conflict and stress items now
  beat cista_unchecked. The peace hot view is still slower: the same tree is 0.94x on the P2996 build
  and 1.11x on the GCC 16 build; in GCC's callgrind XOffset has fewer instructions, D1/LL misses and
  branch mispredictions, so the difference is in what valgrind does not simulate; listed as a
  GCC-specific item to look at in 3.6.
- Uplink columns (new in 3.5, report only): server bounds-tier receive 0.92x zpp_bits; full
  validation ULF.recv 1.44x; client send 1.40x zpp_bits.
- Non-product paths (report only per the 11-10 decision): per-client native deltas NP 7.1x and NC 2.0x
  zpp_bits, class 1 native storage SN 1.88x zpp_bits.
- The 3.4.0 SLG scorecard on the Mac is in the project files under `slg340-20261106/SLG340_REPORT.md`
  (no regression against SLGV9c).

### 3.6: small-message view prefetch, thresholds for the x86 and uplink columns (2026-11-11)

Another cloud machine (Xeon 2.3 GHz, model 173, 4 cores), GCC 16 `-O3`, `tools/perf_gate/x86_columns.sh`
with 10 interleaved batches, paired by (batch, cycle) in `x86_columns.py`. Baseline `d8c5018d`
(including #255), head = baseline + small-message prefetch in `XReceiver::view`. Two rounds of 10
batches each:

| Cell (xoffset_trusted ÷ rival) | Baseline | Head |
| --- | --- | --- |
| UPW.recv ÷ cista_unchecked | 1.151 / 1.151 | 1.022 / 1.000 |
| UPW.recv ÷ zpp_bits | 0.859 / 0.919 | 0.763 / 0.799 |
| UPW.recv head ÷ baseline | | 0.888 [0.85, 0.93] / 0.869 [0.83, 0.91] |
| UCW.recv, USW.recv head ÷ baseline | | 0.97–0.98, 1.00 |
| UP.send ÷ zpp_bits | 0.970 | 0.952 / 0.977 |
| UL.recv ÷ zpp_bits (bounds tier) | 0.88–0.91 | 0.92–0.94 |
| UL.send ÷ zpp_bits | 1.66–1.70 | 1.53–1.55 |

- The peace hot view gap is not in instruction count: callgrind splits UPW.recv into a view part and
  an apply part. XOffset is faster in the view part (no copy, timed 312 versus 408 ns), and in the
  apply part XOffset executes fewer instructions (577 versus 613 per message) but takes 5.3 more D1
  read misses per message, all on the first read of wire bytes: cista `memcpy`s the whole range on
  receive, fetching those lines in parallel at once; XOffset reads in place, so the misses land in the
  apply loop one after another. `view()` now requests every line of wire messages ≤ 1 KiB first (read
  prefetch), and the apply part goes 407 → 350 ns. Larger messages are left to the hardware's
  sequential prefetcher (prefetching the whole range made the 14 KB USW 12% slower).
- Against cista_unchecked it is now within cloud A/A noise (two builds of the same code over 10
  batches differ by 0.98–1.04 in geometric mean).
- #255 (1 MiB initial reservation) has no effect on these cells: the 3.5 library `7a0d3d96` ÷
  `d8c5018d` is UPW 0.996, UCW 1.033, USW 1.037.
- str.append (a 3.6 watch item from STAGE350, Mac aligned build 1.035): the `reloc_bench` str.append of
  v3.4.0 and v3.5.0 has identical Ir, Dr, Dw and D1 misses on GCC 16 and P2996; it is code placement
  on the Mac, and the code is not changed.

## 6a. 3.6.0: cloud x86-64 scorecard columns (gated) and memory

Candidate 7d16b44 (the release commit's library code is identical), cloud Xeon 2.3 GHz, GCC 16 `-O3`
building `bench/slg` (zpp_bits v4.7.6, cista 015b94e), M scale, native off, one process per format per
batch, 20 interleaved batches, pinned to one core (`tools/perf_gate/x86_columns.sh`). Ratios are
xoffset_trusted ÷ rival, geometric mean across batches; the verdict uses the point estimate, upper
bounds are report only.

| Column | Cell | ÷ zpp_bits | ÷ cista_unchecked | Verdict |
| --- | --- | ---: | ---: | --- |
| Server send | L / EB / LD / UC | 0.32 / 0.64 / 0.71 / 0.55 | 0.25 / 0.46 / 0.50 / 0.42 | Pass |
| Server send | UP.send | 1.028 | 0.665 | Warning (1.015 against v3.5.0, small-message fixed cost, 4.0 R5) |
| Client hot view | EBW / LDW / UCW / USW | 0.33 / 0.57 / 0.40 / 0.42 | 0.81 / 0.69 / 0.76 / 0.75 | Pass |
| Client hot view | UPW.recv | 0.83 | 1.007 | Warning (within A/A noise) |
| Uplink | UL.recv / UL.send | 0.98 / 1.35 | 0.96 / 1.08 | Pass (UL.send limit 1.80) |

D3 (shared deltas, M scale, native on, 10 batches): XOffset `D*.send` per delivery ÷ the rival's
`U*.send`, peace 0.80, conflict 0.17 (zpp_bits); client DPTW.recv ÷ cista_unchecked 0.94. S scale (64
clients) peace 1.85 and conflict 1.30 are report only by susu's decision.

Memory (SLG demo M, application level, one process per format, `slg_bench --mode proc`):
xoffset_trusted process RSS 663.5 MiB, 0.98x the most frugal rival (protobuf) (3.5 was 1.20x); page
tables 2.89 MiB, 0.44% of RSS (threshold ≤ 1%). Raw data: `release-3.6.0/x86-7d16b44/`,
`mem-gate-20261111/` (project files).

## 6b. 4.0.0: pre-release scorecard and re-judging borderline D3 cells

4e28a20a, whose library and bench are identical to release candidate cb3e04b2, run with
`tools/perf_gate/repro_scorecard.sh` ([RELEASING.md](RELEASING.md) section 2b): cloud Xeon 2.1 GHz,
4 cores, pinned to core 2, GCC 16 `-O3`, M scale, 8 batches × 21 cycles. Ratios are xoffset_trusted ÷
the strongest rival (zpp_bits, cista_unchecked, plus zpp_bits's shared-encoding approach for the D
cells); win < 0.95, tie 0.95–1.05, loss > 1.05.

**19 main cells: 14 wins, 5 ties, 0 losses.** The five ties: UP.send 1.018, UPW.recv 1.025, UL.recv
0.968, and DP and DC, re-judged below. UL.send 1.33 is report only.

D3 main cells landing in 1.02–1.08 are re-judged under susu's rule of 2026-11-15 (RELEASING.md
section 2c): the whole scorecard runs once under each of three code layouts, and the median is taken.

| Cell | Default layout | `-falign-functions=16` | `=32` | `=64` | Median | Verdict |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| DP (shared deltas, peace) | 1.041 | 1.031 | 0.965 | 1.016 | 1.016 | Tie |
| DC (shared deltas, conflict) | 1.024 | 0.977 | 0.967 | 1.014 | 0.977 | Tie |
| DPTW.recv | 0.861 | 0.854 | 0.869 | 0.821 | (not triggered) | Win |
| DCTW.recv | 0.763 | 0.777 | 0.744 | 0.756 | (not triggered) | Win |

DP moves between 0.97 and 1.04 with code layout: the library's publisher executes no more instructions
than the hand-written version (d_send Ir 1.02x, the rest identical); the difference comes from
instruction cache set conflicts across the whole binary. Raw data: `release-4.0.0/scorecard-default/`,
`release-4.0.0/rejudge/` (project files).

## 7. Limits

- Formal timing comes from one M4 only. On other arm64 cores (different store forwarding and branch
  prediction) the relative speeds may differ; for x86-64 there are only the non-quiet cloud numbers
  of section 6; the cloud GCC 16 + libstdc++ relocation tax numbers differ a lot from the Mac's
  (libstdc++'s `flat_set`/`flat_map` differ from libc++'s) and are only used for diagnosis.
- SLG is one workload with one set of adapter styles (each format's adapter in `bench/slg`); a
  different style changes the numbers.
- Only buffer reuse (protocol c) was measured; numbers with a new buffer per message are not a
  conclusion.
- The upper bounds address the "XOffset is not slower than the rival" side; a cell with a ratio far
  below 1 only says XOffset leads on this workload, not that the size of the lead holds elsewhere.
