# Build, platforms and storage rules

Requirements, building, installing and consuming the library, and the storage rules. The
[README](../README.md) has the quick start; the [user guide](USER_GUIDE.md) is the full reference.

## Requirements

| Use | Requirements |
| --- | --- |
| Full library, C++26 reflection build (servers), `XOffset::xoffset` | P2996 Clang with `<experimental/meta>` and matching libc++, or GCC 16+ (`-freflection`, `<meta>`, libstdc++); CMake 3.30+; 64-bit little-endian target; Linux, macOS or Windows VM APIs |
| Client build, field-list backend | C++20 or C++23 with the platform's native compiler (Android NDK Clang, MSVC 19.44 with `/Zc:preprocessor`, clang-cl, MinGW GCC, Apple clang for iOS 14+ and macOS 12+); one `XOFFSET_RECORD` line per record; CMake 3.20+; 64-bit little-endian target |
| OS storage only, `XOffset::storage` | C++17 compiler; CMake 3.13+; Linux, macOS or Windows VM APIs and threads |
| Maintained validation tools and their tests | Python 3.10+; CMake/CTest 3.21+ for storage, 3.30+ for full; the C++ library has no Python runtime dependency |

Ordinary C++26 support does not imply P2996 support. Clients (Windows, Android, iOS) build the same
library with C++20/C++23 and a one-line field list per record
([user guide 2.1](USER_GUIDE.md#21-field-lists-xoffset_record)); servers keep
C++26 reflection, which also checks each field list, and both builds write the
same wire bytes ([CLIENT_TARGETS.md](CLIENT_TARGETS.md)). The supported
reflection compilers are the Bloomberg clang-p2996
fork pinned in [toolchains/README.md](../toolchains/README.md) and GCC 16 (the
official `gcc:16` image, pinned by digest). Local validation checks that both
compilers produce byte-identical wires and accept each other's.
Native Windows (MSVC, clang-cl) runs the full library only through the
field-list backend; the reflection builds do not support it.

C++20 and C++23 field-list builds use only the C++ standard library and require
no external submodules or third-party headers. C++26 reflection builds and the
full source archive include the locked TypeLayout and ten Boost header modules
that its signature tools and reflection tests use. Neither package includes a
compiler, SDK or runtime. Fixed compiler/image, CMake and dependency recipes are
in [toolchains/README.md](../toolchains/README.md),
[toolchains/vm-validation.json](../toolchains/vm-validation.json), and the
[package guide](PACKAGING.md#fixed-inputs).

## Build and consume

C++20/C++23 builds can configure directly from a plain Git checkout with no
submodules initialized (`-DXOFFSET_CXX_STANDARD=20` or `23`). For C++26
reflection builds from Git, first initialize the exact submodules using the
[LF-preserving recipe](PACKAGING.md#fixed-inputs) (the versioned source archive
already contains them).

For a full source build with an already prepared P2996 toolchain:

```sh
cmake -S . -B build/alpha-full -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/path/to/p2996/bin/clang++
cmake --build build/alpha-full --parallel 2
ctest --test-dir build/alpha-full --output-on-failure -C Release
```

For storage only, use your platform compiler:

```sh
cmake -S . -B build/alpha-storage -DXOFFSET_STORAGE_ONLY=ON
cmake --build build/alpha-storage --config Release --parallel 2
ctest --test-dir build/alpha-storage --output-on-failure -C Release
```

To install a full package (C++20/C++23 zero-dependency, or C++26 reflection with
`-DCMAKE_CXX_COMPILER=...` and `-DXOFFSET_CXX_STANDARD=26`), use a new build
directory and your chosen prefix:

```sh
cmake -S . -B build/xoffset-install \
  -DXOFFSET_CXX_STANDARD=20 \
  -DCMAKE_INSTALL_PREFIX=/path/to/xoffset-prefix \
  -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF
cmake --build build/xoffset-install --config Release
cmake --install build/xoffset-install --config Release
```

An installed full-library consumer uses:

```cmake
cmake_minimum_required(VERSION 3.20)   # 3.30+ for C++26 reflection installs
project(MyConsumer LANGUAGES CXX)
find_package(XOffsetDatastructure CONFIG REQUIRED COMPONENTS full)
if(NOT XOffsetDatastructure_VERSION STREQUAL "4.1.0")
    message(FATAL_ERROR "This consumer requires XOffsetDatastructure 4.1.0")
endif()
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE XOffset::xoffset)
```

Configure the consumer with `-DCMAKE_PREFIX_PATH=/path/to/xoffset-prefix` and a
matching C++20/C++23 compiler (or the matching P2996/GCC 16 toolchain for C++26
installs). For storage, request `COMPONENTS storage`, link `XOffset::storage`,
and use CMake 3.13+. Packages reject numeric, `EXACT` and range version requests
(no release promises compatibility with another one); check the complete version
string as above.

For embedded source consumption, use `FetchContent` or
`add_subdirectory(path/to/XOffsetDatastructure)` and link `XOffset::xoffset`:

```cmake
include(FetchContent)
FetchContent_Declare(
    xoffsetdatastructure
    GIT_REPOSITORY https://github.com/ximicpp/XOffsetDatastructure.git
    GIT_TAG        v4.1.0   # pin a release tag or commit SHA
)
set(XOFFSET_CXX_STANDARD 20 CACHE STRING "" FORCE)
FetchContent_MakeAvailable(xoffsetdatastructure)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE XOffset::xoffset)
```

Subprojects default tests, tools, examples and install rules to OFF. Set
`XOFFSET_STORAGE_ONLY=ON` before adding the subdirectory when only storage is
needed.

## Validation scope

Results are per toolchain, not a promise for every compiler on an OS. GitHub CI is off; the record is
`tools/local_validate.sh` (Linux GCC 13/16, Clang 18, P2996, sanitizers, and the client columns) and the
platform runs in [CLIENT_TARGETS.md](CLIENT_TARGETS.md) (Android NDK, MSVC, clang-cl, MinGW, Apple
clang, Linux arm64, real devices). Every column runs the full CTest and checks the cross-compiler wire
corpus byte for byte; none of them is a timing result.

## Storage and ownership rules

- A strict per-buffer limit is `XBuffer::maximum_capacity(bytes)`. The default is
  64 MiB, or the initial length if larger; the payload ceiling is `0x7ffffff8`.
  `max_capacity(bytes)` is a legacy reservation hint, not a strict limit.
- Growth defaults to `XGrowPolicy::Explicit`; exhaustion throws `XBadAlloc`.
  `XGrowPolicy::Automatic` and `XBufferPool` are opt-in. Pool budgets bound idle
  cached reservations, not live owners or total process memory.
- Storage is chosen when a buffer is created (`storage_kind()` reports it):
  - **VM** (a reserved virtual-memory range, address fixed for life): with an
    `XBufferPool`, with `with_storage(XStorage::Pinned)`, with Automatic growth
    and a maximum above `XBuffer::SLOT_MAX` (256 KiB − 64 B), or when the initial
    size is above `SLOT_MAX`.
  - **Pinned slot** (a slot of the library slab, address fixed for life): a
    maximum of at most `SLOT_MAX`.
  - **Movable slot**: otherwise, which includes the default (Explicit growth,
    64 MiB maximum) for small buffers and loads.
- Addresses stay fixed through Automatic growth, container mutation, save,
  `shrink_to_fit` and `reclaim_tail_pages`. On a movable slot, `grow()` may move
  the payload to a larger slot (or to VM above `SLOT_MAX`, pinned from then on),
  and so may `reload_verified`, `begin_verified_reload` and
  `set_growth_policy(XGrowPolicy::Automatic)`. A move invalidates root references,
  element pointers, `allocator<T>()` results and borrowed spans; `XHandle`
  re-resolves, and `epoch()` changes. `address_stable()` tells which case applies;
  use `with_storage(XStorage::Pinned)` for an address that never changes.
- `reserved_bytes()` counts page-rounded virtual address space (the slot size on
  a slot); `accessible_bytes()` counts enabled pages (the slot capacity on a
  slot), not RSS. Logical shrink retains enabled pages; `reclaim_tail_pages()`
  explicitly attempts page reclamation (a no-op on a slot). Freed slots stay in
  the process for reuse; `XBuffer::trim_slot_storage()` discards their pages.
- Container mutation can move or destroy elements. Borrowed spans and handles do
  not own the buffer; the C++ buffer object must outlive its handles. Synchronize
  writes externally.
- Library container mutations preserve logical contents on allocation failure;
  capacities and allocator bookkeeping need not roll back. User operation
  sequences and external constructor effects are not transactions.
- Arena memory allocated by hand (for example `XBuffer::allocator<T>()`) and not
  reachable from the root must be released before `save_verified`; the exact
  coverage check rejects unreachable regions.
- Bound wire size, per-buffer capacity and aggregate in-flight work before
  accepting untrusted input. Verified validation is not cryptographic
  authentication; see the [compatibility contract](ALPHA_COMPATIBILITY.md).

All library headers installed by CMake (`xoffset*.hpp`) are required for full
consumers. Keep header versions, compile-time options and toolchain settings
consistent across translation units.
A release does not promise stable source API, ABI or arbitrary cross-toolchain
wire compatibility; the core guarantees K0–K9 and the three receive check tiers
are in [user guide section 8](USER_GUIDE.md#8-core-guarantees-and-api-stability).
