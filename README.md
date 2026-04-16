# XOffsetDatastructure

[![CI](https://github.com/ximicpp/XOffsetDatastructure/actions/workflows/ci.yml/badge.svg)](https://github.com/ximicpp/XOffsetDatastructure/actions/workflows/ci.yml)

XOffsetDatastructure is a C++26/P2996 serialization library built around one idea: keep data in a byte-stable in-memory layout so save/load becomes direct byte transfer instead of encode/decode work.

The library stores a single reflected root object inside a relocatable buffer backed by Boost.Interprocess and uses TypeLayout for compile-time layout signatures and byte-copy safety checks.

## Requirements

- Clang with P2996 reflection support and `<experimental/meta>`
- 64-bit little-endian platform
- Git submodules initialized for `external/typelayout` and `external/boost`

Reflection is required. There is no non-reflection fallback mode in this repository.

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

int main() {
    XBuffer xbuf(4096);

    auto* player = xbuf.make<Player>();
    player->id = 1;
    player->level = 10;
    player->name = "Alice";
    player->items.push_back(101);

    std::string bytes = xbuf.save();

    XBuffer loaded = XBuffer::load(bytes);
    auto& restored = loaded.root<Player>();
    return restored.level == 10 ? 0 : 1;
}
```

More complete examples live in:

- `examples/helloworld.cpp`
- `examples/demo.cpp`

## Safety Rules

- Only store types that satisfy `boost::typelayout::is_byte_copy_safe_v<T>`.
- Pointers obtained from the buffer are invalid after `grow()`, `shrink_to_fit()`, or `XCompactor::compact<T>()`.
- There is no per-object delete. The buffer owns all contained objects.
- Writes are not thread-safe. Synchronize externally.
- Prefer `XHandle<T>` if you need a stable reference across buffer relocations.

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

`build.sh` performs configure, build, `ctest`, signature export, and compatibility self-check.

Manual CMake path:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/path/to/clang++
cmake --build build -j
ctest --test-dir build --output-on-failure -C Release
build/bin/Release/export_signatures tools/sigs
build/bin/Release/check_compat
```

If your generator emits single-config binaries into `build/bin/`, use that path instead of `build/bin/Release/`.

## Docker / CI Image

CI uses the prebuilt image `ghcr.io/ximicpp/typelayout-p2996:latest`. You can run the same flow locally:

```bash
docker run --rm \
  -v "$PWD":/workspace \
  -w /workspace \
  ghcr.io/ximicpp/typelayout-p2996:latest \
  bash ./build.sh
```

## Signature Tools

- `tools/export_signatures.cpp` exports the current platform signature into `tools/sigs/`
- `tools/check_compat.cpp` auto-discovers every `.sig.hpp` under `tools/sigs/` and fails if any exported type stops matching across those baselines

Each `.sig.hpp` file in `tools/sigs/` is a platform baseline. The committed headers use a stable generated line, so rerunning the exporter only diffs on real contract drift. Adding a new local platform baseline is just exporting the file, reviewing it, and committing it; CI then treats any baseline drift as a failure.

## Dependency Surface

The vendored Boost checkout is still the full superproject, but the active build only exposes the current header subset needed by XOffsetDatastructure: `container`, `interprocess`, `intrusive`, `move`, `assert`, `config`, `core`, `static_assert`, `throw_exception`, `type_traits`, `container_hash`, `predef`, and `winapi`.

## Repository Layout

- `xoffsetdatastructure.hpp`: public library header
- `tests/`: unit and behavior tests
- `examples/`: demo programs
- `tools/`: signature export and compatibility tools
- `tools/sigs/`: committed signature baselines
- `external/typelayout/`: type-signature engine
- `external/boost/`: vendored Boost dependency tree
