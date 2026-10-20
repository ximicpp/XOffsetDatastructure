# W7 proof: the load-free internal canonical resolver with LAZY window reservation

W7 = W6 (W2 window, G1, G, MV, 32 GiB macOS window) with one change in the
storage layer:

* the canonical window is reserved **lazily, by the first receive slot
  request** (`CanonicalWindow::acquire`, called only by `for_receive`), and
* ordinary (non-receive) storage **always** avoids the window address range,
  whatever the window state (untried, reserved, or unavailable).

`VirtualMemoryStorage` creation and `from_bytes` no longer touch the window
state at all: `ensure_attempted()` is removed and `map_ordinary` no longer
reads `canonical_window_array`. The resolver is unchanged:

```
internal_bytes_at(t, n):
    off = t - BASE
    if (off <= BYTES && n <= BYTES - off)          // compile-time constants
        return *std::launder(reinterpret_cast<std::byte(*)[BYTES]>(BASE)) + off;
    return nullptr;
```

The in-window branch is defined only while the window array object
`std::byte[BYTES]` (begun at BASE by `reserve_locked`) exists. Once created it
is never destroyed or unmapped. Laundering the array type is deliberate:
objects created later inside the window are nested within that array, so the
array stays alive and keeps providing storage. The public `canonical_bytes_at`
still loads `canonical_window_array` in its in-window branch and returns
nullptr while the window is unreserved, so it is safe for any address.

## Invariant W' (unconditional, enforced in code)

> No live ordinary library reservation touches the closed range
> [BASE, BASE+BYTES]. This holds at all times, whatever the window state.

1. **Ordinary reservations** (the `VirtualMemoryStorage` constructor, also used
   by `from_bytes`, both the PROT_NONE path and the full-copy RW path) are made
   only by `map_ordinary`.
   * It maps once (at the window base when the test hook `WindowCollide` is
     armed, otherwise wherever the kernel chooses).
   * If the result touches the closed range (two compares in `touches`), that
     mapping is kept, so the kernel cannot hand it back. The reservation is
     then retried at hints just above (BASE+BYTES+granule) and just below
     (BASE−length−granule) the window. Any candidate that still touches is
     unmapped. Finally the colliding mapping is unmapped.
   * The result is either a mapping that does not touch the range, or nullptr,
     in which case the caller throws `std::bad_alloc`.
   * The colliding mapping is never published to a `VirtualMemoryStorage` and
     no pointer into it is ever formed or resolved.

   Closed-range consequences:
   * A mapping that ends exactly at BASE (its one-past address would be
     in-window), or that starts exactly at BASE+BYTES, is also relocated.
   * When the window is reserved, the kernel can still place a mapping right
     after it; the guard now relocates that one as well (W6 accepted it). This
     is stricter, and the test was updated accordingly.
2. **Window slots** (`for_receive` → `CanonicalWindow::acquire`) are created
   only after `reserve_locked` has succeeded under the window mutex, and only
   after `canonical_window_array` has been stored.
3. **Pool reuse** hands out only reservations made by step 1. Window slots are
   never pooled.
4. **No other library code maps memory.** `mmap` / `VirtualAlloc` appear only
   in `xoffset_virtual_memory.hpp`, and every XBufferCore payload is a
   `VirtualMemoryStorage`.

**Lemma (W7).** Suppose `t` lies in [S.base, S.base+S.reserved] for some live
library reservation S (closed interval, which covers the n == 0 one-past case),
and [t, t+n] lies in the window.
* Then S touches [BASE, BASE+BYTES].
* By W', S is not ordinary, so S is a window slot.
* By (2) the window was reserved, so the array exists.
* Once the window is reserved, every in-window address belongs to the array.

Unlike W2, the lemma no longer depends on the window state at the time S was
mapped. So the reservation can be lazy, and ordinary storage needs no
window-state load.

As in W2, each internal call site only has to show that its target is bounded
by a live library reservation.

## Call sites (re-checked under W')

The call sites are unchanged from W2 (`grep canonical_bytes_at_internal |
wire_canonical_bytes` gives the same list). For every row the "bounded by live
library storage" argument is the one used in W2, and the conclusion now follows
from the W7 lemma.

| Site | Target | Bound |
|---|---|---|
| `resolve_rel32` / `_const` (data, allocator_state, arena, allocator_arena) | `origin + delta`, n = sizeof(T) | The delta is a library-written head field, or a wire field that validation has already bounded to [ctx.base, ctx.end]. Heads point into their binding's arena. |
| `freelist_block_from_offset` ×2 | `state + offset`, n = 16 | Runtime: the arena's own free list. Validation: `offset ∈ [arena_begin, arena_end−16]` and `arena_end ≤ ctx.end − state`, with the existing check order. |
| `try_expand` header | `ptr − 16`, n = 16 | `ptr` is a live allocation of this arena, and `address > state` is checked first. |
| `checked_relative`, `register_allocation` (wire) | untrusted, after `validate_storage_range` | See "wire routing" below. |
| `verify_structural_wire_graph`, first call | `xbuf.get_address()`, n = 1 | The storage base of a live XBufferCore. An empty buffer gives nullptr ≠ BASE. |
| `verify_structural_wire_graph`, end | `base + used`, n = 0 | Only taken when the base is canonical, i.e. a slot. `used ≤ segment_size()` has been checked first. |
| `core_lifetime` begin/rebuild/relocate helpers | destination slot, n ≥ 1 | Arena allocations or element/member slots inside the same arena. Capacity 0 returns early. |
| `receive_raw::reactivate_inline_prefix` | `data`, n = live·sizeof(T) | An array just begun in an arena allocation of a receive owner. |

**Wire routing by `ctx.base` (`wire_canonical_bytes`).**
* If `ctx.base` is in the window, then: ctx.base is the storage base of the
  live buffer being validated (`verify_structural_wire_graph`) or of the
  receive token's owner (raw validator); by W' that storage is a window slot;
  so the window is reserved. The load-free resolver is then valid for every
  target, including untrusted ones, because with the array alive every
  in-window address is an element of it.
* Otherwise the checked public resolver is used. For an ordinary buffer,
  `validate_storage_range` has already confined the target to
  [ctx.base, ctx.end] ⊂ the storage, which by W' is outside the window anyway.
* Neither resolver throws, so the error and first-error order are unchanged.
* `in_range(ctx.base, 0)` also accepts BASE+BYTES. An ordinary storage cannot
  start there, because the closed guard relocates it.

## Availability (not soundness)

* Lazy reservation can fail when the range [BASE, BASE+BYTES] is already
  occupied at the first receive. Possible causes:
  * a non-library mapping placed there earlier in the process lifetime;
  * a transient colliding ordinary mapping during step 1, in a narrow race
    with a concurrent first receive (the kernel only chooses such an address
    when the hook is armed or lower VA is exhausted).
* As before, the attempt is final: receive storage creation then throws
  `std::bad_alloc` for the rest of the process. Ordinary storage is
  unaffected, and invariant W' still holds.
* Under eager W2 such conflicts could only arise before the first VM storage
  creation.

## Residual assumptions (unchanged)

* `std::launder` of the array type relies on the window array staying alive
  while nested objects occupy its storage ([intro.object]/3, [basic.life]/1).
* Nobody unmaps or replaces library reservations behind the library's back.
* In-window targets of freed or never-used slots still resolve (numerically
  equal pointers). Neither resolver is a liveness query.

## Tests (tests/test_canonical_window.cpp)

* **Default variant:** after ordinary create/mutate/save/load/reload/raw
  `Storage`/`from_bytes`:
  * `canonical_window_array` is null and `canonical_window_attempted` is false;
  * no ordinary storage touches the range;
  * `WindowCollide` relocates both the PROT_NONE and full-copy paths before the
    reservation, and the window is still unreserved afterwards.

  The first receive then reserves the window (its slot is in-window). After
  the reservation the collision hook may or may not relocate (≤ 1
  relocation), and the storage never touches the window.
* **`--reserve-failure`:**
  * the same "ordinary-only stays unreserved" checks plus collision relocation
    before the injected failure;
  * then the failed reservation is final;
  * then the collision relocation and "never in the window" checks are
    repeated after the failure, and receive still throws.
* **Small and disabled variants:** unchanged.
