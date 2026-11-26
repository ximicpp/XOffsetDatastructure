# XOffsetDatastructure

**2.0.0-preview.3** · C++20 / C++26 · header-only · 64-bit little-endian

Preview of 2.0.0: the API and the wire format may still change before the final 2.0.0
([RELEASING.md](docs/RELEASING.md) section 1.1).

## The idea

**The data is the message.** XOffsetDatastructure keeps C++ structs, strings, vectors,
sets and maps in one contiguous arena that uses relative offsets, so the bytes mean the
same thing at any address. You mutate the data in place, send the arena bytes, and the
receiver reads and keeps mutating them. There is no serialize or parse step and no IDL:
the schema is a C++ header that both sides include.

- **Mutate in place, send directly.** Edits go straight into the arena; sending is one
  copy of the bytes, receiving is one copy plus the checks for that link.
- **Checks follow the link.** Server-to-server and server-to-client links are trusted;
  client uplinks are validated by default (`XOFFSET_CHECK_UPLINK`).
- **Add-only schema evolution.** Reserve `XSpare` bytes; a later version that adds
  fields in them talks to the old one with no conversion.
- **Built for game backends:** Linux servers that hand state to each other and push it
  to Windows, iOS and Android clients (MSVC and no-exceptions builds included).

It is not cross-language, it does not compress (put zstd on top), and its checks are
structural, not authentication (use TLS on untrusted networks).

```cpp
struct Unit { std::uint32_t id; std::int32_t hp; XString name; XSpare<8> spare{}; };
XOFFSET_RECORD(Unit, id, hp, name, spare)
struct World { std::uint64_t tick; XVector<Unit> units; XSpare<16> spare{}; };
XOFFSET_RECORD(World, tick, units, spare)
XOFFSET_REGISTER_SCHEMA_NAME(World, "game.World")

auto world = TypedXBuffer<World>::create(4096);
auto u = world.root().units().emplace_back();   // build and change in place
u.id() = 1; u.hp() = 100; u.name() = "Archer";
std::string wire;
world.save_verified_into(wire);                 // the arena bytes are the message
send(wire);
```

Full example and the arena layout: [HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md).

## Performance

The CppCon 2024 comparison: the same 8 libraries, the original `Character` schema
(~3 KB), field-by-field checksum verification
([bench/cppcon2024](bench/cppcon2024/README.md)). Cloud x86-64 Linux (Xeon Skylake
2.19 GHz), GCC 16.2 `-O3`, pinned core, medians over 9 processes; compression not timed.

| Library | Send + receive, reuse / fresh (ns) | Read/write, 2024 / tuned (ns) | Wire, raw / zstd-3 (B) |
| --- | ---: | ---: | ---: |
| XOffset | **121** / **231** | **310** / 123 | 3,160 / 3,023 |
| zpp::bits | 775 / 2,166 | 675\* / 171\* | 2,916 / 2,926 |
| FlatBuffers | 1,311 / 2,518 | 675\* / — | 3,680 / 3,128 |
| cista offset | 1,377 / 1,647 | 597 / 136 | 3,216 / 2,992 |
| cista raw | 1,392 / 1,672 | 541 / **122** | 3,216 / 2,992 |
| Cap'n Proto | 5,202 / 6,007 | 675\* / — | 3,656 / 3,131 |
| MessagePack | 12,335 / 13,374 | 675\* / — | 3,542 / 3,391 |
| Protocol Buffers | 18,840 / 29,132 | 1,100 / 697 | 4,095 / 3,805 |

- **Send + receive:** XOffset is the fastest in every column, 6.4x ahead of the next
  library with buffer reuse and 7.1x with fresh buffers.
- **Read/write** is close: tuned cista raw (122 ns) and XOffset (123 ns) tie.
- **Larger nested data widens the gap.** On the realistic `RichCharacter`
  (~12 KB, depth 4, 373 containers) the steady-state send + receive is 942 ns against
  11,250 ns for the next library (cista offset).

\* Libraries that decode into shared `std` containers use the same read/write model.
Methods, the `RichCharacter` table, the validated tier and limits:
[PERFORMANCE.md](docs/PERFORMANCE.md).

## Quick start (C++20)

Needs CMake 3.20+ and GCC 13+, Clang 18+, Apple clang 21+ or MSVC 19.44+ on a
64-bit little-endian target:

```sh
git clone --depth 1 --branch v2.0.0-preview.3 https://github.com/ximicpp/XOffsetDatastructure.git
cd XOffsetDatastructure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DXOFFSET_CXX_STANDARD=20
cmake --build build --config Release --target helloworld
./build/bin/Release/helloworld        # Windows: build\bin\Release\helloworld.exe
```

In your own CMake project, take the release archive (about 3 MB, no
submodules needed for C++20/C++23):

```cmake
include(FetchContent)
FetchContent_Declare(xoffsetdatastructure
    URL https://github.com/ximicpp/XOffsetDatastructure/archive/refs/tags/v2.0.0-preview.3.tar.gz)
set(XOFFSET_CXX_STANDARD 20 CACHE STRING "" FORCE)
FetchContent_MakeAvailable(xoffsetdatastructure)
target_link_libraries(my_app PRIVATE XOffset::xoffset)
```

Next: [GETTING_STARTED.md](docs/GETTING_STARTED.md) has one runnable example
per use case. C++26 reflection builds, installed packages and engines without
CMake: [BUILD_AND_PLATFORMS.md](docs/BUILD_AND_PLATFORMS.md).

## Documentation

| Read | For |
| --- | --- |
| [GETTING_STARTED.md](docs/GETTING_STARTED.md) | Use cases with one runnable example each |
| [HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md) | Relative offsets, the in-arena allocator, the wire, checks per link |
| [USER_GUIDE.md](docs/USER_GUIDE.md) | The full API reference |
| [DEPLOYMENT.md](docs/DEPLOYMENT.md) | Limits, links and production checklists |
| [SCHEMA_EVOLUTION.md](docs/SCHEMA_EVOLUTION.md) | Different schema versions on the two ends |
| [WIRE_FORMAT.md](docs/WIRE_FORMAT.md), [V2_THREAT_MODEL.md](docs/V2_THREAT_MODEL.md) | Wire format; what the checks cover |
| [PERFORMANCE.md](docs/PERFORMANCE.md) | Performance numbers and methods |
| [CHANGELOG.md](CHANGELOG.md) | Releases |

Other topics (delta sync, schema pins, client targets, device tests, roadmap,
security, contributing) are in [docs/](docs/), [SECURITY.md](SECURITY.md) and
[CONTRIBUTING.md](CONTRIBUTING.md).

## License

See [LICENSE](LICENSE).
