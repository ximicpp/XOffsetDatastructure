# XOffsetDatastructure V2 Storage and Wire Format

V2 replaces the V1 fixed-schema storage format. It is not backward compatible:
readers reject V1 input with `wire format 1 is not supported`, and there is no
V1 reader or migration path in the library.

This document describes wire format 2.1 (after 3.0.0): 12-byte container heads
that name no owner, a 40-byte wire header, and no payload header on the wire.
Format 2.0 (3.0.0 and earlier) is rejected with `wire format 2.0 is not
supported; this build reads 2.1 (sender and receiver must upgrade together)`.
Section 7 lists what changed.

## 1. Payload

The payload is one VM reservation. It starts with a 48-byte header
(`XBufferCore::arena_header`):

| offset | field | value |
| ---: | --- | --- |
| 0 | `magic[8]` | `XBUFV2\0\0` |
| 8 | `header_size` | 48 |
| 12 | `flags` | 0 |
| 16 | `root_offset` | payload offset of the root object |
| 24 | `used_end` | bytes in use |
| 32 | `reserved0` | 0 |
| 40 | `reserved1` | 0 |

**Blocks have no headers.** Every block is 8-byte aligned and its length is a
multiple of 8. A block's extent is derived from its owner:

- string: `round8(capacity_ + 1)`
- vector/set/map: `round8(capacity_ * sizeof(T))`
- root: `round8(sizeof(T))`

The allocator never hands out a block larger than requested; when it reuses a
larger free block it splits it, and the remainder stays free. So the extent
derived from a head is always the exact block.

**Exact coverage.** The header, every live block reachable from the root, and the
holes (free blocks) together tile `[0, used_end)` without overlap.

The payload header is not sent: a receiver writes it from the wire header's
`used_bytes` and `root_offset` (`XBufferCore::write_payload_header`) before it
validates the payload, so every payload offset below means the same thing on
both sides.

## 2. Container heads

Every head is 12 bytes with 4-byte alignment: `data_delta` (i32), `size_`
(u32), `capacity_` (u32). `data_delta` is a rel32 from the head to its block.
(In C++ a vector's `size_` and `capacity_` are `char32_t`: 32-bit unsigned like
a string's, but a type that int and unsigned element stores cannot alias, so a
`push_back` loop keeps `capacity_` in a register. The bytes are the same.)

**Inline strings.** A string of at most 10 bytes may be stored in head bytes
0..10 instead of a block:

- Bit 31 of `capacity_` (head byte 11) marks the inline form. A heap capacity is
  below 2^31, so it never sets this bit.
- Bits 24..30 hold the length.
- The byte after the last character and every unused inline byte are 0.

**Owner by address.** A head does not name the payload it belongs to; that is
the payload it lies in. `detail::OwnerMap` keeps one byte per 4 MiB granule of
the 48-bit address space: 0 for no XOffset region, otherwise bits 0..5 give k,
the region's alignment exponent (the payload base is `address & ~(2^k - 1)`),
and bit 7 marks a slot, whose payload starts 64 bytes in. Slot chunks register
once; a VM payload is reserved as whole 4 MiB granules aligned to 2^k and sets
its entries while it is mapped (a pooled mapping keeps them). On Windows the
table is committed at the first registration.

Lookups follow five rules:

1. Only to allocate, grow or retire a block; reads and writes of existing
   elements never look up.
2. Never in relocation or SHIFT (elements move as bytes).
3. The same-payload check of a move is one table read when both heads lie in
   one granule.
4. One lookup per operation, passed down to the regrow paths.
5. A container that destroys its elements passes its owner down, so element
   heads, and the heads nested in aggregate elements, retire their blocks
   without a lookup each.

A head outside every payload (a stack temporary) holds empty and inline values;
an operation on it that needs a block throws. A head constructed in place in a
payload is bound by its address, so a move into it takes the source block.

## 3. Wire

```
XWireHeaderV2 (40 bytes)
[extent table: extent_count x {u32 offset, u32 bytes}]   (sparse only)
payload bytes [48, used_bytes) (dense) or concatenated extents (sparse)
hole table: hole_count x {u32 offset, u32 bytes}
```

A wire is exactly `40 + 8 * extent_count + stored payload bytes + 8 *
hole_count` bytes long; stored payload bytes are `used_bytes - 48` for a dense
wire and the extents' sum for a sparse one. Every offset on the wire (root,
extents, holes) is a payload offset, counted from the start of the payload
header the receiver writes.

**Header fields** (little-endian, aligned to 8):

| offset | field | type | value |
| ---: | --- | --- | --- |
| 0 | `magic` | u32 | `0x01024F58`: bytes `'X' 'O' 2 1` (format 2.1) |
| 4 | `flags` | u32 | 0; bit 31 is reserved for schema evolution |
| 8 | `used_bytes` | u32 | payload `used_end`, payload header included; a multiple of 8, at least 48 |
| 12 | `root_offset` | u32 | in `[48, used_bytes)` |
| 16 | `hole_count` | u32 | |
| 20 | `extent_count` | u32 | 0 for a dense wire |
| 24 | `schema_hash` | u64 | |
| 32 | `root_type_id` | u64 | |

**Versions.** A reader reads the first four bytes before anything else: a
buffer shorter than 4 bytes or not starting with `XO` is `not an XOffset wire
(bad magic)`; `XOFFV1` and `XOFFV2` (the 2.0 header) are named as formats 1
and 2.0; any other version bytes M.m give `wire format M.m is not supported;
this build reads 2.1`. Errors are prefixed with the calling API's name.

**Sparse wire.** The extents and their gaps tile `[48, used_bytes)`, and the
first extent starts at 48. Gaps load as zero bytes wherever they lie; the loaded
payload is then validated exactly like a dense one, so a sparse wire is
accepted exactly when the dense wire of the same payload is, with the same
error. (Before 2026-10-22 every gap byte also had to lie inside a hole; that
encoding rule is dropped, the payload-level accept/reject set is unchanged.)

**Emission.** `XSaveOptions{.emission}` selects the form; `save_verified` and
`save_verified_into` emit the same bytes for the same state.

- `free_runs` (default): free blocks of at least 256 B are sorted by offset,
  adjacent ones merge, and every merged run of at least 4 KiB is skipped.
  Skipped bytes are all holes. (Earlier releases, N10, merged free blocks of
  every size; on runs without small neighbours the two agree.)
- `dense`: one copy of the payload.
- `zero_runs`: zero runs of at least 64 bytes (capacity slack, holes, zero
  fields) are skipped; a run is found from a 64-byte aligned all-zero block
  (aligned in payload offsets) and extended word by word on both sides, and it
  never starts before offset 56, so the first extent keeps at least one word.
  Payloads below 4 KiB, and emissions that would save less than max(256 B,
  used_bytes / 16), stay dense.

The loaded payload is byte-identical in every form, so spare capacity survives
the transfer. `freeze_verified` always emits dense wires; its segments are the
40-byte header, the payload from offset 48, and the hole table.

`save_compact` emits a compact wire directly: the root at offset 48 (the first
payload byte on the wire), then
every block in validation (preorder) order, strings of at most 10 bytes
inline, empty containers without storage, no holes and no extents. With
`XCompactOptions{.slack_div = 0}` every container has capacity equal to size
and the bytes equal `XCompactor::compact` followed by `save_verified`. The
default (`slack_div = 8`) gives vectors of at least 8 elements (not strings)
size / 8 + 1 zeroed spare elements. Either is an ordinary dense wire and loads
with no special handling.

**Output.** On Darwin, outputs of at least 1 MiB are copied in 256 KiB chunks.
Each chunk is preceded by `madvise(MADV_WILLNEED)` on its destination pages.
On Linux, in an output of at least 1 MiB, a copy of at least 256 KiB into
pages that are not yet resident (a new output, not a reused string) goes in
256 KiB chunks, each preceded by `madvise(MADV_POPULATE_WRITE)`. A kernel
without it (before 5.14) answers `EINVAL` once, and outputs are then copied
plainly. The wire bytes are the same either way.

## 4. Allocator

The free table lives in host memory, inside `XBufferCore`; it is never stored in
the payload.

- **Small blocks.** Exact-size LIFO bins cover blocks up to 4096 bytes.
- **Large blocks.** Larger blocks go into an ordered best-fit map.
- **Allocation order:**
  1. reuse an exact-size block;
  2. split the smallest larger free block (requests aligned to at most 8 bytes);
  3. bump the tail within the current extent;
  4. grow and then bump.
- **Over-aligned requests** align the block's address. Any alignment padding is
  recorded as a hole.
- **Retiring** a block records it as a hole and never lowers `used_end`.
  Callers pass the block's derived size.
- **Growing in place** only works for the tail block.
- **Saving.** `save_verified` writes the free table as the wire hole table.
- **Loading.** `load_verified` loads the hole table back into the free table, so
  free space survives reloads and reloads never scan.
- **Host OOM.** If the host runs out of memory while recording a hole, the table
  is marked incomplete. `save_verified` then recomputes the holes from the live
  graph.
- **Unverified loads** start with an incomplete table.
- **Allocations that may run `new_handler`** are prepared first. The payload's
  owner is then re-resolved through its registry record before the change is
  published.

**Owner lookup.** A head's payload is found from its address (section 2); the
current `XBufferCore*` is stored in a host page placed just before the payload,
so allocation and retirement need no registry or thread-local lookup. That page
is never part of the payload.

## 5. Validation

The typed validator, the raw validator and the receive path apply the same checks:

- **Wire header**, in this order: magic and version, length (at least 40
  bytes), flags, fingerprint (root type id, then schema hash), `used_bytes`
  (at least 48, a multiple of 8, at most the format's payload limit) and
  `root_offset`, the extent table, the total length, then the holes and the
  payload walk.
- **Payload header:** the receiver writes it, so a wire cannot carry a bad one.
  The raw validator, whose caller fills the payload itself, still checks magic,
  header size, flags, reserved fields, and that `used_end` equals `used_bytes`.
- **Holes:** every hole entry is 8-aligned, a multiple of 8, at least 8 bytes,
  starts at or after offset 48, and fits in the payload. Holes are registered as
  free ranges.
- **Graph walk:** a walk from the root in member order. Each binding must be
  exact. Every extent derived from a head must be in bounds and 8-aligned. Live
  blocks may not overlap each other or a hole.
- **Strings:** heap strings must be NUL-terminated. Inline strings must have a
  length of at most 10 and zero padding.
- **Other values:** `bool` must be 0 or 1; sets and maps must be strictly ordered.
- **Coverage:** live bytes plus hole bytes must equal `used_end`.
- **Sparse wires:** gap bytes are zero in the loaded payload; there is no
  separate gap check.

These checks are not weaker than V1's. V1 detected a corrupted block pointer
through the per-block header self-check. In V2, a redirected pointer leaves its
original block uncovered or overlaps another block, and the coverage check
rejects it. V2 also rejects unreachable dead regions, which V1 accepted.

## 6. API changes from V1

**Removed:**

- `XAllocatorStateV1`
- `XBuffer::allocator_state()`
- containers' `XAllocatorStateV1*` constructors, `allocator_state()`, and
  `stored_allocator_proxy::state`
- `XBufferCore::AllocationHeader`
- `arena_header::allocator_offset`
- `find_allocator_storage` and `construct_allocator_storage`
- `fixed_detail::release_cached_blocks` and `FreeBlockHeader`
- the FreelistIndex diagnostics
- `XWireHeaderV1`, `XWIRE_*_V1` and `wire_magic_matches_v1` (readers recognize
  V1 input by its first bytes and reject it)
- `RawActivationStats::{allocation_headers, allocator_states, free_block_headers}`

**Changed:**

- `fixed_detail::allocator_ref` is now `{XArena* segment}`.
- `try_expand_dynamic_bytes(alloc, ptr, old_bytes, new_bytes, align)`
- `XArena::deallocate(ptr, bytes)`
- `XArena::try_expand(ptr, old_bytes, new_bytes)`
- `XArenaAllocator::deallocate` now uses its `n` argument.
- `receive_raw::validate(..., holes, hole_count, scratch, gaps, gap_count)`
- `begin_verified_receive<T>(payload, cap, holes, extents)`
- `Operation::complete(header, payload, holes, extents)`
- `VerifiedSnapshot::segments()` now returns 3 spans: header, payload, holes.
- Arena memory that is not reachable from the root must be released before a
  verified save, because exact coverage would otherwise reject it.

**Added:**

- `XBufferCore::{allocate_block, retire_block, try_expand_block, free_block_count, free_block_bytes, free_table_complete, for_each_free_block, write_free_blocks, load_free_blocks, replace_free_blocks, owner_from_arena}`
- `XBufferCore::WireHole`
- `XWireHeaderV2`
- `XFixedVector::assign(first, last)`
- `XFixedString::{is_inline, owned_extent}`
- `Operation::{hole_bytes, extent_bytes}`

**Move ownership checks** (`XOFFSET_MOVE_OWNERSHIP_CHECKS`): the check that
excludes moving a container into or out of its own subobject runs only in debug
builds by default.

## 7. Changes in format 2.1

Format 2.1 is a breaking change: 2.1 readers reject 2.0 wires and 2.0
readers reject 2.1 wires, so senders and receivers upgrade together. Every
message is 104 bytes shorter (56 bytes of wire header, the 48-byte payload
header), and payloads are smaller where they hold containers (4 bytes per
head).

**Wire header.** `XWireHeaderV2` is the 40-byte layout of section 3. Removed
fields: `header_size`, `format_major`, `format_minor`, `endian_tag`, `crc32c`,
`reserved0`, `schema_hash_hi` and `hole_bytes` (the exact-coverage walk already
accounts for every hole byte); `schema_hash_lo` is now `schema_hash`. Constants:
`XWIRE_MAGIC`, `XWIRE_FORMAT_MAJOR` (2), `XWIRE_FORMAT_MINOR` (1),
`XWIRE_FLAGS` (0), `XWIRE_PAYLOAD_BEGIN` (48). Removed: `XWIRE_MAGIC_V1`,
`XWIRE_MAGIC_V2`, `XWIRE_FORMAT_MAJOR_V2`, `XWIRE_FORMAT_MINOR_V2`,
`XWIRE_ENDIAN_TAG_V2`, `XWIRE_FLAGS_V2`, `wire_magic_matches_v2`.

**Heads.** 12 bytes without `arena_delta` (section 2). Removed:
containers' `bind()`, `bind_like()` and `field_binding()`.

**Direct receive and reload.** `begin_verified_receive<T>(used_bytes, ...)` and
`begin_verified_reload<T>(used_bytes, ...)` still take `used_bytes`, but
`payload_bytes()` now spans payload `[48, used_bytes)`: a dense wire's payload
bytes go there whole, and a sparse extent at payload offset `o` lands at
`o - 48`. `complete()`'s payload count is therefore `used_bytes - 48`.
`VerifiedSnapshot::segments()[1]` likewise starts at payload offset 48.

**Added:** `XBufferCore::write_payload_header(base, root_offset, used_end)`
for callers of the raw validator that build a payload from a wire.

