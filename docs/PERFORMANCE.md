# Performance numbers (4.3.0)

Current published numbers, how they were measured, and their limits. Ratios are XOffset ÷ rival:
**below 1 means XOffset is faster**. Compression is never timed; sizes are after zstd level 3. Older
per-release tables (3.0.0 to 3.6.0) are in the Git history of this file.

| Comparison | Where |
| --- | --- |
| CppCon 2024 `Character`: send + receive, read/write loop, modified buffers | [bench/cppcon2024](../bench/cppcon2024/README.md); summary in the [README](../README.md#performance) |
| SLG game-server scorecard (release verdict) | Section 1 below |
| Where the lead holds: message size, receiver checks, churned buffers, five rivals | Section 2 |
| Memory, relocation tax, phone | Sections 3 and 4 |
| Instruction-count regression gate (not a speed claim) | [PERF_GATE.md](PERF_GATE.md) |

## 1. SLG scorecard (4.3.0)

`bench/slg` (frozen tree, see its `MANIFEST`) models a strategy game server: whole state on login,
per-client deltas, a shared map viewport, client orders going up, and players moving between servers.
Each cell times only data writing, sending and receiving.

| Item | Setting |
| --- | --- |
| Host | Cloud Intel Xeon @ 2.10 GHz, 4 cores, pinned to CPU 2, GCC 16.2 `-O3`, libstdc++; quiet machine (load average ~1.0) |
| Tree | `c210e09e` (release candidate; the release commit changes only the version and documentation) |
| Command | `tools/perf_gate/repro_scorecard.sh` (`ZSTD=1`, [RELEASING.md](RELEASING.md) section 2b) |
| Samples | M scale, one process per format, 8 interleaved batches × 21 cycles, seed 20261021, geometric mean over batches |
| Trust model | XOffset `xoffset_trusted` row; rivals zpp_bits (`v4.7.6`) and `cista_unchecked` (`v0.16`, plus zpp_bits's shared-encoding approach for the D cells); no rival runs a separate verification pass |
| Verdict | Against the strongest rival of each cell: win < 0.95, tie 0.95–1.05, loss > 1.05 |

**19 main cells: 17 wins, 2 ties, 0 losses.** The ties are DP (1.001 [0.92–1.11]) and DC (0.965
[0.90–1.08]), both against `zpp_bits_dirty shared`. Of the 9 report-only cells, 8 win and UL.send is
0.964 [0.89–1.07]. The benchmark programs run the same machine code as in 4.2.0 (asm gate); against
the 4.2.0 run on a Skylake host, DC moved from win to tie (0.839) and UPW.recv from tie to win
(0.966), within the host and run variation described below.

| Group | Main cell ratios (÷ strongest rival, batch min–max) |
| --- | --- |
| Server to server | A 0.396 [0.37–0.41], B 0.272 [0.27–0.29], S 0.251 [0.23–0.27], SD 0.270 [0.26–0.29] |
| Server send | L.send 0.353 [0.33–0.41], EB.send 0.607 [0.58–0.65], LD.send 0.504 [0.47–0.54], UP.send 0.752 [0.72–0.79], UC.send 0.477 [0.45–0.50], DP **1.001** [0.92–1.11] (tie), DC **0.965** [0.90–1.08] (tie) |
| Client receive | L.recv 0.369 [0.34–0.45], EBW.recv 0.782 [0.76–0.80], LDW.recv 0.658 [0.61–0.69], UPW.recv 0.913 [0.83–1.01], UCW.recv 0.715 [0.69–0.76], DPTW.recv 0.744 [0.67–0.83], DCTW.recv 0.596 [0.55–0.64] |
| Uplink | UL.recv 0.842 [0.80–0.89]; report-only UL.send 0.964 [0.89–1.07] |

Shared-delta borderline rule ([RELEASING.md](RELEASING.md) section 2c): cells landing in 1.02–1.08 are
re-judged across three `-falign-functions` layouts. In 4.3.0 no D cell landed in 1.02–1.08 (DP 1.001,
DC 0.965, DPTW.recv 0.744, DCTW.recv 0.596), so no alignment re-judge was required.

### Reading the scorecard (review of 2026-11-17, updated for 4.3.0)

The directions hold: XOffset leads where encoding and decoding are skipped and pays about 1.0–1.4x
where containers are read and written through relative offsets. How far it leads depends on message
size, on the checks the receiver runs and above all on how churned the sender's buffer is (section 2):
the lead is largest at 3–12 KB, not for the largest messages, and small messages tie only when the
receiver runs full validation. These notes change no verdict.

- **S and SD against zpp_bits include a decode the trusted tier does not need.** The storage process's
  `store_check` (`bench/slg/bench/runner.hpp`) decodes the whole player for a format without a
  stand-alone check, while trusted XOffset only copies the bytes. On the same basis the ratios against
  zpp_bits (S 0.099, SD 0.184 in 4.3.0) are about 1.4–1.5x higher (review of 4.0.0: 0.24 instead of
  0.171, 0.41 instead of 0.276). Their strongest rival is cista_unchecked (0.251 and 0.270), so both
  stay wins.
- **UL.recv and UL.send mostly time the clock.** Two `steady_clock` reads per message cost about 30 ns
  on cloud Xeon hosts, which pulls small-order ratios toward 1. In 4.3.0 UL.recv is 0.842 [0.80–0.89]
  (win) and UL.send is 0.964 [0.89–1.07] (report only; 0.681 on the 4.2.0 Skylake host).
- **The Mac run is against zpp_bits only.** After PR #290 (d13ef15b) the Mac mini scored 18 wins,
  1 tie, 0 losses with zpp_bits as the only rival. cista_unchecked, the strongest cloud rival of six
  cells (B, S, SD, EBW.recv, UPW.recv, UCW.recv), was not run there, so it is not the same measure.
- **zpp_bits always checks bounds.** Receive cells against it compare a checked decoder with XOffset's
  unchecked trusted tier, as the trust model intends. Equal-check figures: UL.recv (bounds tier), the
  full-validation rows of [bench/cppcon2024](../bench/cppcon2024/README.md) and section 2.
- **UPW.recv depends on the host.** On 4.1.0 it scored 0.900 on a 2.3 GHz cloud Xeon and 0.976
  [0.941, 1.012] (tie) on a 2.1 GHz Xeon with 48 KiB L1d; on the 4.2.0 Skylake host it scored 0.966
  [0.87–1.08] (tie) and on the 4.3.0 host (2.1 GHz Xeon, 4 cores) 0.913 [0.83–1.01] (win).
- **Bytes.** XOffset's wire carries spare capacity and free blocks: 1.11–1.37x zpp_bits after zstd-3
  (shared deltas DP 0.68x / 1.04x vs `zpp_bits_dirty shared`, DC 0.38x / 1.01x vs
  `zpp_bits_dirty shared`), 1.34–1.54x uncompressed between servers (`A`/`B`/`S`/`SD`). Bytes are report
  only; the wire format is not tuned for them.
- **Defaults.** Measured with the 4.3.0 defaults (the same as 4.2.0): Wire 3.0 one-image format, `XWireEmission::automatic`
  for churned buffers, hardening switches off, client uplink bounds check on.

## 2. Where the lead holds

[`bench/premises`](../bench/premises/README.md) (2026-11-17, library 4.1.0) sends the 2024 `Character`
with every container count scaled, from 68 bytes to 729 KB in zpp::bits, on both receive tiers, from
clean and churned sender buffers, against zpp::bits, cista, FlatBuffers, Protocol Buffers and cereal
with the published adapters. Cloud Xeon 2.1 GHz (48 KiB L1d, 2 MiB L2 per core), 31 processes, each
cell paired within a process; ratios against the strongest rival of the tier, medians;
[results](../bench/premises/results/2026-11-17/summary.md) with every interval.

Buffer as filled:

| zpp::bits bytes | 68 | 100 | 276 | 780 | 2,916 | 11,460 | 45,636 | 729,156 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Trusted tier | 0.79 | 0.71 | 0.47 | 0.30 | 0.24 | 0.21 | 0.47 | 0.57 |
| Full validation (`reload_verified`) | 1.07 | 1.01 | 0.73 | 0.51 | 0.59 | 0.54 | 0.69 | 0.86 |

Churned sender buffer (a read/write pass leaves the state unchanged; after random edits every rival
is measured on the same final state; the strongest rival is zpp::bits in every cell):

| zpp::bits bytes | 780 | 2,916 | 5,764 | 11,460 | 45,636 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Wire sent, filled → after one read/write pass | 1,000 → 2,272 | 3,064 → 7,928 | 5,976 → 12,176 | 11,952 → 24,288 | 47,296 → 91,384 |
| Trusted tier, after one read/write pass | 0.67 | 0.57 | 0.71 | **1.39** | 1.05 |
| Trusted tier, after 100,000 random edits | 0.90 | 0.60 | 0.58 | **1.18** | **1.09** |
| Full validation, after one read/write pass | **1.19** | 1.00 | **1.11** | **1.77** | **1.35** |
| `save_compact` then trusted, after one pass | 0.91 | 0.76 | 0.61 | 0.55 | 0.77 |

- **Trusted tier, buffer as filled: a win at every size against every rival**, 0.79 at 68 bytes
  (cista with offsets) and 0.76 of zpp::bits. The other rivals are further behind (FlatBuffers
  0.11–0.33, cista 0.09–0.73, Protocol Buffers and cereal below 0.2).
- **The lead does not grow with size.** Sending and receiving on the trusted tier is two copies of the
  wire plus about 400–1,300 instructions, whatever the fields (cachegrind). Up to about 12 KB of wire
  the sender's buffer, the wire and the receiver's buffer fit the 48 KiB L1 data cache and the copies
  run at 40–53 bytes per ns; from about 16 KB every line misses L1 (3,038 simulated misses per 47 KB
  message) and they run at 17–20. The rivals spend most of their time per field, so the misses slow
  them proportionally less.
- **Small messages tie only with full validation.** At 68 bytes the trusted tier executes 387
  instructions against zpp::bits's 672; full validation walks the message and reaches 655. Against zpp::bits,
  full validation crosses 0.95 at 123 bytes [105, 146]. UL.recv (bounds tier, a tie) receives orders
  of about 76 bytes in zpp::bits.
- **Churn decides more than the checks do.** One pass of the read/write loop leaves the same wire as
  100,000 passes; random edits and containers that grew to 2x and shrank back leave about the same
  (wire about 2–2.7x `save_compact` without slack). XOffset's time follows the bytes it sends, the rivals'
  the logical state, so a churned buffer roughly doubles XOffset's cost, and more where the wire leaves
  L1: the trusted tier flips between 5.8 and 11.5 KB messages (228 ns → 1.51 µs at 11.5 KB). With full
  validation a churned buffer ties at best. `save_compact` brings the lead back at every churn level
  measured (0.53–0.94); on a buffer as filled it costs 1.5–3.2x a plain send. After zstd-3 a churned
  wire is 1.00–1.09x the same state sent clean from 3 KB up (1.09–1.24x at 780 bytes). Since 4.2.0,
  `XWireEmission::automatic` is the default for `save_verified` / `save_verified_into` and emits
  churned or high-spare images compact automatically (see [CHANGELOG.md](../CHANGELOG.md) and the
  [README](../README.md#performance)).
- **SLG demo cases with all five rivals** (8 batches, M scale, the scorecard's seed, D cells not run,
  2.1 GHz host): trusted tier 13 wins, 2 ties (UPW.recv 0.999, UL.recv 1.02), 0 losses; FlatBuffers,
  Protocol Buffers and cereal are never the strongest rival except FlatBuffers in UL.recv. With every
  receiver checking (XOffset `reload_verified`, cista `DEEP_CHECK`, the FlatBuffers Verifier): 14
  wins, 1 loss (UL.recv 1.43 against zpp::bits: small orders, see above).

## 3. Memory and relocation tax

- **Memory** (3.6.0, SLG demo M, one process per format, `slg_bench --mode proc`): xoffset_trusted RSS
  663.5 MiB, 0.98x the most frugal rival (protobuf); page tables 2.89 MiB, 0.44% of RSS.
- **Relocation tax** (`bench/reloc`, XOffset container ÷ the std container with the same function,
  Mac M4, last measured for 3.3.0): 17 of 19 items within 1.05. The two above are `vec.push_back`
  (each stored element recomputes the data pointer) and `set.erase.fresh` against `flat_set` (bounds
  re-check and slot clearing), both about 1.09–1.12.

## 4. Phone

iPhone 15 Pro Max, SLG M scale, 3.2.x, against FlatBuffers 25.12.19 on the same phone: A 0.305,
B 0.179, C 0.252, D 0.858. Not comparable with any desktop number; Android has no timing.
Method: [DEVICE_TESTING.md](DEVICE_TESTING.md).

## 5. Limits

- One workload (SLG) and one benchmark shape (`Character`, scaled from 68 bytes to 729 KB in section
  2), with one adapter style per rival; a different workload or adapter changes the numbers.
- x86-64 numbers come from non-quiet cloud hosts; Mac numbers from one M4. Other cores may differ.
- Only buffer reuse was measured, with caches warm between messages; a new buffer per message, or a
  server whose other work evicts the buffers between messages, is not covered. The L1 effects of
  section 2 move with the core's cache sizes.
- A ratio far below 1 says XOffset leads on this workload, not that the margin holds elsewhere.
