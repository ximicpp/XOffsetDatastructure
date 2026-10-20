# W9 notes — semantic and contract changes vs C14 and vs R1+M2

**Composition.** W9 = R1+M2 (`/Volumes/XOffsetCS/candidate-r1m2-20261020`) + G1
+ G + MV. There is **no canonical window**: no `CanonicalWindow`, no
`map_ordinary` relocation, no window macros and no window tests.

**R1 and M2 relative to C14.**
* R1 moved the canonical lookup into an inline acquire check plus a
  `noinline` active helper.
* M2 resolves `data()` once per vector node in the owns recursion.
* Neither changed semantics. Therefore every item below is a change both vs
  C14 and vs R1+M2, unless it says otherwise.

## 1. Receive pointer provenance: degraded to the flat-address model (contract change)

**C14 / R1+M2 behaviour.** While any receive owner was live
(`canonical_active_count_ != 0`), internal resolution could return pointers
derived from the receive storage's placement `std::byte[MAX]` array. This
applied to rel32 heads, free-list blocks, the `try_expand` allocation header,
wire validation (`checked_relative`, `register_allocation`, the validated end),
the core-lifetime helpers and `receive_raw::reactivate_inline_prefix`.

**W9 behaviour.** Every internal resolution is flat `origin + delta`, for
ordinary and receive storage alike. There is no canonical lookup, no atomic and
no compare.
* Receive data therefore relies on the same disclosed, implementation-defined
  pointer-from-address behaviour as ordinary loads.
* There is no additional provenance or standard-conformance promise for
  receive data.
* README.md and docs/ALPHA_COMPATIBILITY.md say so, as do the comments at
  `resolve_rel32`, the public `canonical_bytes_at`, `for_receive`,
  `core_lifetime` and `receive_raw`.

**Unchanged for source compatibility.**
* The public `XBufferCore::canonical_bytes_at` API and its registry-based
  semantics: R1's inline acquire check plus the active helper with the cold
  registry and TLS cache rules.
* Canonical registration and marking of receive owners
  (`mark_canonical_registration`, `canonical_active_count_`).
* The receive storage still begins and registers its `std::byte[MAX]` array.
* The raw receive validator and activator inside `finish_verified` still form
  pointers from the receive token's own byte-array origin. This is validation
  of not-yet-published bytes, not library resolution of a live buffer.

**Tests.**
* No test assertion had to change. The `test_canonical_*` tests exercise the
  public `canonical_bytes_at` semantics, which are unchanged, and they pass.
* Receive tests compare data pointers numerically; flat and array-derived
  pointers are numerically equal, so their results are identical.

## 2. Wire validation extent check (stricter, only for corrupted extents)

`verify_structural_wire_graph` now checks `used_end > segment_size()` for
every storage before computing the flat validated end. R1+M2 applied this check
only to canonical (receive) storage.
* For a valid ordinary arena it cannot fire: `used_end ≤ size` is an
  allocator invariant and is checked at load.
* So it only changes the outcome for a corrupted live extent. That case now
  raises "wire validation: payload extent out of bounds" instead of building
  an out-of-bounds validation context.

## 3. G1: growth zeroing elision (no content change)

`VirtualMemoryStorage::zero_from_` records the lowest offset from which every
reserved byte is still OS-zero. `grow_to` clears only [logical, min(new,
zero_from_)).

| Event | Effect on `zero_from_` |
|---|---|
| `from_bytes` | raised to the copied length |
| pooled owner | carries its high-water mark through the pool (`swap_storage`) |
| successful tail reclaim | lowered to the retained size |
| `for_receive` | set to `maximum_capacity`, so receive storage keeps full zeroing: the whole maximum is read/write before a transport writes |

* Observable contents are identical: every newly exposed byte still reads
  zero. Fewer pages are touched, so minor faults and RSS are lower.
* The G1-only diagnostic variant, which lacks the receive line, is not
  shippable.
* Tests (`test_virtual_memory_storage`, C++17 storage-only): shrink and
  regrow, pool reuse, partial and full `from_bytes`, reclaim, receive growth
  after writes past the logical length, and pooled receive storage.

## 4. G: non-throwing growth probe (removes one callback point)

In `allocate_dynamic_bytes`, a failed no-growth Core attempt now returns
nullptr through a noexcept probe (`allocate_aligned_body<true>`). It no longer
throws and catches an `XBadAlloc`. The recovery is unchanged: revalidate,
`release_cached_blocks` if cached bytes exist, revalidate, then allocate with
growth.

Observable changes:
* That step no longer constructs a diagnostic exception, so it makes no host
  allocation and gives new_handler no callback point.
* A host OOM in that diagnostic construction, which previously surfaced as
  `std::bad_alloc`, cannot happen any more.

Final error types and their order are unchanged.

Tests:
* `test_allocator_failures`: zero host allocation attempts during growth
  (this fails on the base), and release-before-grow order under Automatic and
  Explicit policies.
* `test_owner_identity::callback_during_allocation_diagnostic` is rewritten:
  the probe path asserts zero callbacks, and the four reentrancy actions run
  at the remaining callback point (the `release_cached_blocks` reservation)
  with the original outcomes.

## 5. MV: move-assignment fast path (no semantic change)

* **Inline leaf owns.** `move_assignment_owns_address_fast` is the generic
  function's single containment test for string heads and vectors of
  trivially copyable elements; every other type calls the generic function.
  The order of the two directions is unchanged.
* **`set_rel32`.** Inline, with the throw moved to a cold function; same type
  and message.
* **Empty destination.** The release case for an empty destination is
  inlined; a live destination still calls the unchanged `release()`.
* **Binding reuse.** When both heads have the same non-null state, the
  source's binding is the destination's (same field, no intervening write).

Tests (`test_same_arena_move`): the fast and generic owns are compared address
by address within ±24 bytes of the storage edges; leaf descendant aliasing
takes the copy fallback; two distinct states in one arena are not treated as
one allocator; same-state transfer is covered.

## 6. Fork policy (documented; no behaviour change)

* The owner registry and VmStoragePool have no atfork handlers (comment at
  `registry_state`).
* A child forked while another thread was inside the library may only exec.
  Forking with no other thread inside the library is supported.
* `test_verified_receive::fork_after_receive` (POSIX): the parent holds a
  receive; the child does an ordinary verified load, a new receive and an
  edit, and reads the inherited receive.

## 7. Unchanged

* **Receive error types (as in R1+M2).** VM and storage failures in
  `begin_verified_receive` throw `std::bad_alloc`. `XBadAlloc` is used for its
  size-limit checks and for finish-time registry preparation.
* V1 wire format, emitted bytes, first-error order of valid inputs, the
  allocator algorithm and owner-identity rules.
* No per-process window reservation cost: there is no window at all.
* R1+M2 receive pooling and storage placement.
