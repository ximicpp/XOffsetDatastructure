# XOffsetDatastructure

**4.0.0** · C++20 / C++26 · header-only · 64-bit little-endian

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
 │ header │ root WorldView        │ Unit[]   │ hole   │ "Archer Captain" │
 │ 48 B   │ tick │ units{Δ,n,cap} │ Unit ... │ (free) │                  │
 └────────┴───────────────┬───────┴─▲────────┴────────┴─▲────────────────┘
                          └─────────┘                   │
                     rel32 Δ from head to block         │
                          Unit.name{Δ,n,cap} ───────────┘

 wire = 40 B header + used arena bytes + hole table
```

1. **Relative offsets instead of pointers.** Every container head is 12 bytes:
   a signed 32-bit offset from the head to its data block, a size and a
   capacity. Short strings (up to 10 bytes) sit inside the head. The root is
   found by its offset from the start of the arena. Nothing in the payload is
   an absolute address, so the bytes mean the same thing at any address, in
   any process, on any 64-bit little-endian machine.
2. **An allocator inside the arena.** Containers allocate 8-byte aligned blocks
   from the arena itself. A freed block becomes a hole; the buffer keeps the
   holes in a free table (exact-size bins for small blocks, best fit for large
   ones) and reuses them. That is what makes in-place, variable-length
   mutation possible: a vector that outgrows its block moves to a new one and
   the old block is reused by the next allocation.
3. **The wire is the arena.** `save_verified` writes a 40-byte wire header
   (format version, used size, root offset, schema fingerprint, root type),
   the used part of the arena, and the hole table. Large free runs or zero
   runs can be skipped on the wire. The receiver copies the bytes into its own
   buffer, loads the hole table back into its free table and carries on: one
   copy on each end, no encode or decode, and the free space survives the hop.
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
   (zeroing removed elements and padding, delayed reuse inside `edit()`, save
   self-checks) is off by default and turned on together with
   `-DXOFFSET_HARDENED=1`. Without it, bytes of elements a container removed
   can travel in `save_verified` output; use `save_compact` or a hardened
   build where a receiver must not see them
   ([threat model 4.1](docs/V2_THREAT_MODEL.md)).

The format is specified in [V2_FORMAT.md](docs/V2_FORMAT.md) (wire format 2.1).

## Performance

Two comparisons, both on a cloud x86-64 Linux host (Xeon, GCC 16 `-O3`,
pinned core). The host is noisy, so each figure is a median over many
processes and ratios between 0.95 and 1.05 count as a tie. Ratios are
XOffset ÷ rival: **below 1 means XOffset is faster**. Compression is never
timed; sizes are reported after zstd level 3.

### CppCon 2024 `Character`

The game character from the CppCon 2024 talk (64-item map, waypoints,
attributes, 8 pieces of equipment with attributes each), rerun with 4.0.0 and
current rules: random numbers are drawn before timing, every library's map is
the same sorted flat map, receiving means "the receiver can read and modify
the state", every library really frees what the loop removes, and each
iteration uses a new map key. Median of 15 processes, library at e36cf44
([method and all rows](bench/cppcon2024/README.md)).

**Send + receive** (one message, buffers reused, XOffset trusted tier):

| Library | ns / message | XOffset ÷ lib |
| --- | ---: | ---: |
| XOffset | 83 | 1.00 |
| zpp::bits | 340 | 0.24 |
| FlatBuffers (build + read back) | 614 | 0.14 |
| cista (raw / offset, unchecked) | 637 / 639 | 0.13 |
| Cap'n Proto | 2,268 | 0.037 |
| MessagePack | 6,112 | 0.014 |
| Protocol Buffers | 8,789 | 0.009 |

With the full validation tier instead, XOffset takes 171 ns, still 0.50 of
zpp::bits.

These rows send a buffer that was filled once and never modified, which is
XOffset's best case: its wire is the buffer, so the spare capacity and the
blocks that modifications have freed travel with it, while a rival's wire
depends only on the state. Sending the buffer that the read/write loop below
has just run on (same state size, heavy churn; a separate run of 15 processes
with zpp::bits at 319 ns, [details](bench/cppcon2024/README.md#send--receive-of-a-buffer-that-has-been-modified-2026-11-17)):

| XOffset buffer | Wire bytes | ns / message | ÷ zpp::bits | ÷ cista raw |
| --- | ---: | ---: | ---: | ---: |
| As filled, trusted tier | 3,064 | 78 | 0.25 | 0.12 |
| After the read/write loop, trusted tier | 7,928 | 181 | 0.57 | 0.27 |
| As filled, full validation tier | 3,064 | 191 | 0.60 | 0.28 |
| After the read/write loop, full validation tier | 7,928 | 331 | 1.04 | 0.49 |

So on the trusted tier a heavily modified buffer is still sent and received
in a bit over half of zpp::bits's time (faster in all 15 processes), not a
quarter, and on the full validation tier it ties zpp::bits.

**Read/write loop** (one iteration: change scalars and position, add and
remove equipment, waypoints, an attribute and a map item):

| Layout | XOffset | std containers | cista raw / offset | Protocol Buffers |
| --- | ---: | ---: | ---: | ---: |
| 2024 layout, ns | 144 | 343 | 236 / 259 | 592 |
| XOffset ÷ lib | | 0.42 | 0.61 / 0.55 | 0.24 |
| Tuned layout\*, ns | 60 | 83 | 66 / 67 | 368 |
| XOffset ÷ lib | | 0.72 | 0.91 / 0.90 | 0.16 |

\* Small attribute arrays stored inline, range append, erase at a known
position; applied to every library that can express it.

In-place read/write is the fastest of these libraries in both layouts, in
every one of the 15 processes. The margin over cista is small on the tuned
layout (0.90–0.91); the separate run above measured 0.98 against cista raw
(faster in 9 of 15 processes) and 0.89 against cista offset, so read it as a
tie to a small win. Protocol Buffers' default `RemoveLast` keeps the removed
messages for the next `add_`, so its loop allocates nothing (77 ns on the
2024 layout, report-only, about half of XOffset's 144 ns); the scored row
frees them like every other library. **One read/write iteration plus one
send and receive** costs XOffset 144 ns (tuned), against 390 ns for zpp::bits
on std containers (0.37), 661–682 ns for cista (0.21–0.22) and 9,157 ns for
Protocol Buffers (0.016). That sum uses the as-filled buffer. With the buffer
the loop has modified it is 250 ns in the separate run: 0.63 of zpp::bits,
0.36 of cista and 0.027 of Protocol Buffers (2024 layout: 331 ns, 0.51, 0.36
to 0.37 and 0.036).

Size: the `Character` wire is 3,064 bytes raw and 2,986 bytes after zstd-3
(zpp::bits 2,926, Protocol Buffers 3,800). The wire header makes small
XOffset messages about 100 bytes larger than zpp::bits; this is accepted by
design and reported, not optimized away. That is the as-filled buffer too:
after the read/write loop the same state is 7,928 bytes on the wire (3,173
after zstd-3), and `save_compact`, which leaves out spare capacity and free
blocks, writes it in 3,552.

### SLG game-server scorecard

`bench/slg` models a strategy game server end to end: whole state on login,
per-client deltas, a shared map viewport, client orders going up, and players
moving between servers. Each cell times only data writing, sending and
receiving, against the strongest rival for that cell (zpp_bits or unchecked
cista, neither with a separate verification pass, XOffset on its trusted
tier). For the 4.0.0
release ([PERFORMANCE.md 6b](docs/PERFORMANCE.md)): **19 gated cells, 14 wins,
5 ties, 0 losses**, 8 batches × 21 cycles, the borderline shared-delta cells
re-judged under three code layouts.

A review on 2026-11-17 found the verdicts sound and added reading notes
([PERFORMANCE.md 6b](docs/PERFORMANCE.md#reading-this-scorecard-review-of-2026-11-17)):

- The scorecard was measured on 4.0.0 and has not been rerun on the current
  tree.
- The two storage cells (S, SD) against zpp_bits include a decode on zpp's
  side that the trusted tier does not need. On the same basis they are 0.24
  and 0.41, not 0.17 and 0.28. Unchecked cista is the strongest rival of both
  cells (0.41 and 0.39), so they stay wins.
- The client-order cells (UL) take about 42 ns per order on the cloud host,
  about 30 ns of which is the two clock reads around each message, for
  XOffset and rival alike. That pulls the ratio toward 1: UL.recv is a tie at
  0.97 there and 0.82 on the Apple M4.
- The later Apple M4 run (18 wins, 1 tie, 0 losses) has zpp_bits as its only
  rival. Unchecked cista, the strongest rival of six cloud cells, was not run
  there, so that count is not the same measure as 14 / 5 / 0.
- zpp_bits always bounds-checks its reads and has no unchecked mode, so its
  receive cells compare a checked decoder with XOffset's unchecked trusted
  tier, as the trust model intends.
- XOffset messages are larger: 1.11 to 1.46 times zpp_bits after zstd-3, and
  1.34 to 2.07 times uncompressed between servers.

These numbers hold for these workloads, adapters and hosts. A different
workload or a different way of writing the rival adapters can change them.
[PERFORMANCE.md](docs/PERFORMANCE.md) has the per-release history (including
the earlier Apple M4 results), the protocol and the limits.

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
| [V2_FORMAT.md](docs/V2_FORMAT.md), [V2_THREAT_MODEL.md](docs/V2_THREAT_MODEL.md) | Wire format; what the checks do and do not cover |
| [ALPHA_COMPATIBILITY.md](docs/ALPHA_COMPATIBILITY.md) | Compatibility and input contract |
| [CLIENT_TARGETS.md](docs/CLIENT_TARGETS.md), [DEVICE_TESTING.md](docs/DEVICE_TESTING.md) | Client toolchains and real-device records |
| [PERFORMANCE.md](docs/PERFORMANCE.md), [bench/cppcon2024](bench/cppcon2024/README.md) | Performance numbers and methods |
| [CHANGELOG.md](CHANGELOG.md), [RELEASING.md](docs/RELEASING.md) | Releases |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Building, testing and conventions for library development |

## License

See [LICENSE](LICENSE).
