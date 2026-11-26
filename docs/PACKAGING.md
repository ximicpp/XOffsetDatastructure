# Source archives, install archives and external consumption

This library supports two build paths that produce byte-identical wires: **C++20 / C++23 field-list
builds** (standard platform compilers, zero external dependencies or submodules) and **C++26 reflection
builds** (GCC 16+ or P2996 Clang, with pinned TypeLayout/Boost headers for reflection signature tools).
From the final 2.0.0 releases follow Semantic Versioning: incompatible wire, delta or public API changes
come only in a new major version, and every `2.x` release reads the wires and deltas of every other
`2.x` release; ABI is not promised ([RELEASING.md](RELEASING.md) section 1.1). The 2.0.0 previews
(`2.0.0-preview.N`) may still change the API and the wire format. Read
[compatibility and input boundary](ALPHA_COMPATIBILITY.md) first.
This version uses Wire Format 3.0 ([WIRE_FORMAT.md](WIRE_FORMAT.md)) and does not read Wire 2.1 or V1
wires; save/load copy bytes into a working buffer rather than borrowing unaligned external memory directly.

## Three deliverables

| Deliverable | Contents | Environment inputs not included |
| --- | --- | --- |
| `xoffsetdatastructure-<VERSION>.tar.gz` source archive | All 30 `xoffset*.hpp` library headers, CMake, tests/examples, required tools, every current document (`docs/*.md`, `SECURITY.md`, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`) and the benchmark and demo reports they link to, plus the 851 pinned Boost/TypeLayout headers and their licenses for C++26 reflection builds | Compiler, libc++/system runtime, SDK, build outputs, experiment directories and large historical evidence |
| `xoffsetdatastructure-<VERSION>-full.tar.gz` install archive | All 30 `xoffset*.hpp` headers, `XOffset::xoffset` and `XOffset::storage`, CMake config, version/compatibility documents and licenses (C++20/C++23 installs have zero third-party headers; C++26 installs also include the 851 private dependency headers under `include/xoffset-deps/`) | Compiler, SDK, runtime, native program binaries |
| `xoffsetdatastructure-<VERSION>-storage.tar.gz` install archive | The OS storage headers (`xoffset_virtual_memory.hpp`, `xoffset_slot_storage.hpp`, `xoffset_error.hpp`, `xoffset_config.hpp`), `XOffset::storage`, CMake config, version/compatibility documents and the project license | Full XBuffer/container headers, Boost, TypeLayout, compiler, SDK, runtime |

The source archive's dependency closure can be checked by content lock in an extracted directory with
no `.git`. Every relative link in a Markdown file of the archive resolves inside it: `prepare_release.py`
refuses to create or audit an archive with a link that has no target. It is not a complete Boost distribution, nor an offline research archive containing all
historical raw evidence. `VALIDATION_RUNNER.md` in the source archive keeps some historical entry-point
notes; the run entry points included in this archive are native full/storage and package validation;
historical wire fixtures, date-specific Windows/network runners and experiment tools are outside the
distribution.

## Requirements

| Target | Minimum requirements |
| --- | --- |
| `XOffset::storage` | C++17, CMake 3.13+, Linux/macOS/Windows VM APIs, thread support |
| `XOffset::xoffset` (C++20 / C++23 field-list) | GCC 13+, Clang 18+, Apple clang 21+ (macOS 12+ / iOS 14+, automatic `XOFFSET_USE_PMR=0` below macOS 14 / iOS 17), Android NDK Clang (API 28+ for tests), MSVC 19.44 (`/Zc:preprocessor`), clang-cl, or MinGW-w64 GCC; CMake 3.20+; 64-bit little-endian; zero external submodules |
| `XOffset::xoffset` (C++26 reflection) | GCC 16+ (`-freflection`, libstdc++) or P2996 Clang (`-freflection -fexpansion-statements`, matching libc++); CMake 3.30+; 64-bit little-endian |
| The maintained Python validation entry points and their tests | Python 3.10+; storage JUnit validation needs CMake/CTest 3.21+ and full C++26 needs 3.30+; the C++ library has no Python run-time dependency |

C++20 and C++23 builds propagate `cxx_std_20` / `cxx_std_23` (and `/Zc:preprocessor` on MSVC / clang-cl)
and require no submodules. C++26 reflection builds propagate `cxx_std_26`, reflection flags and Threads;
C++26 compilers without P2996 reflection cannot substitute for the reflection path (use
`-DXOFFSET_CXX_STANDARD=20` or `23` instead). Native Windows (MSVC, clang-cl), Android NDK, macOS and iOS
use the C++20/C++23 field-list path (see [CLIENT_TARGETS.md](CLIENT_TARGETS.md)).

## Fixed inputs

[toolchains/README.md](../toolchains/README.md) and
[vm-validation.json](../toolchains/vm-validation.json) record the SHA-256 of the pinned image, compiler
sources, dependencies and the official CMake 4.2.3 archive. The historical Linux Docker input is an
immutable registry digest with compiler revision `a5270822dede40aaaf2c62381b89f110d2cfda4c`; the newer
WSL continuation compiler is `9ffb96e3ce362289008e14ad2a79a249f58aa90a`, and the two are not the same
profile. The Dockerfile is a recipe that builds the pinned compiler source, but its Ubuntu base tag and
apt inputs are still mutable, so a rebuilt image must not be described as a byte copy of the verified
registry digest.

The source archive already includes the pinned dependencies and needs no submodule download. A Git
checkout uses the parent gitlinks and keeps LF bytes:

```sh
git -c core.autocrlf=false -c core.eol=lf submodule update --init -- external/boost external/typelayout
git -c core.autocrlf=false -c core.eol=lf -C external/boost submodule update --init -- libs/container libs/core libs/config libs/throw_exception libs/static_assert libs/container_hash libs/type_traits libs/winapi libs/intrusive libs/predef
```

Boost is pinned at `da041154c6bac1a4aa98254a7d6819059e8ac0b0` and TypeLayout at
`fe295929d0a5bfa7809fb737a6eb3db279599346`. The full configuration checks the contents of 11 include
trees and 3 licenses against `cmake/XOffsetDependencyLock.cmake`, independent of Git metadata. Do not
upgrade dependencies with `--remote`, and do not change the lock to accept CRLF or other differences.
When the bytes of an existing checkout do not match, protect your changes and diagnose first; repeating
the update does not mean it is fixed.

Example of preparing the pinned CMake (the output must be a new directory):

```sh
python3 tools/prepare_vm_cmake.py --platform linux-x86_64 --out build/cmake-alpha
```

The tool downloads and checks the locked archive, or `--archive` can point to an already downloaded
file; CMake ends up in `build/cmake-alpha/root/bin`. The CMake 3.28.3 shipped in the pinned Linux image
is not enough to build full. For example, run the full check in the pinned image from a Linux shell:

```sh
XOFFSET_IMAGE=$(python3 -c 'import json; print(json.load(open("toolchains/vm-validation.json"))["profiles"]["linux-p2996"]["image"])')
docker run --rm --platform linux/amd64 -v "$PWD":/workspace -w /workspace \
  -e PATH=/workspace/build/cmake-alpha/root/bin:/opt/p2996-toolchain/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  "$XOFFSET_IMAGE" bash ./build.sh --compiler /opt/p2996-toolchain/bin/clang++ \
  --build-dir build/docker-alpha-full -j 2
```

`tools/prepare_vm_linux_current.py --compiler-root /opt/p2996-toolchain --out <new directory>` is
another local recipe that restores an existing `9ffb96e...` compiler: it checks the compiler and
prepares the pinned private runtime/link wrapper; it does not download the compiler, and it does not
pass off host system headers as a complete sysroot. When taking this path, build explicitly with the
output `bin/clang++`; do not relabel the new revision as the old image with the historical
`linux-p2996` identity check. The pinned compiler/SDK inputs for macOS are listed separately in the
toolchain lock.

SDK, standard library include/library and run-time search paths are supplied by the caller's toolchain
file or explicit arguments. Convenience paths detected on the local machine are never written into the
exported targets. Do not add the experimental libc++ to the system tool loader path globally.

## Creating, auditing and extracting archives

Run in a source tree whose pinned dependencies are prepared (`<VERSION>` below is the content of
`XOFFSET_VERSION`; `prepare_release.py` reads the version from that file in the same source tree):

```sh
python3 tools/prepare_release.py create --source-dir . --out build/release-source
python3 tools/prepare_release.py audit \
  --archive build/release-source/xoffsetdatastructure-<VERSION>.tar.gz \
  --manifest build/release-source/xoffsetdatastructure-<VERSION>.manifest.json \
  --checksums build/release-source/SHA256SUMS
python3 tools/prepare_release.py extract \
  --archive build/release-source/xoffsetdatastructure-<VERSION>.tar.gz \
  --out build/release-source-extracted
```

The create output contains the archive, a `.manifest.json` with the same name and `SHA256SUMS`; the
archive root is `xoffsetdatastructure-<VERSION>/`, which contains `SOURCE_MANIFEST.json`. `audit` can
run without an external manifest/checksums, but for external distribution the archive/manifest identity
should be pinned separately. `extract` audits before extracting, and the source ends up in
`build/release-source-extracted/xoffsetdatastructure-<VERSION>/`. Integrity hashes are not signatures
or source authentication.

Verified install prefixes can be packed into full/storage archives:

```sh
python3 tools/prepare_release.py pack-prefix --prefix /path/to/verified-full-prefix \
  --component full --out build/release-full
python3 tools/prepare_release.py pack-prefix --prefix /path/to/verified-storage-prefix \
  --component storage --out build/release-storage
python3 tools/prepare_release.py extract \
  --archive build/release-full/xoffsetdatastructure-<VERSION>-full.tar.gz \
  --out build/release-full-extracted
```

Install archives also carry an external `.manifest.json`/`SHA256SUMS`, with `INSTALL_MANIFEST.json`
inside. After extracting full, the prefix is
`build/release-full-extracted/xoffsetdatastructure-<VERSION>-full`; pass it to the consumer project's
`CMAKE_PREFIX_PATH`; the storage root name ends with `-storage`. `audit`/`extract` support all three
archive kinds. Passing packing and hashing does not replace an actual consumer build from the extracted
archive.

All outputs use new directories; source/out do not accept symlinks. The create output must not equal or
contain the source, and must not be inside the cmake/tests/tools/examples/include/external/docs/toolchains
directories being copied; a separate directory such as `build/release-source` works. The tool never
overwrites existing artifacts or triggers CI or a release.

## Building and installing

Install a zero-dependency C++20/C++23 full package from the source root (no submodules required):

```sh
cmake -S . -B build/xoffset-install \
  -DXOFFSET_CXX_STANDARD=20 \
  -DCMAKE_INSTALL_PREFIX=/path/to/xoffset-prefix \
  -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF
cmake --build build/xoffset-install --config Release
cmake --install build/xoffset-install --config Release
```

To install a C++26 reflection package (including the locked Boost/TypeLayout headers), pass
`-DXOFFSET_CXX_STANDARD=26` and `-DCMAKE_CXX_COMPILER=/path/to/gcc16-or-p2996`.

For storage only, add `-DXOFFSET_STORAGE_ONLY=ON`: it uses an ordinary C++17 platform compiler and skips
the full headers and reflection checks. A full install provides both `full` and `storage`; requesting
`full` from a storage install fails explicitly. At the top level of the source tree, tests/tools/examples
are built by default; as a `FetchContent` or `add_subdirectory` dependency these four are off by default
and can each be turned on explicitly: `XOFFSET_BUILD_TESTS`, `XOFFSET_BUILD_TOOLS`,
`XOFFSET_BUILD_EXAMPLES`, `XOFFSET_INSTALL`.

## Consuming installs and sources

Minimal `CMakeLists.txt` consuming a full install:

```cmake
cmake_minimum_required(VERSION 3.20)   # 3.30+ for C++26 reflection installs
project(MyConsumer LANGUAGES CXX)
# A stable release also accepts a version: find_package(XOffsetDatastructure 2.0 ...).
find_package(XOffsetDatastructure CONFIG REQUIRED COMPONENTS full)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE XOffset::xoffset)
```

```sh
cmake -S /path/to/consumer -B build/consumer \
  -DCMAKE_PREFIX_PATH=/path/to/xoffset-prefix
cmake --build build/consumer --config Release
```

Storage uses `COMPONENTS storage`, `XOffset::storage` and CMake 3.13+. With no component specified,
full is requested. The install config exposes `XOffsetDatastructure_VERSION` (from `XOFFSET_VERSION`),
`XOffsetDatastructure_VERSION_NUMERIC`, `XOffsetDatastructure_VERSION_PRERELEASE` and
`XOffsetDatastructure_VERSION_COMPATIBILITY`. Stable releases (the final `2.0.0` and later) use
`SameMajorVersion` matching (`same-major-version`); **prereleases conservatively reject numeric,
`EXACT` and range requests** (`exact-full-version-only`) so CMake never takes a prerelease for a
stable version (find unversioned and compare `XOffsetDatastructure_VERSION` with `STREQUAL`).

Source consumption via `FetchContent` (recommended for C++20/C++23 projects):

```cmake
include(FetchContent)
FetchContent_Declare(
    xoffsetdatastructure
    # The release's source archive (about 3 MB): no Git history, no submodules.
    URL https://github.com/ximicpp/XOffsetDatastructure/archive/refs/tags/v2.0.0-preview.4.tar.gz
)
set(XOFFSET_CXX_STANDARD 20 CACHE STRING "" FORCE)
FetchContent_MakeAvailable(xoffsetdatastructure)
target_link_libraries(my_app PRIVATE XOffset::xoffset)
```

Or via `add_subdirectory(path/to/XOffsetDatastructure)` followed by
`target_link_libraries(my_app PRIVATE XOffset::xoffset)`. When you need storage only, set
`XOFFSET_STORAGE_ONLY=ON` before adding the subdirectory and link `XOffset::storage` instead.

## Install contents and licenses

- A **C++20 / C++23 full install** installs all 31 `xoffset*.hpp` headers under `include/`, CMake config
  files under `lib/cmake/XOffsetDatastructure/`, `share/XOffsetDatastructure/xoffset-third-party-manifest.txt`
  recording zero third-party dependencies, and the project MIT license under
  `share/XOffsetDatastructure/licenses/LICENSE`. `tools/prepare_release.py pack-prefix --component full`
  packs and audits both zero-dependency C++20/C++23 installs and C++26 installs.
- A **C++26 reflection full install** additionally installs the private dependency root
  `include/xoffset-deps/` (TypeLayout's 19 include files and 10 Boost modules, 851 headers total, with
  original copyright notices kept), Boost's `LICENSE_1_0.txt` and TypeLayout's `LICENSE` /
  `LICENSE_1_0.txt` under `share/XOffsetDatastructure/licenses/`, and per-file SHA-256 digests in
  `share/XOffsetDatastructure/xoffset-third-party-manifest.txt`.
- Both full and storage installs contain `share/XOffsetDatastructure/VERSION`, `CHANGELOG.md` and
  `ALPHA_COMPATIBILITY.md`.
- The include/lib/share subdirectories must be inside the prefix; exported targets only use relocatable
  install paths.

## Consumer acceptance and platform boundary

```sh
python3 tools/validate_vm_package.py --suite storage --out build/alpha-package-storage
python3 tools/validate_vm_package.py --suite full --out build/alpha-package-full \
  --compiler /path/to/p2996/bin/clang++ --library-dir /path/to/matching/libcxx
```

On Windows, running the first command with `python` from an x64 MSVC developer prompt is enough; this
does not depend on the date-specific Windows runner. `--source-dir`, `--sdk`, `--generator` and
`--jobs` can be given. Use a new output directory, outside the source directories the validator copies.
The tool does not download dependencies.

The validator pins a copy of the source, builds the install, checks the exact package contents,
licenses and version policy, then moves the prefix so the original production source path is
unavailable, and runs 3 installed and 3 source-subproject multi-translation-unit consumers. It covers
owner transfer, pool return/reuse, clear with a live owner, destroying the handle first and static
destruction across translation units, and checks that the source and install packages are unchanged
before and after. It does not replace full, sanitizer, TSan or wire validation. Shared libraries,
dynamic plugins, cross-CRT use, mixed toolchains or mixed header versions are not covered by this
acceptance.

Below are recorded baselines from the 0.1.0-alpha.1 (V1 format) era, not V2; V2 validation records are
in the Validation scope of [BUILD_AND_PLATFORMS.md](BUILD_AND_PLATFORMS.md#validation-scope) (GitHub
CI has been off since 2026-10-23; since then the local validation of `tools/local_validate.sh` and
`tools/platform_matrix.sh` is the record). Acceptance of an archive itself
is reported separately and is not inherited from the baselines:

| Environment/configuration | Recorded result | Scope |
| --- | --- | --- |
| Windows x64 MSVC 19.44 | storage 2/2; installed 3/3, subproject 3/3 | Native run on 2026-10-16; full P2996 not supported/not verified |
| WSL2 x86_64 GCC 11.4 | storage 2/2; installed 3/3, subproject 3/3 | Functional evidence from a same-architecture VM |
| WSL2 P2996, initial recovery profile | full 48/48, plus 3 configure checks | `linux-current-20261016` |
| WSL2 P2996, final complete profile | full installed 3/3, subproject 3/3; network ASan/UBSan 34 groups/182 sub-scenarios; 2 controlled defects detected | `linux-current-complete-v2-20261016`; not a re-run of all 48 full items on the final profile |
| Historical macOS arm64, pinned P2996/AppleClang storage | full 48/48; full and storage each installed 3/3, subproject 3/3 | Earlier record, not a new local run |
| Historical Linux x86_64 pinned Docker image | full install and source consumption 3 items each | Emulated on an arm64 host, functional evidence only |

This evidence and its identities were in `docs/VM_FINAL_DELIVERY.md`, which was moved out of the source
tree on 2026-10-27; how to retrieve it is in [ARCHIVED_EVIDENCE.md](ARCHIVED_EVIDENCE.md); the slimmed
source archive does not contain these research records. WSL or Docker can be used for later Linux
performance work, and a separate physical machine is not currently an admission requirement; the
virtualization class must still be stated and noise qualification obtained first. In this WSL's only
78-process F/F calibration, none of the four metrics passed the local anchor and drift thresholds, so no
2% comparison capability was established, and there are no D/F, ablation or adoption conclusions.
Observations of background load or balanced power do not amount to causal attribution, and the original
historical records are not rewritten because of them.

This version does not promise the three N7 I/O extensions, a production asynchronous transport or
business network integration. The network scope verified so far is a synchronous TCP loopback reference
and a separate lifecycle model; without real business prerequisites, F stays.

## Client binary size and compile time (SC-14)

How much larger a client's install package gets after adding XOffset, how much more each additional
root schema adds, and how long one translation unit takes to compile. The measurement program is
`tools/client_size/size_client.cpp`: after receiving a message it runs `try_load_verified`, reads fields,
changes one field, `save_compact`, and dispatches on the first byte to N root schemas (`Msg1`..`MsgN`,
each with an `XString`, two `XVector`s and a fixed-size array, with `XOFFSET_RECORD` field lists). The
N=0 baseline is the same program without XOffset. Built the same way as a real app: `MinSizeRel`
(`-Os`), `-ffunction-sections -fdata-sections` plus `--gc-sections` (`-dead_strip` on Apple), bytes
after strip.

```sh
tools/client_size/measure.sh android-arm64 OUT [--std 20|23]   # needs ANDROID_NDK
tools/client_size/measure.sh ios-device OUT                      # Mac, Xcode
```

Android arm64-v8a, NDK r29 Clang 21.0.0, API 28, `c++_static`, C++20 field-list build, commit e97c681
(bytes):

| Exceptions | Baseline (no XOffset) | 1 schema | Library itself (1 schema minus baseline) | 2 | 4 | 8 | Per additional schema |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| On | 193,416 | 261,248 | **+67,832** | 273,312 | 297,456 | 348,144 | **+12,414** |
| Off (`-fno-exceptions`) | 193,320 | 258,040 | **+64,720** | 267,752 | 287,176 | 327,752 | **+9,959** |

Demo client (`mapdemo_client` of `demo/mapstate`, with networking, handshake, three encodings and all
validation):

| Build | With zstd 1.5.7 | Without zstd |
| --- | ---: | ---: |
| Exceptions on | 1,006,176 | 639,792 |
| Exceptions off | 973,368 | 607,720 |

- The library itself is about 65 KB and each root schema about 10–12 KB (about 2.5 KB less without
  exceptions). C++23 builds differ by under 0.5% (+67,960 / +12,745; without exceptions
  +64,864 / +10,279).
- zstd takes about 366 KB of the demo client; clients that do not compress drop it.
- Compile time of one translation unit (median of 3, cloud Xeon, `-Os`): the program without XOffset
  0.56 s, 1 schema 2.64 s, 8 schemas 5.09 s, the demo's `client_core.cpp` 5.15 s. Header parsing takes
  about 2 s, then about 0.35 s per schema; keep send/receive code in a few translation units instead of
  including the schemas in every file.

iOS arm64 (`ios-device`), Apple Clang 21.0.0 from Xcode 27.1 (clang-2100.3.34.2), `-dead_strip`,
`strip -x`, C++20 field-list build, commit cdfe3a8 (library headers identical to e97c681 except that on
Mach-O the 64 MiB owner table is put in a zero-fill section) (bytes):

| Exceptions | Baseline (no XOffset) | 1 schema | Library itself (1 schema minus baseline) | 2 | 4 | 8 | Per additional schema |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| On | 50,448 | 108,648 | **+58,200** | 125,288 | 142,136 | 175,856 | **+9,601** |
| Off (`-fno-exceptions`) | 50,328 | 107,848 | **+57,520** | 108,088 | 124,920 | 158,608 | **+7,251** |

| Build | With zstd 1.5.7 | Without zstd |
| --- | ---: | ---: |
| Exceptions on | 676,848 | 294,160 |
| Exceptions off | 641,296 | 258,616 |

- On iOS the library itself is about 57 KB and each root schema about 7–10 KB, both about 15% smaller
  than on Android; C++23 builds differ by under 0.1% (+58,216 / +9,602; without exceptions
  +57,536 / +7,254).
- Compile time of one translation unit (median of 3, Mac, `-Os`): without XOffset 0.18 s, 1 schema
  0.77 s, 8 schemas 1.44 s, `client_core.cpp` 1.36 s.
- Before the fix, every iOS program was 64 MiB larger: the Mach-O compiler put zero-initialized
  variables declared `inline` in `__DATA,__data`, so the whole owner table was written into the
  executable. `xoffset_slot_storage.hpp` puts it in `__DATA,__bss` on Apple (3.2, PR #162); on ELF it
  was already in `.bss` and takes no file space. iOS clients on 3.1 should upgrade to a version with
  this fix.
