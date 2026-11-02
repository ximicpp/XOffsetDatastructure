# Delta sync

A receiver that already holds a copy of a buffer (a client that has its
units, a storage server that has a player) should not get the whole buffer
again when the sender changes a few fields. Delta sync sends only the payload
bytes that changed and patches the receiver's copy in place. Nothing is
encoded per field: a delta carries raw payload bytes at payload offsets, which
mean the same thing on every 64-bit little-endian target (the wire rule of
[V2_FORMAT.md](V2_FORMAT.md)). After a delta applies, the receiver's payload is
the sender's payload byte for byte.

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

`save_delta_into` returns `XDeltaStats` (key, holes, empty, runs, run_bytes,
compared_bytes, delta_bytes). `XDeltaOptions`:

- `merge_gap` (default 2): two changed runs at most this many unchanged bytes
  apart are sent as one run. A run costs about 2 bytes of framing.
- `digest` (default false): the delta carries the xxHash64 of the whole
  resulting payload; the receiver rejects a delta whose result differs. This
  catches a replica that was changed locally (see section 4). One hash pass
  over the payload on each side.

## 2. Format 1.0

Little-endian. A delta is exactly the header, the optional fields its flags
announce, the run section and the optional hole section; nothing may follow.

| offset | field | type | value |
| ---: | --- | --- | --- |
| 0 | `magic` | u32 | `0x00014458`: bytes `'X' 'D' 1 0` (format 1.0) |
| 4 | `flags` | u32 | bit 0 KEY, bit 1 HOLES, bit 2 DIGEST; other bits 0 |
| 8 | `stream` | u64 | the sender's stream id, nonzero |
| 16 | `version` | u32 | version after this delta, at least 1 |
| 20 | `used_bytes` | u32 | payload `used_end` after this delta: at least 48, a multiple of 8, at most `MAX_PAYLOAD` |
| 24 | `root_offset` | u32 | root offset after this delta, in `[48, used_bytes)` |
| 28 | `run_count` | u32 | entries of the run section |

Then, in this order:

- KEY: `schema_hash` (u64), `root_type_id` (u64): the wire fingerprint of the
  root type. A key delta always has HOLES.
- DIGEST: xxHash64 (seed 0) of payload bytes `[48, used_bytes)` after this
  delta.
- Run section: `run_count` x { varint `gap`, varint `length`, `length` bytes }.
  The run starts `gap` bytes after the end of the previous run (the first:
  after offset 48), so runs are ascending and disjoint; `length` is at least 1
  and the run ends at or before `used_bytes`. Its bytes replace the payload
  bytes there.
- HOLES: varint `count`, then `count` x { varint `gap / 8`, varint
  `bytes / 8` }: the whole new hole table in offset order, each hole starting
  `gap` bytes after the end of the previous one (the first: after offset 48).
  Without HOLES the hole table does not change.

A varint is unsigned LEB128 of at most 5 bytes holding a value below 2^32.

**Applying.** A key delta applies to an all-zero payload of `used_bytes`
bytes; a non-key delta applies to the payload of version `version - 1` of the
same stream, whose bytes past its `used_end` read as zero (every buffer keeps
that invariant). The receiver copies the runs, writes the payload header from
`used_bytes` and `root_offset` (zeroing what lies past a smaller `used_end`),
then verifies.

**Receiver checks**, in this order: magic and version; length of the header;
flags; stream and version nonzero; for a key delta the fingerprint (root type
id, then schema hash) against `T`, for a non-key delta that the cursor holds a
key delta of `T` and that `stream` is the cursor's and `version` follows it;
`used_bytes`, `root_offset`; the run section; the hole section; the total
length. Then, after patching: the digest, then exactly the verification of
`load_verified<T>` (root at its offset, graph walk, holes, exact coverage).
A non-key delta also needs a buffer whose verified root is of type `T` and,
without HOLES, a complete free table.

**Errors.** As for wires (`bad_magic`, `unsupported_version`, `truncated`,
`unsupported_flags`, `root_type_mismatch`, `schema_mismatch`,
`size_mismatch`, `root_offset_out_of_bounds`, `invalid_extent_table` for a run
out of bounds, `invalid_payload`, `root_mismatch`, `capacity_exceeded`), plus
two codes: `delta_base_mismatch` (the delta is for another state than the
replica holds, or the replica holds no key delta) and `digest_mismatch`. The
messages start with `apply_delta_verified<T>()`.

## 3. Guarantees

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
  verification a wire of the same payload passes in `load_verified<T>`, so a
  delta cannot produce a payload a wire could not.
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

## 4. Rules for senders and receivers

- **The replica is read-only.** A receiver that edits its copy no longer
  holds the version the next delta expects. Nothing but the digest notices
  that (the result still verifies, it is just not the sender's), so either
  send with `digest = true` or copy the replica (`clone()`) before editing.
  To send edits back, the receiver keeps its own buffer and baseline and
  sends deltas the other way.
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
  the delta then carries the new block and the hole of the old one. Reserving
  capacity for the expected size keeps deltas to the changed elements.
  Strings of at most 10 bytes are stored inline in their head.
- **Per receiver, not per message.** One baseline is one copy of the
  payload's used bytes on the sender. Receivers that always get the same
  deltas (replicas of one buffer) can share one baseline and one delta.

## 5. Cost

Sender, per delta: one comparison of the payload against the baseline
(64-byte blocks, then words), one copy of the changed runs into the baseline,
the free table sorted by offset; with `digest`, one hash pass. Receiver, per
delta: the header and section checks, one undo copy and one copy of the
changed runs, then the full verification of `load_verified<T>` (proportional
to the payload, not to the delta); with `digest`, one hash pass.

Viewport-like model in `tests/test_delta_sync.cpp` (120 units in 128 stable
slots, each tick 30% move a few cells, 5% lose hp, 2 leave and 2 enter):
about 318 bytes per tick in 77 runs, against 5946 bytes for `save_compact`
and 7225 bytes for `save_verified` of the same view; the key delta is 4387
bytes.

## 6. Not in format 1.0

- Moved data is not detected (an erase from the middle sends the shifted
  tail).
- Verification is of the whole payload, not of the changed part only.
- No schema conversion of deltas.
