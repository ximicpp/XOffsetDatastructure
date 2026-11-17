# Client targets (K8)

The core scenarios are server to server and server to client: Linux servers, Windows / iOS / Android
clients, all 64-bit little-endian, independent of any particular client engine. Server and clients must
use the same schema and be released together (see [DEPLOYMENT.md](DEPLOYMENT.md) section 2). The library
has two build paths with byte-identical wires:

- **C++26**: P2996 reflection describes each record; needs clang-p2996 or a GCC with reflection. Servers
  take this path.
- **C++20 / C++23**: each record has a one-line `XOFFSET_RECORD` field list
  (`-DXOFFSET_CXX_STANDARD=20|23`); no reflection is needed, and native client compilers can build it.
  Clients take this path; in the server's C++26 build the same field list is checked entry by entry
  against reflection.

K8 (clients and servers use the same library and the same wire) is a core guarantee (delivered in 3.2.0,
promoted from goal to core guarantee on 2026-11-04): the field-list backend has landed (#149, since
3.2.0), and the Android, Windows, macOS and iOS simulator toolchains in the table below pass; 3.2.0-rc.1
also passed on a real Android phone and real Windows and macOS machines (next section); on an iPhone 15
Pro Max (iOS 26.6.2) CTest 138/138, the reference corpus and the demo pass
([DEVICE_TESTING.md](DEVICE_TESTING.md) section 9). All three client targets building natively and
passing CTest (iOS with an address space measurement and a fixed minimum deployment target), consistent
cross-target wire corpora, and signatures matching the `tools/sigs/` baselines are the scope of this
guarantee, and it is re-run before every release ([USER_GUIDE.md](USER_GUIDE.md) section 8; test mapping
in [STABILITY_TESTS.md](STABILITY_TESTS.md)).

## C++20 / C++23 field-list build

Entry point: `tools/native_targets.sh TARGET --std 20|23 --ref <reference corpus>` (toolchain files in
`cmake/toolchains/`). Every target runs the full Release CTest, then the wire corpus checks: write the
local corpus, read the reference wires (all must be accepted and re-save byte-identically), modify the
reference wires with fixed steps and save again (one copy each with `save_verified` and
`save_compact`), which must equal the bytes the reference end produced, and finally `diff -r` against
the reference corpus. The reference corpus is written by Linux x86-64 GCC 16 C++26 (reflection): 8 cases
(elided, empty, medium_holes, reloaded_mutated, small, sparse, plus compact_slack0 and compact_slack8
saved by `save_compact`), each with two modified copies (`<case>.edit.xwire`,
`<case>.edit-compact.xwire`), 24 files in total. 3.2.0-rc.1 and earlier had only the first 6.

Regression protection: `tools/local_validate.sh` has these columns (`msvc-c++20`, `msvc-c++23`,
`clang-cl-c++20`, `mingw-c++20`, `ndk29-c++20`, `ndk30-c++20`), with the reference corpus freshly
written by the GCC 16 C++26 build of the same commit. CTest includes the no-exception `*_noexc`
programs. The `client-c++20` step of the default PR tier (`--tier pr`) makes every PR run at least one
C++20 client column: the first of MSVC, clang-cl, NDK r29, NDK r30, MinGW that can run locally; if none
can, the step fails. `--clients`, `--tier daily` (once a day, on `main`) and `--release` run all client
columns. To install MSVC in a fresh cloud container use `tools/msvc-wine/fetch_msvc.py /opt/msvc` (then
`tools/msvc-wine/case_links.py /opt/msvc` for clang-cl).

| Target | Compiler | Standard | How it actually runs | Result |
| --- | --- | --- | --- | --- |
| Android arm64-v8a | NDK r29 Clang 21.0.0, API 28, `c++_static` | C++20 | Fully static, CTest under `qemu-aarch64-static` on cloud x86-64 | 129/129 (including 21 no-exception tests); corpus all accepted, byte-identical |
| Android arm64-v8a | NDK r30 Clang 21.0.0 | C++20 | Same as above | 129/129 (including 21 no-exception tests); corpus all accepted, byte-identical |
| Windows x64 | MSVC 19.44 (VS 2022 17.14 toolset 14.44.35207, Windows SDK 10.0.26100), `/MT` | C++20, C++23 | cl/link (`cmake/toolchains/msvc-wine-x86_64.cmake`) and CTest under wine 9.0 in the cloud | 129/129 for both standards (including 21 no-exception tests); corpus all accepted, byte-identical |
| Windows x64 | clang-cl 19 + the same MSVC STL and SDK, `/MT` | C++20 | `cmake/toolchains/clang-cl-x86_64.cmake`, CTest under wine | 129/129 (including 21 no-exception tests); corpus all accepted, byte-identical |
| Windows x64 | MinGW-w64 GCC 13.2 (posix threads), `-static` | C++20 | CTest under wine | 129/129 (including 21 no-exception tests); corpus all accepted, byte-identical |
| macOS arm64 | Apple clang 21.0.0 (Xcode 27.1, MacOSX 27.0 SDK, system libc++), deployment target macOS 14.0 | C++20, C++23 | Native on an M4 Mac (macOS 27.2), `tools/native_targets.sh macos` | Commit 1108193d: 137/137 for both standards (including 21 no-exception tests); all 24 reference wires accepted, modified bytes identical to Linux, `diff -r` matches |
| iOS simulator | Apple clang 21.0.0 (Xcode 27.1, iPhoneSimulator 27.1 SDK), deployment target iOS 17.0 | C++20 | iOS 27.0 simulator (iPhone 17) on an M4 Mac, `ios-simulator.cmake`, CTest via `simctl spawn` | 129/129; corpus all accepted, byte-identical. Simulator processes use the Mac's address space; the address space measurement on a real device is still to do |
| Linux x86-64 | GCC 13 / 16, Clang 18, P2996 Clang (C++20 each, plus C++23 for GCC 16) | C++20, C++23 | Matrix columns of `tools/local_validate.sh`, including ASan columns (run by `--tier daily` and `--release`; the PR tier runs only diff-c++20) | 106/106 for all; field-list and reflection builds give identical validation results case by case (differential 2×200000 cases) |

Notes per target:

- **Android**: allocation failure injection in the tests uses `aligned_alloc`, which bionic has only
  from API 28, so test builds default to API 28; the library itself does not depend on it. Without KVM
  only fully static bionic programs run, so the toolchain file adds `-static` when
  `XOFFSET_ANDROID_QEMU=ON`. bionic's `__cxa_thread_atexit_impl` allocates thread destructor nodes with
  `operator new` (glibc uses calloc), so the first time a thread uses a thread_local cache, tests that
  replace `operator new` see one extra allocation.
- **Windows (MSVC, clang-cl)**:
  - `XOFFSET_RECORD` needs the conforming preprocessor `/Zc:preprocessor`. The CMake interface target
    adds it automatically; without CMake, the traditional preprocessor fails at compile time with an
    explanation.
  - The library temporarily defines `WIN32_LEAN_AND_MEAN` and `NOMINMAX` when including `<windows.h>`
    (only when the user has not defined them), so `rpcndr.h`'s `small` macro does not come in. `near`
    and `far` are still macros; do not use them as identifiers.
  - MSVC's ABI limits `alignas` to 8192 and a single type to at most 0x7fffffff bytes.
  - `[[no_unique_address]]`: MSVC ignores it. On GCC/Clang, when it makes members overlap,
    `XOFFSET_RECORD` rejects it outright (members must not overlap); a reflection-only server accepts it
    and prints an 8-byte pin, and an `XOFFSET_PIN_SCHEMA` written from that pin fails to compile on an
    MSVC client (MSVC's layout is 12 bytes). Do not use it in shared types.
  - `std::string::resize_and_overwrite` is available on MSVC only with `/std:c++latest` (CMake's
    C++23); in C++20 the library takes the plain `resize` path.
- **Windows (MinGW GCC)**: GCC on MinGW uses emulated TLS (emutls), and the library's per-thread caches
  go wrong at thread exit, so these caches are off by default with GCC + MinGW/Cygwin
  (`XOFFSET_THREAD_CACHES=0`).
- **macOS / iOS (std::pmr)**: the library uses `std::pmr` from the system libc++. The system libc++ has
  these symbols only from macOS 14 / iOS 17, and the libc++ headers carry no availability annotations
  for pmr, so lower deployment targets still compile and link and only fail when loading on an old
  system. So the default deployment target of C++20/23 builds is macOS 14, and lower deployment targets
  are a compile-time error. NDK, MinGW and MSVC link the standard library statically into the program
  and have no such system version threshold.

## Real devices and machines: 3.2.0-rc.1 (39de636, 2026-10-28)

The tester pulled `v3.2.0-rc.1` on their own machines and produced the table below with
`tools/native_targets.sh`, `tools/platform_matrix.sh` and `tools/device/android_device.sh`; the
reference corpus was the 6 wires written by GCC 16.2 C++26 reflection of the same commit in WSL2 Docker.
The results belong only to commit 39de636. Checks, records of failures and the evidence index are in
[validation/2026-10-28-rc1-platform/README.md](validation/2026-10-28-rc1-platform/README.md).

| Target | Compiler | Standard | How it actually runs | Result |
| --- | --- | --- | --- | --- |
| Android arm64-v8a real phone (Xiaomi M2102K1C, Android 11) | NDK r29 Clang 21, API 28 | C++20 | CTest run over adb on the phone, compile-fail tests on the computer | 114/114 + 15/15; corpus all accepted, byte-identical; demo with two clients E2E PASSED |
| Windows x64 real machine (Windows 11) | MSVC 19.44.35229, `/MT`, `/utf-8` | C++20, C++23 | Native | 129/129 for both standards; corpus all accepted, byte-identical |
| macOS arm64 real machine (SDK 26.4) | Apple clang 21.0.0 | C++20, C++23 | Native | 129/129 for both standards; corpus all accepted, byte-identical |
| macOS arm64 real machine (the same one) | clang-p2996 9ffb96e3 | C++26 reflection | Native | 158/158; corpus byte-identical |
| Linux x86-64 remote VM (KVM) | Clang 20.1.2 provided by Zig | C++20 | Native | 129/129; corpus all accepted, byte-identical |
| Linux x86-64, WSL2 Docker | GCC 16.2.0 | C++26 reflection | Writes the reference corpus; also the server of the Android demo | 158/158 |

129 and 158 are the full lists of the two backends; neither contains the other, and they cannot be added
up. The first Windows build under code page 936 failed on non-ASCII characters in test sources and passed
with `/utf-8` (afterwards the project added `/utf-8` for MSVC automatically). This round has no
performance data, no iPhone and no bare-metal Linux; on Windows and macOS only the library and the corpus
ran, without the cross-device demo.

## C++26 reflection build: local platform matrix (records since 3.1.0)

GitHub CI has been off since 2026-10-23, and the files in `.github/workflows/` no longer run. The record
now is `tools/platform_matrix.sh TARGET`: it builds, runs the full CTest, runs `--read` and `diff -r`
against the reference corpus written by Linux x86-64 GCC 16, and writes the results into
`PLATFORM_RESULT.md` in the build directory.

| Target | Where it runs | Compiler | Latest result |
| --- | --- | --- | --- |
| Windows x64 | Cloud Linux, CTest under wine | Self-built MinGW-w64 GCC 16.2.0 (`tools/build_mingw_toolchain.sh`, `cmake/toolchains/mingw-w64-x86_64.cmake`) | 3.1.0 RC (e913d22): 105 of 107 pass, 2 skipped, V2.1 corpus byte-identical |
| Linux arm64 | colima Linux VM on a Mac M4 (Linux 6.8 aarch64, 4 KiB pages), with the Mac checkpoint batches | GCC 16 (gcc:16 arm64 image pinned by digest), P2996 d77eff1 | 3.1.0 RC (e913d22): 107/107 for both, V2.1 corpus byte-identical |
| Linux x86-64 | Cloud | GCC 16 (writes the reference corpus), P2996 | 3.1.0 RC (e913d22): 107/107 for both, P2996 corpus byte-identical to the reference |

The two items skipped on Windows are `test_canonical_cold_no_callback` and `test_canonical_zero_target`:
they test the per-thread owner cache, and since GCC on MinGW uses emutls, accessing a `thread_local` at
thread exit gets a new block, so these caches are off entirely on MinGW GCC (`XOFFSET_THREAD_CACHES=0`),
and these two report a skip with exit code 77.

Three things to note on Windows (all handled in the repository):

- The dependency lock hashes submodule bytes, so set `core.autocrlf false` and `core.eol lf` before
  checkout; otherwise Boost's `* text=auto` converts files to CRLF and the lock check fails.
- The top-level build links the runtime statically on MinGW (`-static`). Internal allocations such as
  `std::pmr` in the dynamic `libstdc++-6.dll` do not go through the `operator new` the tests replace, so
  allocation failure injection tests would miss them. This only affects this repository's own tests and
  tools, not the exported interface.
- The signature export tool writes files in binary mode, so the committed signature baselines are LF on
  every host.

NDK Clang has no P2996 reflection, and the NDK has shipped no GCC since r18; the P2996 toolchain has only
a macOS libc++ and `*_osx` compiler-rt, with no iOS / iOS simulator versions; MSVC and clang-cl have no
reflection either. So Android, iOS and MSVC clients all use the C++20 / C++23 field-list build above.

## Historical record: GitHub CI (2026-10-21, wire format 2.0)

Results of `.github/workflows/client-targets.yml` before CI was turned off, format 2.0, with fewer tests
than now; historical reference only.

| Target | Compiler | What was verified | Result |
| --- | --- | --- | --- |
| Linux x86-64 (reference) | GCC 16 (gcc image pinned by digest) | Writes the reference wire corpus | The baseline other targets compare against |
| Linux arm64 (GitHub `ubuntu-24.04-arm`, native) | GCC 16 (pinned by arm64 platform manifest digest) | Debug full CTest; reads the x86-64 corpus and accepts all of it; writes a local corpus and `diff -r` against the x86-64 corpus | All pass, wires byte-identical |
| Windows x64 (`windows-latest` + MSYS2 UCRT64) | MinGW-w64 GCC 16.2 (requires ≥ 16) | Debug full CTest 87/87 (Ninja); a Release build reads the x86-64 corpus and accepts all of it; writes a local corpus and `diff -r` against the x86-64 corpus | All pass; 5 corpus files (empty, small, medium_holes, reloaded_mutated, sparse) byte-identical |

## Handshake (since 3.2.0)

When client and server schema versions differ, the 3.2 server does the conversion
([SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)). The client only adds a handshake, with
`xoffset_handshake.hpp`: after connecting it sends `make_schema_hello<...>()`, reads the server's ack with
`read_schema_ack<...>()`, and prompts for an update when rejected. These two functions do not throw; in
no-exception builds the handshake messages are loaded through `try_load_verified` (next section). The
client does not include `xoffset_evolution.hpp` and has no conversion code.

## No-exception builds (SC-8)

Engines without exceptions (`-fno-exceptions`, MSVC `/EHs-c- /D_HAS_EXCEPTIONS=0`) can include XOffset
directly, receive with `try_load_verified` / `try_reload_verified` / `try_begin_verified_reload` to get
error codes and error text, and send other non-returnable failures to `set_fatal_handler`
(`docs/USER_GUIDE.md` section 6.2). The 24 `*_noexc` programs in CTest and `test_vm_storage_noexc` (the
C++17 OS layer) are all compiled without exceptions; `tools/no_exceptions_differential.sh` compares the
results of builds with and without exceptions on the same batch of mutated wires. The C++26 reflection
build (P2996, GCC 16) and the C++20 field-list build (GCC 16, P2996 Clang) both ran these tests on cloud
Linux x86-64; the no-exception builds of other client targets are verified with each platform column.

## Scope

Only 64-bit little-endian targets are supported. Client targets are Windows x64, Android arm64-v8a, iOS
arm64 (real devices and simulator) and macOS arm64; servers are Linux x86-64 and arm64. 32-bit clients and
big-endian platforms are out of scope.

## Running on phones: the test host (3.2)

iOS cannot run command-line programs, and pushing tests one by one to a real Android phone is awkward.
`-DXOFFSET_TEST_HOST=ON` builds every test, demo client and measurement program (`xo_slg_bench` SC-6,
`xo_mem_probe` SC-12) as a shared library (`cmake/XOffsetTestHost.cmake`) loaded and run by a host
program: on iOS an app (`tools/test_host/host_ios.mm`, with the libraries in the app's `Frameworks/`), on
other platforms a command-line host. On real devices and locally the host stays resident by default
(`xoserve`; `XO_HOST_SERVE=0` goes back to running one program per launch; the simulator is not yet
verified and still defaults to one per launch): it starts once, the computer sends programs in one by
one, and they run in sequence in the same process; the app screen shows running totals (task packages and
demo snapshot bytes received, programs run and passed, the last few programs); the screen does not refresh
during slg timing and memory probing, and each of these two kinds of programs runs in a freshly launched
app. An iPhone process has about 3.4 GiB of address space, and in the 6th real-device run (2026-10-28)
each loaded program library kept its own 64 MiB owner table, so about 50 libraries filled it up. Now the
program libraries are compiled with `XOFFSET_OWNER_MAP_SHARED` and share one table exported by the host
(`xoffset_owner_map_shared`); after each program, the host tries to reserve `XO_HOST_HEADROOM_MIB`
(default 2048) MiB of address space, and if it cannot, it writes its results and exits, and
`xo_host_run.py` restarts the host and sends the next program. The 8th real-device run (2026-10-29)
confirmed there was enough address space (slg and the memory probe passed), but about 18% of
`devicectl copy` calls reported CoreDeviceError 7000: the host deleted `<id>.ready` as soon as it saw it,
and when devicectl looked for the file again after copying, it was gone. Now the host writes `<id>.taken`
to mark it taken, and deletes `.ready` after 10 seconds (or in the next resident process); when copying
`.ready` reports an error, `xo_host_run.py` first checks whether the app already received it and resends
only if not. `XO_HOST_LOCAL_RECHECK_MS` makes the local backend re-check copies the way devicectl does,
to reproduce this in the cloud. In the cloud, under `ulimit -v 3500000`, in resident mode with this check
on, CTest, the reference wires, slg, the memory probe and the demo all ran through; the 9th real-device
run (2026-10-29) passed everything in resident mode, with no copy errors, and CTest took 1 minute 57
seconds (the 7th run, launching once per program, took 3 minutes 49 seconds). The host catches the
programs' `exit`/`_Exit`, signals and output, and results are written to the host's data directory (on
iOS the app's `Documents/xo`); `tools/test_host/xo_host_run.py` on the computer installs, launches,
transfers files and collects results:

| Backend | Target | Tools used |
| --- | --- | --- |
| `local` | Checking the same flow on Linux / macOS | Starts the command-line host directly |
| `sim` | iOS simulator | `xcrun simctl` |
| `device` | iPhone (USB or the same Wi-Fi) | `xcrun devicectl`; signing with `tools/test_host/sign_ios_app.sh` |

`xo_host_run.py` can serve as CTest's `CMAKE_CROSSCOMPILING_EMULATOR` (`ios-simulator.cmake` sets it so
when `XOFFSET_TEST_HOST=ON`, and `ios-device.cmake` uses `XO_HOST_DEVICE`), or as the client wrapper
command for `demo/mapstate/tools/run_e2e.sh`. It copies input files and output directories back and forth
between the computer and the device, so programs see the same paths as on the computer; the exit code is
the program's own, or 128+signal when killed by a signal. `operator new` / `delete` replaced by a program
(allocation counting, failure injection) are forwarded by the host, and allocations inside the C++
runtime reach them too, just as in an ordinary executable (`test_host_new_forwarding` checks this).
Checked with the `local` backend on cloud Linux: 129/129 CTests pass, and the demo end to end (self-test
23/23, corpus replay 60/60, three stress tiers and malicious input) all pass.
