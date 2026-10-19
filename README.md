# XOffsetDatastructure

Development candidate: see [CANDIDATE.md](CANDIDATE.md) and the
[next-version design](docs/PERFORMANCE_V3_DESIGN.md).
The alpha version below identifies ancestry; this candidate is not a published release.

**0.1.0-alpha.1** is an experimental C++26/P2996 library for reflected objects
and offset containers stored in an address-stable virtual-memory arena. It is
for users who can pin an experimental compiler and validate their own schemas.
The independent OS storage header can also be used with C++17.

An owning buffer keeps its base address through growth, logical shrink and save.
Saving produces an owning byte copy; loading validates and copies bytes into a
new reservation. The format avoids per-field decoding. Save/load are not
zero-copy operations.

Start with the [package guide](docs/VM_PACKAGING.md) and read the
[alpha compatibility contract](docs/ALPHA_COMPATIBILITY.md) before persisting or
accepting wire data. [CHANGELOG.md](CHANGELOG.md) records this prerelease.

## Requirements

| Use | Requirements |
| --- | --- |
| Full library, `XOffset::xoffset` | P2996 Clang with `<experimental/meta>` and matching libc++; CMake 3.30+; 64-bit little-endian target; Linux, macOS or Windows VM APIs |
| OS storage only, `XOffset::storage` | C++17 compiler; CMake 3.13+; Linux, macOS or Windows VM APIs and threads |
| Maintained validation tools and their tests | Python 3.10+; CMake/CTest 3.21+ for storage, 3.30+ for full; the C++ library has no Python runtime dependency |

Ordinary C++26 support does not imply P2996 support. The full API has no
non-reflection fallback. Native Windows full-library support is not established
for this alpha; the tested Windows target is storage only.

The source archive includes the locked TypeLayout and ten Boost header modules.
Full installed packages include the same 851 dependency headers. Neither package
includes a compiler, SDK or runtime. Fixed compiler/image, CMake and dependency
recipes are in [toolchains/README.md](toolchains/README.md),
[toolchains/vm-validation.json](toolchains/vm-validation.json), and the
[package guide](docs/VM_PACKAGING.md#fixed-inputs).

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
the arena, register each root schema, and satisfy both byte-copy safety and V1
wire admission. A schema name alone does not establish compatibility.
See `examples/helloworld.cpp`, `examples/stable_storage.cpp` and
`examples/demo.cpp` for more examples.

## Build and consume

Extract the versioned source archive; it already contains the required dependency
headers. In a Git checkout, first initialize the exact submodules using the
[LF-preserving recipe](docs/VM_PACKAGING.md#fixed-inputs).

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
if(NOT XOffsetDatastructure_VERSION STREQUAL "0.1.0-alpha.1")
    message(FATAL_ERROR "This consumer requires XOffsetDatastructure 0.1.0-alpha.1")
endif()
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE XOffset::xoffset)
```

Configure the consumer with `-DCMAKE_PREFIX_PATH=/path/to/xoffset-prefix` and the
same compatible P2996 toolchain. For storage, request `COMPONENTS storage`, link
`XOffset::storage`, and use CMake 3.13+. Alpha packages reject numeric, `EXACT`
and range version requests; check the complete version string as above.

For embedded source consumption, use `add_subdirectory(path/to/XOffsetDatastructure)`
and link the same target. Subprojects default tests, tools, examples and install
rules to OFF. Set `XOFFSET_STORAGE_ONLY=ON` before adding the subdirectory when
only storage is needed.

## Validation scope

These are recorded profile-specific results, not a promise for every compiler
on each OS. The 2026-10-16 continuation kept the accepted production headers
unchanged. Release-archive validation is reported separately from this baseline.

| Environment | Recorded result | Boundary |
| --- | --- | --- |
| Native Windows x64, MSVC 19.44 | storage 2/2; installed 3/3; subproject 3/3 | Windows full P2996, sanitizer and performance unverified |
| WSL2 Ubuntu 22.04 x86_64, GCC 11.4 | storage 2/2; installed 3/3; subproject 3/3 | Same-architecture virtual machine |
| WSL2, P2996 `9ffb96e...`, initial restored profile | full 48/48; 3 separate configure checks | `linux-current-20261016` profile |
| WSL2, same compiler, final runtime/link profile | full installed 3/3 and subproject 3/3; network ASan/UBSan 34 groups / 182 subcases; 2 controlled defects detected | `linux-current-complete-v2-20261016`; this is not a rerun of all 48 full tests |
| Historical native macOS arm64, pinned P2996 | full 48/48; full and storage installed 3/3 plus subproject 3/3 | Earlier recorded runs, not rerun on this Windows host |

The full development checkout contains `docs/VM_WINDOWS_CURRENT_RESULTS.md`,
`docs/VM_LINUX_CURRENT_RESULTS.md`, and `docs/VM_FINAL_DELIVERY.md` with evidence
identities. These research records and their historical archives are omitted
from the compact source distribution. The maintained full/storage entrypoints
are described in [VM_VALIDATION_RUNNER.md](docs/VM_VALIDATION_RUNNER.md) and the
[package guide](docs/VM_PACKAGING.md).

WSL and Docker are acceptable environments for further Linux performance work;
record their virtualization class and qualify measurement noise. The one current
WSL F/F calibration ran 78 serial processes, but all four metrics failed the
predeclared local-anchor and drift qualifications. It establishes no 2% comparison
capability, speedup, equivalence or regression. No D/F comparison, ablation or
optimization adoption follows from it. The development checkout records this in
`docs/VM_N3_CURRENT_RESULTS.md`; the original evidence remains unchanged.

The synchronous TCP loopback reference and separate lifecycle model do not
establish production asynchronous transport support. The three N7 I/O extensions,
production business integration and performance attribution are outside this
alpha's commitments.

## Storage and ownership rules

- A strict per-buffer limit is `XBuffer::maximum_capacity(bytes)`. The default is
  64 MiB, or the initial length if larger; the payload ceiling is `0x7ffffff8`.
  `max_capacity(bytes)` is a legacy reservation hint, not a strict limit.
- Growth defaults to `XGrowPolicy::Explicit`; exhaustion throws `XBadAlloc`.
  `XGrowPolicy::Automatic` and `XBufferPool` are opt-in. Pool budgets bound idle
  cached reservations, not live owners or total process memory.
- `reserved_bytes()` counts page-rounded virtual address space;
  `accessible_bytes()` counts enabled pages, not RSS. Logical shrink retains
  enabled pages; `reclaim_tail_pages()` explicitly attempts page reclamation.
- Arena/root addresses remain stable through growth and save. Container mutation
  can move or destroy elements. Borrowed spans and handles do not own the buffer;
  the C++ buffer object must outlive its handles. Synchronize writes externally.
- Library container mutations preserve logical contents on allocation failure;
  capacities and allocator bookkeeping need not roll back. User operation
  sequences and external constructor effects are not transactions.
- Bound wire size, per-buffer capacity and aggregate in-flight work before
  accepting untrusted input. Verified validation is not cryptographic
  authentication; see the [compatibility contract](docs/ALPHA_COMPATIBILITY.md).

Both public headers are required for full consumers. Keep header versions,
compile-time options and toolchain settings consistent across translation units.
The alpha does not promise stable source API, ABI or arbitrary cross-toolchain
wire compatibility.


候选扩展：[完整校验后的直接接收与可变访问](docs/VERIFIED_RECEIVE.md)。支持域、完成/取消合同及平台内存成本见说明；最终验收记录随本机证据另行提供。
