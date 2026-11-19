# Changelog

Key changes per release. Measurement details and the long form of every entry are in the Git history
of this file and in the pull requests.

## Unreleased

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
