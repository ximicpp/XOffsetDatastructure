# Shared deltas (XS 2.0)

A server that sends every client its own view of a shared world (the units,
marches and tiles a client sees) should not encode an entity once per
client. A shared delta is written per record: the server encodes each
changed entity once per tick, and every message whose client holds the
entity carries a copy of the same bytes. The client keeps its own layout:
it finds the record by the op's id in its own storage and sets it in place.

[DELTA_SYNC.md](DELTA_SYNC.md) is the other kind of delta: payload bytes at
payload offsets, applied to a copy with the sender's layout (one baseline per
receiver). Use it for a player's own state and between servers that keep a
replica; use shared deltas for views of a shared world.

Header: `xoffset_shared_delta.hpp` (sender side, record codec) and
`xoffset_receiver.hpp` (`XReceiver::apply_sync`). Both schema backends (C++26
reflection, C++20 `XOFFSET_RECORD` field lists) and builds without
exceptions. Example: `examples/shared_delta.cpp`.

## 1. Use

### Sender

Once per tick, each changed entity's ops go into a body store (`XSyncOut`):
a change op from the entity's dirty members, and a whole op for clients the
entity enters. Then one `XSyncWriter` per client copies the ops it needs.

```cpp
namespace xs = XOffsetDatastructure::xsync;
enum : unsigned { kHead = 0, kUnit = 1 };          // table numbers, fixed by the protocol
using UB = xs::XRecordBody<Unit>;

xs::XSyncOut store, msg;
// Once per tick: a unit's change op from its dirty flags (bit i: member i,
// declaration order), written once for all its holders.
for (Unit& u : changed) {
    const std::size_t at = store.size();
    remember(u, at, xs::put_change_op(store, u.id, u, dirty_members(u)));
}
// Per client: the head (table 0), then sections of ops.
xs::XSyncWriter m(msg, head, ++client.version);    // key: (msg, head, version, stream)
m.leaves(kUnit, client.left_slots.data(), client.left_slots.size());
m.begin(kUnit, xs::enter);
for (Unit* u : client.entered) m.whole(client.slot_of(*u), *u);      // or a shared whole op
m.end();
m.begin(kUnit, xs::change);
for (Unit* u : client.kept_and_changed) m.op(store.data() + at_of(*u), len_of(*u), client.slot_of(*u));
m.end();
m.finish();
send(client, msg.view());
```

- `XRecordBody<R>`: `put_whole` / `apply_whole` / `check_whole` (every
  plain byte and container), `put_change` / `apply_change` / `check_change`
  (the members a mask names), `changed(now, prev)` (the members that
  differ), `put_diff` (records without containers: the members that differ,
  and `prev` takes them), `clear`, `same`.
- `put_whole_op`, `put_change_op`, `put_diff_op` append an op (u32 id and
  the body) to a store and keep `kSyncSlack` bytes past it.
- `XSyncWriter::op(op, len)` copies a shared op; `op(op, len, id)` copies it
  under another id (the client's slot); `op_upto<N>` copies N bytes with no
  branch on the length (0: no op); `reserve(n)` then `op_in_room` skips the
  capacity check per op. `whole`, `change` and `diff` encode in place.
  `begin` / `end` frame a section (an empty one is dropped); `leaves` writes
  a section of leave ops. `finish(version)` writes the head's version at the
  end (a sender that touches the client's connection state only after the
  ops, once a prefetch of it has landed).

### Ids and slots

An op's id is what the receiver's `find` takes. Two ways:

- **Slots (fastest).** The server gives each client's entries their slots
  in that client's storage (free slots of the client, taken when an entry
  enters, given back when it leaves) and writes the slot as the id. The
  client indexes its storage directly: `find` is a bounds check and `enter`
  makes room for the slot. The server keeps this per client anyway (what
  each client holds); a slot that leaves is given out again from the next
  message on.
- **Entity ids.** The client looks ids up in an index of its own,
  `xs::XSyncSlots` (open addressing, at most half full; entering and leaving
  allocate nothing once it has grown; `clear()` before a key message).

### Receiver

A target that maps ids to the client's own records, a cursor per stream,
and a receiver built for the link.

```cpp
struct ViewTarget {
    using tables = std::tuple<Head, Unit>;           // table K's record type; table 0 is the root
    template <unsigned K> static constexpr bool keyed = K == kUnit;   // units enter and leave
    XBuffer* buf;                                    // the client's view
    std::vector<bool> held;                          // slot held

    View& v() { return buf->root<View>(); }
    template <unsigned K> std::int64_t find(std::uint32_t id) {
        if constexpr (K == kHead) return id == 0 ? 0 : -1;
        else return id < held.size() && held[id] ? std::int64_t(id) : -1;
    }
    template <unsigned K> auto& record(std::int64_t h) {
        if constexpr (K == kHead) return v().head;
        else return v().units[std::size_t(h)];
    }
    template <unsigned K> std::int64_t enter(std::uint32_t slot) {     // keyed tables
        if (slot >= v().units.size()) { v().units.resize(slot + 1); held.resize(slot + 1); }
        held[slot] = true;
        return slot;
    }
    template <unsigned K> void leave(std::uint32_t slot, std::int64_t h) {   // keyed tables
        xs::XRecordBody<Unit>::clear(v().units[std::size_t(h)]);
        held[slot] = false;
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

### The target

| Member | Called | Contract |
| --- | --- | --- |
| `tables` | compile time | `std::tuple` of the record types, one per table; 1 to 64 tables. Table 0 is the root record: no containers, not keyed, carried whole by every message. Table numbers are the tuple indices and part of the protocol |
| `keyed<K>` | compile time | whether entries of table K enter and leave (units in a view). A fixed table holds a fixed set of entries (the view's tiles): its ids are whatever `find` accepts |
| `find<K>(id)` | the root (id 0), change and leave ops, fixed-table enters; every op under `full` | the entry's handle, `< 0` if the target does not hold `id` |
| `record<K>(h)` | once per op, after the op's enter | the record. The reference is used for that op only: adding an entry or a container operation may move the records |
| `enter<K>(id)` | keyed enters | adds the entry and returns its handle; `< 0` if it cannot add it |
| `leave<K>(id, h)` | keyed leaves | drops the entry. The record is left as it is: clear it here (`XRecordBody<R>::clear`) if free records must read as empty |
| `applied<K>(id, h)` | optional, after the root, an enter or a change wrote the record | e.g. copy a staging record back or mark the entry dirty |

- An enter sets every plain byte and every container of the record (a
  container the body leaves out is emptied), so a reused slot keeps nothing
  of the entry that held it before.
- A handle stays valid until its own entry leaves: `leave` must not move
  other entries.
- Handles are the target's own numbers: slot indices, indices into a span
  (tiles inside the view), anything that fits an `int64_t`.
- Force the target's `find`, `enter` and `leave` inline: in a large
  translation unit whether they inline can depend on unrelated code.

### Streams, versions and key messages

A stream is the sequence of messages one server sends to one client. A key
message (`kFlagKey`) carries the stream id and enters every entry the client
holds; every other message has the next version of that stream. The cursor
keeps the stream and version of the last message applied.

- A message whose version is not the cursor's plus one, or a delta before any
  key, is rejected with `delta_base_mismatch` and changes nothing: ask the
  server for a key message (lost or reordered message, client restart).
- Before applying a key message (`xs::is_key(msg)`), empty the keyed tables.
- A message `admit` rejects after its head may be partly applied: the
  cursor is then empty, so only a key message applies next.
- `cursor.clear()` forgets the stream (only a key applies next);
  `cursor.release_scratch()` frees the scratch `full` keeps.

## 2. Format XS 2.0

`tests/test_shared_delta_format.cpp` pins the bytes of a key message and of
the delta after it, for the sender and for both receive tiers. A change to
any of these bytes is a new format with its own format byte; a 2.0 receiver
rejects other format bytes (`unsupported_version`, XS 1.0 among them) and
unknown flags (`unsupported_flags`). All integers are little-endian.

```
message  := 'X' 'S' u8 format (0x02) u8 flags u32 version
            [u64 stream; only with KEY]
            root
            section*                                  -- until the end of the message
flags    := KEY (0x01): a key message; no other bit is defined
root     := the whole body of table 0's record
section  := u32 (count << 8 | table << 2 | kind)  count x op
kind     := enter 0, change 1, leave 2 (keyed tables only); tables 1 to 63
op       := leave:  u32 id
            enter:  u32 id  whole(R)
            change: u32 id  change(R)
whole(R) := R's plain bytes, in order          -- scalars, enums, bools; no padding, no container heads
            [cmask  per set bit: container]     -- only when R has containers
change(R):= mmask  per set bit, plain members first: member bytes (sizeof)
                    then members with containers: container, or whole(member type) for a nested record
cmask    := one bit per container of R (schema order, nested records and arrays walked in place);
            u8, u16, u32 or u64 by count; a container whose bit is clear is empty
mmask    := one bit per member of R (declaration order); u8, u16, u32 or u64 by count
container:= XString: u32 length, bytes | XVector<E>: u32 count, count * sizeof(E) bytes
```

- A whole body sets every plain byte and every container of the record, so
  an enter needs no clear.
- A change body names members, not bytes: a plain member (a scalar, a nested
  record or array without containers) is its `sizeof` bytes; a string or
  vector member is replaced whole; a nested record that holds containers is
  its own whole body.
- A section of the same table and kind may appear more than once, in any
  order; a section of no ops is allowed (`XSyncWriter` drops them).
- Supported records: scalars, bools, fixed-underlying enums, nested ordinary
  records and arrays of them, `XString`, `XVector` of elements without
  containers; 1 to 64 members and at most 64 containers per record. Sets,
  maps and vectors of records with containers do not compile.

## 3. What the receiver checks

| Tier | Default on | Checks | A rejected message |
| --- | --- | --- | --- |
| `admit` | `server_peer`, `from_server` | head (magic, format, flags), stream and version, the root in the message, and for every section its table and kind, for every op its id: change and leave of an entry not held, enter the target refuses, sections on the root or unknown tables, kinds a table does not take, a section or op past the end | may be partly applied; the cursor is then empty |
| `full` | `apply_sync<XCheck::full>` on the same links | all of the above, the root and every body against its record type (bools 0 or 1, mask bits only for members and containers that exist, lengths inside the message), keyed enters of entries not held, and that no entry (table, id) appears twice in one message. Then the root and the ops apply | changes nothing (records and cursor), except when the target refuses an enter while the checked ops apply |

`admit` trusts bodies: they are the sender's own, over a link the deployment
trusts (server to server, server to client over TLS or an internal network;
the trust model of [USER_GUIDE.md](USER_GUIDE.md) section 8). Bodies that are
not the sender's may be read past their end and may write any byte of a
record. Under `XOFFSET_FORCE_FULL_CHECK=1` (builds without `NDEBUG`, ASan
and TSan builds by default) `admit` runs the full check.

Errors (`XStatus::code()`): `bad_magic`, `truncated` (shorter than its head
and root, a section or op past the end), `unsupported_version`,
`unsupported_flags`, `delta_base_mismatch` (no key yet, version gap),
`invalid_payload` (bad table or kind, ids, a body that does not check, an
entry twice, the target refused an enter).

## 4. Guarantees

- After a message applies, every entry the target holds has the sender's
  record bytes for its plain bytes and the sender's container contents.
  `tests/test_shared_delta_sync.cpp` runs a stream of 1,500 messages with
  units entering, changing and leaving through both tiers and compares the
  two clients with the server after every message.
- What `full` accepts writes only plain bytes of the record type (bools 0
  or 1; padding inside a plain member is copied as sent) and changes
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
  client holds (which entries, their slots, the stream's version) and sends
  a key message when the client joins or asks for one.
- An entry appears at most once per message: a slot that leaves is given
  out again from the next message on, and a leave and an enter of the same
  id belong to two messages.
- Table numbers and record types are part of the protocol: both ends need the
  same schema (the handshake of [SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md);
  on a mismatch send whole wires).
- The receiver's records are its own copy of the sender's: a change body
  sets the members it names whatever the receiver holds.

## 6. Cost

The `bench/slg` viewport cases time the D3 rows (`SLG_D3=1`): the server's
once-per-tick encoding and per-client messages (`DP.send`, `DC.send`,
`DS.send` for peace, conflict and siege, slot bookkeeping included) and each
client's apply through `apply_sync` on its view (`DPT.recv` and the other
`T` rows: admit; `DP.recv` and the others: full). The rival is the shared
zpp_bits sender of the same cases. CHANGELOG.md has the measured ratios.

## 7. Not in XS 2.0

- Sets, maps and vectors of records with containers inside a record.
- Patches inside a string or vector: a changed container is sent whole.
- Moving an entry between tables in one op.
- Compression: messages are small and sent as they are; a transport can
  compress them (the bench counts zstd-3 bytes).
- A receiver that is not a target: the bench's and the example's targets are
  slots in an XBuffer view, but any storage that gives a stable reference per
  entry works.
