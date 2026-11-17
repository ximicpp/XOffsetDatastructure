# Performance numbers (4.0.0)

Current published numbers, how they were measured, and their limits. Ratios are XOffset ÷ rival:
**below 1 means XOffset is faster**. Compression is never timed; sizes are after zstd level 3. Older
per-release tables (3.0.0 to 3.6.0) are in the Git history of this file.

| Comparison | Where |
| --- | --- |
| CppCon 2024 `Character`: send + receive, read/write loop, modified buffers | [bench/cppcon2024](../bench/cppcon2024/README.md); summary in the [README](../README.md#performance) |
| SLG game-server scorecard (release verdict) | Section 1 below |
| Memory, relocation tax, phone | Sections 2 and 3 |
| Instruction-count regression gate (not a speed claim) | [PERF_GATE.md](PERF_GATE.md) |

## 1. SLG scorecard (4.0.0)

`bench/slg` (frozen tree, see its `MANIFEST`) models a strategy game server: whole state on login,
per-client deltas, a shared map viewport, client orders going up, and players moving between servers.
Each cell times only data writing, sending and receiving.

| Item | Setting |
| --- | --- |
| Host | Cloud Xeon 2.1 GHz, 4 cores, pinned to one core, GCC 16 `-O3`, libstdc++; not a quiet machine |
| Tree | 4e28a20a (library and bench identical to release candidate cb3e04b2) |
| Command | `tools/perf_gate/repro_scorecard.sh` ([RELEASING.md](RELEASING.md) section 2b) |
| Samples | M scale, one process per format, 8 interleaved batches × 21 cycles, medians |
| Trust model | XOffset `xoffset_trusted` row; rivals zpp_bits and `cista_unchecked` (plus zpp_bits's shared-encoding approach for the D cells); no rival runs a separate verification pass |
| Verdict | Against the strongest rival of each cell: win < 0.95, tie 0.95–1.05, loss > 1.05 |

**19 main cells: 14 wins, 5 ties, 0 losses.** The ties are UP.send 1.018, UPW.recv 1.025, UL.recv 0.968,
and DP and DC below. UL.send is report only.

Shared-delta cells landing in 1.02–1.08 are re-judged by running the scorecard under three more code
layouts and taking the median:

| Cell | Default | `-falign-functions=16` | `=32` | `=64` | Median | Verdict |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| DP (shared deltas, peace) | 1.041 | 1.031 | 0.965 | 1.016 | 1.016 | Tie |
| DC (shared deltas, conflict) | 1.024 | 0.977 | 0.967 | 1.014 | 0.977 | Tie |

DP's publisher executes no more instructions than the hand-written rival; the spread comes from
instruction cache set conflicts across the binary. Raw data: project files `release-4.0.0/`.

### Reading the scorecard (review of 2026-11-17)

The directions hold: XOffset leads where encoding and decoding are skipped, by more for larger
messages, ties on small messages, and pays about 1.0–1.4x where containers are read and written
through relative offsets. These notes change no verdict.

- **S and SD against zpp_bits include a decode the trusted tier does not need.** The storage process's
  `store_check` (`bench/slg/bench/runner.hpp`) decodes the whole player for a format without a
  stand-alone check, while trusted XOffset only copies the bytes. On the same basis S is 0.24 (not
  0.171) and SD 0.41 (not 0.276) against zpp_bits. Their strongest rival is cista_unchecked (0.409 and
  0.386), so both stay wins.
- **UL.recv and UL.send mostly time the clock.** Two `steady_clock` reads per message cost about 30 ns
  on this host, of the ~42 ns per order for either format, which pulls the ratio toward 1. UL.recv is
  0.82 on the Apple M4.
- **UL.send 1.33 is superseded.** The order's values were drawn inside the timed stage. With them drawn
  before the timer (PR #290) UL.send is 0.93 [0.87, 0.99] on a cloud host and 0.76 on the Mac.
- **The Mac run is against zpp_bits only.** After PR #290 (158d0754) the Mac mini scored 18 wins,
  1 tie, 0 losses with zpp_bits as the only rival. cista_unchecked, the strongest cloud rival of six
  cells (B, S, SD, EBW.recv, UPW.recv, UCW.recv), was not run there, so it is not the same measure.
- **zpp_bits always checks bounds.** Receive cells against it compare a checked decoder with XOffset's
  unchecked trusted tier, as the trust model intends. Equal-check figures: UL.recv (bounds tier) and the
  full-validation rows of [bench/cppcon2024](../bench/cppcon2024/README.md).
- **Bytes.** XOffset's wire carries spare capacity and free blocks: 1.11–1.46x zpp_bits after zstd-3,
  1.34–2.07x uncompressed between servers. Bytes are report only; the wire format is not tuned for them.
- **Age.** Measured on 4.0.0, before PRs #290, #292, #293 and #298. No full scorecard has run on a
  later tree.

## 2. Memory and relocation tax

- **Memory** (3.6.0, SLG demo M, one process per format, `slg_bench --mode proc`): xoffset_trusted RSS
  663.5 MiB, 0.98x the most frugal rival (protobuf); page tables 2.89 MiB, 0.44% of RSS.
- **Relocation tax** (`bench/reloc`, XOffset container ÷ the std container with the same function,
  Mac M4, last measured for 3.3.0): 17 of 19 items within 1.05. The two above are `vec.push_back`
  (each stored element recomputes the data pointer) and `set.erase.fresh` against `flat_set` (bounds
  re-check and slot clearing), both about 1.09–1.12.

## 3. Phone

iPhone 15 Pro Max, SLG M scale, 3.2.x, against FlatBuffers 25.12.19 on the same phone: A 0.305,
B 0.179, C 0.252, D 0.858. Not comparable with any desktop number; Android has no timing.
Method: [DEVICE_TESTING.md](DEVICE_TESTING.md).

## 4. Limits

- One workload (SLG) and one benchmark (`Character`), with one adapter style per rival; a different
  workload or adapter changes the numbers.
- x86-64 numbers come from non-quiet cloud hosts; Mac numbers from one M4. Other cores may differ.
- Only buffer reuse was measured; a new buffer per message is not covered.
- A ratio far below 1 says XOffset leads on this workload, not that the margin holds elsewhere.
