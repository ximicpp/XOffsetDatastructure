# Changelog

Key changes per release. Measurement details and the long form of every entry are in the Git history
of this file and in the pull requests.

## Unreleased

## 4.3.0: add-only schema evolution

A type can gain fields without conversion: a field added in reserved `XSpare<N>` bytes keeps the first
version's fingerprint, so old and new programs exchange whole wires, image deltas and stored images
both ways at the same cost, and any other change starts a new generation that the handshake refuses
on the network and `evolve_load_verified` upgrades once on load for stored data. Per-peer conversion
(`XPeerSchemas`, `XBroadcast`) is removed; Wire format 3.0 is unchanged, and types that declare no
family keep their fingerprints and machine code. The client headers build after Unreal Engine and
Apple assert macros (source renames below), and from 5.0.0 releases follow Semantic Versioning with
the wire format in the contract. Pre-release scorecard: 19 main cells, 17 wins, 2 ties, 0 losses
([PERFORMANCE.md](docs/PERFORMANCE.md)); pre-release K4 fuzz (`--budget hour`) with 0 findings (GCC 16
ASan+UBSan differential, 4 × 662,370 cases + replay; P2996 libFuzzer, 6 reflection and 5 field-list
targets including `schema_family`; 3-way outcome-diff); `--release` validation 27/27: Linux C++26
`gcc`/`asan`/`tsan`/`p2996` 282/282, 8-column C++20/23 matrix 247/247, MSVC C++20/C++23, clang-cl and
MinGW under Wine and Android NDK r29/r30 under QEMU 247/247 each plus 52 no-exceptions tests, wire
matrix 8 builds × 64 reads with the same bytes. Ir gate against v4.2.0: every total and stage 1.0000
on GCC 16 and P2996, and the benchmark programs have the same machine code as v4.2.0.

- Compatibility policy ([RELEASING.md](docs/RELEASING.md) section 1.1, USER_GUIDE section 8,
  SECURITY.md): 4.x releases keep promising nothing between versions; from 5.0.0, the first release
  meant for outside projects, incompatible wire or API changes come only in a new major version, and
  every release of one major version reads every other one's wires and deltas.

- The client headers build after engine macros: no library identifier is named `check`, `verify`
  or `require` and called, so Unreal Engine's `check`/`verify` and Apple `AssertMacros.h`'s
  `check`/`verify`/`require` no longer break the build. New test `test_engine_macros` (and
  `_noexc`) defines those macros first and builds and runs the client headers.
  **Breaking (source):** `XView::check()` is now `XView::tier()`, and the
  `XOFFSET_SYNC_CHECKS` helper `XSyncDirtyTable::verify(pub)` is now `verify_delivered(pub)`.
  Renames only: no change to the wire or to the generated code.
- The C++20/C++23 compiler floors are the oldest versions that ran the full CTest and the wire
  corpus: GCC 13, Clang 18, Apple clang 21 and MSVC 19.44 (were GCC 12, Clang 16, Apple clang 15
  and MSVC 19.38, never run).
- Docs: `FetchContent` takes the release archive (`URL .../archive/refs/tags/vX.Y.Z.tar.gz`, about
  3 MB) instead of a Git clone (about 2 GB with every branch and the recursive Boost submodule);
  a section on using the headers without CMake (engine build systems, Unreal Engine); the iOS
  address-space advice puts limits, slots and pools before the Extended Virtual Addressing
  entitlement, which has not run on a device; the K5 guarantee and the trusted tier no longer
  mention the hole table that wire format 3.0 removed; `XBufferPool` byte limits must allow for
  page rounding.
- Examples: `stable_storage` sized its pool from the maximum capacity, so the second mapping did
  not fit the reserved-bytes limit and an `assert` failed in Debug builds; it now sizes the pool
  from `reserved_bytes()`. `helloworld`, `stable_storage`, `schema_handshake`, `delta_sync` and
  `shared_delta` (and the C++26 `demo`) now run under CTest with `assert` on.
- Add-only type versions without conversion ([SCHEMA_EVOLUTION.md](docs/SCHEMA_EVOLUTION.md)
  sections 1 to 5). `XSpare<N>` reserves N opaque zero bytes; `XOFFSET_SCHEMA_FAMILY(New, Prev)` declares that
  `New` only takes fields from `Prev`'s spare bytes (or turns an `XSpare<N>` element into a record of N
  bytes), and the build fails with the rule and the field when it does anything else. Every version
  of a family has the first version's fingerprint, so old and new programs exchange whole wires, image
  deltas and stored images both ways with no conversion, and the handshake answers `same` between them
  (the hello lists the family's earlier field-name digests; its format does not change). Shared
  deltas number change bodies by the first version's members, so masks stay compatible. Types that
  declare no family keep their fingerprints and their code (the same machine code on GCC 16 and P2996);
  the Ir gate's new `family.*` rows require a send, receive and delta between versions to cost exactly
  what they cost within one version. New test `fuzz_schema_family` (pre-release fuzz now about 4.5 hours).
- One evolution model ([SCHEMA_EVOLUTION.md](docs/SCHEMA_EVOLUTION.md), rewritten): a field added in
  reserved bytes keeps the fingerprint (above); any other change starts a new generation. Programs of
  different generations do not talk (the handshake refuses them), and stored data of an older
  generation is upgraded once on load (`evolve_load_verified` with `XOFFSET_SCHEMA_LINEAGE`, each entry
  the last version of a generation). Nothing converts messages per peer any more.
  **Breaking (source):** removed from `xoffset_evolution.hpp`, with what to use instead:
  - `XPeerSchemas` (`accept`, `load_verified`, `save`, `save_into`, `sends_working_version`, ...):
    `accept_schema_hello<Roots...>` (`xoffset_handshake.hpp`) answers the hello; afterwards the peer
    works in this program's generation, so the plain `load_verified` and `save_*` calls apply. A server
    that must still serve a peer of an older generation converts each message itself with
    `evolve<Old>(buffer)` and `save_compact`.
  - `XBroadcast<W>`: send the same wire to every peer (one generation, one encoding).
  - `make_server_schema_hello<Roots...>(label)`: `make_schema_hello<Roots...>(label, XSchemaRole::server)`.
  - `XOFFSET_SCHEMA_NAME_ALIAS` (a rename under the same layout): renames are not compatible changes;
    keep the released names, or start a new generation.
  - `XPeerSchemas::load_verified` for stored data: `evolve_load_verified<W>`.
  **Behaviour change:** the handshake verdict `convert` (value 1) is retired: nothing writes it, and
  `accepted()` and `read_schema_ack` read an ack that has it (from 4.2 or earlier) as a rejection. The
  hello and ack formats, pins and field-name digests do not change.
- `evolve` never copies `XSpare` bytes: a spare member of the target stays zero, a field taken from the
  source's spare bytes starts at its initial value, and `evolve_plan` prints `spare (not copied)`.
  Before, generations with spare members of different sizes did not compile, and equal sizes copied
  the old spare bytes into the new type.
- Map-state demo: version 1 of `ViewSnapshot` is frozen again with reserved bytes and version 2 takes
  `scene_units` from them, so A sends a version-1 client the same wires as any client, deltas included,
  and forwards write-backs between both versions (it converted every snapshot for it before).

## 4.2.0: one-image Wire 3.0 and in-place relayout

Wire format 3.0 merges the arena image and the wire into one 40-byte-header format with an in-band
allocator and whole-state delta format 2.0, `XWireEmission::automatic` emits churned buffers
compact by default, `relayout()` / `relayout_advised()` compact long-lived delta senders in place,
and same-version layout interoperability is checked at compile time. C++20/C++23 builds drop the
Boost submodule dependency, optional `std::pmr` (`XOFFSET_USE_PMR=0` default on older Apple
targets) lowers the Apple floor to macOS 12 / iOS 14, and `-fno-exceptions` builds gain complete
`try_*` create, clone, save, delta and compact entry points. Pre-release scorecard: 19 main cells,
17 wins, 2 ties, 0 losses ([PERFORMANCE.md](docs/PERFORMANCE.md)); pre-release K4 fuzz (`--budget
hour`) with 0 findings across GCC 16 (1.17M differential cases + replay) and P2996 Clang (36M+
libFuzzer executions across 9 reflection/fieldlist targets + 3-way outcome-diff); Linux C++26
`gcc`/`asan`/`tsan`/`p2996` and 6-column C++20/23 matrix, Windows MSVC C++20/23 (214/214 each),
macOS 12.0 C++20/23 and iOS 14.0 simulator (214/214 each) all pass with 27/27 Wire 3.0 corpus
identity (plus 2026-11-24 physical Android 11 and iPhone 15 Pro Max verification). Ir gate against
v4.1.0: 0 failures, every scenario total level or better (`slg.A`–`slg.D` 0.863–0.976,
`formal.emit_after`/`mutate`/`reload` 0.912–0.959); warnings on `reloc.map/set.erase/find`
(1.020–1.035) and GCC `slg.A.apply` (1.040), plus ledger exemptions for `slg.B.back_encode` and
P2996 `slg.A.apply`.

- **Breaking:** same-version interoperability is checked at compile time
  ([USER_GUIDE.md](docs/USER_GUIDE.md) section 2.3). A wire record must have the natural layout
  (each member at the next offset its type's alignment allows, the size rounded up to the
  record's alignment): `#pragma pack`, `/Zp`, `-fpack-struct`, `alignas` on a member,
  `[[no_unique_address]]` and unnamed bit-fields no longer compile, where before a peer built
  differently was only rejected on load by the fingerprint. `std::array<E, 0>` is rejected (its
  layout differs between libstdc++/libc++ and MSVC). The platform check uses `std::endian`, adds
  8-bit bytes, a one-byte `bool`, IEEE 754 `float` and `double` and size-aligned fixed-width
  scalars, and can no longer be turned off (`XOFFSET_DISABLE_PLATFORM_CHECKS`, `XOFFSET_LITTLE_ENDIAN` and `XOFFSET_ARCH_64BIT`
  are gone). Compile time only: no change to the wire or to the generated code.
- Optional `std::pmr` (`XOFFSET_USE_PMR`, header-only `XMemoryResource` when `0`, default `0` below
  macOS 14 / iOS 17): lowers Apple deployment targets to macOS 12 / iOS 14 with zero `std::pmr`
  symbol imports (verified on macOS 12.0, iOS 14.0 simulator and physical iPhone 15 Pro Max).
- Zero-submodule C++20/C++23 builds: CMake no longer requires `external/boost` for C++20/C++23
  targets, `tools/prepare_release.py pack-prefix --component full` packages and audits
  zero-dependency C++20/C++23 installs alongside C++26 installs, and installed C++26 `find_package`
  accepts GCC 16+ in addition to P2996 Clang.
- Complete `-fno-exceptions` API coverage: added non-throwing `try_create`, `try_clone`,
  `try_clone_into`, `try_save_verified`, `try_save_verified_into`, `try_save_compact`,
  `try_save_compact_into`, `try_save_delta`, `try_save_delta_into` and `try_compact`, and fixed
  `XCompactor`, `ref()` and `pin_root()` under `-fno-exceptions`.
- Added schema-driven JSON diagnostics (`xoffset_dump.hpp`), `docs/ROADMAP.md`, and public
  repository governance docs (`SECURITY.md`, `CODE_OF_CONDUCT.md`, issue and PR templates).
- Eliminated MSVC `/W3 /WX` narrowing and signed/unsigned comparison warnings
  (`std::cmp_less`/`std::cmp_equal`) and consumer `-Wextra` warnings; optimized `XResult`, receiver
  bounds walk, shared-delta publish/fanout/apply, inline container access and `XBuffer::edit()` hot
  paths.
- **Breaking:** wire format 3.0 ([WIRE_FORMAT.md](docs/WIRE_FORMAT.md) section 6). A buffer's
  memory and its wire are the same bytes: one 40-byte header (magic and version, fingerprint,
  `used_bytes`, extent count, free directory, free bytes), the root at offset 40, and the free
  blocks and their directory inside the image. No hole table on the wire. A 3.0 reader rejects
  2.1 wires and a 2.1 reader rejects 3.0 wires. Send, receive, clone and reset are one copy and a
  few checks; the full check walks the free directory. `begin_verified_reload<T>(used_bytes,
  extent_count)` takes the image size; root types are aligned to at most 8 bytes. Whole-state
  deltas move to format 2.0 (the same framing, image offsets with the root at 40); a 2.0
  receiver rejects 1.0 deltas.
- Deltas of the one image (format 2.0): the hole table section is gone. The allocator's own writes
  (the header's free state, the free directory, the blocks it takes and gives back) are touches,
  so a delta carries them like any other change, and the receiver's free state is the bytes it
  applies. The touch log lists byte runs (up to 25 a period; when full, runs less than a block
  apart in touch order are joined before it overflows to stamps). An `XDeltaRecord::touches`
  baseline whose log lists every run since its last delta writes the run frame on a short path,
  one run or several, and `save_delta_into(std::span<char>, ...)` writes it into the caller's
  memory. Instructions (callgrind, `touch_set` and the delta, Character fixture), GCC 16 / P2996:
  publish one scalar 448 -> 118 / 584 -> 117, publish and apply (admit) 749 -> 283 / 920 -> 266,
  32 scattered fields published and applied with the full check 11,082 -> 6,526 / 12,211 -> 6,894.
  SLG demo delta keys (GCC 16): sends 17-26% fewer instructions, receives 0.8-3.8% fewer, full
  compares 0.6-12.5% fewer.
- The full check registers the free blocks after the live walk: a live block is checked against
  the live blocks only, each free block against both. Checked receive of the Character wire
  (GCC 16) 2,695 -> 2,503 instructions, a wire with free blocks 3,694 -> 3,621, the formal
  scenario's reload 15.75 M -> 15.51 M. A live block over a free block is now reported as
  "invalid free blocks" (or as overlapping live ranges when it also meets a live block).
- Less host-side state per buffer. The root type is one key per type (`root_is<T>()` is one
  compare; `reset<T>()` of the same type keeps it), the owner record is made on first use (the
  verified paths and `stable_erase_if` still find their owner through it), and the root cache and
  the binding fields are gone. `edit`, `build` and `read` with no expected growth, on a buffer with
  no other scope open whose payload stays put, open without a control block, and each instantiates
  its callable once. A first block is taken with a lean bump; every load checks that `used_bytes`
  is a multiple of 8, as the format requires. Instructions (callgrind, fixed part, GCC 16 / P2996):
  clone 264 -> 44 / 367 -> 42, reset to an empty root 82 -> 37 / 74 -> 27, an empty edit scope
  60 -> 13 / 59 -> 12, create and destroy a buffer 1,088 -> 756 / 1,320 -> 896; a build of one
  layout through the map's sorted base 1,621 -> 1,507 (GCC 16).
- `save_delta_into(std::string&, ...)` of a touches baseline takes the run-frame short path inline
  (a buffer with a recorded failure still refuses). A compared delta whose changed bytes are one
  run in one block is a run frame ([DELTA_SYNC.md](docs/DELTA_SYNC.md), "One changed field").
  GCC 16, Character fixture, one scalar: touches publish 182 -> 126, publish and apply 327 -> 269;
  compared publish 1,907 -> 1,863, publish and apply 2,243 -> 2,034.
- The host-side free table is gone: `FreeTable`, its per-thread cache and recycler, and the paths
  that inserted into it and took from it (`xoffset_arena.hpp` 4,089 -> 3,199 lines). The in-band
  allocator is the only one and no longer sits behind `XOFFSET_INBAND_ALLOCATOR` (the macro is now
  ignored). Removed: `XBufferCore::trim_free_table_cache()` and `free_table_inline_entries`.
  `prefetch_for_update()` prefetches the owner map entry, the owner word and the arena header.
  `free_table_complete()` stays: a free block the directory has no room for is still lost until
  `save_verified` recomputes the free blocks. Instructions unchanged (SLG and formal totals 0.9995
  to 1.0000, GCC 16 and P2996).
- `save_compact_into` into a reused output takes one walk with nothing else around it: a buffer
  with T's root key writes the compact wire straight into the output's capacity (no owner record,
  no header recheck; the alias and header checks run with `XOFFSET_SAVE_SELF_CHECK`, as for
  `save_verified_into`), and anything else, a first save into a short string for example, takes
  the general path as before. The writer builds each container head, inline string and the wire
  header in registers: they were assembled on the stack and copied out, and each copy waited on a
  failed store-to-load forward, which cost more time than all the writer's instructions. Blocks
  of up to 64 bytes are copied and spare capacity zeroed by a few moves instead of `memcpy` and
  `memset` calls; the bytes past the last element in the last word are still cleared. Character
  fixture, GCC 16 (scratch timing, median of 5 interleaved runs): tight 185 -> 62 ns, default
  slack after 2,000 read-write rounds 194 -> 87 ns; the hand-written floor (compact into a fresh
  arena, then send) is 115 ns. Instructions (callgrind, GCC 16 / P2996): 1,853 -> 1,442 /
  2,027 -> 1,801, a nearly empty root 475 -> 194 / 618 -> 378. SLG A 0.974 / 0.963; B to D,
  formal, delta and build cells unchanged.
- **Changed default:** `XSaveOptions{}` is `XWireEmission::automatic`. `save_verified` and
  `save_verified_into` send an image whose free blocks are at least 512 bytes and at least an
  eighth of it as its compact wire (`save_compact`, default slack), and any other image as before
  (`free_runs`). The choice reads the header's free bytes: a clean send runs the same instructions
  as before. The receiver gets the same contents with no free block. `free_runs`, `dense` and
  `zero_runs` still send the image; tests and the cross-compiler corpus that pin image bytes now
  ask for `free_runs`. `save_compact_into` is inline up to its reused-output path (P2996 1,801 ->
  1,476 instructions). Character after churn, send plus trusted receive vs zpp::bits (premises
  sweep, 15 processes, GCC 16): 11.5 KB 1.27-4.3 -> 0.33-0.36, 46 KB 1.01-3.5 -> 0.59-0.62; with
  the full check 1.65-4.96 -> 0.54-0.57 and 1.27-4.31 -> 0.73-0.77; clean cells unchanged. Below
  12 KB of image the trusted pair is 0.85 to 1.17 of before (copying an image that stays in L1
  costs about as much as the compact walk; 2.9 KB Character about +5%, still 0.41 of zpp::bits),
  the checked pair 0.52 to 0.75. Spare capacity inside containers (no free block) is not in the
  header; the next entry counts it.
  SLG demo, M scale (time): B 0.82, S save and receive 0.57-0.86, A 0.98 (its responses: 9% of
  them go compact, a ~4 KB message of many small blocks, +5.8% for encode plus decode). SLG
  instructions (GCC 16 / P2996): A 0.996 / 1.005, B 0.980 / 0.974, C 1.000 / 1.000, D 0.978 /
  1.000; on P2996 slg.A rises in `A.apply`, battle results written into players that arrived
  compact (small vectors without spare capacity), not in a send or receive.
- The automatic emission counts spare capacity. A vector grown in place and shrunk back keeps its
  spare capacity inside its live block, where the header's free bytes do not see it, so such an
  image went out whole (the premises g8 cells, every vector grown 8 times and shrunk back: 7 to
  10 times zpp::bits). A container whose size falls from above a quarter of its capacity to a
  quarter or below, with a capacity of at least 1 KiB, now registers its head in a host-side list
  of up to 8 heads (allocated on the first registration, never on the wire). When the free bytes
  alone keep the image, the automatic emission reads the listed heads from the image, drops the
  ones grown back above half of their capacity or no longer read as a head inside `used_end`, and
  applies the same rule to the free bytes plus the spare. The list follows move, swap and clone;
  reload and reset drop it. Every in-place shrink registers (`pop_back`, `erase`, map erase,
  `resize`, `clear`, `stable_erase_if`, a shorter `assign_range`/`assign_bytes`, POD assignment);
  `XString` does not. Premises sweep (GCC 16, 15 processes, send plus receive ÷ zpp::bits): g8
  trusted 1.38-8.83 -> 0.32-0.59, full check 2.28-10.01 -> 0.57-0.75, the wire now the compact
  wire (12,392 -> 992 bytes at k = 1/4, 717,680 -> 53,184 at k = 16); k = 1/4 g4 likewise
  (0.71 -> 0.47); the other 442 cells send the same bytes with at most 3 more instructions per
  message (30 of them 13 to 62 fewer). Instructions (ops cells, GCC 16): push
  and pop +1, append 8 and resize back +2, insert and erase in the middle of a vector +3, map
  insert and erase +6; the clean send unchanged (the flag shares the free state's 16-bit word: one
  load and compare); checked receive -12 (`reload_into`'s small path is inline); building a
  Character element by element +65 (1.7%; no hook runs there: GCC inlines differently inside that
  build function). Perf gate (GCC 16 / P2996): map and set erase +2.0 to 2.5% / +2.4 to 3.0% (the
  quarter test runs on every erase from a container with at least 1 KiB of capacity), slg
  `A.apply` +1.9% / +2.8%; every scenario total within 0.3%. The hooks keep the callers' code as
  on main: `clear()` tests the old size (a capacity load there was kept by the refill loop that
  followed), an assignment in place tests the capacity after the copy and reads the old size out of
  line, `erase` has one hook after its branches, the flat map and set `erase(key)` are inline at
  the caller under Clang, and a move assignment between payloads copies out of line.
- The automatic emission counts the spare capacity of freshly built messages from 16 KiB of
  image. `push_back` doubles a vector's capacity in place, so a message built element by element
  can be up to half spare; nothing shrank, so the spare watch does not list it (premises k = 5,
  every container just past a doubling: 23,544 bytes sent whole). From 16 KiB of `used_bytes`
  (`wire_spare_scan_min`), when the free bytes and the watched heads alone keep the image, the
  choice also reads the vector, map and set heads inside the root object (its members, the
  records it holds by value, fixed arrays; not the heads inside container elements, not strings;
  a root type with more than 64 such heads, `wire_spare_scan_max_heads`, is not read) and counts
  capacity - size - (size / 8 + 1) elements of each, signed (the compact wire keeps size / 8 + 1
  spare elements, so a vector filled to capacity counts minus that slack); the same rule then
  applies (at least 512 bytes and an eighth of the image). Below 16 KiB the image copy stays in
  L1 and beats the compact walk whatever the spare. No container hook, no state, no wire change:
  the heads' positions are known at compile time, and `save_verified_into` reaches the scan from
  the compare that already bounds its small path (16 KiB instead of 1 MiB for such a root, out
  of line from there). Premises (GCC 16): k = 5 23,544 -> 16,792 bytes, k = 17 93,432 -> 56,488;
  send plus trusted receive 0.45 and 0.77 of before (0.35 and 0.61 of zpp::bits; 15 processes),
  with the full check 0.51 and 0.73; instructions 0.67 and 0.58. The other 494 cells send the
  same bytes. Instructions per message: below 16 KiB the same (the shrunk g4 and g8 cells +1);
  from 16 KiB, images that stay the image +77 (the scan of Character's 5 heads; k = 16 to 256),
  churned images that go compact on their free bytes -18 (they no longer pass through the
  general path). Ops cells unchanged except building a Character element by element, +25 (0.4%;
  nothing of this runs there: GCC inlines a container accessor differently inside that build
  function). SLG instructions (GCC 16): A, D and S unchanged, B -0.8%, C +0.01%.
- `XBuffer::relayout<T>()` and `TypedXBuffer::relayout()`: a sender that edits its content in
  place tick after tick rewrites it, in place and with the same values, as the image
  `save_compact` writes (no holes, vectors with the slack of `XCompactOptions{}`). Freed blocks and
  spare capacity never left such a buffer, and every path that pays per used byte paid for them:
  delta compares, key deltas, `touches` runs over whole containers, `clone()`, the replicas'
  memory (after 100,000 churn edits `used_end` was 2.2-2.5 times the compact size). The lineage
  stays (proxies and `XHandle` re-resolve), the epoch advances; no delta code changed: a copy
  baseline (also with `touched_only`) compares the whole payload once, a `touches` baseline sends
  a key delta. `relayout_advised<T>()` is true once `used_end` is twice the size of the last
  relayout of the content (measured with one walk on the first call for new content, then 13
  instructions). Refused with nothing changed: inside a pin scope (`borrowed`), with a recorded
  failure, without a verified root of `T` (`no_root`), on a storage or scratch allocation
  failure. Never for a replica that applies deltas. Two words in `XBufferCore`'s padding (size
  unchanged); no other path changed. Cost (GCC 16, Character fixture): one relayout k = 1
  0.50 us / 2.2 K instructions, k = 16 9.7 us / 99 K. Steady churn with relayouts as advised
  (instructions per tick of the hint, the relayout, the delta and its admit receive; the edit not
  counted), k = 1 / k = 16: compare-all -13% / -26%, `touches` -1% / -35%, `touched_only` -1% /
  -5%, whole-state send and reload -12% / +1.7%; the sender's (and each replica's) mean `used_end`
  8,280 -> 5,982 and 128,760 -> 74,618 bytes. USER_GUIDE section 4.7, DELTA_SYNC section 5.
- Fixed: a client that includes `<windows.h>` before the library compiles again (a variable in
  `save_delta_into` was named `small`, which `rpcndr.h` defines as a macro).
- `xoffsetdatastructure.hpp` includes seven parts in its former order: `xoffset_arena.hpp`,
  `xoffset_containers.hpp`, `xoffset_wire.hpp`, `xoffset_schema_hash.hpp`, `xoffset_buffer.hpp`,
  `xoffset_compactor.hpp` and `xoffset_registration.hpp`. Includes do not change; the code is the same.
- `xoffset_config.hpp` holds the compiler and platform detection, the attribute macros (24 per-file
  macros are 11 shared ones) and the build switches with their defaults; the storage headers include it
  too. Removed: the `XOFFSET_*_API` feature macros (every feature they announced is always present),
  `XOFFSET_REGISTER_TYPE` / `_CONTAINER` / `_MAP` and the `XOFFSET_REGISTER_MIGRATION_*` macros,
  `XFlatSet` / `XFlatMap` (the same types as `XSet` / `XMap`), `XFixedBlob`,
  `schema_snapshot_body` and unused internal helpers. `is_v1_wire_admitted_v` is now
  `is_wire_admitted_v`.
- `XString`, `XSet` and `XMap` are one class each: `XFixedString`, `XFixedFlatSet` and
  `XFixedFlatMap` are gone, and `XFixedVector` is `XVectorCore`, the base of `XVector`, `XSet` and
  `XMap` (no longer a field type). Behavior, layout and wire bytes are unchanged.
- The `XBuffer` and `TypedXBuffer` load, reload and delta-apply entries take the bytes as one
  `XByteView` (a `std::string`, `std::string_view` or byte span) instead of a string and a span
  overload, and the validation memory resource as a defaulted `upstream` argument: the
  `*_with_resource` entries are gone (`reload_verified` with a null `upstream` uses the buffer's own
  scratch). Removed aliases: `segment_size()` (use `get_size()`), `arena_allocator<T>()` (use
  `allocator<T>()`) and `TypedXBuffer::ref()` (use `root()`).
- Removed the experimental direct receive (`begin_verified_receive`, `finish_verified`, the second
  validator `receive_raw::validate`, the custom lifetime hooks `wire_activation` and
  `wire_future_lifetime`, `canonical_bytes_at`, `freeze_verified` / `VerifiedSnapshot` and
  `VirtualMemoryStorage::for_receive`), the optional shared validation scratch pool
  (`XOFFSET_BUILD_VALIDATION_SCRATCH_RUNTIME`, `XOffset::ValidationScratch`) and the snapshot index
  (`XOFFSET_BUILD_SNAPSHOT_INDEX_RUNTIME`). Receiving goes through `load_verified`, `reload_verified`,
  `begin_verified_reload` and `XReceiver` as before. With them went the owner index and the storage's
  second byte origin that only the direct receive used (the storage is 8 bytes smaller, the owner keeps
  its size and its per-line layout), and an empty loop over the live elements that in-place container
  growth still ran.
- A vector that grows into a new block moves trivially copyable elements of up to 16 bytes with one
  block copy instead of one element at a time (the loop reloaded the size on every element). 64
  `push_back` calls of a `float` into an empty vector, regrowing on the way: 2,097 -> 1,885
  instructions, 171-184 -> 120 ns.
- A buffer that gives its storage back (to a slot, to the pool or to the system) passes on how far it
  actually wrote: on a settled buffer (exact free table, nothing written past its used size or the extent
  of its last reset) that is its used size, so the next owner of that slot or mapping clears only those
  bytes as it grows instead of the whole length the old owner reached. A buffer that is not settled (a
  load that ended below what the storage held, an unverified load) passes on the old length. Building
  the CppCon 2024 character into a new buffer each time with `assign_sorted`: 3,999 -> 3,283
  instructions, 231-243 -> 184-187 ns.
- `XString::assign(std::string_view)` is always inlined and its general path (allocation, growth,
  checks) is out of line on every compiler, as it already was on Clang. Growing a short string to a long
  one and back: 93 -> 64 instructions, 4.1 -> 2.6 ns; setting a short string: 43.5 -> 33.5, 1.7 -> 1.2 ns.
  The build cells take 9 fewer instructions but measure 4-12% slower in time (cause not found).
- Shorter small-block allocation and retirement, same placement: an empty vector's first block (from
  `assign_range`, `reserve` or a first `push_back`) is taken in one call that runs the exact-bin hit or
  the tail bump inline; a retired block entering a bin with room calls nothing; the tail bump and the
  retired block's range are one compare each (`used_end` stays within the payload on every load, reset
  and delta); and a block's offset from its head is stored without the 32-bit reach check (a payload is
  under 2 GiB). Adding an equipment record with an 8-float array and removing it: 302 -> 220
  instructions; filling the CppCon 2024 character without its map: 1,992 -> 1,481.
- Small deltas take a shorter path on both ends; the bytes on the wire are unchanged. A sender with an
  `XDeltaRecord::touches` baseline whose touch log lists at most 8 blocks, with the same hole table,
  used size and root and no digest, writes the touched bytes in one pass; an admitting receiver
  (`XReceiver` on a trusted link) applies a delta with no flags and at most 8 blocks in one pass over its
  bytes, with the same checks. `tests/test_delta_small.cpp` holds both against the general path byte for
  byte. One `touch_set` of a scalar plus `save_delta_into`: 926 -> 436 instructions; with the apply:
  1,550 -> 767. `save_delta_into` also takes a root the buffer has cached instead of looking it up.
- `XSet` / `XMap` `assign_sorted(range)` (also an iterator pair): fills from input already sorted by
  key in one exact block, without lookups; a contiguous range of the element type is one copy. 64
  entries of `XMap<std::uint64_t, 24-byte record>`: 39 ns from a same-layout vector, 58 ns converted
  entry by entry; the CppCon 2024 build with it 152-162 ns (289 ns with 64 `try_emplace` calls). New
  switch `XOFFSET_CHECK_SORTED` checks the order (1 without `NDEBUG` or with `XOFFSET_HARDENED=1`,
  else 0).
- Single-element inserts of trivially copyable elements that grow the container (`push_back` of
  elements above 16 bytes, `emplace_back`, `XMap::try_emplace` / `XSet::insert` at the end or in the
  middle) copy the element into a local first and grow out of line without it, which removes a
  store-forwarding stall per growth: 64 ascending `try_emplace` calls 520 -> 167 ns, the CppCon 2024
  build 623 -> 293 ns.
- `bench/premises`: where the send + receive lead holds, by message size (68 bytes to 729 KB), receiver
  checks and churn of the sender's buffer, against zpp::bits, cista, FlatBuffers, Protocol Buffers and
  cereal. `docs/PERFORMANCE.md` section 2 has the results; its reading notes no longer say the lead
  grows with message size or that small messages tie (both were wrong: the lead is largest at 3–12 KB,
  and small messages tie only under full validation). Churn matters most: one read/write pass makes
  the trusted tier lose from about 11 KB messages, and `save_compact` brings the lead back. README:
  the tuned read/write layout ties cista. Documentation and benchmark only.
- Apple clang client columns re-run on the v4.1.0 tag (macOS arm64 C++20 and C++23, iOS 27.0 simulator
  C++20): full CTest 211/211 each, the 24-file reference corpus accepted and written byte for byte.
  Documentation only; the last such run was for 3.3.0.

## 4.1.0: speed-first defaults

Hardening and zeroing become compile-time switches, off by default; the client uplink bounds check stays
on. Faster first growth of `XVector`, a fairer CppCon 2024 comparison, and an English-only, shorter
documentation set. Pre-release scorecard: 19 main cells, 16 wins, 3 ties, 0 losses
([PERFORMANCE.md](docs/PERFORMANCE.md)); pre-release fuzz with no findings; release validation 27/27.
Ir gate warnings (no failures, every total level or better): GCC `reloc.str.assign_short.copy` 1.039 and
P2996 `reloc.vec.push_back` 1.040 against 4.0.0; the same rows on the other compiler are level.

- `reset()` no longer zeroes the payload it discards (`XOFFSET_SCRUB_RESET`, 0 by default, 1 with
  `XOFFSET_HARDENED=1`). **Behaviour change:** the next content's slack, string tails and padding
  carry the previous content, so a send buffer used for different receivers in turn can put one
  receiver's data into the next one's `save_verified` wire or delta; build with the switch, send with
  `save_compact`, or keep a buffer per receiver ([threat model](docs/V2_THREAT_MODEL.md) 3.2 P4).
  SLG UP.send -4.1% instructions, UC.send -2.6%, a reset alone -43%.
- New switch `XOFFSET_CHECK_UPLINK`, **1 by default**: `-DXOFFSET_CHECK_UPLINK=0` gives
  `XReceiver<XLink::from_client>` the trusted links' default (`XCheck::admit`), for deployments whose
  clients are their own code over a protected transport; any client can then make the server read or
  write out of bounds. It saves 9.6% of the instructions of receiving an SLG uplink order. Default
  builds are unchanged.
- **Hardening is opt-in** (speed first). Measures that cost time on mutate and send are compile-time
  switches, 0 by default: `XOFFSET_SCRUB_POPPED`, `XOFFSET_SCRUB_ERASED`, `XOFFSET_SCRUB_PADDING`,
  `XOFFSET_PIN_QUARANTINE`, `XOFFSET_SAVE_SELF_CHECK`. `XOFFSET_HARDENED=1` turns on every one a build
  does not set itself (the behaviour up to 4.0.0). **Behaviour change:** a default build can carry
  bytes of removed elements and padding in `save_verified` output and deltas; use `save_compact` or a
  hardened build where a receiver must not see them ([threat model](docs/V2_THREAT_MODEL.md) 4.1).
- `XVector`: an empty vector's first block holds at least 4 elements and 32 bytes; `push_back` of a
  small value into a full vector takes a lean path. CppCon 2024 read/write loop, 2024 layout:
  203 -> 144 ns; send + receive 103 -> 83 ns. Wire format unchanged.
- CppCon 2024 bench made fairer: Protocol Buffers frees removed messages like every other library
  (its `RemoveLast` reuse is a report-only row); a new map key every iteration; report-only rows for
  send + receive of a buffer after the read/write loop.
- Performance documents updated with the 2026-11-17 review of every published comparison
  (`docs/PERFORMANCE.md` section 1). No verdict changed.
- Documentation: README rewritten as an introduction; `docs/GETTING_STARTED.md` with one runnable
  example per use case; repository is English only (`tools/check_no_cjk.py`); historical evidence
  removed from the tree (see `docs/ARCHIVED_EVIDENCE.md`). The source package now carries
  `docs/BUILD_AND_PLATFORMS.md`.

## 4.0.0: shared deltas in the library

Pre-release scorecard: 19 main cells, 14 wins, 5 ties, 0 losses ([PERFORMANCE.md](docs/PERFORMANCE.md)).
Release validation 27/27; pre-release fuzz with no findings.

- Shared delta publisher in the library (`xoffset_shared_publisher.hpp`: `XSyncPublisher`,
  `XSyncConn`, `XSyncMessage`, `XSyncDirtyTable`, `XSyncImageTable`); `XOFFSET_SYNC_CHECKS` checks its
  contracts in debug builds.
- **Breaking:** shared-delta format XS 2.0 replaces XS 1.0 (no XS 1.0 reader). Each entity is encoded
  once per tick and every message copies the bytes; no varints or byte masks.
- `xsync::XSyncSlots`: per-table id -> slot index for clients of `apply_sync`.
- Configure picks C++26 when the toolchain has reflection and otherwise builds C++20 with a note.
- `tools/perf_gate/repro_scorecard.sh`: one command from a ref to the SLG scorecard. bench/slg gains a
  zpp_bits shared-encoding rival for the D cells.

## 3.6.0: losing cells and memory

- Growth policy: `XGrowPolicy::Relocating` buffers move when they outgrow their storage. Proxies
  (`XRef<T>`, `ref<T>()`, named field accessors, batch operations) and pinned `edit()` / `read()`
  scopes stay valid across a move. **Breaking:** `TypedXBuffer<T>::root()` and `reset()` return a
  proxy instead of `T&`.
- Shared deltas send changed members only (`XRecordDelta<T>::fields`).
- Free table about 2.4 KiB instead of 12 KiB; retired blocks of at least 16 KiB return their pages
  (`MADV_DONTNEED`); memory 0.98x the most frugal rival.
- `XReceiver::view` prefetches wires of at most 1 KiB.
- Cloud x86 scorecard columns gate releases (`tools/perf_gate/x86_columns.sh`).

## 3.5.0: performance on the production platform

- Shared deltas are a product interface: `XReceiver<L>::apply_sync`, format XS 1.0;
  `fuzz_shared_delta`.
- Client uplink check: `XReceiver<XLink::from_client>::view<T>` and `XView<T>` (bounds tier,
  read-only, no copy).
- A delta announcing a payload above the replica's `maximum_capacity()` is refused before anything is
  copied.
- Faster small-message server send on x86 (`XBufferCore` 64-byte aligned, hot fields in the first line).
- `tools/fuzz_prerelease.sh --budget hour`.

## 3.4.0: delta sync on trusted links

- `XReceiver<L>::apply_delta<T>`: deltas checked at the link's tier. Delta format 1.0 frozen.
- Fuzzing covers the trusted tier.
- Faster `XMap` / `XSet` random-order inserts and small-message saves.
- VM storage keeps its host header inside the payload's first 64 bytes.
- Withdrawn: the `root<T>()` cache refill of 3.3.1 (slower on the Mac).

## 3.3.1: trusted receive

- `xoffset_receiver.hpp`: `XReceiver<XLink>` picks the receive check by connection type
  (trusted tier for server links).
- A maximum capacity above the 2 GiB payload limit fails at create with `capacity_exceeded`.
- Merges and releases are judged in the cloud only.

## 3.3.0: native delta sync

- Delta sync (`xoffset_delta.hpp`, [DELTA_SYNC.md](docs/DELTA_SYNC.md)): `XDeltaBaseline`,
  `save_delta_into<T>`, `XBuffer::touch` with `touched_only`, baselines without a copy; deltas are
  checked as strictly as `load_verified` but only where they can change the buffer.
- Fixed-size messages (`xoffset_fixed_message.hpp`, experimental).
- Pre-release fuzzing replaces nightly fuzzing (`tools/fuzz_prerelease.sh`), including delta sync
  and the client builds.
- Many hot-path improvements (inline small-block bump, root cache, in-place reloads keep the free
  table, one-pass `save_verified_into`). `bench/hft` added (record only).

## 3.2.1

- iOS test host serves by default; `demo/mesh` batch fixes. Library unchanged.

## 3.2.0: clients on native compilers

Includes 3.2.0-rc.1 and rc.2.

- **Client build path:** C++20/C++23 with one `XOFFSET_RECORD` field list per record; MSVC, Android
  NDK Clang and Apple clang build clients. Builds without exceptions (`-fno-exceptions`, `/EHs-c-`)
  with non-throwing receive entry points.
- Schema evolution: `xoffset_handshake.hpp`, `xoffset_evolution.hpp`
  ([SCHEMA_EVOLUTION.md](docs/SCHEMA_EVOLUTION.md)).
- Schemas that mean different things on different platforms (keys ordered by plain `char`, enums
  without a fixed underlying type) fail to compile.
- **Breaking:** container iterators are `fixed_detail::contiguous_iterator<T>` instead of `T*`.
  `T* p = v.begin()` no longer compiles; use `v.data()` or `std::to_address(it)`.
- Demos: `demo/mapstate` (Linux servers to native clients), `demo/mesh` (cross-device). iOS test host,
  `tools/schema_guard`, `contrib/zstd_dict`. Opt-in `XOFFSET_OWNER_MAP_SHARED`.

## 3.1.0: wire format 2.1

- **Breaking:** wire format 2.1 (40-byte header, 12-byte container heads). A 2.1 reader rejects
  2.0 wires.
- Faster dense small-message reloads and `XVector` inserts.
- Nightly libFuzzer without GitHub Actions.

## 3.0.0: first release of the C++26 line

- `save_compact` / `save_compact_into` (compact wire, message composition from parts);
  `XSaveOptions{.emission}`; `XBuffer::clone()` / `clone_into()`;
  `begin_verified_reload` for receiving directly into a buffer.
- **Contract change:** small buffers live on slots of a library slab (`xoffset_slot_storage.hpp`).
- Faster verified loads (bitmaps per 8-byte granule; a fast path for canonical wires).
- Release procedure ([RELEASING.md](docs/RELEASING.md)); V1/VM-era documents removed.

## 0.3.0-beta.1: production-readiness beta

- Untrusted-input hardening ([V2_THREAT_MODEL.md](docs/V2_THREAT_MODEL.md)), coverage-guided fuzzing,
  schema pins (`XOFFSET_PIN_SCHEMA`), cross-compiler wire corpus, GCC 16 support.
- In-place buffer reuse: `reload_verified<T>`.

## 0.2.0-alpha.1: V2 storage and wire format

- Header-less blocks, a binned free table that travels as the wire hole table, inline strings of up to
  10 bytes, sparse wires.

## 0.1.0-alpha.1

- Reflected root objects and offset containers in an address-stable VM arena, verified transport,
  schema registration, handles, and packaging tooling.
