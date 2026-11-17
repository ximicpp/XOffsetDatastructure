# XOffsetDatastructure Deployment Guide (4.0.0)

This guide is for engineers integrating XOffset into servers and clients. It covers the
prerequisites for releases, security and networking. API usage is in [USER_GUIDE.md](USER_GUIDE.md),
and the input boundary in [ALPHA_COMPATIBILITY.md](ALPHA_COMPATIBILITY.md).

## 1. Scope

XOffset moves mutable state **between C++ programs that share the same schema**: between servers,
and between servers and clients. Compilers and platforms may differ; the schema must be the same.
It is not a cross-language protocol and not a long-term storage format.

Server-to-client use is currently a goal and is not advertised publicly. Status in 4.0.0 (details in
[CLIENT_TARGETS.md](CLIENT_TARGETS.md)):

- Clients build with the C++20/C++23 field lists (`XOFFSET_RECORD`, each platform's native
  compiler) and servers with C++26 reflection; the wire is byte-identical. Windows x64 (MSVC 19.44,
  clang-cl 19, MinGW GCC 16) and Android arm64 (NDK r29/r30, run under cloud qemu-aarch64) pass the
  full tests on 4.0.0, read the reference corpus written by the server, and after editing save the
  same bytes as the server; the same results for macOS arm64 and the iOS simulator (Apple clang) come
  from 3.3.0 and were not re-run for 4.0.0 (releases since 3.3.1 use the cloud only).
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
  K6); otherwise the wire is rejected. Without the evolution support of section 2.1, schemas cannot
  evolve.
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
- Schema evolution (interop between old and new versions) is available since 3.2.0; see section 2.1.

### 2.1 Since 3.2: version chains and three release rules

A 3.2 server can exchange data with peers on several versions at once
([SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)): on connect the client sends an `XSchemaHello`, the
server judges it with `XPeerSchemas` and acks, and afterwards sends and receives in the peer's
version. With it, "server and clients release together" above is replaced by three rules:

1. **Servers release before clients.** All conversion happens on the server; clients only know their
   own version.
2. **The server's version chain covers every client version still in use.** How many to keep is a
   business decision; versions removed from the chain get `reject_unknown` at handshake, and the
   client can prompt for an update.
3. **Release in two steps when the server must be able to roll back.** First release a server that
   knows the new version but still works in the old one, then switch the working version in the next
   release. A rollback to the previous server then still knows the already upgraded clients.

At release, freeze the current version with `schema_freeze freeze` and put `schema_freeze check` in
CTest, to catch renaming fields without a layout change or forgetting to add the previous version to
the chain.

## 3. Build consistency

Toolchains verified for 3.1.0 (local platform matrix, 3.1.0 RC e913d22; GitHub CI has been off since
2026-10-23):

| Use | Platform | Compiler and standard library |
| --- | --- | --- |
| Server | Linux x86-64, Linux arm64 | GCC 16.2 (libstdc++), or clang-p2996 `d77eff1` (with its libc++). Both produce byte-identical wires and can be mixed |
| Client | Windows x64 | MinGW-w64 GCC 16.2 (`tools/build_mingw_toolchain.sh`, `cmake/toolchains/mingw-w64-x86_64.cmake`). GCC's MinGW TLS is emulated, so per-thread caches are off on this target (`XOFFSET_THREAD_CACHES=0`) |
| Client | Android, iOS, macOS, Windows (MSVC, clang-cl) | No usable toolchain for 3.1.0; since 3.2.0 they use the C++20 field-list build: Android NDK r29/r30, MSVC 19.44, clang-cl 19, Apple clang for iOS (Xcode 27.1, simulator 129/129) and macOS (Xcode 27.1, C++20 and C++23 137/137 each), see [CLIENT_TARGETS.md](CLIENT_TARGETS.md) |

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

- **Always use `load_verified`** (or `TypedXBuffer<T>::load`, `begin_verified_receive`), **and always
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
  an integer field value was changed is accepted. The wire header (format 2.1) has no checksum field,
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
by `save_verified`, `freeze_verified` and delta sync may contain elements already erased from
containers, and content the buffer held before it was reset. When sending to receivers that should not see that
content (typically clients), choose one:

- Build the server with `-DXOFFSET_HARDENED=1` (CMake: `-DXOFFSET_HARDENED=ON`) to restore zeroing;
- Or send on that path with `save_compact`: it writes no holes, and rewrites slack and string tails
  as 0.

| Source | Default build | `XOFFSET_HARDENED=1` build | What you need to do |
| --- | --- | --- | --- |
| P1 freed blocks (holes): old blocks left after replacing a string with a longer value, erasing elements, or vector migration | Blocks are zeroed when freed, so holes in the arena are themselves 0 (not a switch) | Same | When forwarding received data, the bytes in the sender's wire holes are sent on unchanged (they are the sender's bytes, not this end's memory). Rebuild with `save_compact` before forwarding, see section 4.2 |
| P2 spare container capacity: old elements in `[size, capacity)` after `clear()`, shrinking `resize()`, `erase`, `pop_back`; old string tails after shortening | Old elements and characters stay in the slack and are sent with the wire until a later append overwrites them | Vacated bytes are zeroed on shrink (`XOFFSET_SCRUB_ERASED`, `XOFFSET_SCRUB_POPPED`; `clear()` is therefore O(n)) | In the default build, send buffers from which content the receiver must not see was erased with `save_compact` |
| P3 implicit padding bytes in structs | Padding carries bytes from the value the element was built from (usually on the stack), so wires with the same content may differ | Container elements built or moved by the library (including `XMap` key-value pairs) have their padding zeroed (`XOFFSET_SCRUB_PADDING`) | Add `XOFFSET_REQUIRE_NO_PADDING(T)` to schemas sent to clients, or define `XOFFSET_REJECT_IMPLICIT_PADDING` globally, so implicit padding fails to compile; add explicit fields per the error. Types without implicit padding are not affected |
| P4 content from before a reset: after `reset<T>()`, bytes of the previous content stay in the slack, string tails and padding of the new content's blocks | Not zeroed. **One send buffer that builds messages for different receivers in turn carries content of the previous receiver's message inside the next wire** | `reset` zeroes all used bytes (`XOFFSET_SCRUB_RESET`) | In the default build, for a send buffer used for different receivers in turn: build with `-DXOFFSET_SCRUB_RESET=1`, or send with `save_compact`, or give each receiver its own buffer |

Still keep in mind:

- All data **currently alive** in a buffer is sent. Do not put server-internal fields or other
  players' data in the same buffer as what you send down; build a separate buffer containing only
  visible data before sending.
- Validation only guarantees structural safety and does not check content; see "What validation does
  not guarantee" above.

### 4.2 Forwarding client uplink data: rebuild with `save_compact`

When the server forwards client A's uplink data to client B (chat, parties, trading, spectating), do
**not** `load_verified` and then directly `save_verified` or `freeze_verified`. Validation checks only
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
3. **Direct client-to-client links only for the same version.** When two clients exchange data
   directly with no server in between, nobody converts versions, so the handshake (`XSchemaHello`,
   section 2.1) must conclude `same`; otherwise reject. When relayed through a server, the server can
   convert data sent down to older clients per section 2.1. The demo forwards only to clients of the
   same version: A groups clients by their handshake version.
4. **No-exception clients: leave headroom in `maximum_capacity`.** Modifying a received buffer makes
   it grow. A compact wire has no slack, so inserting an element or making a short string longer must
   grow it. When growth exceeds `maximum_capacity`, builds with exceptions throw `XBadAlloc`, which
   can be caught to abandon the edit. Builds without exceptions (`-fno-exceptions`, `/EHs-c-`) can
   only enter fatal error handling and the process terminates. So on no-exception clients, every
   buffer that will be modified needs a limit of "largest valid message + the most one edit can add"
   plus headroom, and the client should check sizes itself before modifying. The demo's snapshot
   limit is 16 MiB; a view snapshot is about 15 KB and one edit adds less than 1 KB. Read-only buffers
   do not grow and are not affected. `writeback` in `mapdemo_selftest` verified this with a buffer
   whose limit is only 256 bytes larger than the wire: inserting throws `XBadAlloc`.

## 5. Capacity and resource limits

`maximum_capacity` also determines the virtual address space each buffer reserves. The default is
64 MiB (or the initial length, whichever is larger); the hard payload limit is `0x7ffffff8` (about
2 GiB), which is not a recommended per-request budget.

| Scenario | Recommendation | Basis |
| --- | --- | --- |
| Server receiving client data | Default `maximum_capacity` of **1 MiB**; check the length before reading a wire and drop it if larger | Client uplink messages are mostly KB-sized; adjust upward to 2x the largest valid business message |
| Server to server | Set the limit to the largest valid size of that kind of state (one state block can reach hundreds of MB) | Data between servers counts as authenticated, but abnormal data must still not bring down the process |
| Client receiving or building | Use a smaller `maximum_capacity` (2x the largest message sent down); when creating and destroying buffers often, reuse free reservations with `XBufferPool`; **on iOS keep the sum of `maximum_capacity` over all live buffers within 3 GiB** | Each live buffer reserves 64 MiB of address space by default; a pool only reuses free reservations and does not shrink those of live buffers. iPhone measurements below |

iPhone measurements (iPhone 15 Pro Max, iOS 26.6.2, 8 GB RAM, signed with a free developer account,
`xo_mem_probe`, 2026-10-28):

- One process can reserve about 3.4 GiB of address space: at most 54 buffers of 64 MiB (3.375 GiB),
  or 2 of 1 GiB; one buffer's `maximum_capacity` can be at most 2047 MiB. Creating buffers after that
  throws `XBadAlloc`; the process is not killed.
- Actual writes: one buffer failed to allocate at 1920 MiB written, the process stayed up, and
  `os_proc_available_memory` was about 3.4 GiB at startup.
- So with the default 64 MiB, no more than about 50 buffers can be alive at once. Clients that hold
  many buffers must set small limits on each: the SLG stress test at M scale (one buffer per each of
  512 players) uses 2 MiB per player, battle and report buffer (`SLG_XOFFSET_CAP_MIB=2`) and 256 MiB
  for map shards; with 256 MiB each, even S scale (64 players) fails.
- Android (64-bit Linux kernel) and desktop systems have much larger address spaces and no such
  limit; their limit is the memory actually written.
- Since 3.4 the host header of a VM buffer sits in the first 64 bytes of the payload's first page; when
  the limit is exactly a multiple of 4 MiB (such as 64 MiB), each buffer maps one more 4 MiB granule
  (inaccessible and not resident while unused), with alignment unchanged. If iOS's limit counts mapped
  address space, about 50 buffers of the default 64 MiB can be alive at once (the 54 above was measured
  on 3.3; 3.4 has not been re-measured on an iPhone). Setting the limit to a multiple of 4 MiB minus 64
  bytes (such as `64 MiB - 64`) avoids mapping the extra granule.

Also:

- One valid buffer does not mean total resources are controlled: also limit the number of concurrent
  buffers, queue lengths, total memory or virtual address budget, and the processing time of a single
  request.
- Every wire has a 40-byte header and every buffer has a virtual memory reservation. The cost of
  high-frequency messages of about a hundred bytes has not been measured; measure first for such
  cases.
- The 1 MiB and "2x" in the table above are suggested starting points, not measurements; adjust them
  to your business data when integrating.

## 6. Network transport

- A wire is the raw memory layout and is larger than varint-style formats. SLG M scale, player state
  message in cycle 20: 3.1.0 dense wire 13,667 B, FlatBuffers 8,423 B; protobuf about 4.5 KB
  (2026-10-21 prototype). The extra size is mostly container slack and holes; for read-only messages
  sent down, `save_compact` with `XCompactOptions{.slack_div = 0}` removes it, estimated on par with
  FlatBuffers (not measured on 3.1.0).
- Links inside the server data center usually do not need compression.
- **On client links (especially mobile networks) add zstd compression outside the library**, or lz4
  (faster, lower ratio). Compress after `save_verified` and decompress before `load_verified`; the
  library's validation is unaffected. Also limit the decompressed length to guard against compression
  bombs.
- A whole-buffer wire carries the whole state; for incremental updates use delta sync
  ([USER_GUIDE.md](USER_GUIDE.md) sections 4.5 and 4.6).

### 6.1 Choosing compression

Compression is not in the library; the library only produces and validates wires. Choose by message
size:

| Message | Choice | Basis (measured on cloud x86-64) |
| --- | --- | --- |
| Large snapshots (KB and above: whole scene, view, full state) | **zstd level 3**, standard frame | Demo whole scene 321 KB → 123 KB (0.38), view 15.1 KB → 6.4 KB (0.42); `save_compact` + `XCompactOptions{0}` plus zstd-3 makes SLG A/B/C 7–9% smaller than FlatBuffers' raw size |
| Small messages (tens to hundreds of bytes: events, battle reports, commands) | **zstd dictionary**; when several are sent at the same moment, **also batch** them (several per frame) | Without a dictionary zstd barely helps small messages: an SLG battle report (class D, XOffset 197 B, FlatBuffers 158 B) compresses to XOffset 135.9 B, FlatBuffers 114.0 B (1.19x); with a 16 KB dictionary: single message 56.2 B versus 56.3 B (1.00), batches of 8 / 32 / 128 average 40.4 / 37.2 / 34.5 B versus 41.9 / 38.1 / 35.3 B (0.96 / 0.98 / 0.98). Encoding about 1.2–1.5 µs and decoding about 0.5–0.7 µs per message |
| Tight CPU (low-end phones, small per-frame budget) | **lz4** | Lower ratio than zstd, faster; this project has no lz4 numbers, measure when integrating |

Dictionary compression for small messages uses `contrib/zstd_dict` (a separate component, C++17, links
zstd 1.5.x, not part of the library):

- **Train the dictionary from server samples**: `xoffset_zstd_dict train --id N --binding auto INPUT...`,
  where the samples are real XOffset wires (several thousand or more). Training is deterministic: the
  same samples give the same dictionary.
- **Both ends carry the dictionary version**: the dictionary id is the version number and is written
  in every frame header. A client reports its dictionary id when subscribing, and the server rejects
  the subscription on a mismatch; the decoder rejects frames with another id before decoding
  (`dict_id_mismatch`). Changing the dictionary = changing the id; keep old and new dictionaries on
  the server until old clients have upgraded.
- **Dictionaries are bound to a schema**: `--binding auto` binds the dictionary to the schema
  fingerprint in the sample wire headers, and the encoder rejects wires of other schemas; once bound,
  each frame saves the fixed 24 bytes of the wire header, which the decoder restores. Changing the
  schema means retraining the dictionary.
- **Length limits**: before decoding, frames are rejected by their declared length and the caller's
  limit, with no allocation first; batch frames limit the count and the length of each message. What
  comes out is still an unvalidated wire and goes through `load_verified` as usual.
- Frames carry no checksum; integrity is left to the transport (TCP/TLS) and `load_verified`.

The `demo/mapstate` event stream is a complete example: A pushes a batch of UnitEvents every 20 ticks
(144–168 B each), and clients subscribe with `--events raw|zstd|zdict|batch`. Average network bytes
per message at the 10000-unit tier: raw 146, zstd 152, dictionary 68, batches of 100 per frame 52.

## 7. Integration checklist

- [ ] All senders and receivers use the same schema; when the schema changes, server and clients release together (since 3.2 the version chain and three rules of section 2.1 can be used instead)
- [ ] Versions or schema fingerprints are compared at handshake (since 3.2 with `XSchemaHello` / `XPeerSchemas`)
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
- [ ] Client links chose compression per section 6.1 (zstd-3 for large snapshots, dictionary or batching for small messages, lz4 when CPU is tight), with a limit on the decompressed length
- [ ] When using dictionaries: both ends compare the dictionary id, the dictionary is bound to the schema, and it is retrained with a new id when the schema changes
