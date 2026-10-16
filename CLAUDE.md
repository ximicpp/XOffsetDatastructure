# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Project Is

XOffsetDatastructure is a **header-only C++26 library** (`xoffsetdatastructure.hpp`,
with `xoffset_virtual_memory.hpp` for the OS storage layer) for
fixed-schema, zero-decode data buffers. It requires **Clang with P2996 reflection support**
(a custom Clang fork) — not standard Clang or GCC.

Key concepts: frozen-layout containers (`XString`, `XVector`, `XSet`, `XMap`, `XBlob`),
compile-time type admission via TypeLayout, reflection-based zero-boilerplate construction,
verified load/save, and arena compaction.

## Build & Test

### Quick build (preferred)

```bash
./build.sh          # auto-detects P2996 Clang, builds, runs tests, exports signatures
./build.sh --debug  # debug build
./build.sh -j 8     # parallel jobs
```

### Docker build (when no local P2996 compiler)

```bash
# MUST use 'bash' prefix — './build.sh' alone fails on permissions
docker run --rm -v $(pwd):/workspace -w /workspace \
  -e PATH=/workspace/build/cmake-tool/root/bin:/opt/p2996-toolchain/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  ghcr.io/ximicpp/typelayout-p2996@sha256:fab845dadf9c7171d104cd34f8a3941119809e49baa7d0cd7ce1a7a2d71ad999 bash ./build.sh

# Apple Silicon: add --platform linux/amd64
# First prepare CMake with:
# python3 tools/prepare_vm_cmake.py --platform linux-x86_64 --out build/cmake-tool
```

### Run a single test

```bash
cd build
./bin/Release/test_basic_types       # run one test directly
ctest -R "test_basic_types" --verbose  # or via CTest
```

### CMake from scratch

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=~/clang-p2996-install/bin/clang++
cmake --build . --config Release -j$(nproc)
ctest --verbose
```

**CMake rule**: full-library settings are target-scoped in `cmake/XOffsetBuildChecks.cmake`.
Full targets require CMake 3.30+ for transitive `cxx_std_26`; storage keeps its CMake 3.13 minimum.
Do not apply them globally to the independent C++17 storage targets.

### Standalone OS storage (no reflection or submodules)

```bash
cmake -S . -B build-storage -DXOFFSET_STORAGE_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-storage --config Release --parallel 2
ctest --test-dir build-storage -C Release --output-on-failure
```

This validates the OS owner and pool only. It does not establish full XBuffer
or Windows P2996 support. Fixed full-library inputs are recorded in
`toolchains/vm-validation.json`; use `tools/check_vm_toolchain.py` before
historical comparisons. Supply the SDK explicitly rather than relying on xcrun's
current default. `build.sh --build-dir PATH --sdk PATH` supports a fresh build
directory with an explicit macOS SDK.

## Test Suite (38 library executables, 48 CTest cases)

- **Basic (7)**: test_basic_types, test_vector, test_map_set, test_nested, test_compaction, test_modify, test_xbuffer_api
- **Reflection (29)**: test_reflection_core, test_reflection_advanced, test_type_signatures, test_field_limit_fix, test_type_safety, test_wire_admission, test_enum_support, test_xstring_direct_assign, test_xhandle, test_schema_envelope, test_fixed_layout_containers, test_error_paths, test_memory_efficiency, test_zero_boilerplate, test_complex_nesting, test_inheritance, test_adaptive_reservation, test_main_containers, test_runtime_regressions, test_mutation_failures, test_wire_validation, test_stable_storage, test_vm_lifecycle, test_vm_transport, test_owner_registry, test_tail_expansion, test_storage_policies, test_allocator_model, test_allocator_failures
- **OS storage (2)**: test_virtual_memory_storage, test_vm_pool (standalone C++17; supplement the full reflection suite)

The 38 executables plus 5 lifecycle exit scenarios, 2 actual registry allocation
failure scenarios, and 3 tooling checks total 48 CTest cases. Configure-time
reflection, positive admission and polymorphic-rejection checks are separate.
The generated `build/test-inventory.json` is authoritative for the selected mode;
storage-only currently has 2 programs/2 CTest cases.

When adding a test: register in the appropriate list in `tests/CMakeLists.txt`.
`build.sh` runs CTest's inventory directly. Keep the CI inventory and docs current.

## Architecture

### Public header: `xoffsetdatastructure.hpp`

The public library header is organized as follows; its independent VM owner
lives in `xoffset_virtual_memory.hpp`:

1. **Platform guards** — enforces 64-bit little-endian only
2. **TypeLayout integration** — delegates ALL type safety to `external/typelayout` (`is_byte_copy_safe_v<T>`, `get_layout_signature<T>()`)
3. **Arena runtime** — `XBufferCore` owns a payload-resident arena header, root/allocator offsets, backend free blocks, and verified reopen logic
4. **Frozen-layout containers** — `XString`, `XVector<T>`, `XSet<T>`, `XMap<K,V>`, `XBlob` — all backed by XOffset-owned fixed ABI containers
5. **Type safety** — `static_assert(is_byte_copy_safe_v<T>)` at entry points (`make<T>()`, `allocator<T>()`, `compact<T>()`). Admission delegated entirely to TypeLayout.
6. **Reflect construct/transfer** — P2996-based zero-boilerplate: `reflect_init_all<T>()` auto-injects allocators, `reflect_transfer_init_all<T>()` deep-copies
7. **XBuffer** — user-facing API: `make<T>()`, `root<T>()`, `save_verified<T>()`, `load_verified<T>()`, `grow()`, `compact<T>()`
8. **XCompactor** — reflection-based deep migration using per-type strategies (Bitwise, AllocatorAware, Container, Composite)
9. **Registration macros** — `XOFFSET_REGISTER_TYPE`, `XOFFSET_REGISTER_CONTAINER`, `XOFFSET_REGISTER_MAP` — register types into both TypeLayout and XCompactor

### Key dependency: TypeLayout (`external/typelayout`)

Git submodule providing the type-signature engine. XOffset does NOT implement its own type introspection — everything is delegated to TypeLayout. Initialize with `git submodule update --init --recursive`.

### Compiler requirement

Clang P2996 fork (from bloomberg/clang-p2996). Searched at: `/usr/local/bin/clang++`, `~/clang-p2996-install/bin/clang++`, `/opt/clang-p2996/bin/clang++`, `/opt/p2996-toolchain/bin/clang++`. Compile flags: `-std=c++26 -freflection -fexpansion-statements -stdlib=libc++`.

## Code Conventions

- **Naming**: PascalCase classes, snake_case functions, mPascalCase members, UPPER_SNAKE_CASE constants
- **Namespace**: `XOffsetDatastructure` for the library, `boost::typelayout` for TypeLayout APIs
- **Tests**: Each test file has a `main()` returning 0 on success, 1 on failure. Uses `assert()` for validation, `std::cout` for status.
- **Registration macros must be at global scope** — they include a `_XOffset_NS_Sentinel` check that `static_assert` fails inside namespaces.

## Branches & CI

- **`next_cpp26`** — active development branch
- **`main`** — stable branch
- CI runs on push to both: `.github/workflows/ci.yml` reads the fixed Linux image
  digest from `toolchains/vm-validation.json`, verifies compiler/dependency
  identity, builds, runs CTest and checks signature drift in `tools/sigs/`.
  Separate Linux/macOS/Windows jobs run only the ordinary C++17 OS layer.
  A configured job is not a verified platform until its execution passes.

## OpenSpec Change Process

Design changes go through `openspec/changes/<name>/` with proposal.md, design.md, tasks.md, and spec updates. Completed changes are archived to `openspec/changes/archive/`. Tasks use tags: `[needs-tests]`, `[needs-docs]`, `[core]`, `[tests]`, `[docs]`.
