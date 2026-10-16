# Migrating to stable XBuffer storage

All owning `XBufferCore`, `XBuffer`, `TypedXBuffer`, create, raw/verified load,
and compactor output paths now use a move-only VM reservation. Unsupported
platforms fail at compilation; reservation/initial enable failures throw
`XBadAlloc`. No hidden vector, heap, or relocation fallback is available.

## Capacity configuration

```cpp
XBuffer ordinary(4096); // max(initial, 64 MiB) logical hard limit
XBuffer small(4096, XBuffer::maximum_capacity(64 * 1024));
XBuffer large(4096, XBuffer::maximum_capacity(512 * 1024 * 1024));
```

The hard limit is checked before page rounding and may not exceed
`XBufferCore::MAX_PAYLOAD` (`0x7ffffff8`). A limit smaller than the initial
payload or the required arena/bootstrap allocations is rejected. Automatic mode
may expand an initially small logical length during bootstrap. There is no
method to change the limit after reservation. Choose another buffer and copy
or compact into it when a different limit is required.

`XOFFSET_DEFAULT_MAXIMUM_CAPACITY` changes the default at compilation. It must
have the same value in every translation unit. The default and legacy numeric
reservation hints produce `max(initial, default, hint)` as the final hard limit.
The historical `XBuffer::max_capacity(bytes)` is still a minimum hint, so a
small old hint does not silently become a smaller hard maximum. To request an
exact maximum, use the distinct `maximum_capacity(bytes)` wrapper.

`compute_reservation`, `MIN_RESERVE`, `MAX_RESERVE`, and `GROWTH_HEADROOM`
remain available for source compatibility as the historical vector hint
formula; they no longer control storage selection or its default limit.

Create and load support explicit limits:

```cpp
auto buffer = TypedXBuffer<Player>::create(4096,
    XBuffer::maximum_capacity(8 * 1024 * 1024));
auto wire = buffer.save_verified();
auto loaded = TypedXBuffer<Player>::load(wire,
    XBuffer::maximum_capacity(8 * 1024 * 1024));
```

Raw `load_unverified`, `XBuffer::load_verified<T>`, and the corresponding span
overloads also accept this wrapper. Without an override, historic wire
reservation fields remain accepted as minimum hints; new saves normalize the
field to the payload length, keeping OS metadata entirely process-local.

## Replacing direct vector access

Before:

```cpp
auto* storage = buffer.get_buffer();
consume(storage->data(), storage->size());
auto capacity = storage->capacity();
```

After:

```cpp
auto storage = buffer.get_buffer(); // std::span<char>; const buffer gives span<const char>
consume(storage.data(), storage.size());
auto capacity = buffer.maximum_capacity();
auto owned = buffer.copy_bytes(); // std::string, O(logical length) copy
```

The span is a direct borrowed view, not a snapshot. It has a fixed length:
obtain another view after logical growth or shrink. Do not access a retained
span past the buffer's current logical length. Adjust lengths with `grow` or
`shrink_to_fit`, not vector `resize`/`reserve`. Reservation slack and inaccessible
pages never appear in returned spans or exported bytes.

There was no `vector&&` owning constructor in the baseline, and none is added.
Passing a vector's `data()` and `size()` uses the byte-copy constructor; it
preserves the input, including on failure, and does not adopt its allocation.

## Pointers, handles, and ownership

`grow`, logical `shrink_to_fit`, `save`, and `save_verified` preserve the base
and existing live allocation addresses. They do not increment the epoch.
String/vector reserve can now preserve element addresses when its current
allocation is the physical tail and the new capacity fits the existing logical
arena, or Automatic mode can enable enough pages within the hard limit. A non-tail allocation still uses the existing
migration path. Reserve, rebuild, insertion/erase, assignment, and replacement
may therefore still invalidate element pointers and references. Growth does
not stabilize every element or avoid physical page faults.

Allocator requests align the actual allocation address, including alignments
larger than an OS page. Alignment padding must fit the logical arena before
allocation succeeds. Local byte-copy-safe roots can use larger alignment;
verified v1 wire records retain the existing maximum alignment of eight bytes.
Raw loading is trusted interpretation and cannot make an incompatible address
or type layout safe.

Move construction transfers the reservation without copying its payload.
Move assignment and swap change which storage belongs to each C++ object;
object-local epochs advance, and handles resolve again. Handles follow their
original C++ object: source handles become empty after a move; destination
handles resolve a compatible replacement root. Checked handles reject a
replacement of another type or unverified raw bytes. `unsafe_handle` and direct
Core handle construction retain trusted raw interpretation semantics.
Local roots without registered schema names support `const T` handles through
cv-normalized identity. Registered schema and wire type rules are unchanged.
Destroying the owning C++ object invalidates handles even if its reservation
was moved elsewhere. Raw allocation pointers follow the actual reservation,
not the C++ object: they remain usable through an ownership transfer until that
storage is released. A raw allocator object is bound to the transferred arena,
and likewise requires that arena to remain alive.

Compaction creates another reservation. Keeping the original buffer alive
preserves its existing data; assigning the compacted result over it releases
the old reservation. No in-place compaction promise is made.

## Failure, memory, and transport costs

`grow(0)` succeeds without work, including on an empty default object.
Nonzero growth on an empty or moved-from object fails. Exceeding the hard limit
or failing page enable/preparation returns `false`, preserving the old logical
payload and allocator state. Enabled pages may remain after a preparation
failure, and `accessible_bytes()` records them. Deterministic, thread-local test
hooks in `detail` inject reserve, enable, and post-enable preparation failures.

Logical shrink retains physical pages. It keeps pages enabled
and does not promise RSS reduction. Regrowth clears the newly exposed range,
including previously used bytes. POSIX `mprotect` changes permissions; success
does not insure future physical allocation against OS out-of-memory failures.
Windows commit charge and Linux/macOS permission enablement are different
concepts. No external `mprotect`/unmapping of owned reservations is supported.

Save keeps the v1 normalized format and copies the logical segment into its
output. Verified save writes its existing header and payload directly into the
final string, without an intermediate payload string. Fresh-owner load copies
input over the complete logical range without first clearing it, and retains
all current type/schema/structure validation. Neither path
reads nor exports the reservation tail, and neither is advertised as zero-copy.
No sparse format or new scatter/gather protocol is introduced.

The owner registry uses a synchronized hash index of exact arena base addresses.
Its mutex and index have process lifetime, including global-buffer shutdown.
Creating/loading a new owner can fail with `XBadAlloc` if index insertion cannot
allocate; replacement preserves the old owner on that failure. Move, swap, and
destruction do not allocate index nodes. Independent buffers can have independent
thread lifetimes; concurrent use or destruction of the same buffer still needs
synchronization.

## Optional growth, reclamation, and reuse

`maximum_capacity(n).with_growth(XGrowPolicy::Automatic)` applies to Core and
derived constructors, create, raw load, verified load, and typed load. Default
and legacy constructors remain Explicit. `growth_policy()` and
`set_growth_policy()` query/change the policy without changing the fixed limit.
Automatic growth doubles the logical extent or fits the requested allocation,
whichever is larger, bounded by that limit. It never relocates the reservation.
New-block wire allocations try current capacity and release cached free blocks before
growing. Eligible live tail allocations can grow in place.

A hard-limit or OS enable/preparation failure throws `XBadAlloc`. An OS failure
during tail expansion is propagated directly rather than silently retried via
another allocation. Existing values and uncompleted allocation metadata remain
valid; cache ownership/bookkeeping may change before a later allocation failure.
As with Explicit mutations, exceptions from later element construction can leave
increased capacity. Move/swap transfer growth policy; moved-from objects reset to
Explicit. Compaction produces an independent, default-configured buffer.

`reclaim_tail_pages()` preserves logical size, base, epoch, and live contents.
Call `shrink_to_fit()` or save first to reduce the logical end if desired. Only
whole pages beyond that end can be discarded; the last partial page stays intact.
The result reports `NoOp`, `Reclaimed`, `Failed`, or `DiscardedButAccessible` and
attempted/discarded/inaccessible byte counts. `complete()` is true for NoOp or
Reclaimed. Counts describe completed OS operations, not measured RSS. A Linux
discard followed by protection failure is retryable and retains truthful
accessible accounting. Regrowth still clears the newly exposed bytes. Default
save/shrink retain pages; physical reclamation trades idle memory for later
faults and initialization time.

`XBufferPool pool({count, reserved_bytes, accessible_bytes})` creates a shared,
bounded cache; `.with_pool(pool)` enables it for a buffer configuration. All
three limits apply to idle mappings, not live owners. Capacity matching is exact.
`pool.stats()` reports cached counts/bytes and cumulative hits/misses;
`pool.clear()` releases idle mappings. Live owners remain usable and can return
to the pool afterward. Their shared handle keeps the pool state alive. Empty
default pool handles allocate nothing and disable pooling. A pool reserves its
slot metadata once; return/move/swap do not allocate. Pool construction itself
can throw `std::bad_alloc`. Buffer acquisition translates allocation failures to
`XBadAlloc`. Reused normal creation clears logical bytes; load copies every input
byte. Retained dirty tails stay outside the logical range and are cleared before
regrowth exposes them. Destroying a buffer invalidates its pointers even when
its address is subsequently reused by the pool. Pools synchronize their own
state; each live buffer still needs external synchronization.

The existing first-fit, whole-block free-list policy remains in production.
Best-fit/splitting/Core coalescing prototypes passed correctness checks but caused
measured CPU and incremental-growth space regressions; they were not adopted.
The frozen wire layout and admission rules are unchanged. Save/load still
normalize allocator state and may lose process-local Core free-list opportunities.

On POSIX, a fresh, nonpooled load with input length exactly equal to its maximum
capacity combines reservation and initial page enabling. Other loads retain the
reserve/enable path, and all loads preserve validation and failure semantics.

See [follow-up results](VM_MEMORY_REUSE_RESULTS.md) for current adoption evidence;
[the A–D results](VM_STORAGE_IMPROVEMENTS_RESULTS.md) remain historical evidence.
