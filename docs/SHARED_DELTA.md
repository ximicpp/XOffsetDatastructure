# Shared deltas (XS 1.0)

A server that sends every client its own view of a shared world (the units,
marches and tiles a client sees) should not compare and encode an entity once
per client. A shared delta is written per record, as bytes at offsets inside
the record: the server compares each changed entity once per tick against one
shared baseline of it and every connection that holds the entity gets the
same bytes. The client keeps its own layout: it finds the record by id in its
own slots and patches it in place.

[DELTA_SYNC.md](DELTA_SYNC.md) is the other kind of delta: payload bytes at
payload offsets, applied to a copy with the sender's layout (one baseline per
receiver). Use it for a player's own state and between servers that keep a
replica; use shared deltas for views of a shared world.

Header: `xoffset_shared_delta.hpp` (sender side, record codec) and
`xoffset_receiver.hpp` (`XReceiver::apply_sync`). Both schema backends (C++26
reflection, C++20 `XOFFSET_RECORD` field lists) and builds without
exceptions. Example: `examples/shared_delta.cpp`.

## 1. Use

Sender: one baseline record per entity (what every client that holds it
holds), one message per client per tick.

```cpp
namespace xs = XOffsetDatastructure::xsync;
enum : unsigned { kHead = 0, kUnit = 1 };          // table numbers, fixed by the protocol

xs::XSyncOut body, key, msg;
// Once per tick: each changed unit is compared once with its baseline.
for (Unit& u : changed) {
    const std::size_t at = body.size();
    if (xs::XRecordDelta<Unit>::diff(u, baseline_of(u), body)) remember(u.id, at, body.size() - at);
}
// Per client: a writer builds the message in place.
xs::XSyncWriter m(msg, client.needs_key ? xs::kFlagKey : 0, stream_id, ++client.version);
for (std::uint32_t id : client.left) m.op(kUnit, xs::leave, id);
for (std::uint32_t id : client.entered) {          // a key body: the record from cleared
    key.clear();
    xs::XRecordDelta<Unit>::key(unit(id), key);
    m.op(kUnit, xs::enter, id, key.data(), key.size());
}
for (std::uint32_t id : client.kept_and_changed) m.op(kUnit, xs::change, id, body_of(id), len_of(id));
m.finish();
send(client, msg.view());
```

Receiver: a target that maps ids to the client's own records, a cursor per
stream, and a receiver built for the link.

```cpp
struct ViewTarget {
    using tables = std::tuple<Head, Unit>;           // table K's record type
    template <unsigned K> static constexpr bool keyed = K == kUnit;   // units enter and leave
    XBuffer* buf;                                    // the client's view
    xs::XSyncSlots slots;                            // unit id -> index in units

    View& v() { return buf->root<View>(); }
    template <unsigned K> std::int64_t find(std::uint32_t id) {
        if constexpr (K == kHead) return id == 0 ? 0 : -1;
        else return slots.find(id);                  // -1: not held
    }
    template <unsigned K> auto& record(std::int64_t h) {
        if constexpr (K == kHead) return v().head;
        else return v().units[std::size_t(h)];
    }
    template <unsigned K> std::int64_t enter(std::uint32_t id) {          // keyed tables
        // A free slot is reused first; a new one (h == units.size()) grows units.
        return slots.enter(id, [&](std::int64_t h) { v().units.resize(std::size_t(h) + 1); });
    }
    template <unsigned K> void leave(std::uint32_t id, std::int64_t h) {   // keyed tables
        slots.leave(id, h);
    }
};

XReceiver<XLink::from_server> rx(XBuffer::maximum_capacity(1 << 20));
xs::XSyncCursor cursor;                              // which stream and version the view holds
ViewTarget t{&view};
if (xs::is_key(msg)) empty_units(t);                 // a key message enters every entry
if (XStatus r = rx.apply_sync(msg, cursor, t); !r) ask_for_key();   // r.code(), r.error().message
```

`apply_sync` is a member of `XReceiver<XLink::server_peer>` and
`XReceiver<XLink::from_server>`; on `from_client` it does not compile (a
server does not read a client's uplink as a shared delta). The tier follows
the link (admit by default, section 3); `apply_sync<XCheck::full>` checks
every body first.

### Changed members only

A server that keeps dirty flags (which members of a record changed this
tick) need not compare whole records. `XRecordDelta<T>::fields(members)`
turns a bit set of member indices (bit i is the i-th member in declaration
order, `member_count` of them, at most 64) into an `XSyncFields`: the plain
blocks and containers those members cover. Fields combine with `|=`.

```cpp
using RD = xs::XRecordDelta<Unit>;
const xs::XSyncFields f = RD::fields(unit_dirty_bits(u));   // bit i: member i changed
// Compare and sync only those members (the others must equal before already):
RD::diff(u, baseline_of(u), body, f);
// Or change the image every holder has and write the body in one step:
RD::update(image_of(u), f, body, [&](Unit& r) { copy_changed_members(r, u); });
```

- `diff(now, before, out, fields)` compares and copies only the given blocks
  and containers. When the other members of `now` and `before` are equal, it
  writes the bytes of `diff(now, before, out)`.
- `update(rec, fields, out, set)` keeps the given members' old bytes, calls
  `set(rec)` (which may change only those members) and writes the body that
  takes the old record to the new one: the bytes of `diff` against a copy of
  the old record. It returns false and writes nothing when the members did
  not change. There is no second record per entity to keep.
- A record of plain bytes only (`container_count == 0`) is encoded by
  `XSyncWriter::change` straight into the message.

The format is unchanged: a receiver cannot tell which way a body was written.

### The target

| Member | Called | Contract |
| --- | --- | --- |
| `tables` | compile time | `std::tuple` of the record types, one per table; 1 to 64 tables. Table numbers are the tuple indices and part of the protocol between the two ends |
| `keyed<K>` | compile time | whether entries of table K enter and leave (units in a view). A fixed table holds a fixed set of entries (the view's tiles, a header record): its ids are whatever `find` accepts |
| `find<K>(id)` | change and leave ops, fixed-table enters; every op under `full` | the entry's handle, `< 0` if the target does not hold `id` |
| `record<K>(h)` | once per op, after the op's enter | the record. The reference is used for that op only: adding an entry or a container operation may move the records |
| `enter<K>(id)` | keyed enters | adds the entry and returns its handle; `< 0` if it holds `id` already or cannot add it |
| `leave<K>(id, h)` | keyed leaves | drops the entry. The record is left as it is: clear it here (`XRecordDelta<R>::clear`) if free records must read as empty |
| `applied<K>(id, h)` | optional, after an enter or change wrote the record | e.g. copy a staging record back or mark the entry dirty |

- An enter clears the record (`XRecordDelta<R>::clear`) before its body
  applies: the one `enter` returns on a keyed table, the one `find` returns
  on a fixed table. A key body is the record from cleared, so a slot that is
  reused gets nothing of the entry that held it before.
- A handle stays valid until its own entry leaves: `leave` must not move
  other entries (no swap-with-last that changes another entry's handle).
- Handles are the target's own numbers: slot indices, indices into a span
  (tiles inside the view), anything that fits an `int64_t`.
- `xs::XSyncSlots` is the id -> slot index for a keyed table: open
  addressing, at most half full, so entering and leaving allocate nothing
  once it has grown; any 32-bit id; a slot that leaves is reused first
  (`XBasicSyncSlots<XSlotReuse::lowest>` reuses the lowest free slot
  instead). `find`, `enter(id, grow)`, `leave(id, h)` and `take(id, grow, added)`
  (find or enter in one lookup) are forced inline; `clear()` before a key
  message makes every slot free. Force the target's own `find`, `enter` and
  `leave` inline too: in a large translation unit whether they inline can
  depend on unrelated code.

### Streams, versions and key messages

A stream is the sequence of messages one server sends to one client. A key
message (`kFlagKey`) carries the stream id and enters every entry the client
holds; every other message has the next version of that stream. The cursor
keeps the stream and version of the last message applied.

- A message whose version is not the cursor's plus one, or a delta before any
  key, is rejected with `delta_base_mismatch` and changes nothing: ask the
  server for a key message (lost or reordered message, client restart).
- Before applying a key message (`xs::is_key(msg)`), empty the keyed tables;
  an enter of an entry still held is refused.
- A message `admit` rejects after its header may be partly applied: the
  cursor is then empty, so only a key message applies next.
- `cursor.clear()` forgets the stream (only a key applies next);
  `cursor.release_scratch()` frees the scratch `full` keeps.

## 2. Format XS 1.0

Frozen within 3.x. `tests/test_shared_delta_format.cpp` pins the bytes of a
key message and of the delta after it, for the sender and for both receive
tiers. A change to any of these bytes is a new format with its own format
byte; a 1.0 receiver rejects other format bytes (`unsupported_version`) and
unknown flags (`unsupported_flags`).

```
message  := 'X' 'S' u8 format (0x01) u8 flags
            [u64 stream, little-endian; only with KEY]
            varint version  varint count
            count x op
flags    := KEY (0x01): a key message; no other bit is defined
op       := u8 (table << 2 | kind)  varint id  [body]
kind     := enter 0 (body: the record from cleared)
            change 1 (body: the change against what the receiver holds)
            leave 2 (no body; keyed tables only)
body     := inline [conts]
inline   := varint n, n x { varint skip, u8 selector (nonzero),
                            one mask byte per selected word,
                            the masked bytes in order }
conts    := u8 m, m x { u8 (field << 2 | op), payload }    -- only when R has containers
            op 0 STR    varint len, len bytes                 (XString, assigned whole)
            op 1 VSET   varint n, n x (plain bytes of E)      (XVector<E>, replaced whole)
            op 2 VPATCH an inline section over the elements'  (same length, patched)
                        bytes [0, size * sizeof(E))
```

- Varints are LEB128, at most five bytes, values below 2^32.
  `XSyncWriter` writes the count as three varint bytes so the header length
  does not depend on it; a receiver takes any varint.
- Inline sections cover the record in 64-byte blocks counted from its first
  byte. `skip` counts the blocks skipped since the last one written; the
  selector has one bit per 8-byte word of the block; a mask has one bit per
  byte of the word (0xff: the eight bytes in order). Only plain bytes are
  ever written: scalars, enums with a fixed underlying type, bools (0 or 1),
  the same in nested records and arrays. Container heads and padding are
  never written as bytes.
- Containers are numbered in schema order, nested records walked in place;
  `field` values ascend within a body.
- Supported records: scalars, bools, fixed-underlying enums, nested ordinary
  records and arrays of them, `XString`, `XVector` of elements without
  containers; up to 8 KiB and 63 containers. Sets, maps and vectors of
  records with containers do not compile.

## 3. What the receiver checks

| Tier | Default on | Checks | A rejected message |
| --- | --- | --- | --- |
| `admit` | `server_peer`, `from_server` | header (magic, format, flags), stream and version, the op count against the bytes, and for every op its table, kind and id: change and leave of an entry not held, enter of a held one (the target refuses), unknown tables, kinds a table does not take, bytes after the last op | may be partly applied; the cursor is then empty |
| `full` | `apply_sync<XCheck::full>` on the same links | all of the above, every body against its record (an enter against a cleared one): blocks inside the record, only plain bytes masked, bools 0 or 1, container fields ascending and of the right op, lengths inside the message, VPATCH inside the current length; and that no entry appears twice in one message. Then the ops apply | changes nothing (records and cursor), except when the target refuses an enter while the checked ops apply |

`admit` trusts bodies: they are the sender's own, over a link the deployment
trusts (server to server, server to client over TLS or an internal network;
the trust model of [USER_GUIDE.md](USER_GUIDE.md) section 8). Bodies that are
not the sender's may be read past their end and may write any byte of a
record. Under `XOFFSET_FORCE_FULL_CHECK=1` (builds without `NDEBUG`, ASan
and TSan builds by default) `admit` runs the full check.

Errors (`XStatus::code()`): `bad_magic`, `truncated` (shorter than its header,
fewer ops than counted), `unsupported_version`, `unsupported_flags`,
`delta_base_mismatch` (no key yet, version gap), `invalid_payload` (bad op,
bad body, an entry twice, bytes after the last op, the target refused an
enter).

## 4. Guarantees

- After a message applies, every entry the target holds has the sender's
  record bytes for its plain bytes and the sender's container contents.
  `tests/test_shared_delta_sync.cpp` runs a stream of 1,500 messages with
  units entering, changing and leaving through both tiers and compares the
  two clients with the server after every message.
- What `full` accepts writes only plain bytes of the record type and changes
  containers through library operations in the receiver's arena: a buffer
  that holds the records stays one that `load_verified<T>` accepts.
  `fuzz_shared_delta` checks this for every message it accepts, and that
  `admit` gives the same records as `full` on every message `full` accepts.
- `full` rejects a malformed message without changing anything; `admit`
  rejects a message whose framing, stream, version or ops do not match what
  the receiver holds.
- No tier guarantees the content: a message that changes a value to another
  valid value is accepted.

## 5. Rules for senders and receivers

- One stream per client connection. The server keeps per client what the
  client holds (which entries, the stream's version) and sends a key message
  when the client joins or asks for one.
- An entry appears at most once per message (a leave and an enter of the same
  id belong to two messages).
- Table numbers and record types are part of the protocol: both ends need the
  same schema (the handshake of [SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md);
  on a mismatch send whole wires).
- The receiver's records are its own copy of the sender's: writing them
  locally makes the next change bodies apply to something the sender did not
  compare against.

## 6. Cost

The `bench/slg` viewport cases time the D3 rows (`SLG_D3=1`): the server's
once-per-tick compare and per-client messages (`DP.send`, `DC.send`,
`DS.send` for peace, conflict and siege) and each client's apply through
`apply_sync` on its view (`DPT.recv` and the other `T` rows: admit;
`DP.recv` and the others: full). At M scale a message averages about 70 bytes
in peace, 330 in conflict and 960 under siege, before zstd.

Instructions per delivery (GCC 16, M scale, callgrind), against the
hand-written receive loop the bench used in 3.4 (SLG340 baseline):

| Row | 3.4 loop | `apply_sync` | ratio |
|---|---:|---:|---:|
| `DPT.recv` (peace, admit) | 1204 | 1194 | 0.991 |
| `DCT.recv` (conflict, admit) | 4681 | 4487 | 0.959 |
| `DST.recv` (siege, admit) | 13767 | 13225 | 0.961 |
| `DP.recv` (peace, full) | 2052 | 2143 | 1.044 |
| `DC.recv` (conflict, full) | 11053 | 10613 | 0.960 |
| `DS.recv` (siege, full) | 32102 | 31615 | 0.985 |

The full tier also looks for duplicate entries in a message, which the 3.4
loop did not do. The server rows did not change (within 0.2%).

## 7. Not in XS 1.0

- Sets, maps and vectors of records with containers inside a record.
- Moving an entry between tables, or renumbering ids, in one op.
- Compression: messages are small and sent as they are; a transport can
  compress them (the bench counts zstd-3 bytes).
- A receiver that is not a target: the bench's and the example's targets are
  slots by id in an XBuffer view, but any storage that gives a stable
  reference per entry works.
