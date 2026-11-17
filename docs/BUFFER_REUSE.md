# Reusing one XBuffer (4.1.0, experimental API)

When many KB-sized messages are sent and received one by one, the fixed cost of creating and destroying
an `XBuffer` per message (mapping, owner registration, free table) exceeds the serialization itself.
Besides reusing mappings with `XBufferPool`, each side can keep one long-lived buffer and reuse it per
message:

```cpp
// Receiver
XBuffer in = XBuffer::create<Msg>(16 * 1024, cap);
for (const std::string& wire : incoming) {
    in.reload_verified<Msg>(wire);          // full validation, reuses in's mapping and registration
    handle(in.root<Msg>());
}

// Sender
XBuffer out = XBuffer::create<Msg>(16 * 1024, cap);
std::string frame;
for (...) {
    Msg& m = out.reset<Msg>();              // clear, rebuild a default-constructed root
    fill(m);
    out.save_verified_into<Msg>(frame);     // write into frame, reusing its capacity
    send(frame);
}
```

`TypedXBuffer<T>` has `reload_verified(wire)`, `reset()` and `save_verified_into(out)` with the same
names.

## Receiving directly into a reused slot

`reload_verified(wire)` needs one contiguous wire first, and the library then copies the payload into the
buffer. The transport layer can skip this copy and write received bytes straight into the buffer:

```cpp
XWireHeaderV2 h = read_header(conn);         // receive the wire header first, which has the three lengths
auto t = in.begin_verified_reload<Msg>(h.used_bytes, h.hole_count, h.extent_count);
std::memcpy(t.header_bytes().data(), &h, sizeof h);
recv_exact(conn, t.extent_bytes());          // extent table of a sparse wire
recv_exact(conn, t.payload_bytes());         // payload [48, used): dense in one piece; sparse extent o goes to o - 48
recv_exact(conn, t.hole_bytes());            // hole table
t.complete(t.header_bytes().size(), t.payload_bytes().size(),
           t.hole_bytes().size(), t.extent_bytes().size());
t.finish();                                  // the same validator as reload_verified
handle(in.root<Msg>());
```

- Acceptance, validation, the rejection set and the result are exactly the same as calling
  `reload_verified<T>()` on the same wire.
- `payload_bytes()` is `[48, used_bytes)` of the payload: the 48-byte payload header is not on the wire;
  the library writes it at `finish()`. Offsets in the extent table are payload offsets; the extent at
  offset `o` goes to `o - 48` of `payload_bytes()`.
- `complete()` reports the bytes actually written; for a sparse wire the payload count includes the gaps,
  that is, the logical length after all extents have landed (`used_bytes - 48`). A message shorter or
  longer than its header declares, not calling `complete()`, mismatched counts, or declared lengths that
  disagree with the header are all rejected.
- Destroying the token before `finish()` (for example when the connection drops midway) equals a failed
  reload: the buffer becomes empty, with no usable root.
- From begin until `finish()` succeeds, the buffer has no usable root; during that time the buffer must
  not be used, moved or destroyed, and a buffer can have only one token at a time.
- begin does not zero: a dense payload is overwritten in full by the transport layer; for a sparse wire,
  finish clears only the parts of the gaps dirtied by old content (pages never exposed are zero anyway).
- The header, extent table and hole table go into scratch kept by the buffer (returned together with the
  validation bitmaps by `release_reload_scratch()`), so dense wires request no host memory per message.
- A transport layer writing according to the unvalidated extent table must check itself that each extent
  falls inside `payload_bytes()`; an out-of-range table is always rejected at finish.

## Semantics

- Acceptance and validation of `reload_verified<T>(wire)` are exactly those of
  `load_verified<T>(wire, that buffer's MaximumCapacity)` (the same validator, the same rejection set). A
  wire larger than the buffer's `maximum_capacity` is rejected with `XBadAlloc`.
- `reset<T>()` is equivalent to `create<T>(current size, current MaximumCapacity)`.
- The output of `save_verified_into<T>(out)` is byte-identical to `save_verified<T>()`; `out` is a copy
  owned by the caller, unaffected by later changes to the buffer.
- After reuse, bytes left by earlier content never show up in the new content: gaps in the wire, the tail
  and bytes grown later all read as zero.

## Invalidation rules

After calling `reload_verified`, `begin_verified_reload` or `reset`:

- Root references, element pointers or references, `string_view`s from `view()` and borrowed spans
  obtained earlier are all invalid and must not be used again;
- `XHandle`s re-resolve against the new content (the same rule as after move assignment);
- The buffer's address (`get_address()`) does not change; the exception is a buffer in a movable slot
  receiving content that does not fit, which first switches storage (see USER_GUIDE section 3.4); the
  address then changes, and `epoch()` still increments.

## Failure

When `reload_verified` fails, it throws the same kinds of exceptions as `load_verified`, and the buffer
becomes empty with no usable root: `root<T>()` and `save_verified<T>()` throw `XException`, and
`has_root<T>()` returns false. Afterwards `reload_verified` or `reset<T>()` can be called again. The old
content is not kept; when "keep the old content on failure" is needed, keep using `load_verified` to build
a new buffer.

## In-process copies: `clone()` and `clone_into()`

```cpp
XBuffer copy = buf.clone();   // new owner
buf.clone_into(scratch);      // reuses scratch
```

`TypedXBuffer<T>` has `clone()` and `clone_into(TypedXBuffer<T>&)` with the same names.

- `clone()` creates a new owner with the same size, `maximum_capacity`, growth policy and pool as the
  source, and copies the source's used region over; `clone_into(dst)` copies the same content into dst,
  reusing dst's mapping, address and owner registration; dst's `maximum_capacity` and growth policy do
  not change, and its size becomes the source's size (not above dst's limit).
- The free table is carried over as is (an incomplete free table stays incomplete), so when the copy and
  the source then make the same changes, allocation positions and bytes are identical; the
  `save_verified` output equals the source's. Typed root access and schema binding carry over too: an
  unvalidated source still only allows `unsafe_root<T>()` after copying.
- It does not go through a wire and does not validate: the source is a buffer in use in this process, and
  its bytes are trusted as they are.
- The copy and the source are independent; changing or destroying either does not affect the other. Bytes
  left by dst's previous content never show up in the copy: the tail beyond the used region and bytes
  grown later all read as zero.

Invalidation rules: after `clone_into(dst)`, root references, element pointers or references,
`string_view`s from `view()` and borrowed spans into dst's previous content are all invalid;
`dst.epoch()` increments, `XHandle`s re-resolve against the new content; dst's `get_address()` does not
change. `clone()` does not change the source, and references into the source stay valid.

Failure:

- The source of `clone_into(dst)` is dst itself: throws `XException`, the buffer is unchanged.
- The source's used region exceeds dst's `maximum_capacity`, or the system allocation fails: throws
  `XBadAlloc`.
- The source has no arena: throws `XException`.
- Except for self-copy, after a failed `clone_into` dst is an empty arena with no root (the same as a
  failed `reload_verified`); afterwards `clone_into`, `reload_verified` or `reset<T>()` can be called
  again.
- A failed `clone()` only throws; the source is unchanged.

## Memory

Reuse does not automatically return committed pages: after receiving a large message and then small
ones, the pages the large message used stay committed. To return them, call `reclaim_tail_pages()`
(releases whole pages after the logical end), or destroy the buffer.

Validation uses two bitmaps (one bit per 8 bytes, one for live blocks, one for holes). For payloads up to
32 KiB the bitmaps are on the stack and no host memory is requested. For larger payloads,
`reload_verified(wire)` (the form without a resource argument) uses a block of bitmap memory kept by the
buffer itself, which grows to the largest payload received and is then reused as is, so looping reloads
no longer request validation memory from the host allocator per message. This memory is only temporary
host-side memory, not in the payload and not on the wire; it is freed with the buffer, and can be
returned early with `release_reload_scratch()`. `load_verified_with_resource` and
`reload_verified_with_resource` request a block from the resource the caller gives. Wires in canonical
layout (no holes, blocks laid end to end in visiting order, such as the output of
`XCompactor::compact`) take the cursor path, which records only one offset, does not touch the bitmaps,
and requests no validation memory at any size.
