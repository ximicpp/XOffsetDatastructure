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
- server -> client (`include/slg/view.hpp`, revised 2026-11-03 per susu: no
  3x3 block window; a client holds exactly what is inside its viewport). A
  client sees one screen (512x384) of the 4096 map: the tiles under it (32 px,
  about 17x13 = 221 with partly covered ones; terrain, level, owner), the units
  standing in it (40 fields each since 2026-11-03: 33 scalars of int32, int64
  timers and bools, name, alliance tag and a 12-byte owner name, a nested
  commander, 1-3 heroes, 3-6 troops with wounded, 0-5 buffs) and the marches whose line crosses it (line records: start,
  end, start tick, arrival tick). Two cases:
  - EB browse (class 2): all 1,500 clients drag their viewport every tick (up
    to 48 px per axis, a jump elsewhere with p = 1/16) and pull everything in
    the new rectangle in full, one snapshot per client per tick (the spatial
    query of what lies in it is untimed bookkeeping, as for U*). The world
    changes at the peace rate, so this case is all full pulls. EB.send fills
    and encodes one viewport, EB.recv receives it with the format's checks and
    reads every tile, unit and march.
  - U* static (class 3): clients do not move. After one untimed full pull the
    server sends each client, every tick, only what changed inside its
    viewport (units changed, entered or left; marches started, recalled or
    gone; tiles whose owner changed), one delta per client; a client with no
    change gets nothing. Three change rates: UP peace (1 unit in 500 and 1 tile
    in 1024 change per tick, march turnover; the main case), UC combat (peace
    plus two battle zones whose units all change every 1 s tick and whose
    tiles change owner with p = 1/16; reference), US stress (a quarter of all
    units move, 1/64 of the tiles change owner; upper bound, report only).
    After the first pull each client reserves 1.5x + 16 unit and march slots in
    its held view (all formats; a compact view regrows on its first append).
    U*.send fills and encodes one client's delta, U*.recv receives it, U*.apply
    applies it to the held view through the client's id index.
  M: 10,000 units, 1,500 clients, 1,000 marches, 4 ticks per cycle. Bytes
  include zstd-3 and, for XOffset, the save_compact pull.
- native delta (XOffset only, built when the library has
  `XOFFSET_DELTA_SYNC_API` with `XDeltaBaseline::key_delta_into`), keys N*
  beside U*: the server keeps each static client's view in an XBuffer in
  stable slots (a record that leaves zeroes its slot, the lowest free slot is
  reused, tiles stay in place), mutates it in place (N*.send) and writes one
  `save_delta_into` against that client's baseline (N*.diff); the client
  applies it with `try_apply_delta_verified` (N*.recv, patch + full
  verification) and reads in place. Every delivery's view is checked equal to
  the application-level one. Every change is touched and the delta uses
  `touched_only`. The first pull is the baseline's key delta
  (untimed, like the other formats' first pull). Each view reserves 1.5x the
  average units in a viewport plus 32 and a quarter of all marches plus 32.
  `tools/analyze.py` compares N* with the other formats' U*.
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
