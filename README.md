# XOffsetDatastructure

**2.0.0-preview.4** · C++20 / C++26 · header-only · 64-bit little-endian
(preview: API and wire format may change before 2.0.0)

C++ structs, strings, vectors and maps live in one buffer that uses relative offsets.
**The buffer is the message:** change the data in place, send the bytes, and the receiver
uses and changes them directly. No serialization, no IDL. Built for game servers that send
state to each other and to Windows, iOS and Android clients.

```cpp
struct Unit { std::uint32_t id; std::int32_t hp; XString name; XSpare<8> spare{}; };
XOFFSET_RECORD(Unit, id, hp, name, spare)
struct World { std::uint64_t tick; XVector<Unit> units; XSpare<16> spare{}; };
XOFFSET_RECORD(World, tick, units, spare)
XOFFSET_REGISTER_SCHEMA_NAME(World, "game.World")

auto world = TypedXBuffer<World>::create(4096);
auto u = world.root().units().emplace_back();   // change in place
u.id() = 1; u.hp() = 100; u.name() = "Archer";
std::string wire;
world.save_verified_into(wire);                 // the bytes are the message
```

## Performance

CppCon 2024 benchmark, `Character` (~3 KB), x86-64 Linux, GCC 16.2 `-O3`
([method](bench/cppcon2024/README.md), [all numbers](docs/PERFORMANCE.md)):

| Library | Send + receive (ns) | Read/write (ns) | Wire (B) |
| --- | ---: | ---: | ---: |
| XOffset | **121** | 310 | 3,160 |
| zpp::bits | 775 | 675 | 2,916 |
| FlatBuffers | 1,311 | 675 | 3,680 |
| cista offset | 1,377 | 597 | 3,216 |
| cista raw | 1,392 | 541 | 3,216 |
| Cap'n Proto | 5,202 | 675 | 3,656 |
| MessagePack | 12,335 | 675 | 3,542 |
| Protocol Buffers | 18,840 | 1,100 | 4,095 |

XOffset leads send + receive against every library (6.4x the next one); read/write is
close to the fastest (tuned code: XOffset 123 ns, cista raw 122 ns).

## Use it

```cmake
include(FetchContent)
FetchContent_Declare(xoffsetdatastructure
    URL https://github.com/ximicpp/XOffsetDatastructure/archive/refs/tags/v2.0.0-preview.4.tar.gz)
FetchContent_MakeAvailable(xoffsetdatastructure)
target_link_libraries(my_app PRIVATE XOffset::xoffset)
```

Next: [GETTING_STARTED.md](docs/GETTING_STARTED.md) ·
[HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md) · [USER_GUIDE.md](docs/USER_GUIDE.md) ·
[BUILD_AND_PLATFORMS.md](docs/BUILD_AND_PLATFORMS.md) · [CHANGELOG.md](CHANGELOG.md)

License: [LICENSE](LICENSE).
