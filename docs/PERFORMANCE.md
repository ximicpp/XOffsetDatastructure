# Performance numbers (2.0.0)

Current published numbers, how they were measured, and their limits. Ratios are XOffset ÷ rival:
**below 1 means XOffset is faster**. Compression is never timed; sizes are after zstd level 3. Older
per-release tables (3.0.0 to 3.6.0) are in the Git history of this file. The tables were measured on
4.3.0; in the benchmark programs 2.0.0 changes machine code only in growth paths that can report a
capacity error, and instruction counts stay within 0.1% of 4.3.0 per run and 0.5% per stage
([PERF_GATE.md](PERF_GATE.md)).

| Comparison | Where |
| --- | --- |
| CppCon 2024 `Character`: send + receive, read/write loop, modified buffers | [bench/cppcon2024](../bench/cppcon2024/README.md); summary at the end of section 2 |
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
size and on the checks the receiver runs (section 2): the lead is largest at 1–12 KB, not for the
largest messages, and only full validation of messages below about 80 bytes loses, narrowly. Since
4.2.0 a churned sender buffer no longer costs the lead (it goes out compact). These notes change no
verdict.

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

[`bench/premises`](../bench/premises/README.md) (2026-11-25, library 4.3.0, tree `5a54d9d1`) sends the
2024 `Character` with every container count scaled, from 68 bytes to 729 KB in zpp::bits, on both
receive tiers, from clean and churned sender buffers, against zpp::bits, cista, FlatBuffers, Protocol
Buffers and cereal with the published adapters. Cloud Xeon 2.1 GHz (48 KiB L1d, 2 MiB L2 per core),
GCC 16.2 `-O3`, 31 processes pinned to one core, each cell paired within a process; ratios against the
strongest rival of the tier, medians; [results](../bench/premises/results/2026-11-25/summary.md) with
every interval. XOffset runs with its defaults (`XWireEmission::automatic`).

Buffer as filled:

| zpp::bits bytes | 68 | 100 | 276 | 780 | 2,916 | 11,460 | 45,636 | 729,156 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Trusted tier | 0.74 | 0.39 | 0.26 | 0.19 | 0.21 | 0.16 | 0.44 | 0.57 |
| Full validation (`reload_verified`) | **1.07** | 0.72 | 0.55 | 0.42 | 0.57 | 0.47 | 0.67 | 0.81 |

Churned sender buffer (a read/write pass leaves the state unchanged; after random edits every rival
is measured on the same final state; the strongest rival is zpp::bits in every cell):

| zpp::bits bytes | 780 | 2,916 | 5,764 | 11,460 | 45,636 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Wire sent, filled → after one read/write pass | 1,000 → 992 | 3,160 → 3,552 | 6,136 → 6,872 | 11,896 → 13,488 | 46,840 → 53,184 |
| Trusted tier, after one read/write pass | 0.44 | 0.40 | 0.34 | 0.34 | 0.59 |
| Trusted tier, after 100,000 random edits | 0.46 | 0.43 | 0.35 | 0.35 | 0.59 |
| Full validation, after one read/write pass | 0.70 | 0.63 | 0.58 | 0.54 | 0.73 |
| Full validation, after 100,000 random edits | 0.71 | 0.63 | 0.58 | 0.57 | 0.75 |
| Trusted tier, after 100,000 random edits and one `relayout()` | 0.18 | 0.21 | 0.17 | 0.17 | 0.48 |

- **Trusted tier, buffer as filled: a win at every size against every rival**, 0.74 at 68 bytes
  (cista with offsets; 0.70 of zpp::bits) and 0.16–0.21 from 780 bytes to 11 KB. The other rivals are
  further behind (FlatBuffers 0.08–0.32, cista 0.07–0.70, Protocol Buffers and cereal below 0.2).
- **The lead does not grow with size.** Sending and receiving on the trusted tier is two copies of the
  wire plus a few hundred instructions, whatever the fields (cachegrind: 157 instructions per message
  at 68 bytes and 755 at 2.9 KB, against zpp::bits's 680 and 6,264;
  [cachegrind.md](../bench/premises/results/2026-11-25/cachegrind.md)). Up to about 12 KB of wire the
  sender's buffer, the wire and the receiver's buffer fit the 48 KiB L1 data cache and the copies run
  at 36–65 bytes per ns; past 16 KB they slow down, and from about 47 KB every line misses L1 (2,958
  simulated misses per 47 KB message) and they run at 13–21. The rivals spend most of their time per field, so the misses slow
  them proportionally less.
- **Only full validation of very small messages loses, narrowly.** At 68 bytes full validation takes
  1.07 [1.03–1.12] of zpp::bits's time (489 instructions against 680) and crosses 0.95 at 78 bytes
  [75–80] (123 bytes in 4.1.0). UL.recv (bounds tier) receives orders of about 76 bytes in zpp::bits
  and wins in the 4.3.0 scorecard (0.842).
- **Churn no longer costs the lead.** The default emission sends a buffer that churn has left with
  much free space or spare capacity as its compact wire, with spare kept (a vector of at least 8
  elements keeps size / 8 + 1). After one read/write pass, 100,000 passes, 100,000 random edits or
  containers grown by 0.25 to 8 times their size and shrunk back, the wire is 1.13–1.16x
  `save_compact` without slack, so from 2.9 KB up every churned cell wins on both tiers: trusted
  0.32–0.60 and full validation 0.51–0.76 of zpp::bits (4.1.0 sent the churned image, 1.7–2.2 times
  as many bytes, and scored up to 1.39 trusted and 1.77 full). The compact walk costs time: a churned
  sender takes about twice as long as the same state sent clean (138 ns against 71 ns at 2.9 KB), and
  an in-place `relayout()` ([USER_GUIDE.md 4.7](USER_GUIDE.md#47-a-sender-that-edits-in-place-for-a-long-time-relayout))
  gives a long-lived sender its clean time back (0.17–0.50 of zpp::bits).
- **One small churned case ties with full validation.** Below 16 KiB the image copy is faster than the
  compact walk, so a small buffer goes compact only when its free bytes (and remembered shrinks) pass
  the default's threshold ([USER_GUIDE.md section 4](USER_GUIDE.md#4-saving-and-loading)). At 780
  bytes, containers grown by 0.25 to 1 times their size and shrunk back leave a 2.2 KB image
  (2.45–2.48x compact) that is sent as it is: full validation ties there (0.94–1.00), the trusted
  tier wins (0.40–0.42).
- **`XWireEmission::zero_runs` is the slow choice on churned buffers.** It always sends the image and
  scans it for zero runs, so from 2.9 KB up a churned sender took 0.99–9.5 times zpp::bits's time on the
  trusted tier, where the default took 0.32–0.60 (6,727 instructions against the default's 2,119 at
  2.9 KB after one pass), and after growth and shrinking its wire was up to 8 times the default's. Keep
  the default between machines.
- **Bytes after zstd-3** (report only): 1.45x zpp::bits at 68 bytes (the 40-byte header weighs most
  there), 1.09x at 780 bytes, 1.00–1.03x from 2.9 KB up. A churned buffer goes out compact,
  so after zstd it is the same size as the state sent clean (1.02–1.06x zpp::bits after random edits
  from 2.9 KB up).
- **SLG demo cases with all five rivals**
  ([slg_summary.md](../bench/premises/results/2026-11-25/slg_summary.md); 8 batches, M scale, the
  scorecard's seed, D cells not run, the same host): trusted tier 15 wins, 0 ties, 0 losses (closest
  UL.recv 0.859 against FlatBuffers without the Verifier and UPW.recv 0.894); FlatBuffers, Protocol
  Buffers and cereal are never the strongest rival except FlatBuffers in UL.recv, and in SD on the
  checked tier. With every receiver checking (XOffset `reload_verified`, cista `DEEP_CHECK`, the
  FlatBuffers Verifier): 14 wins, 1 loss (UL.recv 1.26 against zpp::bits: small orders, see above).

### The CppCon 2024 `Character` at its published size

[bench/cppcon2024](../bench/cppcon2024/README.md): a 64-item map, waypoints, attributes and 8 pieces of
equipment; the same sorted flat map and the same rules for every library; cloud x86-64 Linux (Xeon,
GCC 16 `-O3`, pinned core), 15 processes, medians; 0.95–1.05 counts as a tie.

| ns per operation | XOffset | zpp::bits\* | cista raw | FlatBuffers | Protocol Buffers |
| --- | ---: | ---: | ---: | ---: | ---: |
| Send + receive, buffer as filled | 83 | 340 | 637 | 614 | 8,789 |
| Read/write loop, 2024 layout | 144 | 343 | 236 | | 592 |
| Read/write loop, tuned layout | 60 | 83 | 66 | | 368 |

\* The read/write rows of zpp::bits are std containers.

- Send + receive is XOffset's best case when the buffer was filled once: its wire is the buffer. In
  the 2026-11-17 4.1.0 baseline (before `automatic` emission became the default), a churned buffer
  grew from 3,064 to 7,928 bytes and took 181 ns (0.57 of zpp::bits trusted, 1.04 full validation).
  Since 4.2.0 (`XWireEmission::automatic` default), the churned buffer automatically emits its compact
  3,552-byte image (measured 2026-11-24, 7 pinned runs on Xeon Skylake, GCC 16.2: 0.43 of zpp::bits
  trusted, 0.68 full validation).
- Read/write is the fastest of the scored rows on the 2024 layout. On the tuned layout it ties cista:
  0.90–0.98 in the published processes, 1.00 [0.78, 1.25] in a rerun of 21 processes on 4.1.0.
  Protocol Buffers' default `RemoveLast` keeps removed messages for reuse and then takes 77 ns on the
  2024 layout (report only; the scored row frees them like every other library).
- Sizes after zstd-3: XOffset 2,986 bytes, zpp::bits 2,926, Protocol Buffers 3,800. The 40-byte wire
  header and container heads make the smallest `Character` 84 bytes larger than zpp::bits (29 after
  zstd-3); this is by design.

## 3. Memory and relocation tax

- **Memory** (4.3.0, tree `5a54d9d1`, SLG demo cases at M scale, one process per format,
  `slg_bench --mode proc`, built with every rival as in section 2): xoffset_trusted RSS 538.3 MiB
  (peak the same), 0.92x the most frugal rival (Protocol Buffers 583.9 MiB; zpp::bits 595.8,
  FlatBuffers 595.9, cereal 597.6, cista 698.5); page tables 2.25 MiB, 0.42% of RSS. Both memory gates
  hold (RSS at most 1.10x the best rival, page tables at most 1% of RSS);
  [memory.md](../bench/premises/results/2026-11-25/memory.md). 4.3.0 and 3.6.0 built the same way
  on one host measured 536.3 and 542.8 MiB; the 663.5 MiB published for 3.6.0 came from another host
  and build, so compare RSS within one host only.
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
