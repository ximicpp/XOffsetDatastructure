# V2 threat model (2.0.0-preview.1; "0.3.0-beta.1" in this text means the state since that version)

This document describes what XOffset V2 protects against when receiving an **untrusted wire**, what it
does not, and what callers must add outside the library. Format details are in
[WIRE_FORMAT.md](WIRE_FORMAT.md) and integration requirements in [DEPLOYMENT.md](DEPLOYMENT.md). All
measurements here can be reproduced with the method of section 6.

## 1. Assets and attackers

**Assets**

- Memory safety of the receiving process: reading validated data never goes out of bounds or reads or
  writes memory outside the arena.
- Availability of the receiving process: the memory, address space and CPU one message consumes are
  bounded.
- Confidentiality of the sender's data: a wire contains only what is in the sender's current object
  graph.

**Attackers**

| ID | Attacker | Capability |
| --- | --- | --- |
| A1 | Any network sender (for example a malicious client) | Fully controls the wire bytes sent to the server; knows the schema and the public schema fingerprint |
| A2 | A compromised peer server | Same as A1, and can send very large messages |
| A3 | A man in the middle on the link (without TLS) | Reads and rewrites wires |
| A4 | A client receiving server data | Reads every byte of the wires sent to it |

Out of scope: attackers in the same process as the receiver, side channels, defects in the compiler or
the standard library itself.

## 2. Entry points

Untrusted bytes can only enter through the interfaces below, which all validate with the same
strength:

- `XBuffer::load_verified<T>`, `TypedXBuffer<T>::load`;
- Schema evolution: `evolve_load_verified<W>` for stored data of an older generation (picks the
  generation from the wire header, then goes through `load_verified` above), `accept_schema_hello` and
  `read_schema_ack` (handshake messages, also through `load_verified`, with additional 64 KiB and entry
  count limits). Network messages of an older generation have no entry point: the handshake refuses the
  peer, and a wire of another fingerprint fails `load_verified`.

- Reloading into an existing buffer: `reload_verified` / `try_reload_verified`, `begin_verified_reload`
  (receives directly into an existing buffer's storage), and the same entry points on movable slot
  storage (R1);
- `load_verified<T>` with an `upstream` argument (the validation bitmap comes from a memory resource
  supplied by the caller; validation itself is the same);
- Client builds: each of the entry points above is separately compiled code in the C++20 field-list
  build and in the no-exception build (`try_` entry points).

`load_unverified` does not validate and is **not** an entry point for untrusted input.

Receive checks have three tiers (decided 2026-11-04; the trusted and bounds tier interfaces were added
in 3.4): the trusted tier checks only wire header admission and the image bounds, and is for server to
server, server to client and your own saved files; it is **not** an entry point for untrusted input
(debug and sanitizer builds still validate fully). The trusted tier for whole-buffer receive and deltas
is `XReceiver<XLink::server_peer / from_server>::reload / load / apply_delta` (`xoffset_receiver.hpp`);
`XReceiver<XLink::from_client>` rejects the trusted tier at compile time; a client connection
mistakenly built as a trusted-link receiver is found by code review. A deployment whose clients are its
own code over a protected transport can build with `XOFFSET_CHECK_UPLINK=0` (a switch since 2026-11-17,
default 1, see 4.1): the client link then defaults to the trusted tier too, and only calls that name
`XCheck::bounds` / `XCheck::full` are checked. By definition the trusted tier
does not take untrusted input and does not promise a usable result for arbitrary input; fuzzing and the
`test_receiver` differential tests require only two things: its own parsing and writes never go out of
bounds for any input (ASan), and for input the full tier accepts it gives the same result (the admit
row of the table in section 5). The bounds tier (3.5) is for client uplink:
`XReceiver<XLink::from_client>::view` gives a read-only view `XView<T>` over the wire bytes, with const
access only. It guarantees that no const access reads outside the wire, that the work of a walk is at
most linear in the payload size (blocks can overlap, so the total element bytes walked are capped by
the payload size), and that bools are never anything but 0 or 1 (avoiding undefined behavior). It does
not guarantee that values make sense: input with overlapping blocks, out-of-order set/map keys or
non-zero padding bytes may be accepted, and reads give garbage or lookups miss. To modify client data
in place use the full tier, that is, the entry points above. The harness is the bounds row of the table
in section 5.

## 3. Threats and status

| ID | Threat | Status | Basis / gap |
| --- | --- | --- | --- |
| T1 | A crafted wire makes the receiver read or write out of bounds | **Protected** (a server built with `XOFFSET_CHECK_UPLINK=0` is not protected against client uplink, see 4.1) | Full validation (WIRE_FORMAT section 5); ASan+UBSan CI with Clang and GCC 16; libFuzzer on every PR (multiple schemas, section 5); pre-release GCC 16 ASan+UBSan differential fuzzing of 4.8 million cases with no disagreement. Gap: daily long fuzz runs are not yet running (section 5, F4) |
| T2 | Tampering with field values (A3) | **Not protected** | A wire that only changes scalar values passes validation; the wire header has no checksum (format 2.1 removed the always-zero `crc32c`) and no MAC. TLS or message signatures must be used outside the library |
| T3 | Forging schema identity | No privilege gained | The schema fingerprint (64-bit FNV-1a) is a consistency check, not authentication: an attacker copying the fingerprint can only make the receiver fully validate against **the receiver's own type** |
| T4 | Resource exhaustion | **Bounded by default** (M1: 64 MiB default when no limit is passed) | See 3.1; servers receiving client data should still set a smaller limit (DEPLOYMENT section 5) |
| T5 | Old bytes of deleted data sent with the wire (A4) | **Partly protected** (since 2026-11-16, performance first); essentially protected with `XOFFSET_HARDENED=1`, or with `save_compact` for types without implicit padding | Freed blocks are still zeroed (P1, M3). The default build does not zero bytes vacated by containers (P2) or padding bytes (P3), and `save_verified` and delta sync send them as they are; since 2026-11-17 `reset` does not zero either, so a reused buffer carries bytes of its previous content into the slack and padding of the next (P4). `XOFFSET_HARDENED=1` turns on M4 and M6 (the default behavior in internal-4.0.0 and earlier); `save_compact` never carries old bytes from holes, container slack or string tails in any build, but copies the implicit padding of records as it is (P3, P4). See 3.2 and 4.1 |
| T6 | Different compilers interpret the same wire differently | One case found and fixed | When an enum key defines only `operator<=>`, Clang and GCC order sets oppositely and reject each other's wires; the fix and the CI cross-read job are in the GCC 16 port PR |
| T7 | Invalid business values (negative gold and the like) | **Not protected** | Not part of structural validation; check them as ordinary business input |
| T8 | Amplifying cost with old-generation images (schema evolution) | **Bounded** | Only stored data converts (`evolve_load_verified`); no message from a peer does. An image of the oldest generation of a lineage costs one more full validation plus one conversion per step; the converted object can be larger than the image (new fields, wider types). The conversion target buffer's limit defaults to the `maximum_capacity` passed to the load, and exceeding it fails. Estimate the amplification from the lineage length and shorten it if needed. `fuzz_schema_evolution` covers this entry point |

### 3.1 Resource exhaustion (T4)

**R1 sparse wire amplification.** The gaps of a sparse wire take no wire bytes; they only have to fall
inside holes. A 184-byte wire can declare a `used_bytes` close to 2 GiB:

| Wire | Declared used_bytes | No limit | With `maximum_capacity(1 MiB)` |
| ---: | ---: | --- | --- |
| 184 B | 64 MiB | Accepted, reserves 64 MiB, 4 ms | Rejected |
| 184 B | 1 GiB | Accepted, reserves 1 GiB, 67 ms | Rejected |
| 184 B | 2047 MiB | Accepted, reserves 2 GiB, 140 ms, RSS +8 MiB | Rejected |

Without a limit, `load_verified` reserves the size the wire declares. Consequences:

- Server: every 184-byte message takes 2 GiB of address space and about 140 ms of CPU. A few dozen
  concurrent ones can exhaust the commit limit (failing outright with `vm.overcommit_memory=2`) or slow
  the service down.
- Client (iOS in particular): the address space budget is small, and a few such messages can crash the
  process.
- **Checking the wire length is not enough**: 184 bytes is far below any length limit.

**R2 free block amplification.** Measured with wire format 2.x, whose wire ended with a hole table
(8 bytes per free block): on load every entry went into the host-side free table, and validation also
registered it in the byte index:

| Declared used_bytes | Hole size | Wire | Load time | Free blocks | Peak RSS |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 GiB | 4096 B | 2 MiB | 182 ms | 262,144 | 141 MiB |
| 1 GiB | 64 B | 128 MiB | 797 ms | 16,777,215 | 267 MiB |
| 1 MiB | 8 B | 1 MiB | 5 ms | 131,063 | 5 MiB |

Host memory and time grow linearly with `used_bytes` and the number of holes, never superlinearly. With
`maximum_capacity` set, both are bounded by the limit (peak about 5 MiB with a 1 MiB limit).
Since wire format 3.0 the free blocks and their directory are inside the image: a sparse wire can still
declare many small free blocks, the full check walks every stack (at most `used_bytes / 8` blocks) and
registers each one in the byte index, and no host-side table is built. The bound is the same: linear in
`used_bytes`, capped by the limit.

**R3 other.** Concurrent buffer count, queue length, decompressed length and per-request time are
outside the library and limited by the caller (DEPLOYMENT sections 5 and 6).

**Conclusion:** the data above was measured before mitigation. Since 0.3.0-beta.1, when no limit is
passed, wires are rejected before reserving by the default limit (64 MiB, M1), bounding the R1 and R2
amplification by the default limit; the error for exceeding it names the declared size and the limit
(M2). For client uplink data 64 MiB is still too large; set a smaller limit per DEPLOYMENT section 5.

### 3.2 Leaking old content (T5)

The table below was measured before mitigation: at the time the library did not zero bytes no longer in
use, and a wire is a verbatim copy of the arena bytes, so there were three leak paths. How 0.3.0-beta.1
handles them is under "Status" after the table.

| Path | Example | Measured |
| --- | --- | --- |
| P1 freed blocks (holes) | After replacing a string with a longer value, erasing elements that hold heap strings, or vector migration, the old block goes into the free table with its content unchanged | Old strings (`SECRET-TOKEN-…`, `TOPSECRET-player-name`) appeared in the wire, and remained after `load_verified` and saving again |
| P2 spare container capacity | After `vector.clear()` or a shrinking `resize()`, `[size, capacity)` still holds the old elements | Old element bytes appeared in the wire |
| P3 object padding bytes | Structs with implicit padding (such as `{uint8_t; uint64_t;}`). When reallocated from a free block, or copied in from a temporary on the stack, the padding bytes carry old arena content or stack content | Measured (2026-10-21, Mac): an `alignas(8)` `Troop` with 12 bytes of data and `sizeof` 16; the temporary in `troops.push_back({1, 2, 3})` carried 4 bytes of indeterminate padding that were written into the wire verbatim; two builds of the same data differed by 1 byte at offset 3884 (2 versus 0). With an explicit `std::int32_t pad{};` they were identical. So P3 both makes wires nondeterministic and leaks bytes from the sender's stack |

When a string was overwritten by a shorter value, the old tail was measured not to be sent.

Consequence before mitigation: a wire the server sent to a client could contain data that **had once
been stored in that buffer and was already deleted** (other players' information, server-internal
fields and so on), and P3 could also leak data from the server's stack.

**Status (since 2026-11-16):** performance comes first by default, so zeroing for P2 and P3 became
compile switches, **off by default**; since 2026-11-17 so is the zeroing on reset (P4 below).
`XOFFSET_HARDENED=1` turns them all on (the default behavior in
internal-4.0.0 and earlier), or each can be turned on separately (section 4.1). In the default build, bytes sent
by `save_verified` and delta sync may contain elements already erased from
containers, and content the buffer held before it was reset. `save_compact` writes no holes and rewrites slack and string tails as 0, but copies the implicit
padding of records as it is, so for types with implicit padding it carries the bytes of P3 and P4.

- P1: blocks are zeroed when freed (M3), holes in the arena are 0, and all output paths still copy
  verbatim. This is **not a switch**: deltas generated from touch marks send a hole's bytes only when
  the hole appears or disappears, and both ends rely on holes being 0; turning it off would make the
  copy's bytes differ from the sender's.
- P2: not zeroed by default. After `clear()`, a shrinking `resize()`, `erase`, `pop_back()` or assigning
  a shorter value, old elements and characters stay in `[size, capacity)` until an append at that
  position overwrites them (strings still write the terminating NUL). Blocks owned by erased elements
  are freed and zeroed as usual (M3), so what remains is the elements' own inline bytes (scalar fields,
  inline arrays, inline strings of up to 10 bytes) and the old tails of shortened heap strings.
  `XOFFSET_SCRUB_ERASED=1` zeroes the bytes vacated by `clear`, `resize`, `erase` and assignment, and
  `XOFFSET_SCRUB_POPPED=1` zeroes elements removed by `pop_back` (M4).
- P3: not zeroed by default. Container elements with implicit padding carry the padding bytes of the
  value they were built from, usually bytes from the sender's stack; two builds of the same content can
  give different wires, and a delta can carry the padding of a record reassigned with equal values
  (measured 2026-11-25, GCC 13 and Clang 18 at -O0: every padding byte came from the stack; at -O2 on
  x86-64 none did, which depends on the compiler). Every path copies padding as it is, `save_compact`
  included. `XMap` entries are records too: `XMap<std::int32_t, std::int64_t>` has 4 padding bytes per
  entry. With `XOFFSET_SCRUB_PADDING=1` the padding of container elements the library
  builds or moves (including map entries) is zeroed (M6). The simpler way is to give wire types no
  implicit padding: find it at compile time with `XOFFSET_REQUIRE_NO_PADDING(T)` or
  `XOFFSET_REJECT_IMPLICIT_PADDING` and turn it into explicit fields. Padded records the caller assigns
  directly are out of the library's reach in any build.
- P4, reset (not zeroed by default since 2026-11-17): `reset<T>()` only marks the buffer empty, and the
  bytes of the previous content stay past the new `used_end`. The blocks of the new content are
  allocated from that range; the values written overwrite the old bytes, but the parts of a block that
  are not written (a vector's slack, the bytes from a string's end to the end of its block, implicit
  padding of records) still hold the previous content. So **one send buffer that builds messages for
  different receivers in turn carries content of the previous receiver's message inside the next
  wire** (measured 2026-11-16: one buffer building 40 messages in turn, 3 carried content of the
  message before). With `XOFFSET_SCRUB_RESET=1`, `reset` zeroes all used bytes (the behavior in internal-4.0.0
  and earlier). `save_compact` drops the slack and string tails in any build, but copies the implicit
  padding of records as it is, so with types that have implicit padding it still carries bytes of the
  previous content (measured 2026-11-25: a 1552-byte compact wire built after `reset` carried 140 to
  440 bytes of the message before, in the padding of `{std::uint8_t; std::uint64_t}` records and
  `XMap<std::int32_t, std::int64_t>` entries). The library internals still keep
  "everything after some offset is 0": the range not zeroed is recorded on the buffer, and in-place
  reload and delta receive work as before.
- Forwarding: holes and container slack (`[size, capacity)`) of a loaded wire keep the sender's bytes;
  validation does not require them to be 0, and saving directly after loading forwards them unchanged.
  This is not the receiver's own memory, but a malicious sender can use it to smuggle bytes downstream.
  Rebuild with `save_compact` before forwarding: it writes no holes, rewrites slack as 0 and zeroes
  string tails; implicit padding in records is still copied byte for byte, so forwarded types must have
  no implicit padding ([DEPLOYMENT.md](DEPLOYMENT.md) section 4.2).

Regression tests: `test_wire_hole_zeroing` (P1, P2, forwarding, M1/M2), `test_implicit_padding` (P3) and
`test_reset_used_range` (P4). Each test is built twice: the default build checks that holes are 0, that `save_compact` carries no old
bytes (their types have no implicit padding), and that content is correct; the `XOFFSET_HARDENED=1` build (`*_hardened`) checks that the wire
contains no old bytes at all, that padding is 0 and that the wire is deterministic.

## 4. Mitigations (all implemented in 0.3.0-beta.1)

M1–M6 were implemented in 0.3.0-beta.1; behavior changes are in CHANGELOG. M3 costs about +5% on the
modification path, measured. Since 2026-11-16 the zeroing of M4 and M6 is off by default, since 2026-11-17
the zeroing on reset too, and all are turned on by the switches of section 4.1; M1, M2, M3 and M5 are unchanged.

| ID | Mitigation | Impact |
| --- | --- | --- |
| M1 | When `load_verified` gets no limit, it uses the default limit (64 MiB, adjustable with `XOFFSET_DEFAULT_MAXIMUM_CAPACITY`) and rejects larger wires before reserving; pass `maximum_capacity` explicitly for larger wires | Public API behavior change: wires larger than 64 MiB need an explicit limit. Read path performance unchanged |
| M2 | The error for an insufficient limit became `used_bytes … exceeds maximum_capacity …` (previously the generic `XOffset arena allocation failed`) | Diagnostics only |
| M3 | Blocks are zeroed when freed, so holes in the arena are 0 (P1); output paths unchanged | Changes the bytes of wires with holes (goldens need updating); the cost is on the free path, proportional to the bytes freed. Holes of a loaded wire keep the sender's bytes, and saving directly after loading forwards them unchanged (no local memory leaks, but a malicious sender can smuggle bytes downstream). The first implementation zero-filled holes at output time and made formal output about 22% slower, so it was changed to zeroing on free |
| M4 | (Off by default) Zero the element bytes vacated when a container shrinks (P2): `XOFFSET_SCRUB_ERASED=1` covers `clear`, shrinking `resize`, `erase` and assigning a shorter value; `XOFFSET_SCRUB_POPPED=1` covers `pop_back` | Cost only on the modification path, proportional to the bytes removed; when on, `clear()` is O(n) |
| M5 | Documentation: DEPLOYMENT states "checking the wire length is not enough, a limit must be set", and P1–P3 | No code impact |
| M6 | Padding bytes (P3): compile-time check switches (`XOFFSET_REQUIRE_NO_PADDING`, `XOFFSET_REJECT_IMPLICIT_PADDING`) make a schema with implicit padding an error so users turn it into explicit fields; with `XOFFSET_SCRUB_PADDING=1` (off by default) the library zeroes padding positions when building or moving container elements | The compile-time check has no run-time cost; the cost of zeroing is in 4.1 |

### 4.1 Hardening switches (off by default since 2026-11-16)

The measures below spend time on the hot modification and send paths; they protect against old bytes in
a buffer being sent with the wire (P2, P3 and P4 of T5) or against the caller's own usage errors.
Because performance comes first by default (including reset zeroing since 2026-11-17), every item is a
compile switch, default 0;
`-DXOFFSET_<name>=1` turns one on, and `-DXOFFSET_HARDENED=1` (CMake option `XOFFSET_HARDENED=ON`)
turns on all items not set individually. The switches change the behavior of inline functions, so all
translation units of one program must use the same values.

| Switch | When on | When off (default) | Instructions saved by turning it off |
| --- | --- | --- | --- |
| `XOFFSET_SCRUB_POPPED` | M4: `pop_back()` zeroes the removed element | The element's inline bytes stay in the slack until the next append overwrites them | CppCon 2024 tuned layout read/write 7.6%; SLG UP.apply 1.9% |
| `XOFFSET_SCRUB_ERASED` | M4: `clear()`, shrinking `resize()`, `erase` and assigning a shorter value zero the vacated bytes | Old elements and characters stay in the slack | CppCon tuned layout read/write about 4% more; SLG LD.apply 3.8%, LDW.recv 3.4%; native delta bytes NB.delta 9.9% smaller (zeroing is also a change and has to be sent with the delta) |
| `XOFFSET_SCRUB_PADDING` | M6: implicit padding of container elements the library builds or moves is zeroed | Padding carries bytes from the value it was built from (usually from the stack); wires with the same content may differ | SLG A.apply 1.6%, SD 1.3% |
| `XOFFSET_PIN_QUARANTINE` | Blocks reclaimed inside an `edit()` / `read()` scope are reused only when the scope ends | Blocks can be reused right away. An already invalidated reference in the scope (obtained before a container grew or erased) may read other data written later; the invalidation rules themselves are the same as `std::vector` | CppCon 2024 layout read/write 12.6% (about 18% in time); every SLG cell under 0.5%. The other side: with immediate reuse the buffer is smaller but holes can be more fragmented. When the CppCon 2024 character data is filled in one `edit()`, the wire goes from 3232 bytes with 1 hole to 3112 bytes with 2 holes, and one send/receive round takes 123 more instructions (9%) |
| `XOFFSET_SAVE_SELF_CHECK` | Before every save of a small message (under 4 KiB), re-check the arena header's magic and size, and that the output `std::string` does not overlap the buffer's own storage | Both are sender self-checks: passing the buffer's own storage as the output is undefined behavior; a corrupted arena header is not reported. Larger messages take the generic path, where both checks still run | SLG UP.send 5.7%, UC.send 1.8% |
| `XOFFSET_SCRUB_RESET` | `reset()` zeroes all used bytes; a reset buffer is byte for byte the same as a new one | `reset()` does not zero. The previous content stays past the new `used_end`, and the slack and padding of the new content's blocks carry it; a send buffer that builds messages for different receivers in turn carries bytes of the previous message into the next (P4 in 3.2) | SLG UP.send 4.1%, UC.send 2.6%, EB.send 2.4%, SD 2.2%, LD.send 2.0%; one `reset` alone (`reloc.reset`) 43%. Swapping or moving buffers carries one more field: 9 more instructions per `std::swap`, 3 more per move |

Numbers are callgrind instruction counts on P2996 Clang x86-64 (2026-11-16 and 2026-11-17,
`relax-survey-20261116/`).

**One item that is on by default: the client uplink check `XOFFSET_CHECK_UPLINK` (default 1;
`XOFFSET_HARDENED` does not change it).** `XReceiver<XLink::from_client>` checks by default: `view()`
uses the bounds tier, `reload()` / `load()` / `apply_delta()` the full tier, and `XCheck::admit` on
this link does not compile. Built with `-DXOFFSET_CHECK_UPLINK=0`, the client link defaults to the
trusted tier like the trusted links and checks only the wire header and that the root lies in the
payload; **any client can then send one crafted message that makes the server read out of bounds
(`view`) or write out of bounds (modifying the result of `reload`), crash, or read and change server
memory**. Calls that name `XCheck::bounds` / `XCheck::full` are not affected. It only suits deployments
whose clients are their own code over a protected transport. It saves little: receiving an SLG uplink
order (UL.recv) takes 9.6% fewer instructions, about 46 per order, so this check stays on by default.

Not switches and unchanged:

- Zeroing blocks on free (M3): deltas generated from touch marks rely on "holes are 0", see 3.2.
  Turning M3 off was measured to save only 1.5% of LD.apply and 1.3% of LDW.recv instructions, and delta
  sync breaks.
- Trusted links still check only the wire header; any link can name `XCheck::bounds` or `XCheck::full`
  on a call.
- The M1 default limit, argument checks, `XOFFSET_FORCE_FULL_CHECK` (default 1 in debug and sanitizer
  builds, where the trusted tier runs full validation anyway).

When sending to receivers that must not see deleted data, either build with `XOFFSET_HARDENED=1` (or
its four zeroing switches `XOFFSET_SCRUB_ERASED`, `XOFFSET_SCRUB_POPPED`, `XOFFSET_SCRUB_PADDING`,
`XOFFSET_SCRUB_RESET`), or send on that path with `save_compact` and give the sent types no implicit
padding (`XOFFSET_REQUIRE_NO_PADDING(T)` or `XOFFSET_REJECT_IMPLICIT_PADDING`).

## 5. Continuous fuzzing

Status (since 2026-11-04): the nightly long runs stopped on 2026-10-28 and are replaced by a full
**pre-release** round of `tools/fuzz_prerelease.sh` ([RELEASING.md](RELEASING.md) section 3), covering
every entry point of section 2, and a release ships only with no findings. Entry points and harnesses:

| Entry point | Harness (ASan+UBSan) |
| --- | --- |
| `load_verified` (with and without `upstream`), `TypedXBuffer<T>::load`, `try_load_verified` | `fuzz_v2_wire`, `fuzz_wire_schemas` (libFuzzer); `test_v2_fuzz` (GCC 16 differential, with an independent oracle) |
| `reload_verified`, `begin_verified_reload`, movable slot storage | Same as above (the reload, direct and slot columns, compared byte for byte with loading into a new buffer) |
| Schema evolution: `evolve_load_verified`, `accept_schema_hello`, `read_schema_ack` | `fuzz_schema_evolution` |
| Delta apply: `try_apply_delta_verified` / `apply_delta_verified` (3.3) | `fuzz_delta_sync` (both reflection and field-list builds): the fast paths (none, in_place) are compared with full validation on a cloned copy for verdict, error and result; on rejection the copy and cursor are unchanged; accepted results can be validated and loaded again |
| Trusted tier (3.4): `reload`, `load`, `apply_delta` (admit) of `XReceiver<server_peer / from_server>` | Whole buffer: column 6 of `v2_fuzz_core.hpp` (`fuzz_v2_wire`, `test_v2_fuzz`): what the full tier accepts must be accepted with byte-identical results, and what the oracle rejects at the header stage must be rejected. Deltas: `fuzz_delta_sync` runs admit on another clone: what the full tier accepts must be accepted with the same result, parse-stage errors must be the same, and on rejection the copy and cursor are unchanged. Inputs admit additionally accepts are not read |
| Shared deltas (3.5): `XReceiver<server_peer / from_server>::apply_sync` (XS 2.0 since 4.0, full and admit) | `fuzz_shared_delta` (both reflection and field-list builds): four kinds of input, raw root records and operation sections, real message streams with tampering, whole messages, and key messages; each first goes through full, which on rejection leaves records and cursor unchanged and whose accepted results can be validated and loaded again; what full accepts then goes through admit on a copy, and records and cursor must be the same. Admit does not read inputs full rejects (the message body trusts the sender) |
| Add-only type evolution: one wire, image delta or shared delta body read by several versions of one family (`XOFFSET_SCHEMA_FAMILY`, [SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md) sections 1 to 4) | `fuzz_schema_family` (both reflection and field-list builds), the in-place pair: three versions of one family get every input; a later version accepts nothing an earlier one rejects (full and admit tiers, shared delta change and whole bodies), versions that accept a wire re-save the same bytes, an edit by the earliest version keeps every byte of the later ones, and versions that accept a body end it at the same place and hold the same bytes after applying it |
| Bounds tier (3.5): `XReceiver<from_client>::view` (bounds) | Column 9 of `v2_fuzz_core.hpp` (`fuzz_v2_wire`, `test_v2_fuzz`): the wire is copied into a heap block of exactly its size; dense wires the full tier accepts must be accepted and every field read must be the same; sparse wires and wires the oracle rejects at the header stage must be rejected; inputs additionally accepted are read completely through the view and must stay in bounds (ASan). `test_receiver_view`: crafted out-of-bounds blocks, overlong inline strings, missing NUL, bool 2, overlap amplification and random mutation |
| Client field-list build, no-exception build | The C++20 field-list builds of `fuzz_v2_wire` and `fuzz_wire_schemas` (libFuzzer), and `tools/fuzz_outcome_diff.sh` (compares results and error text one by one across three builds) |

Stages and budgets are described at the top of the script; one release fuzz round takes about 4.5 hours
(GCC 45 minutes, 20 minutes per libFuzzer target per build). Earlier records follow.

Status (0.3.0-beta.1): F1–F3 were implemented and F4's short PR runs ran in CI; the daily long-run
workflow was written, but GitHub scheduled jobs only trigger on the default branch, so it did not run,
and a 4.8-million-case GCC 16 ASan+UBSan differential fuzz run before release stood in for it.

Status (beta.2, since 2026-10-23): the project no longer uses GitHub Actions; `fuzz.yml` and `ci.yml`
are kept but do not run. The daily long runs moved to a cloud Docker host running
`tools/fuzz_nightly.sh` (every day at 02:23 Beijing time):

- GCC 16 ASan+UBSan differential fuzzing: `test_v2_fuzz` with 4 processes, seeds rotating by date
  (`YYYYMMDD×10+i`), case count calibrated to about 45 minutes; on failure it writes the wire and a
  `--replay` command.
- GCC 16 ASan+UBSan replay: `fuzz_<target>_replay` replays `tests/fuzz/regressions/` and the seed corpora
  of both harnesses.
- When a crash is found, `tools/fuzz_report.py` opens a GitHub issue per signature (label `fuzz-crash`;
  the same signature only adds a comment, and a closed one is reopened and marked as a regression), and
  pushes the input to a `fuzz-crash/<signature>` branch as
  `tests/fuzz/regressions/<target>/<signature>`. The fix PR carries this input and the fix closes the
  issue. Runs that fail without a finding are recorded on a single `fuzz-infra` issue. Results are also
  kept in the fuzz archive.
- libFuzzer coverage-guided fuzzing (F1, F2, since 2026-10-27): the same night `tools/fuzz_libfuzzer.sh`
  runs next, building `fuzz_v2_wire` and `fuzz_wire_schemas` (ASan+UBSan) natively on the host with the
  P2996 toolchain built in the cloud (clang-p2996 d77eff1, with compiler-rt's libFuzzer runtime), with
  the same steps as the libfuzzer job of `fuzz.yml`: first replay the existing corpus and seeds in a
  single process, then run `-fork=4` for about 20 minutes, then merge the corpus and reproduce and
  minimize findings. The corpus is kept across nights as a persistent tarball; paths in the
  debug info are mapped to `/workspace`, and signatures match the Docker job. The first night found
  #130 (the harness was too strict about inputs shorter than the header; fixed, and the input went into
  the regression directory).
- Client builds (since 2026-10-28): the same night runs libFuzzer on the C++20 field-list build
  (`v2_wire`, `wire_schemas`), then `tools/fuzz_outcome_diff.sh` replays the same corpus in the
  reflection, field-list and no-exception builds and compares each entry point's result and error text
  one by one.
- Remaining gap: no short PR runs (the merge evidence for PRs is `tools/local_validate.sh`).

- **F1 coverage-guided fuzzing**: libFuzzer harnesses (`LLVMFuzzerTestOneInput`) with entry points
  `load_verified`, built with P2996 Clang and the
  compiler-rt already pinned in CI; seeds are the cross-compiler wire corpus.
- **F2 multiple schemas**: besides the existing differential fuzz schema, add the schemas of the
  cross-compiler corpus (enum keys, fixed-size arrays, floating point, bool, nested vectors).
- **F3 resource assertions**: the harness sets `maximum_capacity` and asserts that each case's
  reservation and peak memory stay under a fixed multiple of the limit, turning R1/R2 into regression
  checks.
- **F4 how it runs**: a short run (for example 5 minutes) on every PR and a long daily scheduled run;
  crashing inputs are added to the regression corpus automatically.

## 6. How to reproduce

Measurement environment: cloud x86-64 container (Intel Xeon 2.1 GHz, 4 cores), GCC 16.2.0, `-O2`,
single runs, not a formal performance measurement. Method: take a small valid wire, enlarge `used_bytes`
and the payload header's `used_end`, add a hole covering the new range (R1) or tile it with many small
holes (R2), make it a single-range sparse wire, call `TypedXBuffer<T>::load`, and record the time,
`reserved_bytes()` and the RSS from `/proc/self/status`. For T5: save after replacing a string or
migrating a vector, and search the wire for the old content.
