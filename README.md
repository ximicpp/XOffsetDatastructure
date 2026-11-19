# XOffsetDatastructure

**4.1.0** · C++20 / C++26 · header-only · 64-bit little-endian

XOffsetDatastructure keeps C++ objects, strings, vectors, sets and maps in one
contiguous, position-independent arena. The arena bytes are the message: you
mutate the data in place, copy the bytes to another process or machine, and
the receiver reads and keeps mutating them. There is no serialization step and
no IDL.

It is built for game backends: Linux servers that hand state to each other and
push it to Windows, iOS and Android clients, all from one C++ header that both
sides include.

- [Core features](#core-features)
- [Example](#example)
- [How it works](#how-it-works)
- [Performance](#performance)
- [Quick start](#quick-start-c20)
- [Documentation](#documentation)

## Core features

| | What you get |
| --- | --- |
| **No serialize step** | Objects live in the arena and point to each other by relative offsets. Sending is one copy of the arena; receiving is a check plus one copy. Nothing is encoded or decoded per field. |
| **Mutate in place** | Read and write fields, append, insert and erase in the containers directly in the buffer, including variable-length changes. Freed space is reused. |
| **Keep mutating after receive** | A received buffer is a normal working buffer: change it and send it on. The free-space table travels with the message, so a buffer does not degrade after many hops. |
| **Any 64-bit LE device** | The same bytes are valid on Linux x86-64/arm64, Windows x64, macOS, iOS and Android. Pin a type's layout once (`XOFFSET_PIN_SCHEMA`) and every compiler that builds it checks the layout at build time. |
| **Plain C++ types** | A schema is an aggregate struct with library containers (`XString`, `XVector`, `XSet`, `XMap`). C++20 needs one `XOFFSET_RECORD` line per type; with C++26 reflection (GCC 16 or the P2996 Clang) the fields come from the type itself and the line, when present, is checked against them. Both builds write identical bytes. |
| **Delta sync** | Once the peer holds a copy, send only the bytes that changed: per-client deltas for a state a client owns, and shared deltas (one encoding per changed entity, shared by every viewer) for a viewport onto a shared world. |
| **Checks chosen per link** | A receiver is typed by the link it serves. Between your own servers and from server to client the trusted tier checks the header and bounds only; a client uplink can be bounds-checked (read-only view) or fully validated before it is kept or modified. |
| **Clients without exceptions** | Client builds use the platform's own compiler (MSVC, Android NDK Clang, Apple clang) in C++20. The receive and mutate paths a client needs report errors as `XStatus` or `buffer.error()`, so `-fno-exceptions` builds work. |

What it does not do: it is not cross-language, it does not compress (use zstd
or similar on top), and its checks are structural, not authentication. Pair a
link with TLS or mTLS when it crosses an untrusted network.

## Example

One header, shared by the server and every client build:

```cpp
// game_protocol.hpp
#include "xoffsetdatastructure.hpp"
#include <cstdint>
using namespace XOffsetDatastructure;

struct Unit {
    std::uint32_t id;
    std::int32_t x, y, hp;
    XString name;
};
XOFFSET_RECORD(Unit, id, x, y, hp, name)      // global scope, right after the type

struct WorldView {
    std::uint64_t tick;
    XVector<Unit> units;
};
XOFFSET_RECORD(WorldView, tick, units)
XOFFSET_REGISTER_SCHEMA_NAME(WorldView, "game.WorldView")   // one stable name per root type
```

The server builds and changes the state in place, then sends the arena bytes:

```cpp
auto world = TypedXBuffer<WorldView>::create(4096, XBuffer::maximum_capacity(1 << 20));

auto units = world.root().units();              // a proxy: stays valid if the buffer grows
auto u = units.emplace_back();
u.id() = 1;
u.hp() = 100;
u.name() = "Archer Captain";                    // own block; <= 10 bytes would sit in the head

world.edit([](WorldView& v) {                   // hot loops: plain references at plain speed
    v.tick += 1;
    for (Unit& unit : v.units) unit.y += 1;
});

std::string wire;                               // reused for every send
world.save_verified_into(wire);                 // one copy, no per-field encoding
send(wire);
```

The client receives into a buffer it keeps, reads plain members, and can
change and send the data again:

```cpp
#include "xoffset_receiver.hpp"

XReceiver<XLink::from_server> rx{XBuffer::maximum_capacity(1 << 20)};
auto held = TypedXBuffer<WorldView>::create(4096, XBuffer::maximum_capacity(1 << 20));

if (XStatus s = rx.reload(held, wire); !s) {     // no exceptions needed
    ask_for_whole_state();
    return;
}
held.read([](const WorldView& v) {
    for (const Unit& unit : v.units) draw(unit.id, unit.x, unit.y, unit.name.view());
});
held.root().tick() += 1;                        // still a normal, mutable buffer
```

Runnable versions: [`examples/helloworld.cpp`](examples/helloworld.cpp), and
one example per use case in [GETTING_STARTED.md](docs/GETTING_STARTED.md):
server to client (`server_to_client.cpp`), deltas (`delta_sync.cpp`,
`shared_delta.cpp`), client uplink (`client_uplink.cpp`) and server to server
(`server_to_server.cpp`).

## How it works

```
 one arena (payload), any address
 ┌────────┬───────────────────────┬──────────┬────────┬──────────────────┐
 │ header │ root WorldView        │ Unit[]   │ free   │ "Archer Captain" │
 │ 40 B   │ tick │ units{Δ,n,cap} │ Unit ... │ block  │                  │
 └────────┴───────────────┬───────┴─▲────────┴────────┴─▲────────────────┘
                          └─────────┘                   │
                     rel32 Δ from head to block         │
                          Unit.name{Δ,n,cap} ───────────┘

 wire = the used arena bytes [0, used), header included
```

1. **Relative offsets instead of pointers.** Every container head is 12 bytes:
   a signed 32-bit offset from the head to its data block, a size and a
   capacity. Short strings (up to 10 bytes) sit inside the head. The root is
   the first block, at offset 40. Nothing in the payload is
   an absolute address, so the bytes mean the same thing at any address, in
   any process, on any 64-bit little-endian machine.
2. **An allocator inside the arena.** Containers allocate 8-byte aligned blocks
   from the arena itself. A freed block becomes a free block; free blocks of
   one size are linked through their first word, and a directory block in the
   arena holds one list head per size, so the allocator's whole state is in
   the arena bytes. That is what makes in-place, variable-length
   mutation possible: a vector that outgrows its block moves to a new one and
   the old block is reused by the next allocation.
3. **The wire is the arena.** The arena starts with a 40-byte header (format
   version, schema fingerprint, root type, used size, free directory), and
   `save_verified` writes the used part of the arena as it is. Large free runs
   or zero runs can be skipped on the wire. The receiver copies the bytes into
   its own buffer and carries on: one copy on each end, no encode or decode,
   and the free space survives the hop.
4. **The schema is the C++ type.** Each record's field list (written once with
   `XOFFSET_RECORD`, or derived by C++26 reflection) gives a layout signature
   and a 64-bit fingerprint. The wire carries the fingerprint, and the receiver
   rejects a wire whose fingerprint differs. `XOFFSET_PIN_SCHEMA` fixes the
   layout text in the shared header so a client compiler that would lay a type
   out differently fails to compile. Types with raw pointers, references or
   virtual functions are rejected at compile time.
5. **Checks per link.** A trusted link checks the wire header and the hole
   table bounds and does not walk the object graph. A bounds check walks the
   container heads so that every read stays inside the message and gives a
   read-only view. A full check validates the whole graph (no overlap, exact
   coverage, sorted keys, valid values) before the buffer may be modified. No
   tier checks content: a well-formed message with odd field values is
   accepted, so the server still applies its own game rules to client input.
6. **Speed first by default.** Hardening that costs time on mutate and send
   (zeroing removed elements, padding and a reset buffer, delayed reuse inside
   `edit()`, save self-checks) is off by default and turned on together with
   `-DXOFFSET_HARDENED=1`. Without it, bytes of elements a container removed,
   and of what a reused buffer held before `reset()`, can travel in
   `save_verified` output; use `save_compact` or a hardened
   build where a receiver must not see them
   ([threat model 4.1](docs/V2_THREAT_MODEL.md)).

The format is specified in [WIRE_FORMAT.md](docs/WIRE_FORMAT.md) (wire format 3.0).

## Performance

Cloud x86-64 Linux (Xeon, GCC 16 `-O3`, pinned core), medians over many
processes; 0.95–1.05 counts as a tie. Ratios are XOffset ÷ rival: **below 1
means XOffset is faster**. Compression is not timed.

**CppCon 2024 `Character`** (64-item map, waypoints, attributes, 8 pieces of
equipment; same sorted flat map and same rules for every library; 15
processes; [method and all rows](bench/cppcon2024/README.md)):

| ns per operation | XOffset | zpp::bits\* | cista raw | FlatBuffers | Protocol Buffers |
| --- | ---: | ---: | ---: | ---: | ---: |
| Send + receive, buffer as filled | 83 | 340 | 637 | 614 | 8,789 |
| Read/write loop, 2024 layout | 144 | 343 | 236 | | 592 |
| Read/write loop, tuned layout | 60 | 83 | 66 | | 368 |

\* The read/write rows of zpp::bits are std containers.

- Send + receive is XOffset's best case when the buffer was filled once: its
  wire is the buffer. After the read/write loop has churned it, the wire grows
  from 3,064 to 7,928 bytes and send + receive takes 181 ns, 0.57 of
  zpp::bits (trusted tier) or 1.04, a tie (full validation tier).
- Read/write is the fastest of the scored rows on the 2024 layout. On the
  tuned layout it ties cista: 0.90–0.98 in the published processes, 1.00
  [0.78, 1.25] in a rerun of 21 processes on 4.1.0.
  Protocol Buffers' default `RemoveLast` keeps removed messages for reuse and
  then takes 77 ns on the 2024 layout (report only; the scored row frees them
  like every other library).
- Across sizes and churn ([bench/premises](bench/premises/README.md), 68 bytes
  to 729 KB, five rivals): on a buffer as filled the trusted tier wins at every
  size, most at 3–12 KB (0.21–0.24 of the strongest rival); full validation
  ties zpp::bits below about 120 bytes. A churned buffer sends about twice the
  bytes and loses the lead from about 11 KB messages up (1.2–1.4 of zpp::bits
  at 11.5 KB, 1.05–1.1 at 46 KB); `save_compact` brings it back (0.53–0.94).
  [PERFORMANCE.md section 2](docs/PERFORMANCE.md#2-where-the-lead-holds).
- Sizes after zstd-3: XOffset 2,986 bytes, zpp::bits 2,926, Protocol Buffers
  3,800. The wire header makes small messages about 100 bytes larger than
  zpp::bits; this is by design.

**SLG game-server scorecard** (`bench/slg`: login state, per-client and shared
viewport deltas, client orders, server hand-offs; XOffset trusted tier against
the strongest unchecked rival per cell): **19 cells, 16 wins, 3 ties, 0
losses** on 4.1.0. XOffset messages are 1.11–1.46x zpp_bits after zstd-3.

These numbers hold for these workloads, adapters and hosts.
[PERFORMANCE.md](docs/PERFORMANCE.md) has the method, the reading notes of the
2026-11-17 review and the limits.

## Quick start (C++20)

Needs Git, CMake 3.20+ and GCC 12+, Clang 16+, Apple clang 15+ or MSVC 19.38+
on a 64-bit little-endian target:

```sh
git clone https://github.com/ximicpp/xoffsetdatastructure.git
cd xoffsetdatastructure
git -c core.autocrlf=false -c core.eol=lf submodule update --init -- external/boost
git -c core.autocrlf=false -c core.eol=lf -C external/boost submodule update --init -- libs/container libs/core libs/config libs/throw_exception libs/static_assert libs/container_hash libs/type_traits libs/winapi libs/intrusive libs/predef
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DXOFFSET_CXX_STANDARD=20
cmake --build build --config Release --target helloworld
./build/bin/Release/helloworld        # Windows: build\bin\Release\helloworld.exe
```

Without `-DXOFFSET_CXX_STANDARD`, configure picks C++26 when the toolchain has
reflection (CMake 3.30+, GCC 16+ or the P2996 Clang) and otherwise builds
C++20 and says so. Next, [GETTING_STARTED.md](docs/GETTING_STARTED.md) walks
through each use case with a runnable example.

| Build | Compilers |
| --- | --- |
| Servers, C++26 reflection | GCC 16+ or the P2996 Clang ([toolchains](toolchains/README.md)); CMake 3.30+ |
| Clients and any C++20/C++23 build | Platform compilers: MSVC 19.44 (`/Zc:preprocessor`), clang-cl, MinGW GCC, Android NDK Clang, Apple clang for iOS 17+ / macOS 14+; CMake 3.20+ |
| OS storage layer only | Any C++17 compiler; CMake 3.13+ |

Installing, package consumption, the validated platform matrix and the
storage and ownership rules are in
[BUILD_AND_PLATFORMS.md](docs/BUILD_AND_PLATFORMS.md).

## Documentation

| Read | For |
| --- | --- |
| [GETTING_STARTED.md](docs/GETTING_STARTED.md) | Use cases with one runnable example each |
| [USER_GUIDE.md](docs/USER_GUIDE.md) | The API: types, mutation, save/load, storage, errors, core guarantees K0–K9 |
| [DEPLOYMENT.md](docs/DEPLOYMENT.md) | Limits, links and checklists for production |
| [DELTA_SYNC.md](docs/DELTA_SYNC.md), [SHARED_DELTA.md](docs/SHARED_DELTA.md) | Per-client deltas and shared viewport deltas |
| [SCHEMA_EVOLUTION.md](docs/SCHEMA_EVOLUTION.md), [SCHEMA_PINS.md](docs/SCHEMA_PINS.md) | Different schema versions on the two ends; pinned layouts |
| [WIRE_FORMAT.md](docs/WIRE_FORMAT.md), [V2_THREAT_MODEL.md](docs/V2_THREAT_MODEL.md) | Wire format; what the checks do and do not cover |
| [ALPHA_COMPATIBILITY.md](docs/ALPHA_COMPATIBILITY.md) | Compatibility and input contract |
| [CLIENT_TARGETS.md](docs/CLIENT_TARGETS.md), [DEVICE_TESTING.md](docs/DEVICE_TESTING.md) | Client toolchains and real-device records |
| [PERFORMANCE.md](docs/PERFORMANCE.md), [bench/cppcon2024](bench/cppcon2024/README.md) | Performance numbers and methods |
| [CHANGELOG.md](CHANGELOG.md), [RELEASING.md](docs/RELEASING.md) | Releases |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Building, testing and conventions for library development |

## License

See [LICENSE](LICENSE).
