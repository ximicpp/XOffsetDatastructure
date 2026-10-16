# XOffsetDatastructure

[![CI](https://github.com/ximicpp/XOffsetDatastructure/actions/workflows/ci.yml/badge.svg)](https://github.com/ximicpp/XOffsetDatastructure/actions/workflows/ci.yml)

XOffsetDatastructure is a C++26/P2996 serialization library built around one idea: keep data in a byte-stable in-memory layout so save/load becomes direct byte transfer instead of encode/decode work.

The library stores a single reflected root object inside an address-stable virtual-memory buffer backed by an XOffset-owned arena runtime and uses TypeLayout for compile-time layout signatures and byte-copy safety checks.

## Requirements

- Clang with P2996 reflection support and `<experimental/meta>`
- 64-bit little-endian platform
- Linux, macOS, or Windows virtual-memory APIs
- Git submodules initialized for `external/typelayout` and `external/boost`

The reflected public library requires P2996; its API has no non-reflection fallback.
The independent `xoffset_virtual_memory.hpp` storage layer requires C++17.

## Minimal Example

```cpp
#include "xoffsetdatastructure.hpp"

using namespace XOffsetDatastructure;

struct Player {
    int32_t id{0};
    int32_t level{0};
    XString name;
    XVector<int32_t> items;
};

XOFFSET_REGISTER_SCHEMA_NAME(Player, "example.Player")

int main() {
    TypedXBuffer<Player> xbuf(4096);
    auto* player = xbuf.make();
    player->id = 1;
    player->level = 10;
    player->name = "Alice";
    player->items.push_back(101);

    std::string bytes = xbuf.save_verified();

    auto loaded = TypedXBuffer<Player>::load(bytes);
    auto& restored = loaded.root();
    return restored.level == 10 ? 0 : 1;
}
```

More complete examples live in:

- `examples/helloworld.cpp`
- `examples/demo.cpp`
- `examples/stable_storage.cpp`

## Stable Storage And Capacity

Every owning buffer reserves a continuous address range and enables read/write
pages as its logical length grows. Successful creation/load establishes a base
address that stays unchanged through `grow()`, logical `shrink_to_fit()`, and
save. `grow()` at the hard limit or an OS enable failure returns `false`; it never
falls back to a relocating allocation.

The default maximum is 64 MiB (or the initial length, if larger). Define
`XOFFSET_DEFAULT_MAXIMUM_CAPACITY` consistently across translation units to
change it globally, or configure a strict per-buffer maximum:

```cpp
TypedXBuffer<Player> buffer(4096, XBuffer::maximum_capacity(8 * 1024 * 1024));
auto* player = buffer.make();
if (!buffer.grow(64 * 1024)) throw XBadAlloc();
// player still points to the same root object.
auto snapshot = buffer.save_verified(); // copies output; base remains unchanged
auto loaded = TypedXBuffer<Player>::load(snapshot,
    XBuffer::maximum_capacity(8 * 1024 * 1024));
```

`maximum_capacity()`/`reserved_capacity()` report the logical hard limit;
`reserved_bytes()` reports the OS reservation after page rounding;
`accessible_bytes()` reports enabled read/write pages, not resident physical
memory; `get_size()`/`segment_size()` report the logical allocator extent.
Logical shrink retains enabled pages. Regrowth zeroes newly exposed bytes.
`reclaim_tail_pages()` explicitly discards complete pages beyond the logical end;
its result distinguishes success, failure, and partial discard/protection success.
For 10,000 default buffers, reservations consume 625 GiB of virtual address
space; configure smaller explicit limits for many small buffers. The existing
offset ceiling remains below 2 GiB.

The old `max_capacity(bytes)` parameter remains a minimum reservation hint,
raising the default when needed. Use `maximum_capacity(bytes)` for a strict
limit. `get_buffer()` now returns a borrowed span over logical bytes; use
`data()` and length queries or `copy_bytes()` for an explicit owning copy.
Distribute `xoffset_virtual_memory.hpp` alongside the public header.

Growth defaults to `XGrowPolicy::Explicit`. To let allocations grow within a
fixed limit, use `.with_growth(XGrowPolicy::Automatic)` or call
`set_growth_policy()`. A shared pool is also opt-in:

```cpp
constexpr std::size_t limit = 8 * 1024 * 1024;
XBufferPool pool({4, 4 * limit, 4 * limit}); // idle count, reserved bytes, accessible bytes
auto options = XBuffer::maximum_capacity(limit)
    .with_growth(XGrowPolicy::Automatic).with_pool(pool);
auto buffer = TypedXBuffer<Player>::create(4096, options);
// Allocations can enable more pages; the configured limit still applies.
auto wire = buffer.save_verified();
auto result = buffer.reclaim_tail_pages(); // optional; regrowth may incur page faults
auto loaded = TypedXBuffer<Player>::load(wire, options);
pool.clear(); // releases idle reservations; live buffers remain valid
```

Pool limits cover idle reservations only. Policy and pool configuration follow
move/swap and are never serialized. See the [follow-up results](docs/VM_MEMORY_REUSE_RESULTS.md)
for the measured costs of these choices and the rejected allocator prototypes.
The [pool workload study](docs/VM_POOL_WORKLOAD_RESULTS.md) covers mixed capacities,
equal total budgets and contention; the [Automatic trace study](docs/VM_AUTOMATIC_TRACE_RESULTS.md)
explains layout tradeoffs and why the tested cache-flush candidate was rejected.
See [the migration guide](docs/STABLE_VM_STORAGE_MIGRATION.md),
[design](docs/STABLE_VM_STORAGE_DESIGN.md), and
[actual validation](docs/STABLE_VM_STORAGE_VALIDATION.md).

Independent C++17 storage builds and reproducible full-library checks are
documented in the [maintained validation entry point](docs/VM_VALIDATION_RUNNER.md).
Pinned compiler/dependency profiles are in [toolchains](toolchains/README.md).
OS-only platform checks and full P2996 support are reported separately.
Validation is profile-specific: native Windows execution and independent native
Linux performance remain unverified. The recorded Linux amd64 full-library
profile was emulated on an ARM64 host; ARM64 GNU storage results are separate.
See the [stage 2 delivery and validation](docs/VM_STAGE2_RESULTS.md) for the installable
package and the current policy decisions.

The [experimental synchronous TCP reference consumer](docs/VM_NETWORK_IMPLEMENTATION.md)
has accepted native macOS functionality and lifecycle evidence. See
[network measurement status](docs/VM_NETWORK_PERFORMANCE.md) for per-size calibration
and pooled-comparison scope. Production transport and representative business
integration remain open; no new public I/O API has been adopted.

## Safety Rules

- Only store types that satisfy `boost::typelayout::is_byte_copy_safe_v<T>`.
- For the fixed-schema verified path, root types should also satisfy `is_v1_wire_admitted_v<T>`.
- Existing live arena allocations retain their addresses through `grow()`, `shrink_to_fit()`, and save. Container expansion, insert/erase, assignment, and replacement may still move or destroy elements. Compaction creates a separate buffer; it does not grow the original storage in place.
- There is no per-object delete. The buffer owns all contained objects.
- Writes are not thread-safe. Synchronize externally.
- `XHandle<T>` follows its C++ buffer object across storage replacement and swap; a moved-from object's handle becomes empty. Checked handles become empty when replaced by unverified data or another root type. The buffer object must outlive its handles.
- Container objects themselves must live inside their buffer. Pass native strings/views to insertion APIs instead of constructing allocator-bound `XString` temporaries on the stack.
- The maximum payload is `XBufferCore::MAX_PAYLOAD` (`0x7ffffff8`, below 2 GiB). Oversized container counts are rejected before narrowing or multiplying them.
- In default Explicit mode, allocation can throw `XBadAlloc` when the current logical extent is exhausted. Automatic mode grows within the configured limit and still throws on hard-limit or OS failure.
- For Explicit mode, catch `XBadAlloc`, call `grow()`, then retry the failed mutation. The root address stays valid, but a failed or successful container operation may change its internal capacity.
- Library container mutations preserve their logical contents on allocation failure; capacity and allocator bookkeeping need not be identical. This does not make a sequence of user operations, ordinary record assignment, or external constructor side effects transactional.
- Prefer `TypedXBuffer<T>::load(...)` or `XBuffer::load_verified<T>(...)`; use `load_unverified()` only for trusted raw payloads.
- `save()` and `save_verified()` use normalized transport semantics: they preserve the admitted object graph and continued mutation, but they do not promise identical post-load spare capacity or allocator fragmentation state.
- Save produces an owning byte copy. Load copies input into a new reservation and verifies it as appropriate. This is zero-decode transport, not universally zero-copy transfer.

Example recovery pattern:

```cpp
try {
    xbuf.root<GameData>().items.push_back(42);
} catch (const XBadAlloc&) {
    if (!xbuf.grow(64 * 1024)) throw;
    auto& root = xbuf.root<GameData>();
    root.items.push_back(42);
}
```

## Build And Test

Clone with submodules:

```bash
git clone --recursive https://github.com/ximicpp/XOffsetDatastructure.git
cd XOffsetDatastructure
```

Quick path:

```bash
./build.sh
```

Useful options:

- `./build.sh --debug`
- `./build.sh -j 8`
- `./build.sh --compiler /path/to/clang++`
- `./build.sh --verbose`

`build.sh` performs configure, build, `ctest`, exact target-matrix signature export, matrix inventory validation, and compatibility self-check.

Manual CMake path:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/path/to/clang++
cmake --build build -j
ctest --test-dir build --output-on-failure -C Release
build/bin/Release/export_signatures tools/sigs
build/bin/Release/check_signature_matrix tools/sigs
build/bin/Release/check_compat
```

If your generator emits single-config binaries into `build/bin/`, use that path instead of `build/bin/Release/`.

## Installed And Embedded Consumers

Full-library targets require CMake 3.30+ and a matching experimental P2996
Clang/libc++ toolchain. The independent storage target requires C++17 and CMake
3.13+. Use `XOffset::xoffset` or `XOffset::storage`; source subprojects default
their tests, tools and examples to OFF. The full installation bundles both
public headers and pinned dependency headers with licenses. See the
[package guide](docs/VM_PACKAGING.md) for install, relocation, component selection
and external multi-file consumer validation.

## Docker / CI Image

CI uses the registry digest pinned in `toolchains/vm-validation.json`. You can run the same flow locally:

```bash
XOFFSET_IMAGE=$(python3 -c 'import json; print(json.load(open("toolchains/vm-validation.json"))["profiles"]["linux-p2996"]["image"])')
python3 tools/prepare_vm_cmake.py --platform linux-x86_64 --out build/cmake-tool
docker run --rm --platform linux/amd64 \
  -v "$PWD":/workspace \
  -w /workspace \
  -e PATH=/workspace/build/cmake-tool/root/bin:/opt/p2996-toolchain/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  "$XOFFSET_IMAGE" \
  bash ./build.sh
```

The preparation command uses a fresh directory and verifies the official CMake
archive against the pinned hash. The compiler image itself is unchanged.

## Signature Tools

- `tools/export_signatures.cpp` exports the exact repository target matrix into `tools/sigs/`
- `tools/check_signature_matrix.cpp` enforces that `tools/sigs/` contains exactly the required target-set baselines
- `tools/check_signature_regeneration.cmake` re-exports into a temp directory and fails on drift
- `tools/check_compat.cpp` compares the committed baselines and fails if any exported type stops matching across the matrix

The default `ctest` run includes inventory checking, baseline regeneration
checking, and compatibility checking. `build.sh` and CI run the same flow as the
full contract check.

The exact target matrix is:

- `x86_64_windows_clang`
- `x86_64_linux_clang`
- `arm64_ios_clang`
- `arm64_android_clang`

Each `.sig.hpp` file in `tools/sigs/` is a committed repository-managed matrix
baseline. The current exporter rewrites the exact set from the explicit wire
catalog plus target ABI metadata and removes stale files. Strong native
per-target evidence still requires target-native export jobs.

## Dependency Surface

The vendored Boost checkout is still the full superproject, but the active build only exposes the current header subset needed by XOffsetDatastructure and TypeLayout. The runtime buffer backend no longer depends on Boost.Interprocess.

## VM Storage Performance

The [native macOS comparison](docs/VM_PERFORMANCE_COMPARISON_RESULTS.md) measures
the fixed `07d6499` vector implementation against the initial VM storage, using
the same P2996 compiler, dependencies and verified-wire checks. It includes all
18 workloads, raw samples and frozen source snapshots. Growth and first-save
costs improve; preallocated append is similar, while many small arenas and large
verified loads regress on the measured host.

The [reviewed improvement plan](docs/VM_STORAGE_IMPROVEMENT_PLAN.md) has now been
implemented: global shutdown and const-handle repairs, direct load/save copying,
an exact-base owner index, and bounded vector/string tail expansion. See the
[follow-up results](docs/VM_STORAGE_IMPROVEMENTS_RESULTS.md) for individual and
combined measurements, validation, and decisions on optional memory policies.
The original comparison above remains historical evidence.

Reproduce the comparison with the configured compiler and dependencies:

```sh
python3 tools/run_vm_perf_compare.py --runs 7 --warmups 1
```

## Repository Layout

- `xoffsetdatastructure.hpp`: public library header
- `xoffset_virtual_memory.hpp`: independent OS storage layer
- `tests/`: unit and behavior tests
- `examples/`: demo programs
- `tools/`: signature export, compatibility and performance tools
- `tools/sigs/`: committed signature baselines
- `external/typelayout/`: type-signature engine
- `external/boost/`: vendored Boost dependency tree
