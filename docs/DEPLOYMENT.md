# XOffsetDatastructure Deployment Guide (2.0.0)

This guide is for engineers integrating XOffset into servers and clients. It covers the
prerequisites for releases, security and networking. API usage is in [USER_GUIDE.md](USER_GUIDE.md),
and the input boundary in [ALPHA_COMPATIBILITY.md](ALPHA_COMPATIBILITY.md).

## 1. Scope

XOffset moves mutable state **between C++ programs that share the same schema**: between servers,
and between servers and clients. Compilers and platforms may differ; the schema must be the same.
It is not a cross-language protocol and not a long-term storage format.

Server to client is a core use case, next to server to server ([README](../README.md)). Client status
in 4.2.0 (details in [CLIENT_TARGETS.md](CLIENT_TARGETS.md)):

- Clients build with the C++20/C++23 field lists (`XOFFSET_RECORD`, each platform's native
  compiler, zero submodules) and servers with C++26 reflection; the wire is byte-identical. On 4.2.0,
  Windows x64 (native MSVC 19.44 C++20 and C++23, 214/214 each), macOS 12.0 arm64 (Apple clang 21
  C++20 and C++23, 214/214 each) and the iOS 14.0 simulator (Apple clang 21 C++20, 214/214) pass the
  full tests, read the 27-file Wire 3.0 reference corpus written by the server, and after editing save
  the same bytes as the server; on 2026-11-24 physical Android 11 arm64-v8a (NDK 27 Clang 18, 206/206)
  and iPhone 15 Pro Max arm64 (`minos 14.0`, `XOFFSET_USE_PMR=0`, 207/207) also verified full CTest and
  27/27 Wire 3.0 corpus identity. On 4.3.0 the cloud Wine/QEMU columns (MSVC 19.44 C++20 and C++23, clang-cl 19,
  MinGW GCC 16, Android NDK r29/r30) pass 247/247 each plus 52 no-exceptions tests, with the same
  corpus bytes as the server.
- End-to-end demo (`demo/mapstate`): a Linux server to Windows, Android emulator, iOS simulator and
  macOS clients all run. Real devices: 3.2.0-rc.1 passed every functional check on one Android 11
  phone (record in [validation/2026-10-28-rc1-platform](validation/2026-10-28-rc1-platform/README.md)),
  and on an iPhone 15 Pro Max (iOS 26.6.2) CTest, the reference corpus and both demo clients all
  passed; steps in [DEVICE_TESTING.md](DEVICE_TESTING.md).
- Client performance data exists only for slg on one iPhone 15 Pro Max ([DEVICE_TESTING.md](DEVICE_TESTING.md)
  section 9: in scenarios A, B and C xoffset takes 0.18 to 0.31 of FlatBuffers' time, D read-only
  battle report 0.86); all other timing comes from Apple M4.

## 2. Same-version releases

- The schema fingerprint in the wire header must match the receiver's type exactly (core guarantee
  K6); otherwise the wire is rejected. Section 2.1 describes the one kind of change that keeps the
  fingerprint.
- All programs that exchange data (every server, every platform's clients) must be built from **the
  same schema definitions** and released together.
- **Any server update that changes the schema must ship together with the clients.** Mobile game
  clients go through app store review, so plan release cycles around the clients.
- When a client version is behind, reject it by version number at login or handshake instead of
  waiting for `load_verified` to fail. `wire_schema_hash_v<T>()` can be part of the handshake and
  compared when the session is set up.
- During rolling upgrades between servers, old and new versions must not mix the same kind of data.
  Switch in batches (only servers of the same version talk to each other at any time), or export with
  the old program and import with the new one during the switch.
- Old and new versions can talk without releasing together when the new one only adds fields in
  reserved bytes; see section 2.1.

### 2.1 Add-only versions and generations

[SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md) has the rule. In short:

1. **Reserve bytes in every release** (`XSpare<N>` at the end of the records that will grow). A later
   version that only takes new fields from them and declares `XOFFSET_SCHEMA_FAMILY` keeps the
   fingerprint and the bytes: servers and clients of every version of the family talk with no
   conversion, deltas included, and update in any order. "Server and clients release together" above
   does not apply to such a change.
2. **Any other change is a new generation**: the handshake (`XSchemaHello` on connect,
   `accept_schema_hello` on the server) refuses a peer of another generation with `reject_unknown`, the
   client prompts for an update, and the connection closes. Release such a change with the clients, or
   run the old and the new service side by side until the old clients are gone. Nothing converts
   messages per peer.
3. **Stored data of an older generation** loads once through `XOFFSET_SCHEMA_LINEAGE` and
   `evolve_load_verified` and is saved in the new generation; keep a backup of the old images until then.

At release, freeze the current version with `schema_freeze freeze` and put `schema_freeze check` in
CTest, to catch renaming fields without a layout change, or a fingerprint change declared neither as an
added field nor as a new generation.

## 3. Build consistency

Toolchains verified for 3.1.0 (local platform matrix, 3.1.0 RC 1a1b210; GitHub CI has been off since
2026-10-23):

| Use | Platform | Compiler and standard library |
| --- | --- | --- |
| Server | Linux x86-64, Linux arm64 | GCC 16.2 (libstdc++), or clang-p2996 `d77eff1` (with its libc++). Both produce byte-identical wires and can be mixed |
| Client | Windows x64 | MinGW-w64 GCC 16.2 (`tools/build_mingw_toolchain.sh`, `cmake/toolchains/mingw-w64-x86_64.cmake`). GCC's MinGW TLS is emulated, so per-thread caches are off on this target (`XOFFSET_THREAD_CACHES=0`) |
| Client | Android, iOS, macOS, Windows (MSVC, clang-cl) | No usable toolchain for 3.1.0; since 3.2.0 they use the C++20/C++23 field-list build: Windows MSVC 19.44 (C++20 and C++23 214/214 each on 4.2.0), Apple clang 21 for iOS (simulator `minos 14.0` 214/214 on 4.2.0; physical iPhone 15 Pro Max 207/207) and macOS (`minos 12.0`, C++20 and C++23 214/214 each on 4.2.0), physical Android 11 arm64-v8a (NDK 27, 206/206), plus cloud Android NDK r29/r30, clang-cl 19 and MinGW GCC 16 (247/247 each on 4.3.0); see [CLIENT_TARGETS.md](CLIENT_TARGETS.md) |

- Pinned compiler sources: clang-p2996 in `toolchains/README.md`, the GCC 16 image digest in
  `tools/local_validate.sh`, and the MinGW toolchain recipe in `tools/build_mingw_toolchain.sh`.
  MSVC and clang-cl have no reflection and can only use the C++20 field-list build.
- MinGW and MSVC have different C++ ABIs: an XOffset module built with MinGW can only be used by an
  MSVC-built engine through a C interface.
- Within one executable, all translation units must use the same header version, compile options
  and dependencies. The same applies to the hardening switches (`XOFFSET_HARDENED`,
  `XOFFSET_SCRUB_*` and others, V2_THREAT_MODEL section 4.1): they change the behavior of inline
  functions. Sender and receiver need not use the same switches; the wire format does not depend on
  them.
- Type signatures (K7): pin every root type you send or receive with `XOFFSET_PIN_SCHEMA`
  ([SCHEMA_PINS.md](SCHEMA_PINS.md)) and put the pin file in the server's and every client's build.
  If any build computes a different layout, compilation fails right away instead of `load_verified`
  rejecting at run time. When changing the schema on purpose, regenerate the pin file and review its
  diff. Pins only detect layout changes; version mismatches are still handled per section 2.
- Only 64-bit little-endian platforms are supported; 32-bit Android builds are not.

## 4. Untrusted input

Data sent by clients to the server, and any data from the network, is untrusted input.

- **Always use `load_verified`** (or `TypedXBuffer<T>::load`), **and always
  set `maximum_capacity`** (section 5). Checking the actual wire length is not enough: a sparse wire
  can declare a size close to the payload limit (about 2 GiB) in a few hundred bytes and make the
  receiver reserve that size and validate the whole graph, exhausting resources. Without a limit,
  the library rejects larger wires before reserving, using the default limit (64 MiB, adjustable with
  `XOFFSET_DEFAULT_MAXIMUM_CAPACITY`), and the error names the declared size and the limit; but the
  default is a fallback, not a budget for each request, so set it explicitly per section 5 when
  receiving untrusted data. The structural safety guarantee (K4) assumes a limit is set.
- Receive client uplink with `XReceiver<XLink::from_client>`: it checks by default (`view` uses the
  bounds tier, `reload` and the others the full tier), and the trusted tier does not compile on it.
  `-DXOFFSET_CHECK_UPLINK=0` turns that default off (a switch since 2026-11-17, default 1): the client
  link then checks only the wire header, like the trusted links. **Do not build a server that faces
  outside clients with it**; any client could then send one crafted message that makes the server
  read or write out of bounds (V2_THREAT_MODEL section 4.1).
- `load_unverified` is only for in-process or already authenticated data.
- **What validation guarantees**: after validation passes, typed access to the root object graph does
  not go out of bounds, and the structure matches the schema (truncation, out-of-bounds, overlap,
  incomplete tiling, invalid bools, unordered sets and schema mismatch are all rejected).
- **What validation does not guarantee**: authenticity and integrity of the content. Data in which only
  an integer field value was changed is accepted. The wire header (format 3.0) has no checksum field,
  no signature and no MAC.
- **So these must be done outside the library**:
  - Source authentication and integrity: TLS or message signatures;
  - Business validity checks: field value ranges, permissions, rates, as for any business input;
  - Resource limits (section 5).

### 4.1 No stale data in the wires you send

A wire is the raw bytes of the arena, so before sending to clients make sure it contains nothing that
should not be sent. The threat model (`docs/V2_THREAT_MODEL.md`) measured the sources below.

**Since 2026-11-16 the default build does not zero the second and third kinds of bytes, and since
2026-11-17 a reset does not zero either** (performance first, V2_THREAT_MODEL section 4.1). Data sent
by `save_verified` and delta sync may contain elements already erased from
containers, and content the buffer held before it was reset. When sending to receivers that should not see that
content (typically clients), choose one:

- Build the server with `-DXOFFSET_HARDENED=1` (CMake: `-DXOFFSET_HARDENED=ON`) to restore zeroing;
- Or send on that path with `save_compact`: it writes no holes, and rewrites slack and string tails
  as 0. It copies the implicit padding of records as it is, so this holds only for types without
  implicit padding (P3 below).

| Source | Default build | `XOFFSET_HARDENED=1` build | What you need to do |
| --- | --- | --- | --- |
| P1 freed blocks (holes): old blocks left after replacing a string with a longer value, erasing elements, or vector migration | Blocks are zeroed when freed, so holes in the arena are themselves 0 (not a switch) | Same | When forwarding received data, the bytes in the sender's wire holes are sent on unchanged (they are the sender's bytes, not this end's memory). Rebuild with `save_compact` before forwarding, see section 4.2 |
| P2 spare container capacity: old elements in `[size, capacity)` after `clear()`, shrinking `resize()`, `erase`, `pop_back`; old string tails after shortening | Old elements and characters stay in the slack and are sent with the wire until a later append overwrites them | Vacated bytes are zeroed on shrink (`XOFFSET_SCRUB_ERASED`, `XOFFSET_SCRUB_POPPED`; `clear()` is therefore O(n)) | In the default build, send buffers from which content the receiver must not see was erased with `save_compact` |
| P3 implicit padding bytes in structs | Padding carries bytes from the value the element was built from (usually on the stack) or, after a reset, bytes of the previous content; wires with the same content may differ. Every path copies padding as it is, `save_compact` included. `XMap` entries are records too (`XMap<std::int32_t, std::int64_t>` has 4 padding bytes per entry) | Container elements built or moved by the library (including `XMap` key-value pairs) have their padding zeroed (`XOFFSET_SCRUB_PADDING`) | Add `XOFFSET_REQUIRE_NO_PADDING(T)` to schemas sent to clients, or define `XOFFSET_REJECT_IMPLICIT_PADDING` globally, so implicit padding fails to compile; add explicit fields per the error. Types without implicit padding are not affected |
| P4 content from before a reset: after `reset<T>()`, bytes of the previous content stay in the slack, string tails and padding of the new content's blocks | Not zeroed. **One send buffer that builds messages for different receivers in turn carries content of the previous receiver's message inside the next wire** | `reset` zeroes all used bytes (`XOFFSET_SCRUB_RESET`) | In the default build, for a send buffer used for different receivers in turn: build with `-DXOFFSET_SCRUB_RESET=1`, or send with `save_compact` (only for types without implicit padding, P3), or give each receiver its own buffer |

Still keep in mind:

- All data **currently alive** in a buffer is sent. Do not put server-internal fields or other
  players' data in the same buffer as what you send down; build a separate buffer containing only
  visible data before sending.
- Validation only guarantees structural safety and does not check content; see "What validation does
  not guarantee" above.

### 4.2 Forwarding client uplink data: rebuild with `save_compact`

When the server forwards client A's uplink data to client B (chat, parties, trading, spectating), do
**not** `load_verified` and then directly `save_verified`. Validation checks only
structure, not whether the bytes in holes and container slack are 0, so A can write anything there,
`load_verified` still accepts it, and `save_verified` sends it to B unchanged. This is not a memory
safety problem; it is a side channel that bypasses business checks.

Rebuild with `save_compact` before forwarding:

```cpp
XBuffer in = XBuffer::load_verified<ChatMsg>(wire_from_a, XBuffer::maximum_capacity(1u << 20));
// ... business checks ...
std::string to_b = in.save_compact<ChatMsg>();   // not in.save_verified<ChatMsg>()
```

`save_compact` rewrites the wire from the object graph, and clears these places that A could write:

- Holes: not written;
- Container slack (`[size, capacity)`): reallocated at the new capacity, with the slack written as 0;
- Bytes after a string's length: zeroed (for inline strings validation already requires this part to
  be 0).

It **does not cover** two places, which need separate handling:

- **Implicit padding in structs**: the root object and container elements are copied as whole bytes,
  so padding bytes are carried along, and validation does not check padding. Forwarded types must
  have no implicit padding: add `XOFFSET_REQUIRE_NO_PADDING(T)` to the forwarded root type (it also
  checks the records and container elements it contains), or define `XOFFSET_REJECT_IMPLICIT_PADDING`
  globally, so padding fails to compile; add explicit fields per the error.
- **The field values themselves**: scalars and string contents are what A sent, and `save_compact`
  forwards them as they are. Check them as ordinary business input (length, range, banned words and
  so on).

Read-only forwarding can pass `XCompactOptions{.slack_div = 0}` for the smallest wire; use the default
slack when B will keep modifying.

### 4.3 Receive, modify, send again: rules for passing data along

Between 64-bit little-endian machines the wire is the buffer itself: after `load_verified` the
receiver reads and writes directly in the buffer, with no field-by-field decoding; when done it sends
with `save_verified`, and the next hop runs `load_verified` again. The `demo/mapstate` client's
`--edits` runs exactly this chain: after receiving a snapshot the client deletes a unit in its own
buffer, changes hp and behavior, adds a unit and makes a short name longer, then sends it up with
`save_verified`; server A accepts and checks it, rebuilds it with `save_compact` and forwards it to
another client, which validates it again. There are four rules.

1. **Always fully validate what you receive from a peer, with a limit.** Uplink data uses
   `load_verified` with `maximum_capacity` (the demo uses 1 MiB, see section 5). Validation only
   proves structural safety; the content was written by the peer and needs separate checks:
   - Check business fields as ordinary input. The demo's `checks::edit_sanity_of` checks that
     coordinates are in view, the ranges of hp and behavior, name length and that ids are unique.
   - To confirm that what arrived is what the peer meant to send, carry an application-level digest
     in the protocol. The demo's frame header carries a content digest computed by the client, and A
     recomputes and compares it on receipt. This only catches transport and implementation errors:
     the digest travels on the same channel as the data and a peer can change both, so **it does not
     stop tampering**. Source authentication and integrity are still left to TLS or signatures
     (section 4).
2. **Rebuild before forwarding.** Before the server forwards one client's data to others, rebuild it
   with `save_compact` (section 4.2). Do not forward the received wire as is, and do not
   `load_verified` and then `save_verified` directly. The demo self-test (`writeback` in
   `mapdemo_selftest`) measured it: bytes a client wrote into container slack are accepted by
   `load_verified` and gone after a `save_compact` rebuild.
3. **Links only within one generation.** Whether two clients exchange data directly or through a
   server, the handshake (`XSchemaHello`, section 2.1) must conclude `same`: the same version, or
   versions of one add-only family, which have the same bytes; otherwise reject. The demo forwards
   write-backs between clients of both versions of its family.
4. **No-exception clients: use `try_*`, `edit()` / proxies, and leave headroom in `maximum_capacity`.**
   Modifying a received buffer makes it grow. In builds without exceptions (`-fno-exceptions`,
   MSVC `/EHs-c- /D_HAS_EXCEPTIONS=0`), use `try_create` / `try_clone` / `try_clone_into` /
   `try_load_verified` / `try_reload_verified` / `try_save_verified*` / `try_save_compact*` /
   `try_save_delta*` / `XCompactor::try_compact` / `XReceiver` for buffer lifecycle, receive, send,
   and compaction, and mutate only through `buf.edit(...)` or `buf.root()` proxies (not non-const
   `plain_root()`): mutations inside `edit()` and proxies fail softly via `SoftFailScope`, leave the
   failing operation without effect, and record the error in `buf.error()` instead of invoking the
   fatal handler. Call `buf.make_room(bytes)` or pass `expected_growth` to `edit()` before multi-step
   edits so a later step does not run after an earlier allocation fails, and register
   `set_fatal_handler(...)` for diagnostics. Leave headroom in `maximum_capacity` ("largest valid
   message + the most one edit can add") for buffers that will be modified. Read-only views
   (`XReceiver::view`) do not grow and are not affected.

## 5. Capacity and resource limits

`maximum_capacity` also determines the virtual address space a VM-backed buffer reserves. The default
is 64 MiB (or the initial length, whichever is larger); the hard payload limit is `0x7ffffff8` (about
2 GiB), which is not a recommended per-request budget. Small buffers created with Explicit growth
and initial size at most `SLOT_MAX` (256 KiB - 64 B) use slab slots rather than individual VM
reservations.

| Scenario | Recommendation | Basis |
| --- | --- | --- |
| Server receiving client data | Default `maximum_capacity` of **1 MiB**; check the length before reading a wire and drop it if larger | Client uplink messages are mostly KB-sized; adjust upward to 2x the largest valid business message |
| Server to server | Set the limit to the largest valid size of that kind of state (one state block can reach hundreds of MB) | Data between servers counts as authenticated, but abnormal data must still not bring down the process |
| Client receiving or building | Set `maximum_capacity` to what the client needs instead of the 64 MiB default; keep small and many buffers on slots (`<= SLOT_MAX`), and use `XBuffer::relocating(max)` or `XBufferPool` for large long-lived ones. On iOS, Apple's Extended Virtual Addressing capability adds address space; it has not been run on a device by this project yet | Without the entitlement, an iPhone limits one process to ~3.4 GiB of virtual address space, shared with the rest of the app (~50 64 MiB VM buffers if nothing else used any) |

On an iPhone 15 Pro Max (iOS 26.6.2) **without** Extended Virtual Addressing, one process can
reserve about 3.4 GiB of address space (~50 buffers of the default 64 MiB VM reservation), and the
engine, its assets and other libraries use the same budget. The measures that need nothing from Apple
come first: limits sized to the data, slots for small messages, a pool or `relocating(max)` for large
long-lived buffers. iOS builds signed with a paid Apple Developer Program team can also add the
`com.apple.developer.kernel.extended-virtual-addressing` entitlement (Xcode: *Signing & Capabilities*
-> *Extended Virtual Addressing*; `tools/test_host/sign_ios_app.sh` supports `XO_IOS_EXTENDED_VA=1`);
it has not been run on a physical iPhone by this project yet ([CLIENT_TARGETS.md](CLIENT_TARGETS.md)
lists it as uncovered). Keep three details in mind when enabling it:

- It expands **virtual address space** (what uncommitted `PROT_NONE` reservations consume), not
  physical RAM: Jetsam still enforces the device's physical memory limit, so call
  `XBuffer::trim_slot_storage()` / `pool.clear()` on scene transitions or backgrounding.
- Check any third-party library in the same process that assumes pointers fit in low address ranges
  (e.g. legacy pointer-tagging runtimes); XOffset uses 32-bit arena-relative offsets and full 64-bit
  host pointers and is unaffected.
- Even with Extended Virtual Addressing enabled, avoid creating thousands of simultaneous
  `Automatic + 64 MiB` VM buffers: use `XReceiver::view` for read-only receives, slots (`<= SLOT_MAX`)
  for small messages, or `XBuffer::relocating(max)` for variable-size long-lived buffers. A limit of
  a multiple of 4 MiB minus 64 bytes (such as `64 MiB - 64`) avoids mapping one extra 4 MiB granule
  per VM buffer. Android and desktop systems already provide much larger virtual address spaces.

Also limit the number of concurrent buffers, queue lengths, total memory and per-request processing
time: one valid buffer does not bound total resources. The 1 MiB and "2x" above are starting points,
not measurements.

## 6. Network transport

- A wire is the raw memory layout plus container slack and holes, larger than varint formats. For
  read-only messages sent down, `save_compact` with `XCompactOptions{.slack_div = 0}` removes the slack
  and holes.
- Links inside a data center usually need no compression. **On client links add zstd (or lz4) outside
  the library**: compress after saving, decompress before receiving, and limit the decompressed length.
- For incremental updates use delta sync ([DELTA_SYNC.md](DELTA_SYNC.md),
  [SHARED_DELTA.md](SHARED_DELTA.md)).

### 6.1 Choosing compression

| Message | Choice | Basis (cloud x86-64) |
| --- | --- | --- |
| KB and above (snapshots, views) | **zstd level 3** | Demo scene 321 KB -> 123 KB, view 15.1 KB -> 6.4 KB |
| Tens to hundreds of bytes (events, reports, commands) | **zstd dictionary**, and batch messages sent together | A 197 B battle report: plain zstd 136 B, with a 16 KB dictionary 56 B, batches of 32 about 37 B per message |
| Tight CPU budget | **lz4** | Faster, lower ratio; not measured here |

Dictionaries use `contrib/zstd_dict` (separate C++17 component, zstd 1.5.x; see its README):

- Train from real server wires: `xoffset_zstd_dict train --id N --binding auto INPUT...`
  (deterministic).
- The dictionary id is its version and is in every frame header; clients report it when subscribing,
  and frames with another id are rejected before decoding. Keep old dictionaries until old clients
  upgrade.
- `--binding auto` binds a dictionary to the schema fingerprint (a schema change means retraining) and
  saves the fixed 24 header bytes per frame.
- Frames are length-checked before decoding and carry no checksum; the output still goes through the
  normal receive check.

`demo/mapstate`'s event stream is a full example (`--events raw|zstd|zdict|batch`).

## 7. Integration checklist

- [ ] All senders and receivers work in one generation: the same schema, or versions of one add-only family (section 2.1); a change of generation releases server and clients together
- [ ] Versions or schema fingerprints are compared at handshake (`XSchemaHello`, `accept_schema_hello`, `read_schema_ack`)
- [ ] Every platform builds with a verified toolchain from the table in section 3, targeting only 64-bit little-endian platforms
- [ ] Every root type sent or received is pinned with `XOFFSET_PIN_SCHEMA`, and the pin file is in the server's and every client's build
- [ ] Network input only goes through `load_verified` paths, with `maximum_capacity` set
- [ ] A server that faces outside clients is not built with `XOFFSET_CHECK_UPLINK=0`, and the receiver of a client connection is `XReceiver<XLink::from_client>` (section 4)
- [ ] Schemas sent to clients have no implicit padding (`XOFFSET_REQUIRE_NO_PADDING` or `XOFFSET_REJECT_IMPLICIT_PADDING`), and buffers sent down contain only data visible to the client
- [ ] When a buffer sent to clients had elements erased that clients must not see, or one send buffer is used for different clients in turn: the server is built with `XOFFSET_HARDENED=1`, or that path sends with `save_compact` (section 4.1; the default build does not zero bytes vacated by containers, and a reset does not zero either)
- [ ] The server has a wire length limit for client data (suggested starting point 1 MiB)
- [ ] Client uplink data is rebuilt with `save_compact` before forwarding, and forwarded types have no implicit padding (section 4.2)
- [ ] When received data is modified and sent again: the receiver fully validates and runs business checks (adding an application-level digest when needed), direct client links are same-version only, and no-exception clients leave limit headroom on buffers they modify (section 4.3)
- [ ] Clients use a smaller `maximum_capacity` or `XBufferPool`
- [ ] There is authentication outside the library (TLS or signatures) and business field checks
- [ ] The number of concurrent buffers and the total memory budget are limited
- [ ] Each buffer, with its proxies, handles and delta baselines, is used by one thread at a time, reads included; one state goes to many connections as one saved copy, and builds with `XOFFSET_HARDENED=1` save after an `edit()` scope, not inside it ([user guide section 3.6](USER_GUIDE.md#36-threads-one-thread-per-buffer))
- [ ] Client links chose compression per section 6.1 (zstd-3 for large snapshots, dictionary or batching for small messages, lz4 when CPU is tight), with a limit on the decompressed length
- [ ] When using dictionaries: both ends compare the dictionary id, the dictionary is bound to the schema, and it is retrained with a new id when the schema changes
