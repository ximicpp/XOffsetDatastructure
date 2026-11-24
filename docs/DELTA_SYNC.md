# Delta sync

A receiver that already holds a copy of a buffer (a client that has its
units or its player, a storage server that has a player) should not get the
whole buffer again when the sender changes a few fields. Delta sync sends only
the payload bytes that changed and patches the receiver's copy in place.
Nothing is encoded per field: a delta carries raw payload bytes at payload
offsets, which mean the same thing on every 64-bit little-endian target (the
wire rule of [WIRE_FORMAT.md](WIRE_FORMAT.md)). After a delta applies, the
receiver's payload is the sender's payload byte for byte.

Header: `xoffset_delta.hpp` (included by `xoffsetdatastructure.hpp`). Both schema backends (C++26 reflection,
C++20 `XOFFSET_RECORD` field lists) and builds without exceptions.

## 1. Use

Sender (one `XDeltaBaseline` per receiver, or per group of receivers that
always get the same deltas):

```cpp
XDeltaBaseline base;                     // what this client holds: nothing yet
std::string d;
buf.save_delta_into<View>(d, base);      // first call: a key delta (full state)
send(d);
// ... mutate buf in place (root<View>().units[i].hp -= 3, ...)
if (buf.save_delta_into<View>(d, base); !d.empty()) send(d);   // only what changed
// The client lost its state or rejected a delta:
base.reset();                            // next delta is a key delta again
```

Receiver:

```cpp
XBuffer view(4096, XBuffer::maximum_capacity(1 << 20));   // or a default XBuffer
XDeltaCursor cursor;                     // which state `view` holds
if (XStatus r = view.try_apply_delta_verified<View>(d, cursor); !r) {
    ask_sender_for_key_delta();          // r.code(), r.error().message say why
}
const View& v = view.root<View>();       // read in place
```

`TypedXBuffer<T>` has the same members without `<T>`. `apply_delta_verified`
throws what `try_apply_delta_verified` returns.

On a trusted link (server to server, server to client) the receiver takes
deltas through `XReceiver` (`xoffset_receiver.hpp`, section 3, "Trusted
links"), which checks only the delta's framing:

```cpp
XReceiver<XLink::from_server> rx(XBuffer::maximum_capacity(1 << 20));
if (XStatus r = rx.apply_delta<View>(view, d, cursor); !r) ask_sender_for_key_delta();
```

Several receivers of the same deltas (every subscriber of one map block) can
share one baseline: the sender writes each delta once and sends it to all of
them. A receiver that joins later starts from the recorded state:

```cpp
std::string k;
block_base.key_delta_into(k);            // key delta at the baseline's stream and version
send_to_new_subscriber(k);               // then the same deltas as everyone else
```

`key_delta_into` does not advance the baseline; the joiner's cursor then holds
the baseline's version and accepts its next delta. It needs a baseline that has
written at least one delta (`!empty()`).

### Touched changes only

By default `save_delta_into` compares the whole payload with the baseline
(64-byte blocks, SSE2 on x86-64). A sender that knows what it changed (a game
server marks dirty objects anyway) can say so and have only that compared:

```cpp
auto& p = buf.root<Player>();
p.armies[a].troops[t].count += 5;
buf.touch(p.armies[a].troops[t]);        // this object
p.items[k] += 1;                         // may insert and move entries
buf.touch(p.items);                      // the map: its head and its block
XDeltaOptions o;
o.touched_only = true;
buf.save_delta_into<Player>(d, base, o); // compares the touched blocks only
```

`touch(value)` marks the object's bytes and everything it reaches (the blocks
of its strings and containers, nested); `touch(p, bytes)` marks raw payload
bytes. A delta with `touched_only` compares the 64-byte blocks touched since
the baseline's previous delta from this buffer and the bytes past the
baseline's used size. The allocator's own writes are touches too (a block
taken from or given back to a free list, the free directory, the header's
free state), so a container that moved needs only its own touch: the block it
left goes with the allocator's. Touches go to a log in the buffer, so one
buffer can feed any number of baselines (a client, the storage process, a
relay). While one baseline pairs with the buffer, the log is a short list of
the byte ranges touched since its last delta (up to 25 runs, adjacent ones
merged; the baseline's lines of them are fetched into cache as they are
touched). When the list is full, a run that starts less than a block past
the end of the one touched before it is joined to it (the untouched bytes
between go with them); with a second baseline, or
more runs than that leaves room for, it also stamps each touched 64-byte
block with the delta it belongs to (four bytes per block), so a baseline that
skipped deltas still finds its blocks.
`touch_set(field, value)` assigns and touches only when the value differs.

The rule: **touch everything you change, after you change it** (a container
that moved is marked where it is now). `touch(container)` marks its new block;
a touch of its head alone (`touch(&v, sizeof v)`) does not cover a move, since
the new block may be a hole below the receiver's used size, where only touched
blocks are compared. A change that is not touched is not
sent; send with `digest = true` while developing to have the receiver catch
that (`digest_mismatch`). Everything is compared, as without the option, for a
baseline's first delta from a buffer (a key delta from a buffer that already
keeps touches counts as that first one: the next delta compares only what was
touched since it), after the buffer's content was replaced
(`reload_verified`, `reset`, `swap`, a key delta applied, storage that moved),
and after a touch outside the payload. A buffer that receives deltas and
forwards them (a relay) gets the blocks each applied delta wrote as touches.

### A baseline without a copy

`XDeltaBaseline(XDeltaRecord::touches)` keeps no copy of the payload: each
delta is a run frame (section 2) of the byte ranges touched since the
baseline's previous delta and the nonzero bytes past the receiver's used
size, compared with nothing. Memory per receiver drops from one copy of the
payload to a few words; the delta grows by the bytes that were touched but
did not change, so touch exactly what changes (`touch_set`, which touches
only the bytes of a scalar that changed, or `touch(p, bytes)` on the field).
Past the list's 25 runs in one period (after the joining above), while a
second baseline pairs with the buffer, or for a baseline that missed a period,
the touched 64-byte blocks go whole. Where the touches do not say what changed (the first delta, content
that was replaced, a baseline that missed deltas the log no longer records)
the delta is a key delta. `key_delta_into` needs a copy and is not
available on such a baseline.

When the list holds every run since the baseline's last delta and nothing
else is due (no `digest`, no `whole_blocks`, the same used size), the run
frame is written on a short path that one compare selects: one run is copied
out directly (about 120 instructions for one scalar, `touch_set` included),
several are sorted and merged in place first. A buffer that recorded a
failure, a copy of the baseline object and a baseline whose last delta did
not fit the caller's memory take the general path. `save_delta_into(std::span<char> out,
baseline, options)` writes into the caller's memory instead of a string and
returns the delta's size (0: nothing changed); a delta larger than `out` is
not written, its size is returned, and the baseline keeps it for its next
`save_delta_into`, which writes it before anything else.

### One changed field

A delta found by comparing with a copy (not a key delta, no `digest`, no
`whole_blocks`, the same used size) whose changed bytes are one run inside
one block (a field set, or a few side by side) is a run frame: the receiver
copies the run instead of applying a masked block. Its bytes are the same as
a touches baseline's for the same change; a one-byte change is 19 bytes (18
as a block frame). Anything more stays a block frame.

### Statistics and options

`save_delta_into` returns `XDeltaStats`: `key`, `empty`, `blocks` (64-byte
blocks sent, or a run frame's runs), `changed_bytes` (image bytes they
carry), `compared_bytes`, `delta_bytes`. `XDeltaOptions`:

- `digest` (default false): the delta carries the xxHash64 of the whole
  resulting payload; the receiver rejects a delta whose result differs. This
  catches a replica that was changed locally and a change the sender did not
  touch (section 4). One hash pass over the payload on each side.
- `touched_only` (default false): see above.
- `whole_blocks` (default false): each changed block goes whole (64 bytes,
  selector 0) instead of by its mask; a block that ends past `used_end` still
  goes by its mask (a run frame: each run widened to the 64-byte blocks it
  touches). The delta is larger and the receiver applies each block with one
  copy. Also taken by `XDeltaBaseline::key_delta_into`, for key deltas to
  receivers where apply time counts more than bytes.

The receiver's `XDeltaCursor::last_check()` says what it checked for the last
delta (`key`, `none`, `in_place`, `full`, or `admit` on a trusted link;
section 3).

`examples/delta_sync.cpp` runs a server and a client through key delta,
small deltas, a lost delta and the key delta that recovers from it.

## 2. Format 2.0

Little-endian. A delta writes image bytes `[32, used_end)`: the header's
`free_dir` and `free_bytes` (offsets 32 to 39) and the payload. The free
blocks are image bytes like the rest (each block's link word, the free
directory), so a delta carries them as it carries any other bytes; offsets 0
to 31 never change, and the root is always at 40. There are two frames: a
block frame (key deltas, and deltas found by comparing with a copy) and a
run frame (`RUNS`: the deltas of an `XDeltaRecord::touches` baseline, and a
compared delta of one changed field, section 1). A
delta is exactly its header, the optional fields its flags announce and its
block or run section; nothing may follow.

**Stability.** Format 2.0 goes with wire format 3.0 (format 1.0, from 3.4.0
to 4.1.0, addressed the 2.x image, whose root was at 48). A sender writes 2.0
and a receiver reads it, on every 64-bit little-endian target, compiler and
schema backend. Once released, nothing in this section changes within 2.0:
not the fields, not their order or meaning, not what a receiver accepts or
rejects. An addition gets a new format byte; a 2.0 receiver rejects any
format byte but `0x20` (1.0's `0x10` included) with `unsupported_version`
before reading further, and a sender writes a newer format only to a receiver
known to read it.
`tests/test_delta_format.cpp` pins the bytes of two small delta streams
(block frames: a key delta, a scalar delta, a digest delta; run frames: the
same changes touched) both ways, written out field by field after the tables
below: the sender must write them (the compared scalar change as its run
frame, the same bytes as the touched one) and the receiver must apply them,
with the full check and with admit, so a framing change fails on every
compiler. What
is not frozen is where the library places data inside the payload (the same
release on both ends gives the same payload; a delta always carries image
bytes at image offsets of the sender's own buffer).

A block frame:

| offset | field | type | value |
| ---: | --- | --- | --- |
| 0 | magic | 2 bytes | `'X' 'D'` |
| 2 | format | u8 | `major << 4 \| minor`: `0x20` (2.0) |
| 3 | flags | u8 | bit 0 KEY, bit 1 RUNS (clear), bit 2 DIGEST, bit 3 EXTENT; other bits 0 |
| 4 | stream | u64 | the sender's stream id, nonzero |
| 12 | version | varint | version after this delta, at least 1 |
| | used | varint | EXTENT only: (`used_end` after this delta − 40) / 8 |
| | block count | varint | entries of the block section |

A run frame:

| offset | field | type | value |
| ---: | --- | --- | --- |
| 0 | magic | 2 bytes | `'X' 'D'` |
| 2 | format | u8 | `0x20` (2.0) |
| 3 | flags | u8 | RUNS set, KEY clear; DIGEST and EXTENT as above |
| 4 | stream | u64 | the sender's stream id, nonzero |
| 12 | version | u32 | version after this delta, at least 1 |
| 16 | used | varint | EXTENT only: (`used_end` after this delta − 40) / 8 |

Without EXTENT, `used_end` stays as the replica holds it. A block frame that
keeps it has a 14-byte header while its version and block count are below
128; a run frame that keeps it, a 16-byte header.

Then, in this order:

- KEY (block frames only): `schema_hash` (u64), `root_type_id` (u64): the
  wire fingerprint of the root type. A key delta always has EXTENT.
- DIGEST: xxHash64 (seed 0) of image bytes `[32, used_end)` after this
  delta.
- Block section (block frames): `block count` × { varint `skip`, u8
  `selector`, mask bytes, data bytes }. The image is cut into 64-byte blocks
  from its start (block `i` holds offsets `64 i` to `64 i + 63`; an image
  starts 64-aligned in memory, so a block is one cache line). Block 0 holds
  the image header in offsets 0 to 39, of which a delta writes 32 to 39 only:
  block 0 never goes whole and its mask bits for offsets 0 to 31 are 0. An
  entry's block index is the previous entry's index + 1 + `skip` (the first:
  `skip`), so blocks are ascending. `selector` 0: the whole block follows (64
  bytes; it ends at or before `used_end`). Otherwise bit `w` of `selector`
  says that a mask byte for the block's 8-byte word `w` follows (in word
  order, each nonzero); bit `j` of that mask byte stands for byte `8 w + j`
  of the block. Then the bytes the masks name, in order. Every byte written
  lies below `used_end`. Those bytes replace the image bytes; the other bytes
  of the block stay.
- Run section (run frames): runs to the end of the delta, each { varint
  `gap`, varint `length`, `length` bytes }. A run starts `gap` bytes after
  the end of the run before it (the first: after offset 32), its `length` is
  nonzero, and its bytes replace the image bytes there. So runs are
  ascending and disjoint; every byte written lies below `used_end`. A run
  frame may have no runs (a new `used_end` alone).

A varint is unsigned LEB128 of at most 5 bytes holding a value below 2^32. The
sender writes a block whole when the masked form would not be shorter (mask
bytes plus changed bytes of at least 64).

**Applying.** A key delta applies to an all-zero image of `used_end` bytes
(the receiver writes offsets 0 to 31 of the header from `T`); a non-key
delta applies to the image of version `version - 1` of the same stream,
whose bytes past its `used_end` read as zero (every buffer keeps that
invariant). The receiver writes the blocks' or runs' bytes and `used_end`
(zeroing what lies past a smaller `used_end`), then checks the result
(section 3).

**Receiver checks**, in this order: magic, format, length of the fixed header,
flags (RUNS never with KEY), the header fields; a key delta has EXTENT; stream
and version nonzero; for a key delta the fingerprint (root type id, then
schema hash) against `T`, for a non-key delta that the cursor holds a key
delta of `T` and that `stream` is the cursor's and `version` follows it;
without EXTENT, that the replica holds a verified root; `used_end` (the root
at 40 lies below it); the block or run section (every entry complete, every
byte it writes inside `[32, used_end)`, no zero mask byte, no empty run); the
total length. Then, after patching: the digest, then the check of section 3.

**Errors.** As for wires (`bad_magic`, `unsupported_version`, `truncated`,
`unsupported_flags`, `root_type_mismatch`, `schema_mismatch`,
`size_mismatch`, `root_offset_out_of_bounds`, `invalid_extent_table` for a
block or run out of bounds, a zero mask byte or an empty run,
`invalid_payload`, `root_mismatch`, `capacity_exceeded`), plus two codes:
`delta_base_mismatch` (the delta is for another state than the replica
holds, or the replica holds no key delta) and `digest_mismatch`. The messages start with `apply_delta_verified<T>()`
(`XReceiver::apply_delta<T, admit>()` for admit).

## 3. What the receiver checks

The result must pass exactly what `load_verified<T>` checks of a wire of the
same payload. A key delta is verified in full (`key`). For the others the receiver keeps, next to the cursor, a
map of the bytes verification reads (per 64-byte block, one bit per byte):
container heads, heap string terminators, set and map keys with everything
they reach, bools, the length and padding of inline strings, the heads of
empty strings; and a map of the live blocks (one bit per 8 bytes: the arena
header, the root and every container block). Every other byte of a verified
payload may take any value without changing what verification visits or
decides. So:

- **`none`**: the delta writes no byte the map marks (scalars, vector elements
  of plain types, map values, inline characters, slack bytes of a container
  block), keeps `used_end`, and writes no byte outside the live blocks (a free
  block's or the free directory's) and not the header's free state: the free
  blocks stay as they were. Nothing is checked; no undo copy is kept.
- **`in_place`**: the marked bytes it writes are bools (0 or 1), inline or
  empty string heads (still inline, length at most 10, zero padding; or all
  zero), sizes of vectors of plain elements (at most the capacity), or the
  heads and blocks of heap strings, sets or maps of plain elements (the
  terminator in place; keys strictly ordered). Those are checked one by one.
  A heap string, vector of plain elements, set or map of plain elements whose
  capacity or block changed (it moved, got its first block, or lost it), and a
  string head that left or entered the inline or empty form, is checked on its
  own the same way, and its new block against the map of live blocks: inside
  the payload, 8-aligned, overlapping no live block once the blocks this delta
  frees are taken out. With a new `used_end`, a byte written outside the live
  blocks or in the header's free state (the free blocks may have changed),
  or any such change, the image's free directory and free blocks, as
  patched, must tile the rest: each checked as the whole verification checks
  it (inside the payload, aligned, each stack of one size, the sizes
  ascending, the free bytes the header names), in no live block and no other
  free region, live and free bytes adding up to `used_end`. That is the
  coverage the whole verification checks, without walking the payload. A vector of records
  (elements with strings, containers or bools) that grows within its
  capacity and keeps its block has only its new records verified, as the
  whole verification would verify them, each block they reach claimed in
  the map of live blocks. One that moved, got its first block or shrank (an
  erase from the middle, a pop, growth past its capacity) has everything its
  old records reached taken out of the map of live blocks and the map of
  checked bytes, and all its records verified again the same way from its
  new block: only that vector, not the rest of the payload.
- **`full`**: anything else (a marked byte no in-place check covers), and
  any delta whose in-place
  check fails: the whole verification then decides, so a delta is accepted or
  rejected, with the same error, exactly as a whole verification would.

The map holds for the buffer state it was built with; a buffer that was
moved, reloaded, reset, swapped, grown into new storage, or that allocated or
freed since, gets its next delta verified in full. The map is built after a
key delta, and after a whole verification of another delta only when a later
delta first needs it (a replica whose deltas keep needing the whole
verification, like a store receiving saves, does not rebuild it each time).

**Trusted links.** `XReceiver<XLink::server_peer>` and
`XReceiver<XLink::from_server>` apply deltas with `XCheck::admit` by default
(`apply_delta`; `last_check()` is `admit`). Everything the parse checks stays:
the format, the stream and version against the cursor, the root type and
schema fingerprint of a key delta, every byte written inside `[32,
used_end)`, nothing after the last section. Besides that only the root object
has to lie inside the used payload; what the bytes mean (the free blocks
included) is the sender's, as for a whole wire taken with admit. No map of
checked bytes is built or kept, and no undo copy unless the delta has a
digest (a digest that does not match rolls the delta back as above). The cursor's map then no longer
holds, so a full apply on the same cursor after admits (`apply_delta<T,
XCheck::full>`, or `try_apply_delta_verified`) verifies the whole payload once
and builds the map again; the map is never built over a payload that was
only admitted. `XReceiver<XLink::from_client>` applies with the full check
only (`admit` does not compile), and `XOFFSET_FORCE_FULL_CHECK=1` (the default
without `NDEBUG` and under ASan or TSan) makes admit the full check.

## 4. Guarantees

- **Byte identity.** After a delta applies, the receiver's image
  `[0, used_end)` equals the sender's, its free blocks and directory
  included, so a later `save_compact` or `save_verified` of either side
  carries the same payload.
- **All or nothing.** A non-key delta that is rejected, at any check, leaves
  the buffer (bytes, used_end, free blocks) and the cursor exactly as they
  were: the patched bytes are rolled back from an undo copy the cursor keeps.
  A key delta rejected by its header checks changes nothing; one rejected
  after them leaves the buffer empty (as a failed `reload_verified`) and the
  cursor cleared.
- **Same checks as a full load.** With the full check, the patched payload
  passes exactly the verification a wire of the same payload passes in
  `load_verified<T>` (section 3), so a delta cannot produce a payload a wire
  could not. With admit (trusted links, section 3) the framing is checked and
  the payload is the sender's, as for a whole wire taken with admit.
- **Order.** Versions count up per stream; a delta applies only to the version
  before it, so a lost, repeated or reordered delta is rejected with
  `delta_base_mismatch`. `XDeltaBaseline::reset()` keeps counting, so deltas
  written before a reset never apply after it. When the 32-bit version runs
  out, the baseline starts a new stream with a key delta.
- **Empty deltas.** When nothing changed (bytes, used_end),
  `save_delta_into` writes nothing and does not advance the version.
- **Pointers.** A successful delta starts a new buffer epoch: element pointers
  into the previous content must not be used; `XHandle`s re-resolve. The root
  keeps its address while the root offset and the storage stay (VM and pinned
  storage never move).

## 5. Rules for senders and receivers

- **The replica is read-only.** A receiver that edits its copy no longer
  holds the version the next delta expects. Nothing but the digest notices
  that (the result still verifies, it is just not the sender's), so either
  send with `digest = true` or copy the replica (`clone()`) before editing.
  To send edits back, the receiver keeps its own buffer and baseline and
  sends deltas the other way.
- **Touch what you change** (only with `touched_only`): section 1.
- **Same schema on both ends.** Deltas carry no schema conversion. The
  fingerprint is checked on the key delta (`schema_mismatch`,
  `root_type_mismatch`). Decide at connect time: when the schema handshake
  (`xoffset_handshake.hpp`, `examples/schema_handshake.cpp`) answers `same`
  for the root type, send deltas; when it answers `convert`, send whole wires
  converted for that client ([SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)), or
  keep a sender-side buffer in the client's version with its own baseline.
  A client that gets `schema_mismatch` on a key delta has the wrong verdict
  and reconnects with a new handshake; asking for another key delta would not
  help.
- **Stable positions.** The comparison is positional. Data whose elements
  stay where they are (slots that are cleared and reused, fields assigned in
  place) gives small deltas. An erase from the middle of an `XVector`, or an
  insert into a sorted `XSet`/`XMap`, shifts every later element and sends
  them all. For a changing set of entities (units in a client's view), keep
  stable slots: a unit that enters takes a free slot, one that leaves clears
  its slot.
- **Growth.** A container that outgrows its capacity moves to a new block;
  the delta then carries the new block and the allocator's writes (the old
  block's link word, the free directory), and the receiver checks the move
  against its map of live blocks. Reserving capacity for the expected size keeps
  deltas to the changed elements and on the cheap checks. Strings of at most
  10 bytes are stored inline in their head.
- **Relayout of a long-lived sender.** A sender edited in place for a long
  time keeps its holes and spare capacity, and every delta compares (and
  every replica holds) its whole `used_end`. `relayout<T>()` between ticks
  rewrites it in place as its compact image ([USER_GUIDE.md](USER_GUIDE.md)
  section 4.7; `relayout_advised<T>()` says when). It advances the epoch, so
  no delta code is involved: the next delta of a copy baseline (also with
  `touched_only`, whose touch log belongs to the old epoch) compares the
  whole payload, nearly every block differs and it is about the size of a
  key delta; a `touches` baseline sends a key delta. Each stream pays that
  once per relayout. Never relayout a replica: its layout is its sender's.
- **Per receiver, not per message.** One baseline is one copy of the
  payload's used bytes on the sender. Receivers that always get the same
  deltas (replicas of one buffer) can share one baseline and one delta.

## 6. Cost

Sender, per delta: the comparison of the payload (or, with `touched_only`, of
the touched blocks) against the baseline in 64-byte blocks, where each block
that differs is copied into the baseline, and the changed bytes gathered by
their masks (one byte shuffle per 8-byte word: SSSE3 when the CPU has it,
NEON on AArch64, byte by byte elsewhere); from a touches baseline, the
touched runs copied as they are; with `digest`, one hash pass. Receiver, per
delta: the header and section checks, the bytes written, then the check of
section 3: for `none` nothing more, for `in_place` an undo copy and the
checks of the bytes written (for a moved container or a changed free block,
its block's bits in the live map and a walk of the free directory), for
`full` the whole verification (proportional to the payload; the map is
rebuilt, also proportional, when a later delta needs it); with `digest`, one
hash pass. The receiver's maps take 29 bytes per 64-byte block of payload
(24 for the bits, 4 for the node index, 1 for the live blocks), plus about
64 bytes per heap string, vector, set or map of plain elements; the sender's
touch log 320 bytes plus, once two baselines pair with it or a period
touches more than 25 runs, 4 bytes per block. The scratch a delta needs
while it is applied (a copy of a small delta, the old bytes it overwrites,
its block or run list) is shared by the cursors of one thread, so a receiver
that holds many replicas keeps one copy, warm in cache, instead of one per
replica (`XDeltaCursor::release_scratch()` frees it); likewise the scratch
of writing one (the changed blocks, the runs, the blocks to compare) is
shared by a thread's baselines (`XDeltaBaseline::release()`). Where
`XOFFSET_THREAD_CACHES` is 0 (GCC on MinGW and Cygwin) each cursor and
baseline keeps its own.

Viewport-like model in `tests/test_delta_sync.cpp` (120 units in 128 stable
slots, each tick 30% move a few cells, 5% lose hp, 2 leave and 2 enter):
about 260 bytes per tick in 39 blocks, against 5946 bytes for `save_compact`
and 7090 bytes for `save_verified` of the same view; the key delta is 3210
bytes.

## 7. Not in format 2.0

- Moved data is not detected (an erase from the middle sends the shifted
  tail).
- No schema conversion of deltas.
