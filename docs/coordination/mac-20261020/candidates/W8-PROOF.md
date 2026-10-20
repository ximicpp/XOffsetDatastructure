# W8 proof: load-free internal canonical resolver, lazy + bounded-retry reservation

W8 = W7 plus four review fixes. None of them changes resolve or mutate
codegen; the objdump of the -O3 driver.o matches W7 instruction for
instruction for `XFixedVector<u32>::operator=(&&)`, `owns<XVector<Attr>>`,
`retire_dynamic_bytes`, `allocate_dynamic_bytes`, `XFixedString::assign`,
`XOffsetAdapter::mutate` and `map_ordinary`.

1. **Error type.** Every failure to obtain receive storage throws exactly
   `std::bad_alloc`. This covers a disabled window, a refused or final
   reservation, an exhausted or fragmented window, and the VM Reserve, Enable
   and Prepare failures. `begin_verified_receive` propagates it unchanged.
   * This is the existing tested contract: `test_verified_receive` catches
     `std::bad_alloc` for the begin-time VM failures.
   * `XBadAlloc` derives from `XException`/`std::runtime_error`, not from
     `std::bad_alloc`, so the two types are distinct.
   * `XBadAlloc` stays reserved for `begin_verified_receive`'s own size-limit
     checks (bytes or capacity above MAX_PAYLOAD, bytes above capacity),
     matching XBuffer's MAX_PAYLOAD checks, and for finish-time registry
     preparation (already tested).
   * Tests catch the exact dynamic type (`typeid == typeid(std::bad_alloc)`,
     and not `XBadAlloc`) for the disabled, exhausted, failed and refused
     cases, through both `for_receive` and `begin_verified_receive`.
2. **Bounded retry.** An OS refusal at the window base no longer ends
   reservation attempts. Any of these leave the window retryable by a later
   receive request, up to `CanonicalWindow::max_reservation_attempts` (4)
   attempts per process:
   * mmap/VirtualAlloc failure;
   * the mapping was placed elsewhere (which is then unmapped);
   * EEXIST from MAP_FIXED_NOREPLACE.

   Configuration errors (page or granularity mismatch) and the injected
   `WindowReserve` failure are final at once. Success is final.
3. **Premises** of the proof are stated explicitly below.
4. **Fork policy.** It is documented in code (registry, window state) and
   tested.

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

**Lemma (W7, unchanged in W8).** Suppose `t` lies in [S.base, S.base+S.reserved] for some live
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


### Lazy reservation and the race (W8, precise)

* `acquire()` is the only caller of `reserve_locked` and runs only for
  `for_receive`. `map_ordinary` (all ordinary creation: constructor,
  `from_bytes`, PROT_NONE and full-copy) never calls it and never reads the
  window state. Its overlap guard is the compile-time constant
  `CanonicalWindow::enabled`.
* Before the first receive, an ordinary library mapping can be inside
  [BASE, BASE+BYTES] only transiently. It must be a kernel-chosen first
  mapping that `map_ordinary` keeps only while it relocates; it is never
  published and is unmapped before `map_ordinary` returns. No ordinary
  library mapping persists in the range.
* A reservation attempt can therefore be refused only by (a) a non-library
  mapping in the range, or (b) such a transient library mapping in a race
  with a concurrent receive. Both are OS refusals at the base, which are now
  retryable up to the bound.
* Soundness does not depend on any of this.
  * The array is begun only by a successful attempt and only once: status
    `Reserved` stops further attempts.
  * A failed attempt leaves no library mapping in or around the range: a
    mapping placed elsewhere is unmapped, and a MAP_FIXED_NOREPLACE failure
    maps nothing.
  * Retries happen under the window mutex.

So the lemma below still holds as stated.

## Premises (explicit)

The proof uses exactly these assumptions. P1–P4 and P7 are outside the
library's control; the others are enforced in code.

* **P1 — exclusive ownership of library address ranges.** No code outside
  the library unmaps, remaps (MAP_FIXED / MAP_FIXED_NOREPLACE / VirtualFree /
  VirtualAlloc), protects, or otherwise changes any library reservation
  (ordinary storage, pooled storage, window slots, the window reservation
  itself). The library never unmaps the reserved window: slots are recycled
  by replacing or decommitting pages in place (`make_inaccessible`), and a
  slot whose recycling fails is quarantined, not unmapped.
* **P2 — kernel non-overlap.** mmap/VirtualAlloc never return a range that
  overlaps an existing mapping of the process. So while the window is
  reserved, no new mapping lands inside it.
* **P3 — window array lifetime.**
  * The `std::byte[BYTES]` array is begun exactly once, by the placement
    array-new in a successful `reserve_locked`. Nothing in the library
    destroys it, begins another complete object over the whole window, or
    unmaps it.
  * Objects later created in slots are nested in the array, which provides
    storage for them ([intro.object]/3), so they do not end its lifetime
    ([basic.life]/1).
  * User code must not end the array's lifetime either, for example by
    creating a non-nested complete object spanning the window or by
    `std::destroy_at` on it. Doing so is outside the supported contract,
    like P1.
* **P4 — flat address ABI.** On the supported targets, `uintptr_t` round trips
  of object addresses equal their numeric addresses. The window bounds are
  compile-time integers compared numerically, and `std::launder` of the
  constant base names the array object that exists there (P3).
* **P5 — configuration consistency.** `XOFFSET_CANONICAL_WINDOW_{BASE,BYTES,
  GRANULE}` are identical in every translation unit of a program (an ABI macro,
  like the library's other configuration macros). Otherwise one TU's resolver
  bounds could disagree with another TU's slot placement.
* **P6 — targets derive from live library storage.** Every internal resolver
  call passes a target that lies within (or one past, for n == 0) the
  reservation of a library storage that stays live for the duration of the
  call. The call-site table below establishes this. Concurrently destroying
  or releasing the storage that is being resolved is already a data race
  under the library's threading contract.
* **P7 — no unsupported fork use.** A child forked while another thread was
  inside the library (registry, VM pool, or window mutex held) may only exec.
  All mutexes have process lifetime and no atfork handlers. With no other
  thread inside the library at fork time, the child inherits a consistent
  state. This includes the window array and slots: same addresses, a
  `MAP_PRIVATE` copy. The child can load and receive; this is tested.
* **P8 — publication order (code).** `canonical_window_array` is stored with
  release semantics, under the window mutex, before any slot is handed out.
  Every slot pointer is obtained under that mutex or derived from such a
  pointer, so any thread that resolves an in-window address of a live slot
  has a happens-before edge to the array's creation. The public resolver's
  relaxed load is therefore coherent.
* **P9 — W' (code).** Proven in the section above, using P1 and P2.
* **P10 — slot origin (code).** Window slots are created only by `acquire()`,
  after a successful reservation. They are never pooled, and ordinary storage
  never adopts a slot (or the reverse).

## Call sites (re-checked under W' and P1–P10)

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

* Receive storage can be unavailable in three cases:
  * the window is disabled (BYTES == 0);
  * a configuration error, or the injected `WindowReserve` failure, ended
    the attempts;
  * `max_reservation_attempts` OS refusals have happened, from persistent or
    repeated foreign occupancy of [BASE, BASE+BYTES] or rare transient races.
* While attempts remain, a refused request throws `std::bad_alloc` and a
  later request retries. Ordinary storage is never affected.

## G1 and receive storage (confirmed)

G1's pristine-tail shortcut (`zero_from_`) applies only to ordinary storage.
`for_receive` makes the whole maximum read/write before any transport write
and sets `zero_from_ = maximum_capacity`. As a result, the initial
`grow_to(logical)` and every later growth zero all newly exposed bytes, even
if the transport wrote beyond the logical length. This is tested by
`receive_growth_zeroes_beyond_logical`.

A G1-only build without that line (the early diagnostic variant, e.g.
`/Volumes/XOffsetCS/diag/src-g1*`) is **not shippable**.

## Residual assumptions

All residual assumptions are the premises above. The resolvers are not
liveness queries: in-window targets of freed or never-used slots still
resolve to numerically equal pointers.

## Tests (tests/test_canonical_window.cpp)

* **default:**
  * Lazy reservation: ordinary create/save/load/reload/Storage/`from_bytes`
    leave the window untouched and outside the range, and `WindowCollide`
    relocates before the reservation.
  * The first receive reserves the window.
  * Boundaries; macOS 512×64 MiB capacity; slot reuse; receive growth zeroing.
  * Live equivalence; concurrency.
  * **Fork after a receive:** the child performs an ordinary verified load
    and a new receive and reads the inherited receive; the parent's receive
    stays intact.
* **`--reserve-failure`:** ordinary-only checks and collision before the
  failure. The injected failure is final; both receive entry points throw
  exactly `std::bad_alloc`. Collision relocation still works afterwards.
* **`--transient-overlap` (POSIX):**
  * A foreign PROT_NONE mapping at BASE refuses the first receive, with
    exactly `std::bad_alloc`.
  * Ordinary storage is unaffected.
  * After the blocker is unmapped, the next receive retries and reserves.
* **`--overlap-retries-bounded` (POSIX):**
  * `max_reservation_attempts + 2` refused receives;
  * after the blocker is removed, the window stays unavailable (final);
  * ordinary storage and collision relocation still work.
* **small:** an exhausted window throws exactly `std::bad_alloc` from both
  entry points.
* **disabled:** both entry points throw exactly `std::bad_alloc`.
