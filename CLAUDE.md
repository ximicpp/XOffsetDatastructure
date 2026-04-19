# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Project Is

XOffsetDatastructure is a **single-header C++26 library** (`xoffsetdatastructure.hpp`) for
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
  ghcr.io/ximicpp/typelayout-p2996:latest bash ./build.sh

# Apple Silicon: add --platform linux/amd64
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

**Critical CMake rule**: Use `add_compile_options(-std=c++26 -freflection ...)` — never `set(CMAKE_CXX_STANDARD 26)` (CMake 3.10 doesn't support it).

## Test Suite (25 tests)

- **Basic (7)**: test_basic_types, test_vector, test_map_set, test_nested, test_compaction, test_modify, test_xbuffer_api
- **Reflection (18)**: test_reflection_core, test_reflection_advanced, test_type_signatures, test_field_limit_fix, test_type_safety, test_wire_admission, test_enum_support, test_xstring_direct_assign, test_xhandle, test_schema_envelope, test_fixed_layout_containers, test_error_paths, test_memory_efficiency, test_zero_boilerplate, test_complex_nesting, test_inheritance, test_adaptive_reservation, test_main_containers

When adding a test: register in `tests/CMakeLists.txt` (add to `REFLECTION_TESTS` list), add `run_test` call in `build.sh`, update `tests/README.md`.

## Architecture

### Single header: `xoffsetdatastructure.hpp`

The entire library lives in one header, organized as:

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
- CI runs on push to both: `.github/workflows/ci.yml` pulls `ghcr.io/ximicpp/typelayout-p2996:latest` Docker image, builds, runs all 23 tests, and checks for type-signature contract drift in `tools/sigs/`.

## OpenSpec Change Process

Design changes go through `openspec/changes/<name>/` with proposal.md, design.md, tasks.md, and spec updates. Completed changes are archived to `openspec/changes/archive/`. Tasks use tags: `[needs-tests]`, `[needs-docs]`, `[core]`, `[tests]`, `[docs]`.
