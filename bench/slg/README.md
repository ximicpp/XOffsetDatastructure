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

## Demo case set (`SLG_CASES=demo`)

```
SLG_REUSE=1 SLG_CASES=demo slg_bench --mode bench --scale M --cycles 41 --out raw.jsonl
```

The game cases as susu grouped them on 2026-11-02, for the comparison with
other formats (the PERF_GOAL gates keep the default A/B/C/D set):

- server processes: A scene <-> battle (unchanged); B scene <-> scene, one hop,
  the receiver keeps the received player as its resident state, plays on it in
  place and sends it on at the next hop; S scene <-> storage, the storage
  process receives the player with the format's checks and keeps the bytes,
  then sends them back on log-in.
- server -> client (`include/slg/view.hpp`, revised 2026-11-02 after the SLG
  use-case research, doc 67f2Y3BpccPrANT5vjBdkH): the 4096 map is split into
  88 blocks of one screen (512x384). Each client view subscribes to the 3x3
  block window around its centre and pulls the whole window (one snapshot per
  block, E*) only when its centre crosses into another block; in between, every
  block's changes go out as one delta per block (U*), encoded once and fanned out
  to every subscriber, which applies it to its held view. Blocks hold units
  (points) and marches (line records: start, end, start tick, arrival tick).
  Three worlds by change rate: P peace (1 unit in 500 changes per tick, march
  turnover), C combat (peace plus two battle zones whose units all change every
  1 s tick), S stress (the earlier E world, a quarter of all units move every
  tick). M: 10,000 units, 1,500 views, 1,000 marches, 4 ticks per cycle. Stage
  keys: E*.send (once per block per tick), E*.recv and E*.index (per client and
  block), U*.send (once per block), U*.recv and U*.apply (per subscriber). Bytes
  include zstd-3 and, for XOffset, the save_compact window.
- native delta (XOffset only, built when the library has
  `XOFFSET_DELTA_SYNC_API` with `XDeltaBaseline::key_delta_into`): the same
  plan, but the server keeps each block in an XBuffer in stable slots, mutates
  it in place (N*.send), writes one `save_delta_into` per block against a
  baseline shared by its subscribers (N*.diff), and a new subscriber gets the
  baseline's key delta (K*.send / K*.recv). Clients apply with
  `try_apply_delta_verified` (N*.recv, patch + full verification) and read in
  place; every delivery's view is checked equal to the application-level one.
  K*.recv is timed like E*.recv: the key delta receive plus the read of every
  unit and march. The server reserves each block's capacity once (1.5x the
  average plus 32); its vectors are as long as the most records the block has
  held, a record that leaves zeroes its slot and the lowest free slot is reused,
  so a new subscriber checks and reads only that many slots.
  `tools/analyze.py` compares N* with the other formats' U* and K* with E*.
- the player's own data (`include/slg/login.hpp`): L, a client logs in and
  gets its whole player once (256 online players at M, a quarter log in again
  each cycle); LD, the server then changes the player every tick (troops,
  items, buildings, resources) and sends a PlayerPatch with only the changed
  parts (dirty marks recorded by the edit); the client applies it in place.
  SD, the dirty-module save to storage (all armies and items as a PlayerPatch).
  XOffset native: LK login key delta, LN self deltas, SN delta to what storage
  holds; every native receiver is checked equal to the application-level one.

There is no C or D in this set, and `MANIFEST` lists the change. With
`SLG_CASES` unset nothing changes: same stage keys, digests and report sums.

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
