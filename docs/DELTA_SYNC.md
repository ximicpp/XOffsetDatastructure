# Delta sync

A receiver that already holds a copy of a buffer (a client that has its
units or its player, a storage server that has a player) should not get the
whole buffer again when the sender changes a few fields. Delta sync sends only
the payload bytes that changed and patches the receiver's copy in place.
Nothing is encoded per field: a delta carries raw payload bytes at payload
offsets, which mean the same thing on every 64-bit little-endian target (the
wire rule of [V2_FORMAT.md](V2_FORMAT.md)). After a delta applies, the
receiver's payload is the sender's payload byte for byte.

Header: `xoffset_delta.hpp` (included by `xoffsetdatastructure.hpp`;
`XOFFSET_DELTA_SYNC_API` is defined). Both schema backends (C++26 reflection,
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
the baseline's previous delta from this buffer, the bytes past the baseline's
used size, and the holes that came or went (a container that moved left its
old block as a hole, cleared; the delta finds it without a touch). Touches go
to a log in the buffer, so one buffer can feed any number of baselines (a
client, the storage process, a relay). While one baseline pairs with the
buffer, the log is a short list of the blocks touched since its last delta
(up to 24; the baseline's lines of them are fetched into cache as they are
touched); with a second baseline, or more blocks than the list holds, it also
stamps each touched block with the delta it belongs to (four bytes per
64-byte block), so a baseline that skipped deltas still finds its blocks.
`touch_set(field, value)` assigns and touches only when the value differs.

The rule: **touch everything you change, after you change it** (a container
that moved is marked where it is now). A change that is not touched is not
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
delta carries every byte touched since the baseline's previous delta (as if
`touched_only`), the bytes past the receiver's used size and those of holes
that came or went. Memory per receiver drops from one copy of the payload
to a few words; the delta grows by the bytes that were touched but did not
change, so touch exactly what changes (`touch_set`, `touch(p, bytes)` on the
field). Where the touches do not say what changed (the first delta, content
that was replaced, a baseline that missed deltas the log no longer records)
the delta is a key delta. `key_delta_into` needs a copy and is not
available on such a baseline.

### Statistics and options

`save_delta_into` returns `XDeltaStats`: `key`, `holes`, `empty`, `blocks`
(64-byte blocks sent), `changed_bytes` (payload bytes they carry),
`compared_bytes`, `delta_bytes`. `XDeltaOptions`:

- `digest` (default false): the delta carries the xxHash64 of the whole
  resulting payload; the receiver rejects a delta whose result differs. This
  catches a replica that was changed locally and a change the sender did not
  touch (section 4). One hash pass over the payload on each side.
- `touched_only` (default false): see above.
- `whole_blocks` (default false): each changed block goes whole (64 bytes,
  selector 0) instead of by its mask; a block that ends past `used_end` still
  goes by its mask. The delta is larger and the receiver applies each block
  with one copy. Also taken by `XDeltaBaseline::key_delta_into`, for key
  deltas to receivers where apply time counts more than bytes.

The receiver's `XDeltaCursor::last_check()` says what it checked for the last
delta (`key`, `none`, `in_place`, `full`; section 3).

## 2. Format 1.0

Little-endian. A delta is exactly the header, the optional fields its flags
announce, the block section and the optional hole section; nothing may follow.

| offset | field | type | value |
| ---: | --- | --- | --- |
| 0 | magic | 2 bytes | `'X' 'D'` |
| 2 | format | u8 | `major << 4 \| minor`: `0x10` (1.0) |
| 3 | flags | u8 | bit 0 KEY, bit 1 HOLES, bit 2 DIGEST, bit 3 EXTENT; other bits 0 |
| 4 | stream | u64 | the sender's stream id, nonzero |
| 12 | version | varint | version after this delta, at least 1 |
| | used | varint | EXTENT only: (payload `used_end` after this delta − 48) / 8 |
| | root | varint | EXTENT only: root offset after this delta − 48, below `used_end` |
| | block count | varint | entries of the block section |

Without EXTENT, `used_end` and the root offset stay as the replica holds
them. A delta that keeps both and carries no hole table has a 14-byte header
while its version and block count are below 128.

Then, in this order:

- KEY: `schema_hash` (u64), `root_type_id` (u64): the wire fingerprint of the
  root type. A key delta always has HOLES and EXTENT.
- DIGEST: xxHash64 (seed 0) of payload bytes `[48, used_end)` after this
  delta.
- Block section: `block count` × { varint `skip`, u8 `selector`, mask bytes,
  data bytes }. The payload is cut into 64-byte blocks from its start (block
  `i` holds offsets `64 i` to `64 i + 63`; a payload starts 64-aligned in
  memory, so a block is one cache line). Block 0 holds the arena header in
  offsets 0 to 47, which no delta writes: block 0 never goes whole and its
  mask bits for those bytes are 0. An entry's block index is
  the previous entry's index + 1 + `skip` (the first: `skip`), so blocks are
  ascending. `selector` 0: the whole block follows (64 bytes; it ends at or
  before `used_end`). Otherwise bit `w` of `selector` says that a mask byte for
  the block's 8-byte word `w` follows (in word order, each nonzero); bit `j` of
  that mask byte stands for byte `8 w + j` of the block. Then the bytes the
  masks name, in order. Every byte written lies below `used_end`. Those bytes
  replace the payload bytes; the other bytes of the block stay.
- HOLES: varint `count`, then `count` × { varint `gap / 8`, varint
  `bytes / 8` }: the whole new hole table in offset order, each hole starting
  `gap` bytes after the end of the previous one (the first: after offset 48).
  Without HOLES the hole table does not change.

A varint is unsigned LEB128 of at most 5 bytes holding a value below 2^32. The
sender writes a block whole when the masked form would not be shorter (mask
bytes plus changed bytes of at least 64).

**Applying.** A key delta applies to an all-zero payload of `used_end` bytes; a
non-key delta applies to the payload of version `version - 1` of the same
stream, whose bytes past its `used_end` read as zero (every buffer keeps that
invariant). The receiver writes the blocks' bytes, writes the payload header
from `used_end` and the root offset (zeroing what lies past a smaller
`used_end`), then checks the result (section 3).

**Receiver checks**, in this order: magic, format, length of the fixed header,
flags, the header varints; a key delta has EXTENT; stream and version nonzero;
for a key delta the fingerprint (HOLES present, root type id, then schema hash)
against `T`, for a non-key delta that the cursor holds a key delta of `T` and
that `stream` is the cursor's and `version` follows it; without EXTENT, that the
replica holds a verified root; `used_end`, root offset; the block section
(every entry complete, inside `used_end`, no zero mask byte); the hole section;
the total length. Then, after patching: the digest, then the check of
section 3.

**Errors.** As for wires (`bad_magic`, `unsupported_version`, `truncated`,
`unsupported_flags`, `root_type_mismatch`, `schema_mismatch`,
`size_mismatch`, `root_offset_out_of_bounds`, `invalid_extent_table` for a
block out of bounds or a zero mask byte, `invalid_payload`, `root_mismatch`,
`capacity_exceeded`), plus two codes: `delta_base_mismatch` (the delta is for
another state than the replica holds, or the replica holds no key delta) and
`digest_mismatch`. The messages start with `apply_delta_verified<T>()`.

## 3. What the receiver checks

The result must pass exactly what `load_verified<T>` checks of a wire of the
same payload. A key delta, and any delta that changes `used_end`, the root or
the hole table, is verified in full (`key`, `full`). For the others the
receiver keeps, next to the cursor, a map of the bytes verification reads (per
64-byte block, one bit per byte): container heads, heap string terminators,
set and map keys with everything they reach, bools, the length and padding of
inline strings, the heads of empty strings. Every other byte of a verified payload may take any value
without changing what verification visits or decides. So:

- **`none`**: the delta writes no byte the map marks (scalars, vector elements
  of plain types, map values, inline characters, free bytes). Nothing is
  checked; no undo copy is kept.
- **`in_place`**: the marked bytes it writes are bools (0 or 1), inline or
  empty string heads (still inline, length at most 10, zero padding; or all
  zero), sizes of vectors of
  plain elements (at most the capacity), or the head and block of a heap
  string, set or map of plain elements whose capacity and block stay (the
  terminator in place; keys strictly ordered). Those are checked one by one.
- **`full`**: anything else (a block or capacity that changed, a vector of
  records that grew, a marked byte no in-place check covers), and any delta
  whose in-place check fails: the whole verification then decides, so a delta
  is accepted or rejected, with the same error, exactly as a whole
  verification would.

The map holds for the buffer state it was built with; a buffer that was
moved, reloaded, reset, swapped, grown into new storage, or that allocated or
freed since, gets its next delta verified in full. The map is built after a
key delta, and after a whole verification of another delta only when a later
delta first needs it (a replica whose deltas keep needing the whole
verification, like a store receiving saves, does not rebuild it each time).

## 4. Guarantees

- **Byte identity.** After a delta applies, the receiver's payload
  `[0, used_end)` equals the sender's, and its hole set equals the sender's
  free table, so a later `save_compact` or `save_verified` of either side
  carries the same payload.
- **All or nothing.** A non-key delta that is rejected, at any check, leaves
  the buffer (bytes, used_end, root, free table) and the cursor exactly as they
  were: the patched bytes are rolled back from an undo copy the cursor keeps.
  A key delta rejected by its header checks changes nothing; one rejected
  after them leaves the buffer empty (as a failed `reload_verified`) and the
  cursor cleared.
- **Same checks as a full load.** The patched payload passes exactly the
  verification a wire of the same payload passes in `load_verified<T>` (section
  3), so a delta cannot produce a payload a wire could not.
- **Order.** Versions count up per stream; a delta applies only to the version
  before it, so a lost, repeated or reordered delta is rejected with
  `delta_base_mismatch`. `XDeltaBaseline::reset()` keeps counting, so deltas
  written before a reset never apply after it. When the 32-bit version runs
  out, the baseline starts a new stream with a key delta.
- **Empty deltas.** When nothing changed (bytes, holes, used_end, root),
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
  fingerprint is checked on the key delta; a client on another schema version
  needs a sender-side buffer in its version (see
  [SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)) and its own baseline.
- **Stable positions.** The comparison is positional. Data whose elements
  stay where they are (slots that are cleared and reused, fields assigned in
  place) gives small deltas. An erase from the middle of an `XVector`, or an
  insert into a sorted `XSet`/`XMap`, shifts every later element and sends
  them all. For a changing set of entities (units in a client's view), keep
  stable slots: a unit that enters takes a free slot, one that leaves clears
  its slot.
- **Growth.** A container that outgrows its capacity moves to a new block;
  the delta then carries the new block and the hole of the old one, and the
  receiver verifies it in full. Reserving capacity for the expected size keeps
  deltas to the changed elements and on the cheap checks. Strings of at most
  10 bytes are stored inline in their head.
- **Per receiver, not per message.** One baseline is one copy of the
  payload's used bytes on the sender. Receivers that always get the same
  deltas (replicas of one buffer) can share one baseline and one delta.

## 6. Cost

Sender, per delta: the comparison of the payload (or, with `touched_only`, of
the touched blocks) against the baseline in 64-byte blocks, where each block
that differs is copied into the baseline; the free table sorted by offset; the
changed bytes gathered by their masks (one byte shuffle per 8-byte word:
SSSE3 when the CPU has it, NEON on AArch64, byte by byte elsewhere); with
`digest`, one hash pass. Receiver, per delta: the header and section checks,
the bytes scattered by their masks, then the check of section 3: for `none`
nothing more, for `in_place` an undo copy and the checks of the bytes written,
for `full` the whole verification (proportional to the payload; the map is
rebuilt, also proportional, when a later delta needs it); with `digest`, one
hash pass. The receiver's map takes 28 bytes
per 64-byte block of payload (24 for the bits, 4 for the node index), plus
about 64 bytes per heap string, set or map of plain elements; the sender's
touch log 384 bytes plus, once two baselines pair with it or a delta
touches more than 24 blocks, 4 bytes per block. The scratch a delta needs while it is applied
(a copy of a small delta, the old bytes it overwrites, its block list and
the hole table) is shared by the cursors of one thread, so a receiver that
holds many replicas keeps one copy, warm in cache, instead of one per
replica (`XDeltaCursor::release_scratch()` frees it); likewise the scratch
of writing one (the hole table, the changed blocks, the blocks to compare)
is shared by a thread's baselines (`XDeltaBaseline::release()`). Where
`XOFFSET_THREAD_CACHES` is 0 (GCC on MinGW and Cygwin) each cursor and
baseline keeps its own.

Viewport-like model in `tests/test_delta_sync.cpp` (120 units in 128 stable
slots, each tick 30% move a few cells, 5% lose hp, 2 leave and 2 enter):
about 270 bytes per tick in 39 blocks, against 5946 bytes for `save_compact`
and 7225 bytes for `save_verified` of the same view; the key delta is 3214
bytes.

## 7. Not in format 1.0

- Moved data is not detected (an erase from the middle sends the shifted
  tail).
- A change of a block, capacity, `used_end` or the hole table is verified over
  the whole payload, not over the changed part only.
- No schema conversion of deltas.
