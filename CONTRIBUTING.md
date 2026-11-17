# Contributing

Notes for people working on the library itself. Installing and consuming the
library, the platform matrix and storage rules are in
[docs/BUILD_AND_PLATFORMS.md](docs/BUILD_AND_PLATFORMS.md).

## Build and test

The full library needs Clang with P2996 reflection
(`-std=c++26 -freflection -fexpansion-statements -stdlib=libc++`).
`build.sh` looks for it at `/usr/local/bin/clang++`,
`~/clang-p2996-install/bin/clang++`, `/opt/clang-p2996/bin/clang++` and
`/opt/p2996-toolchain/bin/clang++`.

```bash
git submodule update --init --recursive   # external/typelayout
./build.sh            # detect compiler, build, run CTest, export signatures
./build.sh --debug    # debug build
./build.sh -j 8       # parallel jobs
```

Without a local P2996 compiler, use the pinned image (the `bash` prefix is
required; on Apple Silicon add `--platform linux/amd64`):

```bash
python3 tools/prepare_vm_cmake.py --platform linux-x86_64 --out build/cmake-tool
docker run --rm -v $(pwd):/workspace -w /workspace \
  -e PATH=/workspace/build/cmake-tool/root/bin:/opt/p2996-toolchain/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  ghcr.io/ximicpp/typelayout-p2996@sha256:fab845dadf9c7171d104cd34f8a3941119809e49baa7d0cd7ce1a7a2d71ad999 bash ./build.sh
```

Run one test:

```bash
cd build
./bin/Release/test_basic_types
ctest -R test_basic_types --verbose
```

Standalone OS storage layer (C++17, no reflection, no submodules):

```bash
cmake -S . -B build-storage -DXOFFSET_STORAGE_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-storage --config Release --parallel 2
ctest --test-dir build-storage -C Release --output-on-failure
```

This checks the OS owner and pool only, not full XBuffer support. Fixed
full-library toolchain inputs are recorded in `toolchains/vm-validation.json`;
run `tools/check_vm_toolchain.py` before comparing against historical results.

## CMake

Full-library settings are target-scoped in `cmake/XOffsetBuildChecks.cmake`.
Full targets need CMake 3.30+ (transitive `cxx_std_26`); the storage targets
keep a CMake 3.13 minimum and stay C++17. Do not apply full-library settings
globally.

## Adding a test

- Register it in the right list in `tests/CMakeLists.txt`.
- If it needs no `try`/`catch` of its own, also add it to
  `NO_EXCEPTIONS_TESTS`; it is then built again as `<name>_noexc` with
  exceptions disabled.
- Each test has a `main()` that returns 0 on success and 1 on failure.
- The generated `build/test-inventory.json` is the authoritative test list for
  the selected build mode. Keep the CI inventory and docs current.

## Code conventions

- PascalCase classes, snake_case functions, `mPascalCase` members,
  UPPER_SNAKE_CASE constants.
- Library namespace `XOffsetDatastructure`; TypeLayout APIs live in
  `boost::typelayout`.
- `XOFFSET_REGISTER_TYPE`, `XOFFSET_REGISTER_CONTAINER` and
  `XOFFSET_REGISTER_MAP` must be used at global scope; inside a namespace they
  fail a `static_assert`.
- English only: documents, comments, strings and tool output. No CJK
  characters in tracked text files (they get garbled under other code pages);
  test data that needs such bytes writes them as escapes. `tools/check_no_cjk.py`
  checks this and runs in the `meta` step of `tools/local_validate.sh`.

## Updating TypeLayout

`external/typelayout` supplies the type-signature engine for the signature
tools (`tools/sigs`, `export_signatures`, `check_compat`) and the C++26 cross
tests; the core headers do not include it. After moving the submodule, grep
for renamed TypeLayout names in `tools/`, `tests/` and `docs/`, rebuild, and
regenerate `tools/sigs/*.sig.hpp` through `build.sh`.

## Branches

`main` is the only development branch. Work on a topic branch and open a pull
request into `main`. The old v2.0 line is kept as tag
`archive/main-v2-practical`.
