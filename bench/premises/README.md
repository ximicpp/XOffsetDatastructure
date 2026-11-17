# Premises sweep: where XOffset's send + receive lead holds

The CppCon 2024 comparison ([`bench/cppcon2024`](../cppcon2024/README.md)) and the SLG scorecard
measure XOffset on two workloads, mostly on its trusted tier and with buffers that were filled once.
This directory measures how the result changes along the three conditions the lead was said to
depend on: message size, the checks the receiver runs, and how churned the sender's buffer is.

## How to run

```sh
bench/premises/run.sh <output directory>     # RUNS=31 CPU=2; SLG=1 also runs the SLG demo cases
```

Needs git, python3 and docker. It builds the image of `bench/cppcon2024` (GCC 16.2, Debian's
protobuf 3.21.12 and FlatBuffers 23.5.26), fetches zpp_bits v4.7.6, cista v0.16 and cereal v1.3.2 at
pinned commits, builds `premises_bench.cpp` with `-std=c++26 -freflection -O3 -DNDEBUG`, runs `RUNS`
processes pinned to one core, and writes `summary.md`. With `SLG=1` it also builds `bench/slg` with
every rival and runs 8 batches of the demo cases (M scale, the scorecard's seed), one process per
format per batch, formats rotated per batch, and writes `slg_summary.md`.

## What one process measures

`premises_bench.cpp` includes `bench/cppcon2024/cppcon2024_bench.cpp` (its `main` renamed), so the
XOffset, zpp::bits, cista, Protocol Buffers and FlatBuffers code is the published adapters'; cereal is
added over the same std model. One message is sender state → bytes → receiver state the receiver can
read and modify, both ends reusing their buffers and objects. Compression is not timed; bytes are
raw and zstd level 3.

- **Size**: the 2024 `Character` with every container count multiplied by k, k from 0 to 256
  (zpp::bits encodes 68 bytes to 729 KB); k = 1 is the published `Character`.
- **Checks**: trusted (XOffset `XReceiver<XLink::server_peer>::reload`; cista `UNCHECKED` and `CAST`;
  FlatBuffers without the Verifier) and checked (XOffset `reload_verified`; cista `NONE` and
  `DEEP_CHECK`; the FlatBuffers Verifier before unpacking). zpp::bits, Protocol Buffers and cereal
  always check bounds while decoding, so the same cell is both. Read-only receives (XOffset views,
  FlatBuffers without unpacking) are report only.
- **Churn** of the sender's XOffset buffer (the rivals' wire depends only on the state): every
  container grown by g times its size and shrunk back (g = 0.25 to 8), the 2024 read/write loop run
  1 or 100,000 times (both leave the state unchanged), and 100,000 random edits (the state changes;
  every rival is then measured on that final state). Churn is given as r = wire bytes ÷ the bytes of
  `save_compact` with no slack for the same state.

Every process runs all 472 cells (churn at k = 1/4, 1, 2, 4 and 16) in its own random order. Each
cell: 3 warm-up messages, then rounds of about 10 ms, 7 rounds, median. Ratios are taken within a
process (XOffset ÷ rival), then the median over processes with a distribution-free 95% interval
(order statistics). A verdict is "firm" when the whole interval is inside its band (win < 0.95, tie
0.95–1.05, loss > 1.05). Crossovers interpolate the median ratio between measured points (in log
bytes along size, in r along churn) with a bootstrap interval over processes.

`--filter REGEX` runs only the cells whose id matches (`--list` prints the ids); `--only ID --iters N`
runs one cell N times without timing, for instruction counts: run it under
`valgrind --tool=cachegrind --cache-sim=yes` at two N and divide the difference by the difference
in N.

## Results

Library v4.1.0, cloud x86-64 Xeon @ 2.10 GHz with 48 KiB L1d and 2 MiB L2 per core:

- [`summary.md`](results/2026-11-17/summary.md): 31 processes of the first version of the bench (360
  cells: churn at k = 1/4, 1 and 16 only).
- [`summary_dirt_k2_k4.md`](results/2026-11-17/summary_dirt_k2_k4.md): churn at k = 2 and 4, 31
  more processes run with `--filter '^(dirt|churn)/[^/]+/(2|4)/|^size/[^/]+/(2|4)/clean$'` (the
  clean cells at the same k are rerun for pairing).
- [`slg_summary.md`](results/2026-11-17/slg_summary.md): the SLG demo cases with every rival, both
  tiers, 8 batches.
- [`cachegrind.md`](results/2026-11-17/cachegrind.md): instructions and simulated L1 data misses per
  message for selected cells.

The raw output of every process is in the project files, `competitiveness-20261117/`.
