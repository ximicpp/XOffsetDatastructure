# W2 proof: the load-free internal canonical resolver

`XBufferCore::canonical_bytes_at_internal(anchor, t, n)` =
`CanonicalWindow::internal_bytes_at(t, n)`:

```
off = t - BASE
if (off <= BYTES && n <= BYTES - off)          // compile-time constants
    return *std::launder(reinterpret_cast<std::byte(*)[BYTES]>(BASE)) + off;
return nullptr;
```

The in-window branch has defined behaviour only while the window array object
`std::byte[BYTES]` (begun at BASE by `reserve_locked`) exists. Laundering the
array type, not `std::byte`, is deliberate: objects later created inside the
window are nested within that array, so the array stays alive (and continues to
provide storage) even where the lifetime of individual element bytes may have
ended. The array is never destroyed or unmapped once created. The public
`canonical_bytes_at` keeps the W1 behaviour: it reads `canonical_window_array`
inside the in-window branch and returns nullptr if the window is unreserved, so
it is safe for any address.

## Invariant W (enforced in code)

> While the window is not reserved, no live library reservation (any
> `VirtualMemoryStorage` mapping) touches the closed range [BASE, BASE+BYTES].

1. The window gets exactly one reservation attempt (`reserve_locked`, under the
   window mutex). Its outcome is final. `canonical_window_attempted` is set
   with release semantics after `canonical_window_array` has been stored.
2. Library reservations are made in only two places:
   * **Window slots** (`for_receive` → `CanonicalWindow::acquire`) exist only
     after a successful attempt.
   * **Ordinary reservations** (the `VirtualMemoryStorage` constructor, also
     used by `from_bytes`) go through `map_ordinary`. It first calls
     `ensure_attempted()`, so the window is reserved eagerly at the first VM
     storage creation and the window state it then reads is final:
     * Window reserved: the kernel cannot return any address inside a range we
       already own.
     * Window unavailable: a mapping that touches the closed range is kept
       mapped (so the kernel cannot hand it back again) while the reservation
       is retried at hints just above and just below the window. The touching
       mapping is then released. If both hints fail, the creation throws
       `std::bad_alloc`.

     The test hook `VmOperation::WindowCollide` forces the first mapping onto
     BASE so this path is exercised deterministically (both the PROT_NONE path
     and the full-copy RW path).
3. Pool reuse hands out only reservations created by step 2. Window slots are
   never pooled.
4. No other library code maps memory. `grep mmap|VirtualAlloc` finds only
   `xoffset_virtual_memory.hpp`, and every XBufferCore payload is a
   `VirtualMemoryStorage`.

**Lemma.** Suppose `t` lies in [S.base, S.base+S.reserved] for some live library
reservation S (closed interval, so the `n == 0` one-past case is covered), and
`[t, t+n]` is in the window. Then S touches [BASE, BASE+BYTES]. By W the window
is reserved, so the array exists. If the window is reserved, every in-window
address belongs to the array anyway.

Each internal call site therefore only has to show that its target is bounded
by a live library reservation.

## Call sites

| Site | Target | Why it is bounded by live library storage |
|---|---|---|
| `resolve_rel32` / `_const` (data(), allocator_state(), arena(), allocator_arena) | `origin + delta`, n = sizeof(T) ≥ 1 | The delta is a head field written by the library (set_rel32) or a wire field that validation has already accepted. Every head points into the arena of its binding (allocation, allocator state, arena header). Inside validation, `data()` is called only for flat set/map ordering, after `validate_wire_value(XFixedVector)` has bounded `[data, data+capacity·sizeof(T)]` to `[ctx.base, ctx.end]` (see "verify end" for why `ctx.end` stays inside the storage). |
| `freelist_block_from_offset` (×2) | `state + offset`, n = 16 | Runtime: offsets come from the arena's own free list, written by the library. Validation (`validate_allocator_state`): before the call, `offset ∈ [arena_begin, arena_end − 16]` and `arena_end ≤ ctx.end − state`, with `state` = `object_at_offset` (bounds-checked against the buffer size). The existing check order is unchanged. |
| `try_expand` header | `ptr − 16`, n = 16 | Internal API precondition: `ptr` is a live allocation of this arena (containers pass only their own data). `address > state` is checked first, and the header precedes the allocation inside the arena. |
| `checked_relative` (wire) | `&delta + delta`, untrusted | Routed through `wire_canonical_bytes`. If `ctx.base` is in the window, then (live library storage + W) the window is reserved, so the load-free resolver is valid for any target. Otherwise the checked public resolver is used. `validate_storage_range` still runs first. Neither resolver throws, so the error and first-error order are unchanged. |
| `register_allocation` (wire) | header `address − 16`, untrusted | Same routing as `checked_relative`, after the same `validate_storage_range` checks. |
| `verify_structural_wire_graph`, first call | `xbuf.get_address()`, n = 1 | xbuf is a live XBufferCore, so the target is its storage base. An empty buffer gives nullptr, which is outside the window (BASE ≠ 0). |
| `verify_structural_wire_graph`, end | `base + used`, n = 0 | Reached only when the base is in the window (canonical), and only after `used > segment_size()` has thrown. So the target is in [base, base+size] ⊂ the slot. For ordinary buffers the raw `base + used` is used, and `used ≤ size` holds by the allocator invariant and the `init_from_bytes` check (`used_end > buffer_.size()` throws), so `ctx.end` stays inside the storage. |
| `core_lifetime`: `future_custom_destination`, `begin_dynamic_array`, `rebuild_dynamic_array`, `begin_fresh_ordinary_outer`, `begin_relocated_ordinary_outer` | the destination slot, n = sizeof(T) or capacity·sizeof(T) ≥ 1 | Storage returned by the arena allocator (`allocate_aligned` / `allocate_dynamic_bytes`), or an element/member slot of an object inside the same arena (relocate_construct's documented same-arena precondition). capacity 0 returns early; a nullptr allocation is outside the window. |
| `receive_raw::reactivate_inline_prefix` | `data`, n = live·sizeof(T) | `data` is the array that `rebuild_dynamic_array` has just begun in an arena allocation (capacity > 0). When n = 0 the target is still the allocation start inside the storage. |

The raw receive validator (`raw_checked_relative`, `CanonicalRawBytes`)
computes offsets against the token's own origin and does not use either
resolver. When it runs without canonical bytes it calls `register_allocation`
with `ctx = [owner base, owner base + extent]`, which is covered by the
`register_allocation` row.

## Residual assumptions

* `std::launder` of the array type relies on the window array staying alive
  while nested objects occupy its storage ([intro.object]/3, [basic.life]/1).
  This is the same footing the W1 per-slot origin had.
* Invariant W assumes nobody unmaps or replaces library reservations behind
  the library's back. Doing so was already unsupported.
* In-window targets of freed or never-used slots still resolve (numerically
  equal pointers). Neither resolver is a liveness query.
