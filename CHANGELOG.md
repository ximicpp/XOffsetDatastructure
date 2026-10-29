# Changelog

## Unreleased

## 3.2.0: clients on native compilers

Against 3.1.0 this release adds the client build path. Clients describe each
record with one `XOFFSET_RECORD` field list (C++20 or C++23, no reflection)
and build with their platform's own compiler: Android NDK Clang, MSVC,
clang-cl, MinGW GCC, and Apple clang for iOS and macOS. Servers keep C++26
reflection, and both builds write the same bytes. It also adds builds
without exceptions, schema evolution with a handshake, compile-time
rejection of schemas whose meaning differs between platforms, the iOS test
host and the end-to-end map demo (Linux servers; Windows, Android, iOS and
macOS clients, an old-version client and client write-back). Wire format
2.1 is unchanged. The entries are in the 3.2.0-rc.1 and 3.2.0-rc.2 sections
below and in the list here.

Status at this tag: the client path is still a target, not a core
guarantee (K8, `docs/CLIENT_TARGETS.md`), and server-to-client use is not
announced. Every client toolchain in `docs/CLIENT_TARGETS.md` passes the
full CTest and writes the reference corpus byte for byte. Real devices: an
iPhone 15 Pro Max (iOS 26.6.2, through the test host app) passed CTest
138/138, the 24 reference wires and both demo clients, and measured SLG M
against FlatBuffers 25.12.19 on the same phone at A 0.305, B 0.179, C 0.252,
D 0.858 (`docs/DEVICE_TESTING.md` section 9); an Android 11 phone (Xiaomi
M2102K1C) passed the functional checks with 3.2.0-rc.1, without timing.
Android timing is the remaining condition for announcing the client path.

Mac M4 checkpoint against `v3.1.0` (CKPT5, `docs/PERFORMANCE.md` section 4):
D÷FlatBuffers 0.933 (upper bound 0.947; 3.1.0 was 1.117), SLG A/B/C
0.994/1.003/1.004 and the formal harness within 1.013, G8 met, G9 17/19 as
in 3.1.0. The four-format SLG step failed in CKPT5 and CKPT5b on intervals
widened by single outlier processes (once on each side). A third run
(CKPT5c) was pooled with both under a rule written before it ran: with all
18+18 processes, A/B/C are 0.999/1.001/1.005 and C's upper bound is 1.0216,
just over the 1.02 limit, so the rule stopped the release for susu's
decision; susu chose to release (2026-10-29). No single run shows C above 1
(1.014, 0.995, 1.007). `map.erase.fresh` runs two more instructions
because container iterators are now a class (rc.1 public API change), at
most 0.5% by the CKPT4 machine-code reading. The 7-night fuzz run is no longer a release
condition (susu 2026-10-28).

- `demo/mesh`: a cross-device data demo (off by default,
  `XOFFSET_BUILD_MESH_DEMO`). Devices register at a hub that relays every
  message; an object of one of six everyday types travels device to device
  as its wire, and each device verifies it, compares it with a reference
  model, edits it in place and sends it on. No library change.
- iOS test host: `XO_HOST_SERVE=1` keeps the app up and runs the programs
  one after another in its process (the default stays one launch per
  program); iOS skips the tests' fork sections; `sign_ios_app.sh` finds the
  stub app in its build directory. `bench/slg` takes `SLG_XOFFSET_CAP_MIB`
  to reserve less address space for the player, battle and report buffers
  (unset: 256 MiB, as before); the iPhone run uses 2 MiB.
- Opt-in `XOFFSET_OWNER_MAP_SHARED`: the owner map becomes a runtime table,
  taken from `xoffset_owner_map_shared(bytes)` when the process exports it
  and otherwise mmapped, so many shared libraries in one process share one
  table instead of 64 MiB of BSS each. The test host's program libraries
  use it, and the serving host restarts when address-space headroom runs
  out, so serving mode fits an iPhone's address space; serving mode stays
  off by default until a phone run confirms it. Without the macro the
  library and its machine code are unchanged (#183).
- Wire schemas that mean different things on different 64-bit
  little-endian platforms fail to compile: a set or map key that orders by
  plain `char` (directly, in an array or record member, or an enum whose
  underlying type is `char`), which Linux and Android arm64 sort unsigned
  and x86-64, Apple and Windows signed, so each side rejected the other's
  wire; and an enum without a fixed underlying type (`int` on MSVC,
  `unsigned int` on GCC and Clang). Use `std::int8_t`/`std::uint8_t` or
  `XString` keys and `enum class` or `enum E : std::uint32_t`. Signatures
  of other types are unchanged. USER_GUIDE 2.2 lists the portable scalars.
- The cross-compiler corpus adds two `save_compact` wires (slack 0 and 8)
  and, for every case, the reference wire edited by fixed steps and saved
  with `save_verified` and `save_compact`: a client that edits a wire
  written elsewhere must write the writer's bytes
  (`test_cross_compiler_wire --read`, `--mutate-ref`).

## 3.2.0-rc.2: preview with iPhone steps

The second preview of 3.2 for device testing; not a release, not announced.
Against 3.2.0-rc.1 it adds the iPhone procedure (a test host app that runs
the library tests, the wire corpus, SLG timing and the memory probe on iOS),
SLG timing and the memory probe in the Android phone script,
the Apple fix that kept a 64 MiB zero table in every Mach-O executable, the
schema and wire gate (`tools/schema_guard`), small-message compression, an
old-version client in the map demo, and no-exceptions receive at the speed of
builds with exceptions. Wire format 2.1 is unchanged.

Status at this tag: 3.2.0-rc.1 passed on one Android phone (Android 11,
CTest 114/114, wire corpus, both demo clients) and on Windows, macOS and
Linux machines (record below). The iPhone steps (`docs/DEVICE_TESTING.md`
section 7) passed on the iOS Simulator through the test host and have not
run on an iPhone; further device testing is paused (susu 2026-10-28). No
Mac checkpoint ran on this commit; the last one (CKPT4, 2026-10-28)
measured D÷FlatBuffers 0.933.

- iOS test host (`-DXOFFSET_TEST_HOST=ON`, default OFF, `tools/test_host/`):
  every executable is also built as a library that a UIKit host app loads
  one per launch; `tools/test_host/xo_host_run.py` drives it as the CTest
  emulator for `ios-device.cmake` and `ios-simulator.cmake`, and
  `tools/device/ios_device.sh sim|device|local` runs the steps build, info,
  net, ctest, corpus, slg, mem and clients. Phone programs
  (`-DXOFFSET_PHONE_PROGRAMS=ON`): `xo_slg_bench` (SC-6, the frozen SLG
  workload on the field-list backend, optionally against FlatBuffers) and
  `xo_mem_probe` (SC-12: largest capacity, live reservations, committed
  memory until the system stops it). Client size (SC-14,
  `tools/client_size/`, numbers in `docs/PACKAGING.md`).
- The test host forwards `operator new` (host API version 2): each hosted
  library registers its replacement forms when it loads, so allocations the
  C++ runtime makes for a program reach the program's replacement; the
  host's own threads keep the default forms. New CTest
  `test_host_new_forwarding`. iOS client size (Xcode 27.1, arm64 `-Os`,
  stripped): library +58,200 B (+57,520 B without exceptions), +9,601 /
  +7,251 B per extra schema (`docs/PACKAGING.md`).
- `tools/device/android_device.sh` adds the steps `slg` (verify, then
  timed runs against FlatBuffers 25.12.19, with thermal and battery state
  before each) and `mem` (`xo_mem_probe`); `docs/DEVICE_TESTING.md`
  describes both and the iPhone procedure (section 7).
- C++20/C++23 builds: `test_schema_guard_slg_values` (added with the schema
  gate) is built only with C++26 reflection; it had broken the field-list
  configure.
- Apple: `owner_map_table` (64 MiB, zero-initialized) is placed in
  `__DATA,__bss` explicitly. As a weak zero-initialized variable Mach-O put
  it in `__DATA,__data`, so every executable carried 64 MiB on disk (iOS
  clients measured 67 MB). Code and access path are unchanged.
- `tools/schema_guard`: a cross-platform schema and wire gate. It compiles
  a probe of each schema on every target, compares layouts and the wire
  each target writes, and `build.sh --schema-gate full` /
  `run_checked.py` require the complete matrix (`--schema-gate local`, the
  default, keeps the platform build and CTest).
- Small messages: `contrib/zstd_dict`, a standalone C++17 component for
  zstd dictionaries bound to one schema (frames leave out the fixed wire
  header; checks before decoding and allocation; batching), and an
  optional unit event stream in the map demo with raw, zstd, dictionary and
  batch encodings. A single event is now the size of FlatBuffers' (1.00x),
  a batch 0.96-0.98x. The library headers are unchanged.
- Map demo: an old-version client. Schema version 1 is frozen
  (`include/mapdemo/schema_v1.hpp`, `schema.lock`); version 2 adds a field.
  The demo uses the library handshake, and A converts snapshots for
  `mapdemo_client_v1` while serving version-2 clients on the same server.
- Map demo: client write-back. With `--edits N` (on in every tier) a
  client edits a received snapshot in place in its own verified buffer and
  sends it with `save_verified`; A verifies it with a 1 MiB cap, checks the
  digest and field rules, and forwards a compacted rebuild to the other
  clients of the same schema version, which verify it again. It passed on
  the cloud Linux run and on the Mac (macOS, Android emulator, iOS
  Simulator; `demo/mapstate/RESULT.md` section 8).
- MSVC: no warning C5030 from `register_owned_bitmap` (it used
  `[[gnu::noinline]]`; now `XOFFSET_DETAIL_NOINLINE`).
- Device and platform results for 3.2.0-rc.1 (commit 39de636 only): an
  Android 11 phone, Windows and macOS machines, a remote Linux VM and WSL2
  (`docs/validation/2026-10-28-rc1-platform/README.md`). Functional only; no
  iPhone, no bare-metal Linux, no performance.
- MSVC builds of this project compile its sources as UTF-8 (`/utf-8`); a
  test failed to build on code page 936. The public headers are ASCII
  (comments, and the path marker of `xoffset_evolution.hpp`), so consumers
  need no source charset option.
- `tools/device/android_device.sh`: the demo ports start at 17800 (were
  47800, inside Android's ephemeral range, where `adb reverse` can fail) and
  `XO_PORT_BASE` picks another start; `device.txt` records the board when
  `ro.soc.*` is missing (before Android 12).
- Result pages of `tools/platform_matrix.sh`, `tools/native_targets.sh` and
  the map demo read the commit from `.xo-commit` in an exported source tree;
  `native_targets.sh` also records the machine.
- Builds without exceptions verify as fast as builds with them. Without
  exceptions the string checks of member and element loops were no longer
  inlined (GCC) or tested a status after every element (Clang, clang-cl);
  an inline string is now checked in place there. The demo client's
  receive took 1.09-1.44x as long without exceptions on normal views,
  now 0.89-0.99x (GCC 16, GCC 13, MinGW, MSVC, clang-cl). Builds with
  exceptions are unchanged.

## 3.2.0-rc.1: preview for device testing

A preview of 3.2 for testing on real Android phones and iPhones; not a
release, not announced. Against 3.1.0 it adds the client build path (C++20/C++23
field lists, Android NDK, MSVC, clang-cl, MinGW, Apple clang), builds without
exceptions, schema evolution with a handshake, and the end-to-end map demo
(Linux servers with C++26 reflection, clients on Windows, Android, iOS, macOS).
Wire format 2.1 is unchanged.

Status at this tag: real devices are untested (the reason for this
preview; `docs/DEVICE_TESTING.md` is the procedure). The Android emulator (API 35
arm64, Mac M4) and the iOS Simulator (iOS 27.0, Xcode 27.1, 129/129) passed the
full CTest, the wire corpus and the demo. No Mac checkpoint ran on this commit; the last Mac
gate (GATE_D4, for #153) measured D÷FlatBuffers 0.908 (upper bound 0.918), and
the mainline checkpoint against 3.1.0 is still due. The 7-night fuzz is no longer a 3.2 condition (susu
2026-10-28); the nightly runs are paused. G9 stays at 17/19. The iOS device
procedure (test host app, signing) follows in the next preview.

- Client build path, field-list backend: with C++20 or C++23 (no reflection)
  each record lists its fields once with `XOFFSET_RECORD`; the library walks
  records through the schema interface (`xoffset_schema.hpp`) in both
  backends, and the C++26 reflection build checks every field list against
  the reflected type. Both builds write the same wire bytes. Toolchain files
  and `tools/native_targets.sh` for Android arm64 (NDK r29/r30), Windows x64
  MSVC 19.44, clang-cl 19 and MinGW GCC, and the iOS Simulator
  (`docs/CLIENT_TARGETS.md`, user guide 2.1).
- Schema evolution: `xoffset_handshake.hpp` (schema hello/ack with a
  field-name digest, `peek_wire_fingerprint`), `xoffset_evolution.hpp`
  (server side, C++26 reflection: per-peer receive and send of older schema
  versions), the schema freeze tool and lineage (`docs/SCHEMA_EVOLUTION.md`).
  Clients need only the handshake.
- Verified reload of a dense small message is faster: `root<T>()` returns
  the root admitted by the last make or verified load while its epoch is
  current, and the reload skips checks of bytes it just wrote.
- End-to-end map demo (`demo/mapstate`): Linux servers A/B (GCC 16, C++26
  reflection) send dense, compact and zstd snapshots to clients built with
  C++20 field lists, with and without exceptions; recorded runs for Windows
  (MinGW, MSVC, clang-cl), Android emulator, iOS Simulator and macOS in
  `demo/mapstate/RESULT.md`.
- SC-8: builds without exceptions (`-fno-exceptions`, MSVC `/EHs-c-`) for
  engine clients. Non-throwing receive entries `try_load_verified`,
  `try_load_verified_with_resource`, `try_reload_verified`,
  `try_reload_verified_with_resource`, `try_begin_verified_reload` and
  `VerifiedReloadToken::try_finish` (and the `TypedXBuffer` forms) return
  `XResult<T>` / `XStatus` with an `XErrc` code and the exact message the
  throwing entry throws. `XException::code()` is new; `XException` and
  `XBadAlloc` moved to the new header `xoffset_error.hpp` (installed with
  the storage package). Without exceptions, failures that cannot be
  returned call the handler set with `set_fatal_handler` and abort.
  Validation is unchanged: the fuzz core compares every rejection of
  `try_load_verified` with `load_verified`, and 24 test programs, the OS
  layer test and the validator fuzz run without exceptions in CTest.
  Raw validation and verified receive have no non-throwing entry yet.
  `xoffset_handshake.hpp` loads handshake messages through
  `try_load_verified` in builds without exceptions.
- Nightly fuzz covers schema evolution: `tools/fuzz_libfuzzer.sh` runs
  `fuzz_schema_evolution` by default (targets v2_wire, wire_schemas,
  schema_evolution), and the GCC 16 replay in `tools/fuzz_nightly.sh` replays
  `tests/fuzz/regressions/schema_evolution/` and its seed corpus through
  `fuzz_schema_evolution_replay`.
- MinGW GCC: a program with two translation units that include
  `xoffset_error.hpp` failed to link (`multiple definition of TLS init
  function for ...pending_error`), with or without exceptions. The pending
  error is now a function-local `thread_local`; `test_vm_storage_noexc` is
  built from two translation units.

### Public API change: container iterators are a class, not `T*`

`XVector`, `XSet`, `XMap`, `XFixedVector`, `XFixedFlatSet` and `XFixedFlatMap`
now use `fixed_detail::contiguous_iterator<T>` for `iterator` and
`fixed_detail::contiguous_iterator<const T>` for `const_iterator`. Before this
change they were `T*` and `const T*`. The new iterator is a
`std::contiguous_iterator` whose `operator*` returns `T&`. That tells the
compiler every element can be read, so a `for (const auto& e : v)` loop with a
conditional field read is if-converted and vectorized, as it is over
`std::vector`. With a raw pointer the loop stayed scalar with a branch.

On the Mac (GATE_D3, against `f0308ee7`) SLG D got faster: D.read went from
82.2 to 67.0 µs, and D÷FlatBuffers from 1.125 to 0.898 (upper bound 0.912),
so O4 is met. A/B/C did not regress (largest upper bound 1.0107), G9 stays
at 17/19, and D on x86 is not slower.

Still compiles:

- range-for, `begin() + n`, `it - v.begin()`, `it[n]`, `&*it`, `it->x` and
  comparisons;
- `v.end() == nullptr`;
- passing a `T*` where an iterator is expected, e.g. `v.erase(ptr)` or
  `v.insert(ptr, x)`;
- std algorithms, `std::span(v)` and `assign(v.begin(), v.end())`.

`iterator` converts to `const_iterator`, not the reverse.

No longer compiles. Use `v.data()` or `std::to_address(it)` instead:

1. `T* p = v.begin();`
2. passing `v.begin()` to a `T*` parameter;
3. `std::memcpy(v.begin(), ...)` and other `void*` parameters;
4. `std::is_pointer_v<XVector<T>::iterator>`;
5. `T* q = it;` for an iterator `it`;
6. `if (v.begin())` as an emptiness test (use `v.empty()`).

`XString` is unchanged. At `-O0` a range-for over a container is about 2.4
times slower per element than with `T*`, but it is still faster than the same
loop over `std::vector`.

## 3.1.0: wire format 2.1

Wire format 2.1 replaces 2.0 (breaking: senders and receivers upgrade
together; 3.0.0 readers reject 2.1 and 3.1.0 readers reject 2.0). Same
toolchains as 3.0.0. Mac M4 at the INT4 checkpoint against `v3.0.0`
(`docs/PERFORMANCE.md` section 3): SLG A 0.991, B 0.951, C 0.950, D 0.846;
formal read_sum 0.950, mutate 0.970, emit_after 0.869 (largest upper bound
0.981). Known gaps: D is still slower than FlatBuffers (1.117, upper bound
1.156), two G9 items are still over 1.05 (`vec.push_back`, `set.erase.fresh`), schema
evolution is not supported, and iOS, Android and MSVC full builds are not verified (`docs/CLIENT_TARGETS.md`).

Release goals (roadmap O1-O7) and where this release stands:

- O1 wire 2.1, frozen for 3.x: R7 (40-byte header, no payload header on the
  wire) and R4 (12-byte container heads) are both in. Bytes per message
  against 3.0.0 (target A.req, A.res, B <= 0.92, D <= 0.70, formal M final
  wire <= 0.94), cloud GCC 16: on the measure the targets were set with
  (M scale, cycle 20, mean per message) A.req 1508 -> 1253 B (0.831), A.res
  1743 -> 1488 (0.854), B 15657 -> 13667 (0.873), D 309 -> 197 (0.638), all
  met. Report only, on the last cycle of the default two-cycle run: A.req
  0.820, A.res 0.902, B 0.857, D 824 -> 712 (0.864; the same 112 bytes
  saved on a larger report). Formal M final wire
  12,627,496 -> 11,182,856 B (0.886), met; formal M input wire 0.917.
  `emit_after` against cereal
  (target UB <= 0.20): met, upper bound 0.168 and 0.170 in two batches (3.0.0
  in the same batches: median 0.188 and 0.194). 2.0 wires are rejected with
  the version named.
- O2 no regression against `v3.0.0`, G1-G3 and G8 still met: met for the
  regression check (INT4 L2 pass, see above) and G1-G3 (A, B, C upper bound
  <= 0.50 against cereal, protobuf and FlatBuffers; largest A against
  FlatBuffers 0.318). G8 (zpp_bits, cista) was not measured in INT4; it is
  inferred from 3.0.0 (met) and the A/B/C ratios above, not measured.
- O3 G9 relocation tax (target 19/19 items UB <= 1.05; floor: no item worse
  than 3.0.0, each open item with a measured root cause): floor met, 17/19
  (3.0.0 in the same batch: 15/19). No item is slower than 3.0.0 by code;
  `vec.mid_insert` (1.092 -> 0.954) and `set.insert.fresh` (1.111 -> 0.995)
  now pass. Still open, with the cause measured on the Mac:
  `vec.push_back` 1.128, upper bound 1.136 (3.0.0: 1.287): `capacity_` now
  stays in a register, but the loop re-derives the data pointer from
  `data_delta` after every element store, because `data_delta` is an
  `int32_t` that an int store may alias (two more instructions per element
  than `std::vector`, 0.28 against 0.24 ns). `set.erase.fresh` 1.092, upper
  bound 1.102 (3.0.0: 1.130): keeping the head in registers across the move
  took 1.0 ns off each erase; the remaining 2.4 ns is in the erase step
  (bounds re-check, `begin()`/`end()` null checks, zeroing the vacated slot),
  not the lookup (`set.find.fresh` 0.755). Rejected levers: writing the
  whole head back or at one width (a narrow load after a 16-byte store
  forwards in 18.4 cycles on the M4), aligning large data blocks (at most
  1.5%), fusing the SHIFT `memmove` with the relocation pass, dropping the
  SHIFT overflow check. 19/19 moves to 3.2.
- O4 D against FlatBuffers (target UB <= 1.0, no API change, full validation
  kept): not met. D against FlatBuffers is 1.117, upper bound 1.156 (3.0.0:
  1.355 / 1.383), and D is 0.846 of 3.0.0. As decided by susu (2026-10-27),
  this release ships these speedups and the target moves to 3.2. At the
  upper bound D needs about 14% more (8.6 µs per cycle, about 4 ns per
  read). The gap is not instruction count (619 Ir per read on the cloud, 786
  for FlatBuffers) but stalls on the M4; the 619 Ir split: structure
  validation 30%, lookup and `root<T>()` 22%, copy and reinit 21%, root and
  free-table lookup 11%, header admission 6%. Plan for 3.2: (1) check on the
  Mac whether the narrow validation reads right after the payload copy hit
  the M4 store-forwarding stall, and if so change the copy shape for small
  payloads (5-10%, not yet verified, the largest unknown); (2) cache the
  validated root pointer (2-3%); (3) trim the validation context set-up and
  fixed lookup costs (2-3%). Together 9-16%, so reaching 1.0 depends on step
  (1).
- O5 stability: changed to a one-time fuzz run on the release candidate
  (susu, 2026-10-27); the seven-night runs move to 3.2. On the RC
  (`e913d22`): GCC 16 ASan+UBSan differential fuzz, 4 x 1,468,565 cases
  (2700 s), 0 inconsistencies, and a clean replay of regressions and seeds;
  libFuzzer (P2996 Clang, ASan+UBSan, `-fork=4`) 1800 s per target,
  `fuzz_v2_wire` 1.26M and `fuzz_wire_schemas` 25.8M executions, 0 findings;
  the 6504 inputs from the 3.0.0 corpora were all refused, the 6353 with the
  2.0 magic with `wire format 2.0 is not supported`. Each job ran less than
  the planned hour (a 30-minute outage). One finding before the RC, #130
  (harness), fixed in #131. 0 open fuzz-crash issues.
- O6 release procedure and documents: met in 3.0.0 (`docs/RELEASING.md`,
  `tools/prepare_release.py`, V1/VM documents out of the tree); 3.1.0 is the
  second release made with it.
- O7 platform matrix: on 3.0.0 code, full CTest passes on Linux x86-64 (GCC
  16, P2996), Linux arm64 (GCC 16, P2996), macOS arm64 (P2996) and Windows
  x64 MinGW-w64 GCC 16 (104 pass, 2 thread-cache tests skipped), and the
  2.0 corpus is byte-identical across them. On the release candidate
  (`e913d22`): met. Linux x86-64 GCC 16 and P2996 107/107, Linux arm64 GCC
  16 and P2996 107/107, Windows x64 MinGW-w64 GCC 16 105 pass and the same 2
  skipped, and the six 2.1 corpus files are byte-identical across all five.
  macOS arm64 (P2996) 107/107 at INT4. iOS was not done.

Changes:

- Documentation: `docs/PERFORMANCE.md` holds each release's Mac numbers, the
  protocol and the limits; README and `docs/USER_GUIDE.md` section 9 point to
  it instead of the round-3 report.
- Wire format 2.1 (breaking; `docs/V2_FORMAT.md` section 7). Senders and
  receivers upgrade together: a 2.1 reader rejects 2.0 wires with `wire format
  2.0 is not supported; this build reads 2.1 (sender and receiver must upgrade
  together)`, and a 3.0.0 reader rejects 2.1 wires.
  - The wire header is 40 bytes instead of 96: magic `'X' 'O' 2 1`, flags,
    `used_bytes`, `root_offset`, `hole_count`, `extent_count` (u32 each),
    `schema_hash`, `root_type_id`. `header_size`, the format fields,
    `endian_tag`, `crc32c`, `reserved0`, `schema_hash_hi` and `hole_bytes` are
    gone.
  - The 48-byte payload header is no longer sent; the receiver writes it from
    `used_bytes` and `root_offset`. The wire carries payload bytes from offset
    48, and every offset on it stays a payload offset. Every message is 104
    bytes shorter.
  - Container heads are 12 bytes (`data_delta`, `size_`, `capacity_`) instead
    of 16: a head no longer stores its payload (`arena_delta`); the owner is
    found from the head's address through a table with one byte per 4 MiB of
    address space, and only to allocate, grow or retire a block. Inline
    strings keep up to 10 bytes. `bind()`, `bind_like()` and `field_binding()`
    are removed.
  - Direct receive and reload: `payload_bytes()` spans payload
    `[48, used_bytes)`, so a sparse extent at payload offset `o` lands at
    `o - 48` and `complete()` reports `used_bytes - 48` payload bytes;
    `VerifiedSnapshot::segments()[1]` starts at offset 48.
  - Readers check the version before anything else and say which format they
    got (1, 2.0, or M.m).
- Performance: `XVector`'s `size_` and `capacity_` are `char32_t` (same bytes),
  a type that int and unsigned element stores cannot alias, and the general
  `push_back` path returns the head fields it changed; a `push_back` loop keeps
  `capacity_` in a register instead of reloading it after every element store
  (G9 measured `vec.push_back` at 1.29x `std::vector` on the Mac M4).
- Performance (Clang builds): an `XVector` insert of a trivially copyable
  element before the end, into spare capacity, runs inline at the call site
  and stores `size_` before moving the tail, and `erase` keeps the head in
  registers across the move. The flat `XSet` / `XMap` inserts and erases use
  the same paths. GCC and MSVC builds keep the previous code. (G9 measured
  `vec.mid_insert` at 1.10x, `set.insert.fresh` at 1.12x and
  `set.erase.fresh` at 1.14x their `std` baselines on the Mac M4.)
- The formal M input wire changes with the format; `bench/formal` pins the new
  one.
- Performance: `reload_verified<T>()` of a dense wire (no extent table) whose
  payload is at most 32 KiB, into a buffer whose storage already holds it,
  takes a straight path: one pass over the header words, the copy in place,
  and no free table when the wire has no holes. Admission, verification,
  errors and the loaded state are those of the general path (SLG D read:
  -32% instructions, P2996).
- Nightly libFuzzer without GitHub Actions or Docker (threat model F1, F2):
  `tools/fuzz_libfuzzer.sh` builds `fuzz_v2_wire` and `fuzz_wire_schemas` with
  the P2996 toolchain's own libFuzzer runtime (ASan+UBSan) and runs the steps
  of `fuzz.yml`'s libfuzzer job on the host: corpus replay, `-fork=4` fuzzing,
  merge, then `tools/fuzz_findings.py` findings and summary. The corpus is kept
  across nights as a tarball; debug paths map to `/workspace`, so signatures
  match the Docker jobs. Its `libfuzzer` result joins `tools/fuzz_nightly.sh`'s
  needs.json for one `tools/fuzz_report.py` report. `fuzz_report.py` now files
  the issue when GitHub refuses the `fuzz-crash/<signature>` branch push, and
  writes inputs up to 4 KiB into the issue as base64.
- Fuzz harness: a wire shorter than the V2 header no longer fails the
  direct-reload check. The transport refuses it before a reload begins, so the
  target keeps its root; the check required the root to be gone (#130, found
  by the first libFuzzer night; input in `tests/fuzz/regressions/v2_wire/`).

## 3.0.0: first release of the C++26 line

The tags `v1.0.0` (2024) and `v2.0-*` belong to the two earlier
implementations. This release is `next_cpp26` at the G8 checkpoint plus the
G9 fixes merged after it; the changes since 0.3.0-beta.1 are listed below.
Same V2 wire format as 0.3.0-beta.1 (its receivers accept the default
`save_verified` emission; `zero_runs` wires need 3.0.0 receivers), same
toolchains (P2996 Clang with libc++, or GCC 16 with libstdc++). Mac M4 at
the G8 checkpoint (SLG M scale, six formats in one process, upper bound of
the geometric mean, XOffset ÷ zpp_bits): A 0.778, B 0.261, C 0.365; D
(read-only report, tracked, not a goal) 1.589, and 1.383 against
FlatBuffers. The Mac checkpoint after the G9 fixes (CKPT3, 30af216 against
the G8 checkpoint 3549272) found no regression: formal upper bound 1.0074,
SLG A 0.979, B 0.970, C 0.990 (upper bound 1.009), P2996 CTest 106/106 on
macOS. Known gaps: the G9 relocation tax is not closed, the D scenario
is slower than zpp_bits and FlatBuffers, schema evolution is not supported,
and iOS, Android and MSVC full builds are not verified
(`docs/CLIENT_TARGETS.md`).

- Performance (G9 relocation tax; same wire bytes, same behavior):
  `XVector::push_back` restates `size_` after the general path; under Clang,
  the SHIFT rebase checks overflow once per moved run and the general path of
  `XString::assign` is out of line; copy-assigning an inline `XString` into a
  bound inline head copies 12 bytes. `bench/reloc` reports
  `str.assign_short.view` and `str.assign_short.copy`.
- Documentation: `docs/RELEASING.md` is the fixed release procedure (Mac
  checkpoint, local validation with TSan, the release PR, the three tag
  commands); `docs/USER_GUIDE.md` 4.3 explains the two conventions behind the
  A.apply / A.simulate gaps (one arena and free table per buffer; capacity
  ships in the wire and is allocated exactly) and what callers can do about
  them. `tools/local_validate.sh` gains a `tsan` step (GCC 16
  RelWithDebInfo, `-fsanitize=thread`, full CTest), run by `--release` and
  not by default.
- Repository: the V1/VM-era documents (`docs/VM_*`, `docs/V1_*`,
  `docs/BOOST_*`, 32 files), `experiments/` (126 files) and the root
  `HANDOFF*.md` / `PROMPT_MAC_20261020.md` left the tree;
  `docs/ARCHIVED_EVIDENCE.md` names the commit that still has them and the
  command that restores them. `docs/VM_PACKAGING.md` and
  `docs/VM_VALIDATION_RUNNER.md` are still current and became
  `docs/PACKAGING.md` and `docs/VALIDATION_RUNNER.md`.
- Release tooling: `XOFFSET_VERSION` may now hold a final version
  (`MAJOR.MINOR.PATCH`) as well as a prerelease (`-alpha|beta|rc.N`); the
  package config sets `XOffsetDatastructure_IS_PRERELEASE` to match and still
  refuses numeric, `EXACT` and range requests. `tools/prepare_alpha_release.py`
  is now `tools/prepare_release.py` and reads the version from the
  `XOFFSET_VERSION` of its own tree instead of a fixed `0.2.0-alpha.1`; its
  manifests use the schemas `xoffset-source/v1` and `xoffset-install/v1`. The
  source and install inventories (also in `tools/validate_vm_package.py`) now
  include `xoffset_slot_storage.hpp`, and the source closure no longer requires
  the off-by-default `demo/mapstate` subdirectory, so both tools work on the
  current tree again.
- GCC on MinGW and Cygwin (emulated TLS): the per-thread caches (slot free
  lists, free-table cache, owner cache) are off, because emutls can free a
  thread's TLS before its `thread_local` destructors run; threads that
  created buffers crashed or hung at exit. `XOFFSET_THREAD_CACHES` (0 there,
  1 elsewhere) selects this; Clang on MinGW keeps the caches. Other targets
  are unchanged.
- Owner registration: an ordinary owner (created, loaded or reloaded) keeps
  only its identity record and no longer enters the process-wide index or
  takes its mutex. Only direct-receive owners, the ones
  `canonical_bytes_at` can answer for, are indexed. Creating and destroying
  an owner costs one host allocation instead of two plus a global lock.
  The unused private `owner_from_payload_ptr` is gone. The CTest cases
  `test_owner_registry_fail_node`/`_fail_rehash` became
  `test_owner_registry_fail-record`/`_record-only`.
- Linux output prefetch: in a `save_verified` / `save_verified_into` output of
  at least 1 MiB, each copy of at least 256 KiB into pages that are not yet
  resident is preceded, per 256 KiB chunk, by `madvise(MADV_POPULATE_WRITE)`,
  so the kernel faults the pages in one call instead of one trap per page.
  Reused (resident) outputs and smaller ones are unchanged. Kernels before
  5.14 answer `EINVAL` once, after which outputs are copied plainly. Same wire
  bytes; Darwin and Windows code unchanged. On the formal M final state (12.6
  MB) a new output takes 36 page-fault traps instead of 3083 (cloud, GCC 16).
- Send path: `XSaveOptions{.emission}` picks the wire form of
  `save_verified` / `save_verified_into`; both APIs emit the same bytes for
  the same state.
  - `XWireEmission::free_runs` (default) skips each run of at least 4 KiB of
    adjacent free blocks of at least 256 B. It replaces N10 (which built a
    1 bit / 8 B free bitmap on every save of a buffer with 4 KiB of holes):
    only the bins of at least 256 B and the large blocks are visited, sorted
    by offset with a radix sort and merged. On the formal M final state it
    skips the same run as N10 (about 6x cheaper to find, cloud box), and on the
    golden model its bytes equal the earlier N10 pin. Skipped bytes are all
    holes, so 0.3.0-beta.1 receivers accept it.
  - `XWireEmission::dense`: one copy of the payload.
  - `XWireEmission::zero_runs` skips every zero run of at least 64 bytes
    (capacity slack, holes, zero fields), for cross-machine sends; the
    receiver loads a byte-identical payload, so spare capacity is kept (K5
    unchanged).
  Receivers no longer require gap bytes to lie in holes: gaps load as zero and
  the payload is validated as before, so the payload-level accept/reject set is
  unchanged (fuzzed: every sparse wire has the outcome and error of its dense
  equivalent); the wire-level accept set grows by sparse wires whose gaps
  cover live zero bytes.
- `save_verified_into` into an output whose capacity already holds the dense
  wire examines and normalizes the source once instead of twice.
- New `save_compact<T>(XCompactOptions)` / `save_compact_into<T>(out, ...)`
  write a compact wire (preorder layout, no holes) by direct traversal of the
  source graph, with no temporary buffer; the source is not modified. By
  default vectors of at least 8 elements (not strings) keep size / 8 + 1
  zeroed spare elements (`slack_div = 8`), so a receiver that edits them does
  not relocate on the first insert. With `slack_div = 0` capacity == size and
  the bytes are identical to `XCompactor::compact<T>(b).save_verified<T>()`
  (tested over 600 random models), 7x (840 KB shard) to 85x (1.6 KB) faster
  than compact-then-save on the cloud box.
- `save_compact_into<T>(out, options, xassign(target, source)...)` composes a
  message from parts: it writes the wire `save_compact_into<T>(out, options)`
  would write after `*target = *source` for every assignment, without doing
  the assignments (the draft and every source are unchanged; nothing is
  allocated in the draft). Targets are non-overlapping member subobjects of
  the root (any depth, scalars and array elements included), not container
  elements; sources are read as they were before the call, from any buffer
  or the draft itself. Target padding is written as zero. A bad target or a
  wire over the limit throws `XException` before any byte is written, and the
  output is then empty. Same wire format and receivers; byte-identical to
  assign + `save_compact` (test_save_composed, fuzz core). SLG A.req on the
  cloud box: A.req_encode x0.76-0.79, instructions per request 2060 -> ~1200.
  Plain `save_compact` / `save_compact_into` now write through the same
  `DirectWriter` (one head writer in the library; the old two-mode writer
  only measures now): same bytes, and a reused output with room is written
  without measuring first, also with slack. Cloud box, SLG M states, reused
  output: player x0.74 tight / x0.59 slack, shard x0.79 / x0.74, battle
  request x0.82 / x0.74; a state that grows before every save x0.81 / x0.62.
- Same-process whole-buffer copy: `XBuffer::clone()` (a new owner with the
  source's size, maximum capacity, growth policy and pool) and
  `clone_into(dst)` (reuses dst's mapping and address like `reload_verified`;
  dst's epoch advances; on failure dst is left an empty arena, a self copy is
  refused unchanged). One copy of the used payload; the exact free table,
  typed root access and schema binding come along, so no wire and no
  verification. `TypedXBuffer<T>` has both. See `docs/BUFFER_REUSE.md`.
- Nightly fuzz without GitHub Actions (threat model F4): `tools/fuzz_nightly.sh`
  runs the GCC 16 ASan+UBSan differential fuzz (`test_v2_fuzz`, 4 processes,
  date-based seeds, about 45 minutes) and the replay of
  `tests/fuzz/regressions/` and the seed corpora on any Linux host with
  Docker, writing findings.json per job. `tools/fuzz_report.py` turns those
  (or the Actions artifacts) into one issue per crash signature (`fuzz-crash`:
  a new signature opens it, a repeat comments, a closed one is reopened as a
  regression) and pushes the input to `fuzz-crash/<signature>` as
  `tests/fuzz/regressions/<target>/<signature>`; failures without a finding go
  to one rolling `fuzz-infra` issue, closed by the next green nightly. The
  libFuzzer jobs need P2996 Clang and are not part of this nightly.
- Fuzz regressions (threat model F4): the two libFuzzer harnesses also build
  without libFuzzer as `fuzz_v2_wire_replay` and `fuzz_wire_schemas_replay`
  (any compiler, not gated by `XOFFSET_BUILD_FUZZERS`). CTest replays
  `tests/fuzz/regressions/<target>/` and each harness's seed corpus in every
  CI job. `test_v2_fuzz` draws each case from its own stream of
  (seed, case), reruns one case with `--replay <seed> <case>`, and with
  `XOFFSET_FUZZ_DUMP_DIR` set writes a failing input wire before aborting.
  `-DXOFFSET_FUZZ_INJECT_CRASH=ON` (drill builds only) makes inputs starting
  with `XOFFSET-INJECT` crash the harnesses. The case streams changed, so a
  given seed now generates different cases than in 0.3.0-beta.1.
- Performance: verified loads index the payload with one bit per 8-byte
  granule (live and free bitmaps; inline up to 32 KiB, otherwise one scratch
  block, retained by `reload_verified`) instead of the paged byte index and two
  monotonic resources, and check each container head once (binding, block
  bounds and alignment) before falling back to the full check sequence for its
  error. Accept/reject sets and first error messages are unchanged
  (`tools/validation_differential.sh`, CI job `validation-differential`).
- Performance: canonical wires (no holes, blocks laid out back to back in
  visiting order, as `XCompactor::compact` writes them) are validated with a
  single cursor instead of the bitmaps, and take no scratch memory at any
  size. At the first block elsewhere or the first hole the live bitmap is set
  to the cursor's prefix and validation continues there, so accept/reject sets
  and first errors are unchanged (`test_v2_fuzz` compares every case with the
  cursor off). Map/set key order is checked without an early exit, and
  validation errors are raised from one out-of-line function.
- API: `XBuffer::begin_verified_reload<T>(used, holes, extents)` receives a
  wire directly into an existing buffer (no library copy, no new owner): the
  transport writes the header, extent table, payload (dense, or each extent at
  its offset) and hole table into the token's spans, calls `complete()` with
  the counts written, then `finish()`. Verification is `reload_verified`'s
  (same validator, same accept/reject set, `test_v2_fuzz` acceptor 6); an
  incomplete, aborted or mis-declared delivery rejects and leaves the buffer
  rootless. Nothing is cleared at begin: a dense payload is overwritten whole,
  and only the parts of sparse gaps that earlier content made dirty are
  zeroed at finish. `VerifiedReloadToken<T>`; `XOFFSET_VERIFIED_RELOAD_API`.
- Storage (R1, **contract change**): small buffers live on slots of a library
  slab (`xoffset_slot_storage.hpp`: 4 MiB chunks, power-of-two classes from
  1 KiB to `XBuffer::SLOT_MAX` + 64 B, per-thread caches) instead of a VM
  reservation each. The rule: VM with a pool, `with_storage(XStorage::Pinned)`,
  Automatic growth and a maximum above `SLOT_MAX`, or an initial size above
  it; a pinned slot when the maximum is at most `SLOT_MAX`; otherwise a
  movable slot, which is the default for small Explicit buffers and loads.
  **`grow()` on a movable slot may move the payload** (to a larger slot, or to
  VM above `SLOT_MAX`), and so may `reload_verified`, `begin_verified_reload`
  and `set_growth_policy(XGrowPolicy::Automatic)` (which pins on VM); a move
  bumps `epoch()` and invalidates pointers, `XHandle` re-resolves. Automatic
  growth, container operations and save never move. New: `XStorage`,
  `MaximumCapacity::with_storage`, `storage_kind()`, `address_stable()`,
  `XBuffer::trim_slot_storage()`, `slot_storage_stats()`,
  `XOFFSET_SLOT_MAX_BYTES` (0 disables slots), `XOFFSET_SLOT_STORAGE_API`. A
  default-constructed or moved-from `XBuffer` accepts `reload_verified` and
  `begin_verified_reload` (storage on demand, 64 MiB Explicit). Wire bytes,
  validation and the reject set are unchanged: `test_v2_fuzz` acceptor 7
  checks slot loads, reloads and direct reloads against the VM load byte for
  byte. Pooled, Automatic-with-large-maximum and formal-protocol buffers stay
  on VM.
- Diagnostics: building with `XOFFSET_VALIDATION_LAYOUT_STATS` counts, per
  root type, canonical / ascending-with-holes / out-of-order wires that pass
  verified validation and prints them to stderr at exit (not thread-safe;
  diagnostic builds only).

## 0.3.0-beta.1: production-readiness beta

Release gates G1–G8 (`PRODUCTION_READINESS_GOAL`): K1–K7 mapped to tests, two
compilers with byte-identical wires, schema pins, threat model with
mitigations, continuous fuzzing, sanitizers on both compilers, client-target
feasibility. Changes since 0.2.0-alpha.1 (which was never tagged):

- Client targets (K8, `docs/CLIENT_TARGETS.md`): CI runs the full CTest on
  native Linux arm64 (GCC 16) and Windows x64 (MSYS2 UCRT64, MinGW-w64
  GCC 16.2) and exchanges the wire corpus with Linux x86-64 (byte-identical,
  mutually accepted). Android and iOS are not feasible yet (no P2996
  toolchain or SDK); see the report. On MinGW the top-level build links the
  runtime statically so test replacements of `operator new` see library
  allocations; the signature exporter writes LF on every host.
- Sanitizers: GCC 16 ASan+UBSan job over the full CTest suite, next to the
  existing Clang ASan+UBSan job.
- `docs/STABILITY_TESTS.md` maps each guarantee K1–K7 to the tests and CI jobs
  that enforce it.
- In-place buffer reuse: `XBuffer::reload_verified<T>(wire)` (same reject set
  as `load_verified`; on failure the buffer is left empty) and
  `XBuffer::reset<T>()`; `reload_verified` reuses its validation scratch
  blocks (`release_reload_scratch()`). See `docs/BUFFER_REUSE.md`.
- Performance: `XVector` middle erase/insert shifts the tail with one
  `memmove` for eligible records (`wire_record_layout<T>::ordinary_members`,
  `stable_erase_if`); host free tables are recycled per thread instead of
  rebuilt per load (`XBufferCore::trim_free_table_cache()`); verified loads
  record ascending live ranges instead of marking the byte index
  (accept/reject and messages unchanged).
- Documentation: USER_GUIDE recommends `XBufferPool` for many small messages;
  K1–K7 and DEPLOYMENT guide; threat model `docs/V2_THREAT_MODEL.md`.

- GCC 16 support for the full library: `<meta>` is used when available
  (`<experimental/meta>` otherwise), and CMake accepts GCC 16+ with
  `-std=c++26 -freflection` and libstdc++.
- Fix: set/map keys of an enum type that defines `operator<=>` but no
  `operator<` were ordered differently by Clang (rewritten `<=>`) and GCC
  (builtin `<`), so each compiler rejected the other's wires. The library now
  calls `<=>` explicitly for such keys; Clang's order, and so its wires, are
  unchanged.
- K7 schema pins: `XOFFSET_PIN_SCHEMA(T, "<signature>")` fails compilation when
  a compiler or target lays out `T` differently from the pinned text, and the
  diagnostic shows the computed layout; `schema_pin<T>(name)` prints the pin.
  See `docs/SCHEMA_PINS.md`. CTest `schema_pin_mismatch` requires the failure.
- Untrusted input (threat model `docs/V2_THREAT_MODEL.md`):
  - M1: `load_verified` / `TypedXBuffer::load` and `begin_verified_receive`
    without an explicit `maximum_capacity` accept a wire that declares at most
    the default maximum (64 MiB, `XOFFSET_DEFAULT_MAXIMUM_CAPACITY`); larger
    declared sizes are rejected before any reservation. Previously a sparse wire
    of a few hundred bytes could make the receiver reserve up to 2 GiB.
    **Behavior change:** pass `XBuffer::maximum_capacity()` for wires above
    64 MiB.
  - M2: the rejection names the declared size and the limit (`XBadAlloc`).
  - M3: a block is cleared when it is retired (freed), so holes and the
    unwritten bytes of reused blocks are zero in the arena itself and every
    output path (`save_verified`, `save_verified_into`, `freeze_verified`)
    stays a plain copy. Holes that a loaded wire brought in keep the sender's
    bytes and are forwarded unchanged on re-save; they are never the
    receiver's memory.
  - M4: container shrink (`clear`, `resize` down, `erase`, `pop_back`,
    `stable_erase_if`, shorter `assign`) zeroes the vacated element and
    character bytes, so spare capacity never carries old values.
  - M6: implicit padding. The library zeroes the padding of every container
    element it constructs or relocates, including map entries (an
    `XMap<std::int32_t, std::int64_t>` entry has 4 padding bytes between key
    and value), so copied temporaries and reused arena blocks no longer put
    the caller's stack or stale bytes into a wire, and the same content
    always yields the same wire. Padding of a record the caller assigns
    directly is not reachable by the library: `has_implicit_padding_v<T>`,
    `XOFFSET_REQUIRE_NO_PADDING(T)` and the opt-in
    `XOFFSET_REJECT_IMPLICIT_PADDING` switch report such types at compile
    time.
  - Wires with holes or spare capacity change bytes (golden values updated).
- New `test_cross_compiler_wire` corpus. CI jobs: GCC 16 build and CTest, and
  a P2996 Clang <-> GCC 16 exchange that requires byte-identical wires and
  mutual `load_verified` acceptance.
- Coverage-guided fuzzing (G5): `fuzz_v2_wire` (libFuzzer, option
  `XOFFSET_BUILD_FUZZERS`) feeds each input to the spec oracle, typed load, raw
  validation and verified receive, which must agree; accepted wires must
  survive read -> mutate -> save -> load byte-stably. It shares its core with
  `test_v2_fuzz` (`tests/fuzz/v2_fuzz_core.hpp`). The new `Fuzz` workflow runs
  it under ASan+UBSan for 5 minutes on pull requests and pushes and for an hour
  nightly, keeping the corpus between runs. `fuzz_wire_schemas` adds three
  schemas (fixed arrays, enums and byte blobs; nested maps with string keys;
  three-level nested vectors) with a differential check across typed load,
  raw validation and verified receive. All acceptors in the fuzz core now
  use one 4 MiB limit, and the oracle rejects wires that declare more.

## 0.2.0-alpha.1: V2 storage and wire format

Experimental prerelease from next_cpp26. Same toolchain requirements as
0.1.0-alpha.1 (pinned clang-p2996). Schema evolution is not supported: a wire is
accepted only when the reader's type layout matches the writer's exactly.

Breaking: the V2 format replaces V1. V1 wires are rejected with
"unsupported wire format version 1"; there is no V1 reader. See
`docs/V2_FORMAT.md`.

- Blocks carry no headers: extents are derived from container heads
  (8-byte aligned, multiples of 8); `XAllocatorStateV1` and in-arena free
  lists are removed.
- Host-side binned free table with exact reuse and splitting; it travels as
  the wire hole table, so free space survives reloads and reloads never scan.
- Validators require live blocks plus holes to tile the payload exactly.
- Strings of up to 10 bytes are stored inline in their 16-byte head.
- The owner is found through a host page before the payload (no registry or
  thread-local lookup on allocate/retire).
- Sparse wires skip free runs of at least 4 KiB; Darwin outputs of at least
  1 MiB are prefaulted in 256 KiB chunks.
- `XFixedVector::assign(first, last)`; move-assignment ownership walk only
  in debug builds (`XOFFSET_MOVE_OWNERSHIP_CHECKS`).
- Public API changes from V1 are listed in `docs/V2_FORMAT.md` section 6.
- Documentation: README, `docs/ALPHA_COMPATIBILITY.md` and `docs/VM_PACKAGING.md`
  describe V2; new `docs/USER_GUIDE.md` (usage, constraints, intended-stable API).
- CI: the main workflow also runs on pushes and pull requests to next_cpp26, and
  adds a Linux ASan+UBSan job (P2996 image, pinned compiler-rt).
- Tests: `test_assign_range` covers the iterator form of `assign(first, last)`.
- `docs/USER_GUIDE.md` section 8 lists the core guarantees K1–K7 (K8, the
  client cross-compile, is a goal) and the optional interfaces E1–E5; new
  `docs/DEPLOYMENT.md` (same-version release, untrusted input, default receive
  limits, client capacity, client-link compression).

## 0.1.0-alpha.1

First alpha packaging of the accepted XOffsetDatastructure implementation.
The production headers and V1 wire format are unchanged by the release packaging.

- Reflected root objects and offset containers in an address-stable VM arena,
  with bounded growth, verified transport, schema registration and handles.
- Explicit growth by default; opt-in Automatic growth, idle-reservation pooling
  and explicit tail-page reclamation.
- Independent C++17 OS storage target and full P2996 C++26 target.
- Relocatable CMake full/storage packages, exact prerelease metadata, bundled
  dependency headers/licenses and multi-translation-unit consumer validation.
- Source and installed-prefix archive tooling, per-file manifests and archive
  integrity audit; compiler, SDK and runtime remain external inputs.

Recorded baseline validation includes native Windows storage, WSL storage/full/
package profiles, WSL network ASan/UBSan and historical macOS evidence. The initial
WSL full 48-test profile and final WSL package/network profile are separate results.
See the source distribution's `docs/VM_PACKAGING.md` for counts and boundaries.
Release-archive validation is reported separately from these baseline results.

This is an experimental prerelease. It does not promise stable source API/ABI
between alphas, general cross-toolchain/schema wire compatibility, Windows full
P2996 support, production asynchronous transport or N7 I/O extensions. The current
WSL F/F calibration did not establish the required noise qualification; no D/F
performance, causal attribution or optimization adoption is claimed. WSL and
Docker remain eligible for subsequent qualified Linux measurements.

Save and load copy bytes. Verified format checks and integrity hashes are not
cryptographic authentication. See `docs/ALPHA_COMPATIBILITY.md` in the source
distribution, or `ALPHA_COMPATIBILITY.md` beside this file in an installed package,
for the type domain, lifetime rules and resource limits.
