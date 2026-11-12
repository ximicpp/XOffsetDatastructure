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
    reads every tile, unit and march. EBW.recv (report column, same for every
    format) receives the same pull once more into one extra held view that
    every pull reuses, as a single device holding one view would (warm caches;
    the 1,500 held views of EB.recv are cold). The warm view must read the same
    as the client's; EB keys and numbers are unchanged. The compressed and
    compact sizes (EB.window_zstd, EB.window_compact*) are taken in a second
    untimed pass after each round's timed pulls (the same pulls rebuilt), so
    the compressor and the compact save do not evict the server's working set
    between two timed pulls.
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
    applies it to the held view through the client's id index. Each client
    holds its view object as a real client would: zpp_bits and the std formats
    hold the decoded object, cista its pointer, and XOffset (2026-11-04) the
    root reference, refreshed in snap_recv, the only point where the view's
    epoch changes, so U*.apply starts at the root rather than looking it up in
    the arena for every delta (-DSLG_RP_CHECK checks each use, off by default).
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
- native browse delta (XOffset only, report only, 2026-11-05, 3.4 line C item
  P3), keys NB beside EB: the server keeps each browsing client's view as for
  N* and moves it with the client's rectangle every tick, so a pan sends only
  what entered or left it. Units and marches come from the same tracker as U*
  (entered, changed, left; slots as for N*); tiles sit in a ring of 17 x 13
  slots, tile (tx, ty) at (tx mod 17, ty mod 13), so a tile keeps its slot
  while it stays in view: a tile that entered is copied into its slot, a slot
  whose tile left is zeroed, a kept tile whose owner changed this tick gets
  its owner (only the spans' difference is visited). `NB.send` (slots +
  mutation) + `NB.diff` (`save_delta_into`, `touched_only`) is the server;
  `NB.recv` the client (patch + full verification, one cold view per client);
  `NBW.recv` one warm device as `N*W.recv` (the first 16 clients' deliveries
  replayed after the cycle, the view read before each, count
  `NBW.deliveries`). Bytes `NB.delta`, `NB.delta_zstd` (zstd 3 per message,
  `NBZ.send` timed after the round), streaming `NBZS.*`, count
  `NB.deliveries`; `NB.check_none` / `NB.check_in_place` / `NB.check_full`
  count how the client's apply was checked (`XDeltaCursor::last_check()`).
  Every delivery is checked equal to the client's EB pull of
  that round (same request id and tick). NB runs in a third pass of each
  round after EB's timed and bytes passes, so no EB stage changes its range;
  the first view is a key delta before the first round (untimed).
  `SLG_NATIVE_OFF=1` drops NB with the other native rows, `SLG_NB_OFF=1` NB alone
  (for A/B runs of the other stages). Under valgrind the
  demo set with native rows now needs `SLG_XOFFSET_CAP_MIB=16` (the extra
  views pass valgrind's address-space limit at 256 MiB per buffer).
- the player's own data (`include/slg/login.hpp`): L, a client logs in and
  gets its whole player once (256 online players at M, a quarter log in again
  each cycle); LD, the server then changes the player every tick (troops,
  items, buildings, resources) and sends a PlayerPatch with only the changed
  parts (dirty marks recorded by the edit); the client applies it in place.
  SD, the dirty-module save to storage (all armies and items as a PlayerPatch).
  XOffset native: LK login key delta, LN self deltas, SN delta to what storage
  holds; every native receiver is checked equal to the application-level one.

- client uplink (`include/slg/uplink.hpp`, 2026-11-10, 3.5 W4, report only):
  UL, every online client sends the server one march order per round (4
  rounds per cycle): one of the armies of the player it holds, a target tile,
  an arrival time and part of each troop type. `UL.send` is the client's fill
  + encode, `UL.recv` the server's receive with the checks it runs on client
  data plus the read of every field. The rivals decode as everywhere else
  here (FlatBuffers' Verifier runs; cista in its format's mode); `xoffset`
  reloads with the full check through `XReceiver<XLink::from_client>`
  (same as `reload_verified`); `xoffset_trusted` reads the wire in place
  through `XReceiver<XLink::from_client>::view` (`XCheck::bounds`, read-only)
  and also times `ULF.recv`, the full-check reload of the same wire plus the
  same read, in the same process (order alternating by message), for the
  bounds / full ratio. Bytes `UL.order`, `UL.order_zstd` (`ULZ.send`), count
  `UL.orders`. Runs after the login case; `SLG_UL_OFF=1` drops it. New keys
  only. Each client keeps one order and refills it: the std and cista
  runners `clear()` one `March`, XOffset builds its buffer for the first order
  and edits it in place after that (2026-11-12, 3.6 perf P2), so `troops`
  keeps its capacity in each and the XOffset wire carries that spare capacity.

XOffset's A adapter (both case sets) calls `prefetch_for_update()` on the two
players' buffers when the battle result starts arriving, inside the timed
A.res_decode: a public cache hint that changes nothing, so A.apply does not
pay the cold allocator lines one after another. The other formats have no
equivalent; libraries without `XOFFSET_PREFETCH_FOR_UPDATE_API` skip it.

There is no C or D in this set, and `MANIFEST` lists the change. With
`SLG_CASES` unset nothing changes: same stage keys, digests and report sums.

Timing scope (susu 2026-11-03, every demo case): a timed stage holds only data
reads and writes, encode/decode, send and receive. Game logic runs untimed
through `untimed()` in `bench/runner.hpp`: the battle simulation
(`A.simulate`), the scene and season edits (`B.scene_edits`,
`B.season_edits`), and the shard ticks (`C.ticks`, `S.ticks_middle`); those
keys are not recorded in this set (with `SLG_CASES` unset they stay
timed, for the perf gate). The world's own events in the viewport cases and
the player edits before LD were already untimed. Byte sizes that compress
(EB.window_zstd, the compact sizes, U*.delta_zstd) are computed after each
round's timed loop, so zstd never runs between two timed deliveries.

Native and application-level XOffset rows come from separate processes
(2026-11-03): the native stages run interleaved with the application-level
ones, per delivery, so whichever runs second finds the world and the player
warm. `SLG_NATIVE_OFF=1` skips N*, LK, LN and SN; the formal runs take every
application-level row from such processes and the native rows from a
`--formats xoffset` process with them on. The native view update writes a
unit whole only when it is new to the view; otherwise it writes the fields
the world marked dirty this tick (`touch(u, bits)`, `MapWorld::dirty`), as a
game server's dirty flags, and every client's native delta goes into one
shared buffer (thread 设计并实现增量同步).

Fairness review (thread 审查三类对比测试的公平性, 2026-11-03): the std model's
maps (items, research, buildings, resources) are `stdm::smap`, a sorted flat
map (one vector of pairs) written in `model_std.hpp`, like XOffset's and
cista's (were `std::map`; briefly `boost::container::flat_map`, which needs
Boost.Move and Boost.Assert outside the dependency lock and ran 5-15% slower
for the rivals). FlatBuffers
collects struct elements and table offsets in reused scratch vectors (no
allocation per list once warm). The storage process runs only the
format's own check where one exists without the application model
(FlatBuffers' Verifier, protobuf's parse; the others decode). All three
runners prefetch the two armies before A.apply with the same
`prefetch_army_for_apply` (army, heroes, troops, buffs; 2026-11-04: XOffset's
earlier `prefetch_for_update()` allocator-line hint was dropped, since the
troop-only write-back does not allocate and the hint cost ~11% of A). LK.recv includes the query, as
L.recv. The native slot bookkeeping (`n_plan`) is timed in N*.send; its unit
and march id -> slot maps are flat open-addressing tables (2026-11-04, were
`std::unordered_map`; slot choice, bytes and states unchanged). cereal
reuses its input stream. Allocator: glibc malloc for every format.

Scoring V3 (2026-11-04, thread 确定下一阶段开发方向, doc GZH4QtBqBrDXKrH9uwTmsH):
new keys only; no existing key or the time range of an existing stage changes,
so earlier batches and the perf gate compare as before.

- Send + compress: `EBZ.send`, `U*Z.send`, `N*Z.send`, `LZ.send`, `LKZ.send`,
  `LDZ.send`, `LNZ.send` time zstd level 3 of each wire the server sent (one
  frame per message, one reused context, no dictionary). They run after the
  round (EB: in the untimed bytes pass) so the timed stages keep their working
  set; send + compress = `*.send` + `*Z.send`. They take their own prefix so
  tools that sum a case's stages by prefix keep their totals. Sizes:
  `EB.window_zstd`, `U*.delta_zstd`, `N*.delta_zstd`, `L.player_zstd`,
  `LK.player_zstd`, `LD.patch_zstd`, `LN.delta_zstd` (bytes, untimed).
- Client, one device with one warm view: `U*W.recv`, `N*W.recv` (static views)
  and `LDW.recv`, `LNW.recv` (self deltas). The first `kWarmDevices` (16)
  clients / players each have a second copy of their view or player; after the
  cycle their deliveries are replayed into it, receive + apply timed per
  delivery as `U*.recv` + `U*.apply`, and before each one the device reads its
  whole view (untimed: a client draws it every frame), so it is warm as on a
  real device. Counts `U*W.deliveries`, `LDW.patches`; the copies are checked
  equal to the clients' views. The copies are laid out after every client's
  own view or player (the same pull again, untimed), so the clients' objects
  keep the addresses they had without them. `EBW.recv` (browse) is unchanged.
- Streaming compression, report only: the warm devices' connections also go
  through one zstd level 3 context per connection, kept across messages, one
  `ZSTD_e_flush` per message: `EBZS.send`, `U*ZS.send`, `N*ZS.send`,
  `LZS.send` + `LDZS.send` (one context for the login and then the patches),
  `LKZS.send` + `LNZS.send`; bytes `*_zs`, counts `*ZS.msgs`, per-connection
  context memory `*ZS.ctx` (`ZSTD_sizeof_CCtx`).
- Trust tier (susu 2026-11-04): server <-> server and server -> client links
  are trusted, so the main columns use the rivals' no-check modes where they
  have one: `flatbuffers_noverify` (no Verifier) and `cista_unchecked`
  (`cista::mode::CAST`), run as formats of their own (`run_v5_split.sh` adds
  them); the checked rows are report only. XOffset keeps its checked paths
  (`reload_verified`, `try_apply_delta_verified`) until its trust-tier
  interface lands.
- XOffset trust tier, whole wires (3.4 P1, `xoffset_receiver.hpp`): format
  `xoffset_trusted` is `xoffset` with every reuse-protocol whole-wire receive
  (A, B, S, SD, L, EB, LD, U* and their warm-device stages, D) through
  `XReceiver<XLink::server_peer>` (server <-> server) or
  `XReceiver<XLink::from_server>` (a client) with `XCheck::admit`, instead of
  `reload_verified`. The same keys and stages. Native delta receives (LK,
  LN, N*, NB, SN) go through the same receivers' `apply_delta` (3.4 P2:
  admit, no map of checked bytes; `SN.check_admit` and no `NB.check_*`
  counts). Builds with `NDEBUG` (otherwise
  `XOFFSET_FORCE_FULL_CHECK` makes admit the full check). Its rows are the
  XOffset main column of the application-level cases and class 1
  (`pool_v6.py`); `xoffset` (full check) is report only.
  Since 2026-11-10 (3.5 W4) the clients' view deltas (U*.recv, U*W.recv) are
  read in place instead: `XReceiver<XLink::from_server>::view` (admit: the
  framing and the root inside the payload) and the delta is applied from the
  wire's bytes, with no copy into a buffer, as `cista_unchecked` and
  `flatbuffers_noverify` read theirs.
- Memory, report only: a `-DSLG_MEMCOUNT=1` build (CMake `SLG_MEMCOUNT=ON`)
  counts the live bytes of every `operator new` (one counter for every format
  and platform, malloc's own overhead not counted); `--mode mem --out m.jsonl`
  runs 2 cycles and writes `MEM.*`: `MEM.player` per resident player (std
  formats: what a deep copy allocates; cista: its buffer; XOffset: arena used,
  plus `MEM.player_arena` and `MEM.player_reserved`), per client the
  application-level delta's `MEM.U*.interest` (the tracker) and
  `MEM.U*.held_index`, and the native delta's `MEM.N*.view`, `.baseline`,
  `.slots` and `MEM.LN.baseline`. Never time with this build.
- Process memory, the 3.6 memory gate (2026-11-11): `--mode proc --out p.jsonl
  --formats <one format>` (any build, Linux only) runs 11 cycles untimed and
  writes the process's own numbers from `/proc/self`: `PROC.rss`, `PROC.hwm`
  (peak RSS), `PROC.pte` (page tables, VmPTE), `PROC.rss_anon`, `PROC.vmas`
  (mappings), and `PROC.base_*` sampled before any format's setup. Run one
  format per process (the numbers belong to the process); `SLG_PROC_SMAPS=<file>`
  also keeps a copy of `/proc/self/smaps` for diagnosis. Never time this run.
  XOffset layout for the gate (2026-11-11): player, battle and report buffers
  are `XBuffer::relocating()` (a slot, a move when one outgrows it), held
  views a fixed 128 KiB slot (`kHeldMax`); the shard keeps 256 MiB. At scale M
  page tables went from 13.1 to 5.6 MiB (0.84% of RSS; the gate is 1%).

D3 shared deltas (2026-11-04, thread 设计共享增量原型, doc X5MZPWya7DWFzdCughpZqL;
`xoffset_shared_delta.hpp`, XS 1.0 since 3.5, docs/SHARED_DELTA.md). XOffset and (since
4.0 W1, below) zpp_bits, class 3 only;
`SLG_D3=1` runs D3 alone (no N*), `SLG_D3=2` both, unset none (keys and stage
ranges as before); `SLG_NATIVE_OFF` turns D3 off too. Needs no
`XOFFSET_DELTA_SYNC_API`. The server keeps one image of every entity as its
holders have it, shared by all connections. Once per tick (`D*.pub`) it notes
the world's dirty flags and compares and encodes every changed unit some view
holds, over its changed members only (`XRecordDelta::update`); a unit no view
holds, and marches and tiles, are encoded on their first use in the tick (the
entries they need are prefetched in `D*.pub`). A delivery copies the bodies of
its entities into one message (`D*.send`). The bytes are those of comparing
whole records. Server per delivery =
(`D*.pub` + `D*.send`) / `U*.deliveries`. The client applies the message to
its own view through `XReceiver<XLink::from_server>::apply_sync` (a `D3Target`
over the view's unit, march and tile slots): `D*T.recv` with `XCheck::admit`
(header, stream and version, every op's table, kind and id; one pass),
`D*.recv` with `XCheck::full` (every body checked, then applied), each into
its own copy of the view, the order alternating by delivery; both are compared with the application-level view
after every delivery. Bytes `D*.delta`, `D*.delta_zstd` (`D*Z.send`); warm
devices `D*W.recv` / `D*TW.recv` (count `U*W.deliveries`), streaming `D*ZS.send`,
`D*.delta_zs`; memory `MEM.D*.conn` (per connection), `MEM.D*.shared` (the
store, baseline and arenas, whole server) and `MEM.D*.shared_per_conn`.
Bookkeeping added for it, untimed: the tracker's fresh flags
(`DeltaPlan::unew`, `mnew`), the world's per-unit dirty flags and its
changed-unit and changed-tile lists.

D3 rival (4.0 W1, 2026-11-12, `adapters/zpp_bits_shared.hpp`): the format
`zpp_bits` has D rows too, the same keys under `SLG_D3=1`, so XOffset's D3 is
compared with a rival that also encodes each entity once per tick instead of
once per client. Same split as XOffset's D3: the shared tracker decides who
sees what, and the holder count follows the same rule (+1 per enter sent, -1
per leave). `D*.pub`: every changed unit some view holds is brought up to date
in the server's image (its dirty members copied from the world) and encoded
whole with zpp_bits, once; units no view holds only collect their dirty flags;
marches, tiles and entering units are encoded on first use in the tick. An
entering unit and a changed one use the same whole-record body. `D*.send`:
header (request id, tick), the ids that left, then a copy of each entity's
body. Client: zpp_bits' checked decoding straight into the held view, each
body over the record its leading id finds through the client's id index (the
`U*.apply` index type), or onto a new record; a leave swaps the last record
in. zpp_bits has one check level, so `D*T.recv` and `D*.recv` run the same
code, each into its own copy of the view; both are compared with the
application-level view after every delivery. The diagnostic format
`zpp_bits_dirty` (not in the default list; its U rows are zpp_bits') is the
second variant: a held unit that changed gets a change body (id, uint64 dirty
mask, only the masked members, each as zpp_bits writes it), entering units
stay whole, and the client decodes the masked members over its record. No
other row changes: without `SLG_D3` the zpp_bits runner runs no D code.

Batches: `run_v5_split.sh` and `pool_v5.py` (project files,
slg-cases-v3-20261102/).

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
