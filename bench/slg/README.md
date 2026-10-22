# SLG benchmark (frozen tree)

The SLG game-server benchmark behind the PERF_GOAL G1–G3 numbers: scenarios
A (request/response), B (player save/load), C (map shard ticks and snapshot),
D (reports), plus the middle-erase pressure item `S.ticks_middle`.

This directory is a frozen copy, not a development area. Its origin and every
file's sha256 are in `MANIFEST`; `python3 bench/check_manifest.py` checks them,
and CI runs it. Changing anything here means re-freezing: update `MANIFEST`
and say why in the PR. New adapters (for example the T4 subtree-copy work) go
in as their own commit, never mixed into a baseline change.

## Build

```
cmake -S bench/slg -B build/slg -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ -DSLG_WITH_XOFFSET=ON \
  -DCEREAL_INCLUDE=<cereal-v1.3.2>/include      # or -DCMAKE_PREFIX_PATH=...
cmake --build build/slg --target slg_bench
```

- `XOFFSET_ROOT` defaults to this repository; pass it to measure another checkout.
- GCC 16 or P2996 Clang. Formats are found, never downloaded: cereal (header),
  protobuf (`find_package`), FlatBuffers (`flatc`), and optionally zpp_bits
  (`-DZPP_BITS_INCLUDE`) and cista (`-DCISTA_INCLUDE`). A format that is not
  found is left out of the build and named in the configure output.
- `SLG_BUILD_PROBES=ON` adds the diagnostic programs `erase_probe` and
  `small_msg_probe` (off by default; they are not measurements).

## Run

```
slg_bench --mode verify --scale S                      # every format agrees, every cycle
SLG_REUSE=1 slg_bench --mode bench --scale M --cycles 41 --out raw.jsonl
python3 bench/slg/tools/analyze.py raw.jsonl
```

`SLG_REUSE=1` is protocol (c), the one PERF_GOAL uses: one long-lived
encoder/decoder state per message slot for every format. FlatBuffers decodes
through `verified_root` (the Verifier runs); XOffset uses `reload_verified`.

## Instruction counts (CI performance gate)

`-DSLG_IR_COUNT=ON` (or `-DSLG_IR_COUNT=1` on a direct compile) builds a
binary for callgrind. Run it under
`valgrind --tool=callgrind --collect-atstart=no`; collection is on only in
timed regions named by `SLG_IR_KEYS` (comma-separated, exact key names, or a
prefix ending in `*` such as `A.*`), and off in cycles below
`SLG_IR_SKIP_CYCLES`. Without the option nothing changes in the timing path
and no valgrind header is needed. See `docs/PERF_GATE.md`.

Instruction counts only catch regressions. Any "faster" claim needs the Mac
formal batch.
