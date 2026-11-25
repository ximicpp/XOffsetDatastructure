# How it works: a short example and the arena

A first look at the API and at what the bytes in the arena are. The [README](../README.md) lists the
core features and the performance numbers; [GETTING_STARTED.md](GETTING_STARTED.md) has one runnable
example per use case, and [USER_GUIDE.md](USER_GUIDE.md) is the full reference.

## A short example

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
    XSpare<8> spare{};                        // bytes a later version can add fields in
};
XOFFSET_RECORD(Unit, id, x, y, hp, name, spare)   // global scope, right after the type

struct WorldView {
    std::uint64_t tick;
    XVector<Unit> units;
    XSpare<20> spare{};
};
XOFFSET_RECORD(WorldView, tick, units, spare)
XOFFSET_REGISTER_SCHEMA_NAME(WorldView, "game.WorldView")   // one stable name per root type
```

Reserve `XSpare` bytes in the first release: a later version that only adds
fields in them talks to the old one with no conversion, in both directions
([SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)). Every connection starts
with the schema handshake (`xoffset_handshake.hpp`), which refuses a peer of
another generation.

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

Runnable versions: [`examples/helloworld.cpp`](../examples/helloworld.cpp), and
one example per use case in [GETTING_STARTED.md](GETTING_STARTED.md):
server to client (`server_to_client.cpp`), deltas (`delta_sync.cpp`,
`shared_delta.cpp`), client uplink (`client_uplink.cpp`) and server to server
(`server_to_server.cpp`).

## The arena

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
   `save_verified` writes the used part of the arena as it is; a buffer that
   churn has left with much free space goes out compact instead (the same
   contents laid out afresh, spare capacity kept). The receiver copies the
   bytes into its own buffer and carries on: one copy on each end, no encode
   or decode, and the receiver keeps editing in place.
4. **The schema is the C++ type.** Each record's field list (written once with
   `XOFFSET_RECORD`, or derived by C++26 reflection) gives a layout signature
   and a 64-bit fingerprint. The wire carries the fingerprint, and the receiver
   rejects a wire whose fingerprint differs. `XOFFSET_PIN_SCHEMA` fixes the
   layout text in the shared header so a client compiler that would lay a type
   out differently fails to compile. Types with raw pointers, references or
   virtual functions are rejected at compile time.
5. **Checks per link.** A trusted link checks the wire header and image
   bounds and does not walk the object graph. A bounds check walks the
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
   ([threat model 4.1](V2_THREAT_MODEL.md)).

The format is specified in [WIRE_FORMAT.md](WIRE_FORMAT.md) (wire format 3.0).
