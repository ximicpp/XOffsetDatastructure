# XOffsetDatastructure Image and Wire Format

This document describes wire format 3.0. A buffer's memory and its wire are
the same bytes: the image. A dense wire is the image as it is; a sparse wire
leaves out ranges that load as zero. There is no hole table, no payload header
written by the receiver, and no host-side copy of the allocator state.

Readers reject every other format. Format 2.1 (4.1 and earlier) gives
`wire format 2.1 is not supported; this build reads 3.0 (sender and receiver
must upgrade together)`. Section 6 lists what changed.

## 1. Image

The image is one storage region (a slot or a VM reservation). It starts with a
40-byte header (`XBufferCore::arena_header`, the same layout as `XWireHeader`):

| offset | field | type | value |
| ---: | --- | --- | --- |
| 0 | `magic` | u32 | `0x00034F58`: bytes `'X' 'O' 3 0` (format 3.0) |
| 4 | `header_bytes` | u32 | 40 |
| 8 | `schema_hash` | u64 | the root type's schema hash |
| 16 | `root_type_id` | u64 | the root type's id |
| 24 | `used_bytes` | u32 | bytes in use, header included; a multiple of 8 |
| 28 | `extent_count` | u32 | 0 in memory and on a dense wire |
| 32 | `free_dir` | u32 | offset of the free directory, 0 for none |
| 36 | `free_bytes` | u32 | bytes in free blocks (the directory block excluded) |

All fields are little-endian. `make<T>()` writes T's fingerprint (`schema_hash`,
`root_type_id`); a load, a clone or a key delta brings the sender's.

**Root.** The root object is the first block, at offset 40. An image has a root
exactly when `used_bytes > 40`. Root types are aligned to at most 8.

**Blocks have no headers.** Every block is 8-byte aligned and its length is a
multiple of 8. A block's extent is derived from its owner:

- string: `round8(capacity_ + 1)`
- vector/set/map: `round8(capacity_ * sizeof(T))`
- root: `round8(sizeof(T))`

The allocator never hands out a block larger than requested; when it reuses a
larger free block it splits it, and the remainder stays free. So the extent
derived from a head is always the exact block.

**Exact coverage.** The header, every live block reachable from the root, the
free directory block and the free blocks together tile `[0, used_bytes)`
without overlap.

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

**Owner by address.** A head does not name the image it belongs to; that is
the image it lies in. `detail::OwnerMap` keeps one byte per 4 MiB granule of
the 48-bit address space: 0 for no XOffset region, otherwise bit 7 is set and
bits 0..5 give k, the region's alignment exponent. Every region (a slot, or a
VM mapping) starts at a 2^k boundary with the 64-byte host header, and its
image of at most 2^k bytes follows, so the image base of an image address
is `((address - 64) & ~(2^k - 1)) + 64`. Slot chunks register once; a VM
mapping is reserved as whole 4 MiB granules aligned to 2^k, k from its maximum
capacity (a 2^k maximum maps one more granule for its last 64 bytes and page
rounding), and sets its entries while it is mapped (a pooled mapping keeps
them). On Windows the table is committed at the first registration.

Lookups follow five rules:

1. Only to allocate, grow or retire a block; reads and writes of existing
   elements never look up.
2. Never in relocation or SHIFT (elements move as bytes).
3. The same-image check of a move is one table read when both heads lie in
   one granule.
4. One lookup per operation, passed down to the regrow paths.
5. A container that destroys its elements passes its owner down, so element
   heads, and the heads nested in aggregate elements, retire their blocks
   without a lookup each.

A head outside every image (a stack temporary) holds empty and inline values;
an operation on it that needs a block throws. A head constructed in place in an
image is bound by its address, so a move into it takes the source block.

## 3. Wire

```
dense:   image bytes [0, used_bytes)                        (extent_count 0)
sparse:  header (40 bytes, extent_count n > 0)
         extent table: n x {u32 offset, u32 bytes}
         the extents' bytes, concatenated
```

A dense wire is exactly `used_bytes` long. A sparse wire is `40 + 8n +` the
extents' sum. Every offset on the wire is an image offset.

**Sparse wire.** The extents are ascending, disjoint and 8-aligned, each a
multiple of 8 bytes, and the first starts at 40. Gaps (between extents and
after the last one) load as zero bytes; the receiver writes the header with
`extent_count` 0. The loaded image is then checked exactly like a dense one.

**Versions.** A reader reads the first four bytes before anything else: a
buffer shorter than 4 bytes or not starting with `XO` is `not an XOffset wire
(bad magic)`; `XOFFV1` and `XOFFV2` (the 2.0 header) are named as formats 1
and 2.0; any other version bytes M.m give `wire format M.m is not supported;
this build reads 3.0`. A `header_bytes` other than 40 is `unsupported header
size`. Errors are prefixed with the calling API's name.

**Emission.** `XSaveOptions{.emission}` selects the form; `save_verified` and
`save_verified_into` emit the same bytes for the same state.

- `free_runs` (default): dense unless `free_bytes` is at least 4 KiB (one
  header read). Otherwise free blocks of at least 256 B are sorted by offset,
  adjacent ones merge, and in every merged run of at least 4 KiB each block's
  body (all of it but its first word, the link) is skipped. A free block's
  body is zero, so the loaded image is the sender's.
- `dense`: the image.
- `zero_runs`: zero runs of at least 64 bytes (capacity slack, free blocks,
  zero fields) are skipped; a run is found from a 64-byte aligned all-zero
  block (aligned in image offsets) and extended word by word on both sides,
  and it never starts before offset 48, so the first extent keeps at least one
  word after the header. Images below 4 KiB, and emissions that would save
  less than max(256 B, used_bytes / 16), stay dense.

The loaded image is byte-identical in every form, so spare capacity and the
free blocks survive the transfer.

`save_compact` emits a compact wire directly: the root at offset 40, then
every block in validation (preorder) order, strings of at most 10 bytes
inline, empty containers without storage, no free blocks and no extents. With
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

The free blocks and their directory live in the image; nothing about them is
kept in host memory.

- **Free block.** `{u32 next, u32 0}` then zero bytes: each size is a LIFO
  stack linked through its blocks' first words (0 ends it).
- **Directory.** `{u32 n, u32 cap}` then `cap` entries `{u32 size, u32 head}`:
  the `n` used entries with ascending sizes (multiples of 8), then unused
  entries `{0xFFFFFFFF, 0}` (`n < cap`, so a scan needs no bound). A size whose
  stack is empty may keep its entry with head 0. The scan compares four
  entries a step; a binary search narrows directories of more than 16 sizes.
- **First directory.** The first retired block of at least 40 bytes hosts it
  at its end; otherwise a 72-byte directory (8 entries) is bumped at
  `used_bytes`. A full directory is compacted when at most half of its entries
  hold a block, else replaced by one of twice the entries; the old block is
  freed.
- **Allocation order:** the exact size's head; else the first larger size
  with a block is split (requests aligned to at most 8 bytes); else bump at
  `used_bytes` within the current extent; else grow and bump.
- **Retiring** a block zeroes it, pushes it on its size's stack and never
  lowers `used_bytes`. Callers pass the block's derived size.
- **Growing in place** only works for the tail block.
- **Lost blocks.** When no directory can be placed (the image is at its
  maximum), a retired block is not recorded and the free state is marked
  incomplete; `save_verified` then recomputes the free blocks from the live
  graph before it sends.

**Owner lookup.** A head's image is found from its address (section 2); the
current `XBufferCore*` is stored in the last word of a 64-byte host header just
before the image (a slot's header, or the first 64 bytes of a VM region, on
the image's first page), so allocation and retirement need no registry or
thread-local lookup. The host header is never part of the image.

## 5. Validation

A receiver chooses a tier (`XReceiver`, docs/USER_GUIDE.md section 8):

- **admit** (trusted links): the wire header (magic and version, header size,
  fingerprint, `used_bytes` at least 48 and at most the format's limit, a
  multiple of 8), the extent table, the total length, and that the image holds
  the root. Everything else, the free directory included, is the sender's
  promise. A dense image of the same type received in place is one compare of
  the header's first 32 bytes and one copy.
- **bounds** (read-only views of client uplinks): the admit checks and a walk
  that keeps every read in bounds. A view never allocates, so it does not read
  the free directory.
- **full** (`load_verified`, `reload_verified`, `XCheck::full`): the admit
  checks, then:
  - **Free directory:** 8-aligned, at or after offset 40, inside the image;
    `n < cap`; sizes strictly ascending multiples of 8; unused entries
    `{0xFFFFFFFF, 0}`. Every stack's blocks are 8-aligned, at or after offset
    40, inside the image, with second word 0; the stacks hold at most
    `used_bytes / 8` blocks and their sizes add up to `free_bytes`. The
    directory block and the free blocks are registered as free ranges.
  - **Graph walk:** a walk from the root in member order. Each binding must be
    exact. Every extent derived from a head must be in bounds and 8-aligned.
    Live blocks may not overlap each other or a free range.
  - **Strings:** heap strings must be NUL-terminated. Inline strings must have
    a length of at most 10 and zero padding.
  - **Other values:** `bool` must be 0 or 1; sets and maps must be strictly
    ordered.
  - **Coverage:** header, live and free bytes must equal `used_bytes`.

A redirected pointer leaves its original block uncovered or overlaps another
block, and the coverage check rejects it; unreachable dead regions are
rejected too.

## 6. Changes in format 3.0

Format 3.0 is a breaking change: 3.0 readers reject 2.1 wires and 2.1 readers
reject 3.0 wires, so senders and receivers upgrade together.

**One header.** The 48-byte payload header (`XBUFV2`, header size, flags, root
offset, `used_end`, two reserved words) and the 40-byte `XWireHeaderV2` are
replaced by the 40-byte header of section 1, which is both. The root offset is
no longer stored: the root is at 40. The flags word is gone; offset 28 holds
the extent count. Removed: `XWireHeaderV2`, `XWIRE_FLAGS`,
`XBufferCore::write_payload_header`. `XWIRE_PAYLOAD_BEGIN` is 40.

**No hole table.** The wire no longer ends with `hole_count x {offset, bytes}`:
the free blocks and the directory are in the image, so a receiver copies them
with everything else. `hole_count` is gone from the header.

**Free runs.** Emission skips the bodies of the free blocks in a run (their
first word, the link, is sent) instead of whole blocks.

**Direct reload.** `begin_verified_reload<T>(used_bytes, extent_count)`:
`image_bytes()` spans the image `[0, used_bytes)` (a dense wire goes there
whole, header included) and `extent_bytes()` holds a sparse wire's extent
table; `complete(image_bytes, extent_bytes)` takes the counts written.
