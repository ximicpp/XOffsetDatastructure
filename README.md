# XOffsetDatastructure

**2.0.0-preview.5** · C++20 / C++26 · header-only · 64-bit little-endian
(preview: API and wire format may change before 2.0.0)

C++ structs, strings, vectors and maps live in one buffer that uses relative offsets.
**The buffer is the message:** change the data in place, send the bytes, and the receiver
uses and changes them directly. No serialization, no IDL. Built for game servers that send
state to each other and to Windows, iOS and Android clients.

```cpp
struct Unit { std::uint32_t id; std::int32_t hp; XString name; XSpare<8> spare{}; }; // spare: optional future fields
XOFFSET_RECORD(Unit, id, hp, name, spare)
struct World { std::uint64_t tick; XVector<Unit> units; XSpare<16> spare{}; };
XOFFSET_RECORD(World, tick, units, spare)
XOFFSET_REGISTER_SCHEMA_NAME(World, "game.World")

auto world = TypedXBuffer<World>::create(4096); // 4096 B to start, not a limit: grows (default cap 64 MiB)
auto u = world.root().units().emplace_back();   // change in place
u.id() = 1; u.hp() = 100; u.name() = "Archer";
std::string wire;
world.save_verified_into(wire);                 // the bytes are the message

auto recv = TypedXBuffer<World>::load(wire);    // receiver validates and uses directly
recv.root().units()[0].hp() = 90;               // still mutable in place, ready to send again
recv.root().units()[0].name() = "Elite Archer";
```

## Performance

Realistic RPG character (`RichCharacter`, ~12 KB, depth 4, 373 containers/strings),
x86-64 Linux, GCC 16.2 `-O3`, 9 processes ([method](bench/cppcon2024/README.md),
[all numbers](docs/PERFORMANCE.md)):

| Library | Send + recv, trusted (ns) | Hop: recv→edit→send (ns) | Equal-check: varied / fresh (ns) | In-place R/W (ns) |
| --- | ---: | ---: | ---: | ---: |
| **XOffset** | **1,027** | **1,818** | **3,201** / **3,418** | **509** |
| cista offset | 11,250 *(11x)* | 13,451 *(7.4x)* | 46,404 / 44,202 *(14.5x / 13x)* | 836 |
| cista raw | 12,045 *(12x)* | 13,790 *(7.6x)* | 41,808 / 38,627 *(13x / 11x)* | 721 |
| zpp::bits | 14,143 *(14x)* | 15,429 *(8.5x)* | 14,143 / 36,971 *(4.4x / 11x)* | 657\* |
| Cap'n Proto | 33,191 *(32x)* | 35,113 *(19x)* | 33,191 / 62,310 *(10x / 18x)* | 657\* |
| FlatBuffers | 36,135 *(35x)* | 37,633 *(21x)* | 45,277 / 66,847 *(14x / 20x)* | 657\* |
| MessagePack | 55,891 *(54x)* | 60,636 *(33x)* | 55,891 / 76,421 *(17x / 22x)* | 657\* |
| Protocol Buffers | 108,441 *(106x)* | 103,785 *(57x)* | 108,441 / 150,721 *(34x / 44x)* | 606 |

Single default XOffset layout (`save_compact()` / `slack_div = 8`; exact-reserved buffers reach
**942 ns** varied / **1,157 ns** fresh, up to **130x**). \* Shared `std` container model
(`FlatBuffers` and `Cap'n Proto` unpack to mutate variable-length fields).

**Why the gap grows (`O(bytes)` vs `O(objects)`):** holding payload size fixed inside L1d
(`Flat-3KB → Rich-3KB`), XOffset scales only with bytes (`124 → 165 ns`), while `zpp::bits`
widens to **12x–39x** and `FlatBuffers` to **46x–57x**.

**Across other workloads** ([docs/PERFORMANCE.md](docs/PERFORMANCE.md)):
- **Classic CppCon 2024 flat `Character` (~3 KB)**: **121 ns** vs `775–18,840 ns` (`6.4x–156x`);
  full check **386 ns** (`2.0x` `zpp::bits`) / **508 ns** fresh (`4.3x`); read/write **310 ns**
  (tuned `123 ns`, `cista raw` `122 ns`).
- **Read-only zero-copy `view` (`Rich-12KB`)**: **249 ns** trusted / **1,591 ns** bounds-checked
  vs `10,017–43,395 ns` (`Cap'n Proto`, `cista`, `FlatBuffers`).
- **SLG server scorecard & memory**: **17 wins, 2 ties, 0 losses** across 19 cells (deltas,
  shared viewports, uplink); lowest single-process RSS (**538 MiB**, `0.92x` Protobuf).
- **Size, churn & tradeoffs**: trusted wins from **68 B to 729 KB** and after **100k random
  edits**; only full validation below `78 B` is narrowly slower (`1.07x` at `68 B`), and
  uncompressed wire is `1.1–1.45x` `zpp::bits` (`1.00–1.12x` after zstd-3).

## Use it

```cmake
include(FetchContent)
FetchContent_Declare(xoffsetdatastructure
    URL https://github.com/ximicpp/XOffsetDatastructure/archive/refs/tags/v2.0.0-preview.5.tar.gz)
set(XOFFSET_CXX_STANDARD 20 CACHE STRING "" FORCE) # zero external deps: GCC 13+, Clang 18+, Apple Clang 21+, MSVC 19.44+
FetchContent_MakeAvailable(xoffsetdatastructure)
target_link_libraries(my_app PRIVATE XOffset::xoffset)
```

C++20 / C++23 uses only the C++ standard library (`XOFFSET_RECORD` per struct). For C++26
reflection (GCC 16+ or P2996 Clang, no `XOFFSET_RECORD` required), use the release archive
([xoffsetdatastructure-2.0.0-preview.5.tar.gz](https://github.com/ximicpp/XOffsetDatastructure/releases/download/v2.0.0-preview.5/xoffsetdatastructure-2.0.0-preview.5.tar.gz),
which bundles pinned dependencies) with `XOFFSET_CXX_STANDARD=26`.

Next: [GETTING_STARTED.md](docs/GETTING_STARTED.md) ·
[HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md) · [USER_GUIDE.md](docs/USER_GUIDE.md) ·
[BUILD_AND_PLATFORMS.md](docs/BUILD_AND_PLATFORMS.md) · [DEPLOYMENT.md](docs/DEPLOYMENT.md) ·
[PERFORMANCE.md](docs/PERFORMANCE.md) · [CHANGELOG.md](CHANGELOG.md)

License: [LICENSE](LICENSE).
