# XOffsetDatastructure

**2.0.0-preview.1** · C++20 / C++26 · header-only · 64-bit little-endian

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

CppCon 2024 `Character` benchmark ([bench/cppcon2024](bench/cppcon2024/README.md);
64-item map, waypoints, attributes, 8 pieces of equipment; same sorted flat map
and rules for every library), cloud x86-64 Linux, GCC 16 `-O3`, medians over
many processes, compression not timed:

| ns per operation | XOffset | zpp::bits\* | cista raw | FlatBuffers | Protocol Buffers |
| --- | ---: | ---: | ---: | ---: | ---: |
| Send + receive, buffer as filled | 83 | 340 | 637 | 614 | 8,789 |
| Read/write loop, 2024 layout | 144 | 343 | 236 | | 592 |
| Read/write loop, tuned layout | 60 | 83 | 66 | | 368 |

\* The read/write rows of zpp::bits are std containers.

Full method, every cell and the limits: [PERFORMANCE.md](docs/PERFORMANCE.md).

## Quick start (C++20)

Needs CMake 3.20+ and GCC 13+, Clang 18+, Apple clang 21+ or MSVC 19.44+ on a
64-bit little-endian target:

```sh
git clone --depth 1 --branch v2.0.0-preview.1 https://github.com/ximicpp/XOffsetDatastructure.git
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
    URL https://github.com/ximicpp/XOffsetDatastructure/archive/refs/tags/v2.0.0-preview.1.tar.gz)
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
