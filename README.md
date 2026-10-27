# XOffsetDatastructure

**3.1.0** is a C++26/P2996 library for reflected objects
and offset containers stored in one contiguous, position-independent arena. It is
for users who can pin an experimental compiler and validate their own schemas.
The independent OS storage header can also be used with C++17.

Objects live in one contiguous arena and refer to each other through relative
offsets, so the arena bytes are the serialized form: there is no per-field
encoding or decoding. Saving produces an owning byte copy; loading validates the
whole object graph and copies the bytes into a new reservation. Save and load
are not zero-copy operations.

This release uses the **V2 storage and wire format** ([format specification](docs/V2_FORMAT.md)).
V1 wires are rejected with `unsupported wire format version 1`; there is no V1
reader or migration path. Start with the [user guide](docs/USER_GUIDE.md) and
the [deployment guide](docs/DEPLOYMENT.md), then read the
[compatibility and input contract](docs/ALPHA_COMPATIBILITY.md) before
persisting or accepting wire data. [CHANGELOG.md](CHANGELOG.md) records each
release; [RELEASING.md](docs/RELEASING.md) is how one is made.

## Requirements

| Use | Requirements |
| --- | --- |
| Full library, `XOffset::xoffset` | P2996 Clang with `<experimental/meta>` and matching libc++, or GCC 16+ (`-freflection`, `<meta>`, libstdc++); CMake 3.30+; 64-bit little-endian target; Linux, macOS or Windows VM APIs |
| OS storage only, `XOffset::storage` | C++17 compiler; CMake 3.13+; Linux, macOS or Windows VM APIs and threads |
| Maintained validation tools and their tests | Python 3.10+; CMake/CTest 3.21+ for storage, 3.30+ for full; the C++ library has no Python runtime dependency |

Ordinary C++26 support does not imply P2996 support. The full API has no
non-reflection fallback. The supported compilers are the Bloomberg clang-p2996
fork pinned in [toolchains/README.md](toolchains/README.md) and GCC 16 (CI uses
the official `gcc:16` image, GCC 16.2.0, pinned by digest). CI checks that both
compilers produce byte-identical wires from the same source and accept each
other's wires.
Native Windows full-library support is not established; the tested Windows
target is storage only.

The source archive includes the locked TypeLayout and ten Boost header modules.
Full installed packages include the same dependency headers. Neither package
includes a compiler, SDK or runtime. Fixed compiler/image, CMake and dependency
recipes are in [toolchains/README.md](toolchains/README.md),
[toolchains/vm-validation.json](toolchains/vm-validation.json), and the
[package guide](docs/PACKAGING.md#fixed-inputs).

## Minimal full-library example

```cpp
#include "xoffsetdatastructure.hpp"
#include <cstdint>

using namespace XOffsetDatastructure;

struct Player {
    std::int32_t level{};
    XString name;
    XVector<std::int32_t> items;
};
XOFFSET_REGISTER_SCHEMA_NAME(Player, "example.Player")

int main() {
    constexpr std::size_t limit = 8 * 1024 * 1024;
    auto buffer = TypedXBuffer<Player>::create(4096,
        XBuffer::maximum_capacity(limit));
    buffer.root().level = 10;
    buffer.root().name = "Alice";
    buffer.root().items.push_back(101);

    auto bytes = buffer.save_verified();
    auto restored = TypedXBuffer<Player>::load(bytes,
        XBuffer::maximum_capacity(limit));
    return restored.root().level == 10 ? 0 : 1;
}
```

`TypedXBuffer<T>::load()` uses the verified path. Use library containers inside
the arena, register each root schema, and satisfy both byte-copy safety and wire
admission. A schema name alone does not establish compatibility: the reader's
type layout must match the writer's exactly (see
[schema changes](docs/USER_GUIDE.md#7-schema-变更)).
See `examples/helloworld.cpp`, `examples/stable_storage.cpp` and
`examples/demo.cpp` for more examples.

## Build and consume

Extract the versioned source archive; it already contains the required dependency
headers. In a Git checkout, first initialize the exact submodules using the
[LF-preserving recipe](docs/PACKAGING.md#fixed-inputs).

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

To install a full package, use a new build directory and your chosen prefix:

```sh
cmake -S . -B build/alpha-install \
  -DCMAKE_CXX_COMPILER=/path/to/p2996/bin/clang++ \
  -DCMAKE_INSTALL_PREFIX=/path/to/xoffset-prefix \
  -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF
cmake --build build/alpha-install --config Release
cmake --install build/alpha-install --config Release
```

An installed full-library consumer uses:

```cmake
cmake_minimum_required(VERSION 3.30)
project(MyConsumer LANGUAGES CXX)
find_package(XOffsetDatastructure CONFIG REQUIRED COMPONENTS full)
if(NOT XOffsetDatastructure_VERSION STREQUAL "3.1.0")
    message(FATAL_ERROR "This consumer requires XOffsetDatastructure 3.1.0")
endif()
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE XOffset::xoffset)
```

Configure the consumer with `-DCMAKE_PREFIX_PATH=/path/to/xoffset-prefix` and the
same compatible P2996 toolchain. For storage, request `COMPONENTS storage`, link
`XOffset::storage`, and use CMake 3.13+. Packages reject numeric, `EXACT` and
range version requests (no release promises compatibility with another one);
check the complete version string as above.

For embedded source consumption, use `add_subdirectory(path/to/XOffsetDatastructure)`
and link the same target. Subprojects default tests, tools, examples and install
rules to OFF. Set `XOFFSET_STORAGE_ONLY=ON` before adding the subdirectory when
only storage is needed.

## Validation scope

Results are profile-specific, not a promise for every compiler on each OS.
GitHub CI has been off since 2026-10-23; the rows that cite CI are records of
the 2.0 format from before that date. Since then the record is local
validation (`tools/local_validate.sh`) and the local platform matrix
(`tools/platform_matrix.sh`, [CLIENT_TARGETS.md](docs/CLIENT_TARGETS.md)).

| Environment | V2 result | Boundary |
| --- | --- | --- |
| 3.1.0 RC `e913d22` (format 2.1), local platform matrix: Linux x86_64 GCC 16 and P2996, Linux arm64 GCC 16 and P2996 (colima on Apple M4), Windows x64 MinGW-w64 GCC 16.2 (wine) | Full CTest 107/107 on each Linux column; Windows 105 pass, 2 skipped by design; 2.1 wire corpus byte-identical across all five | Functional only, no timing; MSVC, clang-cl, Android and iOS cannot build the library ([CLIENT_TARGETS.md](docs/CLIENT_TARGETS.md)) |
| macOS 26.3 arm64 (Apple M4), clang-p2996 `d77eff1` | Debug 78/78; TSan 75/75; ASan+UBSan passes except 8 cases inside the 9 known platform-limit cases (Darwin ASan replaces `operator new`, so injected host allocation failures cannot fire) | Recorded on 2026-10-21 for the V2 merge |
| Linux x86_64, pinned P2996 Docker image | Full build, CTest, signature check and package consumption run in CI ([ci.yml](.github/workflows/ci.yml)); a separate CI job runs ASan+UBSan on the allocator, wire and V2 tests | CI is the record; see the workflow runs |
| Linux x86_64, GCC 16.2.0 (`gcc:16` image) | Debug build, CTest and ASan+UBSan in CI; cross-compiler wire corpus exchanged with P2996 Clang (byte-identical, mutually accepted) | No x86_64 performance comparison with P2996 Clang (cloud GCC timing is reference only) |
| Linux arm64 VM on Apple M4 (colima vz), GCC 16.2.0 vs clang-p2996 `d77eff1` | Release CTest 98/98 with both; per-cycle state and wire bytes identical across the two compilers; SLG A–C xoffset÷baseline ≤ 0.50 under GCC in both batches (reference only) | GCC÷Clang wall-clock has no verdict: both allowed batches were invalid by their A/A and bridge rules; directions in [docs/perf/gcc16-20261026](docs/perf/gcc16-20261026/README.md) (2026-10-26) |
| Linux arm64 (native runner), GCC 16 | Full Debug CTest in CI; wire corpus byte-identical with Linux x86-64 | CI is the record ([client-targets.yml](.github/workflows/client-targets.yml)) |
| Windows x64, MSYS2 UCRT64 MinGW-w64 GCC 16.2 | Full Debug CTest in CI; wire corpus byte-identical with Linux x86-64 | MSVC full P2996 unverified; Android and iOS not feasible yet ([CLIENT_TARGETS.md](docs/CLIENT_TARGETS.md)) |
| Native Windows x64, MSVC | storage only (V1-era record) | MSVC full P2996 unverified |

V2 also passed a 100,000-case differential fuzz of the validators (35,000 under
ASan+UBSan) with zero disagreements. Earlier V1-era platform records
(`docs/VM_FINAL_DELIVERY.md` and the other `VM_`/`V1_` documents) were moved out
of the tree on 2026-10-27; [ARCHIVED_EVIDENCE.md](docs/ARCHIVED_EVIDENCE.md)
says how to retrieve them. They do not describe the V2 format.

## Performance

All timing claims come from one Apple M4 running one game-server workload
(`bench/slg`) against zpp_bits, cista, cereal, protobuf and FlatBuffers in the
same process. [docs/PERFORMANCE.md](docs/PERFORMANCE.md) has the numbers for
each release, the protocol and the limits. Treat them as specific to that
machine, workload and adapter set. No Linux or Windows timing figures are
claimed; the cloud instruction-count gate ([docs/PERF_GATE.md](docs/PERF_GATE.md))
only blocks regressions.

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
  reachable from the root must be released before `save_verified`; the V2 exact
  coverage check rejects unreachable regions.
- Bound wire size, per-buffer capacity and aggregate in-flight work before
  accepting untrusted input. Verified validation is not cryptographic
  authentication; see the [compatibility contract](docs/ALPHA_COMPATIBILITY.md).

Both public headers are required for full consumers. Keep header versions,
compile-time options and toolchain settings consistent across translation units.
A release does not promise stable source API, ABI or arbitrary cross-toolchain
wire compatibility; the [user guide](docs/USER_GUIDE.md#8-核心承诺与-api-稳定性) lists the
core guarantees (K0–K7, starting with K0: the payload holds no absolute
address, so after any modification the whole region can still be copied to
another address, process or machine) and which interfaces they cover.
