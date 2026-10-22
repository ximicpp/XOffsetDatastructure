# XOffsetDatastructure V2 Storage and Wire Format

V2 replaces the V1 fixed-schema storage format. It is not backward compatible:
readers reject V1 input with `unsupported wire format version 1`, and there is no
V1 reader or migration path in the library.

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

## 2. Container heads

Every head is 16 bytes with 8-byte alignment: `arena_delta` (i32), `data_delta`
(i32), `size_` (u32), `capacity_` (u32).

- `arena_delta` is a rel32 to the payload base.
- `data_delta` is a rel32 to the block.

**Inline strings.** A string of at most 10 bytes may be stored in head bytes
4..14 instead of a block:

- Bit 31 of `capacity_` (head byte 15) marks the inline form. A heap capacity is
  below 2^31, so it never sets this bit.
- Bits 24..30 hold the length.
- The byte after the last character and every unused inline byte are 0.

## 3. Wire

```
XWireHeaderV2 (96 bytes)
[extent table: extent_count x {u32 offset, u32 bytes}]   (sparse only)
payload bytes (dense) or concatenated extents (sparse)
hole table: hole_count x {u32 offset, u32 bytes}
```

**Header fields**, in order:

| field | value |
| --- | --- |
| `magic` | `XOFFV2\0\0` |
| `format_major` | 2 |
| `format_minor` | 0 |
| `header_size` | 96 |
| `used_bytes` | |
| `root_offset` | |
| `hole_count` | |
| `hole_bytes` | |
| `root_type_id` | |
| `schema_hash_lo`, `schema_hash_hi` | |
| `flags` | 0 |
| `endian_tag` | 0x01020304 |
| `crc32c` | 0 |
| `reserved0` | 0 |
| `extent_count` | |

**Sparse wire.** The extents and their gaps tile `[0, used_bytes)`, and the
first extent starts at 0. Gaps load as zero bytes wherever they lie; the loaded
payload is then validated exactly like a dense one, so a sparse wire is
accepted exactly when the dense wire of the same payload is, with the same
error. (Before 2026-10-22 every gap byte also had to lie inside a hole; that
encoding rule is dropped, the payload-level accept/reject set is unchanged.)

**Emission.** `save_verified` and `save_verified_into` emit a dense wire (one
copy of the payload) by default. `XSaveOptions{.elide_zero_runs = true}` skips
zero runs of at least 64 bytes (capacity slack, holes, zero fields): a run is
found from a 64-byte aligned all-zero block and extended word by word on both
sides. Payloads below 4 KiB, and emissions that would save less than
max(256 B, used_bytes / 16), stay dense. The loaded payload is byte-identical
either way, so spare capacity survives the transfer. `freeze_verified` always
emits dense wires. (Earlier releases skipped free runs of at least 4 KiB by
default, N10; such wires still load.)

**Output.** On Darwin, outputs of at least 1 MiB are copied in 256 KiB chunks.
Each chunk is preceded by `madvise(MADV_WILLNEED)` on its destination pages.

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

**Owner lookup.** The current `XBufferCore*` is stored in a host page placed just
before the payload, so allocation and retirement need no registry or
thread-local lookup. That page is never part of the payload.

## 5. Validation

The typed validator, the raw validator and the receive path apply the same checks:

- **Wire header:** format version, sizes, schema, and the hole byte sum.
- **Payload header:** magic, header size, flags, reserved fields, and that
  `used_end` equals `used_bytes`.
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
- `XWireHeaderV1`, `XWIRE_*_V1` (only `XWIRE_MAGIC_V1` remains, so readers can
  reject V1 input), and `wire_magic_matches_v1`
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
