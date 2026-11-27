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

CppCon 2024 benchmark, `Character` (~3 KB, buffer reused), x86-64 Linux, GCC 16.2 `-O3`
([method](bench/cppcon2024/README.md), [all numbers](docs/PERFORMANCE.md)):

| Library | Send + receive (ns) | Read/write (ns) | Wire (B) |
| --- | ---: | ---: | ---: |
| XOffset | **121** | **310** | 3,160 |
| zpp::bits | 775 | 675\* | 2,916 |
| FlatBuffers | 1,311 | 675\* | 3,680 |
| cista offset | 1,377 | 597 | 3,216 |
| cista raw | 1,392 | 541 | 3,216 |
| Cap'n Proto | 5,202 | 675\* | 3,656 |
| MessagePack | 12,335 | 675\* | 3,542 |
| Protocol Buffers | 18,840 | 1,100 | 4,095 |

\* Shared `std` container model (`FlatBuffers` and `Cap'n Proto` do not mutate variable-length
fields in place). `121 ns` uses the trusted server tier (`231 ns` fresh vs `zpp::bits` `2,166 ns`);
with **full-graph validation** (`reload_verified`), XOffset takes **386 ns** reuse (**2.0x**
`zpp::bits`) and **508 ns** fresh (**4.3x**). In-place read/write leads on the 2024 model
(**310 ns** vs `541–1,100 ns`; tuned: XOffset `123 ns`, `cista raw` `122 ns`).

On a deeper, realistic RPG character (`RichCharacter`, ~12 KB, depth 4, 373 containers/strings;
full table in [docs/PERFORMANCE.md](docs/PERFORMANCE.md)):
- **Trusted send + receive**: **942 ns** varied / **1,157 ns** fresh (**9.7x–130x** vs the other
  7 libraries); multi-hop `recv → edit → send` is **1,818 ns** (**7.4x–57x**).
- **Equal-check (every receiver validates)**: **3,201 ns** varied / **3,418 ns** fresh — still
  **4.4x / 10.8x** faster than `zpp::bits`, **11x–20x** faster than `cista DEEP_CHECK` and
  `FlatBuffers + Verifier`, and **34x–44x** faster than `Protocol Buffers`. In-place read/write
  remains #1 (**509 ns** vs `606–836 ns`).

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
