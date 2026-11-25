# XOffsetDatastructure

**2.0.0** · C++20 / C++26 · header-only · 64-bit little-endian

XOffsetDatastructure keeps C++ objects, strings, vectors, sets and maps in one
contiguous, position-independent arena. The arena bytes are the message: you
mutate the data in place, copy the bytes to another process or machine, and
the receiver reads and keeps mutating them. There is no serialization step and
no IDL.

It is built for game backends: Linux servers that hand state to each other and
push it to Windows, iOS and Android clients, all from one C++ header that both
sides include.

## Core features

| | What you get |
| --- | --- |
| **No serialize step** | Objects live in the arena and point to each other by relative offsets. Sending is one copy of the arena; receiving is a check plus one copy. Nothing is encoded or decoded per field. |
| **Mutate in place** | Read and write fields, append, insert and erase directly in the buffer, including variable-length changes; freed space is reused. A received buffer is a normal working buffer: change it and send it on. |
| **Send the bytes to any 64-bit LE device** | The same bytes are valid on Linux x86-64/arm64, Windows x64, macOS, iOS and Android. C++26 reflection builds (servers) and C++20 builds with the platform compiler (clients: one `XOFFSET_RECORD` line per type, `-fno-exceptions` works) write identical bytes, and `XOFFSET_PIN_SCHEMA` makes a compiler that would lay a type out differently fail to build. |
| **Delta sync** | Once the peer holds a copy, send only the bytes that changed: per-client deltas, and shared deltas (one encoding per changed entity) for a viewport onto a shared world. |
| **Add-only schema evolution** | Reserve `XSpare<N>` bytes in a type's first release; a later version that only adds fields in them talks to the old one in both directions with no conversion. The schema handshake refuses any other mismatch. |
| **Checks chosen per link** | Between your own servers and from server to client, the trusted tier checks the header and bounds only; a client uplink is bounds-checked or fully validated before it is kept. |

It is not cross-language, it does not compress (put zstd or similar on top),
and its checks are structural, not authentication (use TLS across untrusted
networks). A short example and the arena layout:
[HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md).

## Performance

XOffset time ÷ rival time on cloud x86-64 Linux (Xeon, GCC 16 `-O3`, pinned
core, medians over many processes): **below 1 means XOffset is faster**, and
0.95–1.05 is a tie. Compression is not timed.

| Workload | XOffset against the rivals |
| --- | --- |
| **SLG game server** (`bench/slg`): login state, per-client and shared viewport deltas, client orders, server hand-offs; trusted tier against the strongest unchecked rival per cell (zpp::bits, cista) | **19 cells: 17 wins, 2 ties, 0 losses** |
| **Message size**, 68 bytes to 729 KB ([bench/premises](bench/premises/README.md)), five rivals | Trusted tier wins at every size, most at 1–12 KB (0.16–0.21 of the strongest rival); full validation loses narrowly only below about 80 bytes (1.07 at 68 bytes) |
| **Churned sender buffer** (edits, containers grown and shrunk back) | Goes out compact by default: from 3 KB up 0.32–0.60 of zpp::bits's time trusted, 0.51–0.76 fully validated |
| **Bytes after zstd-3** (report only) | 1.00–1.03x zpp::bits from 2.9 KB up, 1.45x at 68 bytes (the 40-byte header weighs most there); SLG messages 1.11–1.37x |

CppCon 2024 `Character` (64-item map, waypoints, attributes, 8 pieces of
equipment; same sorted flat map and rules for every library):

| ns per operation | XOffset | zpp::bits\* | cista raw | FlatBuffers | Protocol Buffers |
| --- | ---: | ---: | ---: | ---: | ---: |
| Send + receive, buffer as filled | 83 | 340 | 637 | 614 | 8,789 |
| Read/write loop, 2024 layout | 144 | 343 | 236 | | 592 |
| Read/write loop, tuned layout | 60 | 83 | 66 | | 368 |

\* The read/write rows of zpp::bits are std containers.

Measured on 4.3.0; 2.0.0 runs the same machine code. The numbers hold for
these workloads, adapters and hosts: method, every cell and the limits are in
[PERFORMANCE.md](docs/PERFORMANCE.md).

## Quick start (C++20)

Needs CMake 3.20+ and GCC 13+, Clang 18+, Apple clang 21+ or MSVC 19.44+ on a
64-bit little-endian target:

```sh
git clone --depth 1 --branch v2.0.0 https://github.com/ximicpp/XOffsetDatastructure.git
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
    URL https://github.com/ximicpp/XOffsetDatastructure/archive/refs/tags/v2.0.0.tar.gz)
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
