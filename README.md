# XOffsetDatastructure

**2.0.0-preview.2** · C++20 / C++26 · header-only · 64-bit little-endian

This is a preview of 2.0.0: the API and the wire format may still change before the final 2.0.0
([RELEASING.md](docs/RELEASING.md) section 1.1).

XOffsetDatastructure keeps C++ objects, strings, vectors, sets and maps in one
contiguous, position-independent arena. The arena bytes are the message: you
mutate the data in place, copy the bytes to another process or machine, and
the receiver reads and keeps mutating them. There is no serialization step and
no IDL.

It is built for game backends: Linux servers that hand state to each other and
push it to Windows, iOS and Android clients, all from one C++ header that both
sides include.

It is not cross-language, it does not compress (put zstd or similar on top),
and its checks are structural, not authentication (use TLS across untrusted
networks). A short example and the arena layout:
[HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md).

## Performance

All 8 libraries from the CppCon 2024 comparison under the same sorted flat map,
field-by-field checksum verification and rules ([bench/cppcon2024](bench/cppcon2024/README.md));
cloud x86-64 Linux (Intel Xeon Skylake @ 2.19 GHz, GCC 16.2 `-O3`, pinned to CPU 2,
medians over 9 processes, compression not timed).

### Realistic nested RPG character (`RichCharacter`, ~12 KB, depth 4, 373 containers/strings > SSO)

16 distinct player shapes cycled through a steady-state worker (`Varied RPC`), new
buffers and live session objects per player (`Fresh session`), cross-server handoff
(`Receive -> Gameplay Mutate -> Send`, 16 varied players), strict equal-check
validation where every receiver validates the full graph, and in-place gameplay edits:

| Library | Varied RPC (ns) | Fresh session (ns) | Hop: recv→edit→send (ns) | Equal-check varied / fresh (ns) | In-place edit (ns) | Wire, raw / zstd-3 (B) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| XOffset | **942** (`1,027`†) | **1,157** (`1,328`†) | **1,818**† (`2,227`‡) | **3,201**† / **3,418**† (`4,668` / `4,715`) | **509** | 16,256 / 10,850 |
| cista offset | 11,250 | 11,223 | 13,451 | 46,404 / 44,202 | 836 | 18,450 / 11,218 |
| cista raw | 12,045 | 11,610 | 13,790 | 41,808 / 38,627 | 721 | 18,450 / 11,190 |
| zpp::bits | 14,143 | 36,971 | 15,429 | 14,143 / 36,971 | 657\* | 11,860 / 9,834 |
| Cap'n Proto | 33,191 | 62,310 | 35,113 | 33,191 / 62,310 | 657\* | 16,384 / 10,777 |
| FlatBuffers | 36,135 | 57,780 | 37,633 | 45,277 / 66,847 | 657\* | 16,416 / 11,544 |
| MessagePack | 55,891 | 76,421 | 60,636 | 55,891 / 76,421 | 657\* | 12,515 / 11,036 |
| Protocol Buffers | 108,441 | 150,721 | 103,785 | 108,441 / 150,721 | 606 | 13,889 / 12,247 |

- Trusted tier (`Varied RPC`, `Fresh session`, `Hop`): XOffset uses
  `XReceiver<XLink::server_peer>`, cista uses `UNCHECKED`/`CAST`, FlatBuffers omits
  its Verifier; `zpp::bits`, Cap'n Proto, MessagePack and Protocol Buffers always
  check while decoding.
- Equal-check tier (`Equal-check varied / fresh`): every receiver validates
  (XOffset `reload_verified` / `load_verified`, cista `DEEP_CHECK`, FlatBuffers
  `Verifier`). XOffset leads `zpp::bits` by **4.4x** (varied) and **10.8x** (fresh),
  `cista` by **11–14x**, and `FlatBuffers` by **14–20x**. Equal-check `Hop`:
  XOffset **3,923 ns**† vs `zpp::bits` 15,429 ns (3.9x) and `FlatBuffers` 46,594 ns (11.9x).
- Read-only zero-copy `view` (send + view without unpack): XOffset **249 ns**
  (trusted) / **1,591 ns** (bounds-checked) vs Cap'n Proto 10,178 ns, cista
  10,017–10,143 ns / 38,280–43,395 ns, FlatBuffers 22,654 ns / 33,640 ns.
- † XOffset steady-state compact wire (`save_compact()` / `relayout()`, default
  `slack_div = 8`, which keeps spare slots so the next hop mutates without reallocating
  the item map; raw/zstd-3: 17,224 / 10,969 B). ‡ Organic unreserved `push_back`
  buffer (no manual `reserve()`). Exact `reserve(n)` with zero headroom is fastest for
  forwarding (`942` / `1,157` ns) and takes `6,325` ns on a mutating hop when `items`
  doubles and triggers `XWireEmission::automatic` compaction.
- \* Shared `std` container model (FlatBuffers and Cap'n Proto cannot grow or shrink
  in place, so a mutating receiver must unpack and rebuild).

### Classic CppCon 2024 `Character` (~3 KB flat POD-heavy baseline)

Same host and 9 processes, original 2024 schema (64-item POD map, waypoints,
attributes, 8 equips; 14 containers total, ~80% flat POD vectors, depth 2):

| Library | Send + receive, reuse / fresh (ns) | Read/write, 2024 / tuned (ns) | Wire, raw / zstd-3 (B) |
| --- | ---: | ---: | ---: |
| XOffset | **121** / **231** | **310** / **123** | 3,160 / 3,023 |
| zpp::bits | 775 / 2,166 | 675\* / 171\* | 2,916 / 2,926 |
| FlatBuffers | 1,311 / 2,518 | 675\* / — | 3,680 / 3,128 |
| cista offset | 1,377 / 1,647 | 597 / 136 | 3,216 / 2,992 |
| cista raw | 1,392 / 1,672 | 541 / 122 | 3,216 / 2,992 |
| Cap'n Proto | 5,202 / 6,007 | 675\* / — | 3,656 / 3,131 |
| MessagePack | 12,335 / 13,374 | 675\* / — | 3,542 / 3,391 |
| Protocol Buffers | 18,840 / 29,132 | 1,100 / 697 | 4,095 / 3,805 |

XOffset full validation on the 2024 shape: 386 ns reuse / 508 ns fresh. An L1d-resident
iso-byte control (`Flat-3KB` -> `Rich-3KB`, 107 nested containers) confirms the widening
lead is structural rather than cache-driven (`zpp::bits` moves from 5.8x to 12.0x
same-shape, 16.9x varied, 39.3x fresh; `FlatBuffers` from 11.3x to 45.7x–57.0x).

Full method, iso-byte breakdown, SLG scorecard and limits:
[PERFORMANCE.md](docs/PERFORMANCE.md).

## Quick start (C++20)

Needs CMake 3.20+ and GCC 13+, Clang 18+, Apple clang 21+ or MSVC 19.44+ on a
64-bit little-endian target:

```sh
git clone --depth 1 --branch v2.0.0-preview.2 https://github.com/ximicpp/XOffsetDatastructure.git
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
    URL https://github.com/ximicpp/XOffsetDatastructure/archive/refs/tags/v2.0.0-preview.2.tar.gz)
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
| [HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md) | A short example; relative offsets, the in-arena allocator, the wire, checks per link |
| [USER_GUIDE.md](docs/USER_GUIDE.md) | The API: types, mutation, save/load, storage, threads, errors, core guarantees K0–K9 |
| [DEPLOYMENT.md](docs/DEPLOYMENT.md) | Limits, links and checklists for production |
| [DELTA_SYNC.md](docs/DELTA_SYNC.md), [SHARED_DELTA.md](docs/SHARED_DELTA.md) | Per-client deltas and shared viewport deltas |
| [SCHEMA_EVOLUTION.md](docs/SCHEMA_EVOLUTION.md), [SCHEMA_PINS.md](docs/SCHEMA_PINS.md) | Different schema versions on the two ends; pinned layouts |
| [WIRE_FORMAT.md](docs/WIRE_FORMAT.md), [V2_THREAT_MODEL.md](docs/V2_THREAT_MODEL.md) | Wire format; what the checks do and do not cover |
| [ALPHA_COMPATIBILITY.md](docs/ALPHA_COMPATIBILITY.md) | Compatibility and input contract |
| [CLIENT_TARGETS.md](docs/CLIENT_TARGETS.md), [DEVICE_TESTING.md](docs/DEVICE_TESTING.md) | Client toolchains and real-device records |
| [PERFORMANCE.md](docs/PERFORMANCE.md), [bench/cppcon2024](bench/cppcon2024/README.md) | Performance numbers and methods |
| [CHANGELOG.md](CHANGELOG.md), [RELEASING.md](docs/RELEASING.md) | Releases |
| [ROADMAP.md](docs/ROADMAP.md) | Decided future work that is not scheduled (big-endian hosts) |
| [SECURITY.md](SECURITY.md) | Vulnerability reporting and security scope |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Building, testing and conventions for library development |

## License

See [LICENSE](LICENSE).
