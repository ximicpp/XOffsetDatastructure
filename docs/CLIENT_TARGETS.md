# Client targets (K8)

Servers are Linux; clients are Windows, iOS and Android; all are 64-bit little-endian. Server and
clients use the same schema and are released together ([DEPLOYMENT.md](DEPLOYMENT.md) section 2). The
library has two build paths that write byte-identical wires:

- **C++26** (servers): P2996 reflection describes each record. Needs GCC 16+ or the P2996 Clang.
- **C++20 / C++23** (clients): each record has one `XOFFSET_RECORD` field list
  (`-DXOFFSET_CXX_STANDARD=20|23`), so native client compilers build it. In a C++26 build the same list
  is checked against reflection.

NDK Clang, MSVC, clang-cl and Apple clang have no reflection, so Android, Windows and iOS clients use
the field-list build. K8 (clients and servers share the library and the wire) is a core guarantee,
re-run before every release ([STABILITY_TESTS.md](STABILITY_TESTS.md)).

## Field-list build

`tools/native_targets.sh TARGET --std 20|23 --ref <reference corpus>` (toolchain files in
`cmake/toolchains/`) runs the full Release CTest, then the wire corpus checks: read every reference wire
and re-save it byte-identically, modify it with fixed steps and save it with `save_verified` and
`save_compact` (both must equal the reference end's bytes), and `diff -r` the local corpus against the
reference. The reference corpus (24 files: 8 cases and two modified copies each) is written by the
Linux x86-64 GCC 16 C++26 build of the same commit.

`tools/local_validate.sh` has the client columns `msvc-c++20`, `msvc-c++23`, `clang-cl-c++20`,
`mingw-c++20`, `ndk29-c++20`, `ndk30-c++20`. Every PR tier runs at least one of them (the first that can
run locally); `--clients`, `--tier daily` and `--release` run all. To install MSVC in a fresh cloud
container: `tools/msvc-wine/fetch_msvc.py /opt/msvc`, then `tools/msvc-wine/case_links.py /opt/msvc`
for clang-cl.

| Target | Compiler | Standard | How it runs |
| --- | --- | --- | --- |
| Android arm64-v8a | NDK r29 / r30 Clang 21, API 28, `c++_static` | C++20 | Fully static, CTest under `qemu-aarch64-static` |
| Windows x64 | MSVC 19.44 (VS 2022 17.14), `/MT` | C++20, C++23 | `cmake/toolchains/msvc-wine-x86_64.cmake`, CTest under wine |
| Windows x64 | clang-cl 19 with the same STL and SDK | C++20 | `cmake/toolchains/clang-cl-x86_64.cmake`, CTest under wine |
| Windows x64 | MinGW-w64 GCC 13.2, `-static` | C++20 | CTest under wine |
| macOS arm64 | Apple clang 21, deployment target macOS 14.0 | C++20, C++23 | Native on a Mac, `tools/native_targets.sh macos` |
| iOS simulator | Apple clang 21, deployment target iOS 17.0 | C++20 | `ios-simulator.cmake`, CTest via `simctl spawn` |
| Linux x86-64 | GCC 13 / 16, Clang 18, P2996 Clang | C++20, C++23 | Matrix and ASan columns of `tools/local_validate.sh` |

On every target the full CTest passes (including the no-exception `*_noexc` programs) and the corpus
is accepted and byte-identical. Real devices (3.2.0-rc.1 and 3.2.x): an Android 11 phone, Windows 11
and macOS machines, and an iPhone 15 Pro Max passed CTest and the corpus; evidence in
[validation/2026-10-28-rc1-platform](validation/2026-10-28-rc1-platform/README.md) and
[DEVICE_TESTING.md](DEVICE_TESTING.md).

### Notes per target

- **Android**: test builds default to API 28 (failure injection uses `aligned_alloc`); the library does
  not need it. Without KVM only fully static programs run, so `XOFFSET_ANDROID_QEMU=ON` adds `-static`.
  bionic allocates thread destructor nodes with `operator new`, so tests that replace it see one extra
  allocation the first time a thread uses a cache.
- **MSVC, clang-cl**: `XOFFSET_RECORD` needs `/Zc:preprocessor` (the CMake target adds it). The library
  defines `WIN32_LEAN_AND_MEAN` and `NOMINMAX` around `<windows.h>` unless you did; `near` and `far`
  are still macros. MSVC ignores `[[no_unique_address]]`, so do not use it in shared types: the layout
  differs from GCC/Clang and a pin fails. Sources build with `/utf-8`.
- **MinGW GCC**: emulated TLS breaks the per-thread caches at thread exit, so they are off
  (`XOFFSET_THREAD_CACHES=0`) and two cache tests report a skip.
- **macOS / iOS**: the system libc++ has `std::pmr` only from macOS 14 / iOS 17, and lower targets would
  fail only at load time, so lower deployment targets are a compile error.
- **Windows checkouts**: set `core.autocrlf false` and `core.eol lf` before checkout, or the dependency
  lock check fails on CRLF files.

## C++26 build on other platforms

`tools/platform_matrix.sh TARGET` builds, runs the full CTest, reads and diffs the reference corpus,
and writes `PLATFORM_RESULT.md` in the build directory. Columns: Windows x64 under wine (self-built
MinGW-w64 GCC 16, `tools/build_mingw_toolchain.sh`), Linux arm64 (GCC 16 and P2996 in a VM on a Mac),
Linux x86-64 (GCC 16 and P2996). GitHub CI is off; these runs are the record.

## Handshake and no-exception builds

When client and server schema versions differ, the server converts
([SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)). The client only sends `make_schema_hello<...>()` and reads
`read_schema_ack<...>()`; neither throws, and the client has no conversion code.

Engines without exceptions (`-fno-exceptions`, MSVC `/EHs-c- /D_HAS_EXCEPTIONS=0`) receive with
`try_load_verified` / `try_reload_verified` / `try_begin_verified_reload` and route other failures to
`set_fatal_handler` ([USER_GUIDE.md](USER_GUIDE.md) 6.2). `tools/no_exceptions_differential.sh`
compares builds with and without exceptions on the same mutated wires.

## Scope

64-bit little-endian only: Windows x64, Android arm64-v8a, iOS arm64 (device and simulator) and macOS
arm64 clients; Linux x86-64 and arm64 servers. 32-bit and big-endian targets are out of scope.

## Running on phones: the test host

`-DXOFFSET_TEST_HOST=ON` builds every test, demo client and measurement program as a shared library
(`cmake/XOffsetTestHost.cmake`) run by a host: an iOS app (`tools/test_host/host_ios.mm`) or a
command-line host elsewhere. On devices the host stays resident (`XO_HOST_SERVE=0` runs one program per
launch) and the computer sends programs in one by one. Program libraries share one owner table
(`XOFFSET_OWNER_MAP_SHARED`); when less than `XO_HOST_HEADROOM_MIB` (default 2048) MiB of address space
is left, the host writes its results and exits, and the runner restarts it.

`tools/test_host/xo_host_run.py` installs, launches, transfers files and collects results:

| Backend | Target | Tools used |
| --- | --- | --- |
| `local` | Linux / macOS, to check the flow | The command-line host |
| `sim` | iOS simulator | `xcrun simctl` |
| `device` | iPhone (USB or Wi-Fi) | `xcrun devicectl`; sign with `tools/test_host/sign_ios_app.sh` |

It works as CTest's `CMAKE_CROSSCOMPILING_EMULATOR` (`ios-simulator.cmake`, `ios-device.cmake` with
`XO_HOST_DEVICE`) or as the client wrapper of `demo/mapstate/tools/run_e2e.sh`. Programs see the same
paths as on the computer; the exit code is the program's own, or 128 + signal. A program's replaced
`operator new` / `delete` are forwarded by the host (`test_host_new_forwarding`).
