# β group on W9a — B2, A1, B6 adopted; B3 not adopted (counterexample)

* **Base:** W9a (`/Volumes/XOffsetCS/candidates/w9a`).
* **Candidate:** `/Volumes/XOffsetCS/candidates/w9a-beta` = W9a + B2 + A1 + B6,
  plus the B3 counterexample test.
* **Patches against W9a:** `beta-B2.patch`, `beta-A1.patch`, `beta-B6.patch`,
  `beta-B3.patch` (test only) and `w9a-beta.patch`.
* **No allocation decision changes.** S/M/L output is byte-identical to
  `diag/b7/w9a-*`. The wire, canonical and query files are `cmp`-equal, and
  every SHA field in `r.json` is equal (see `diag/b7-beta`).

## B2 — batch value-initialization in the default-scalar reallocation (M1 reopened as a new W9a experiment)

### Scope

Only `XFixedVector<T>::resize_default_scalar` (`requires is_scalar_v<T>`) is
changed, and only its reallocation branch: `count > capacity_` and in-place
expansion has failed. The following are unchanged:
* fill `resize(count, value)`, the alloc-aware argument overloads and custom
  construction, which still call `resize_construct`;
* the self-alias paths;
* the in-capacity and in-place-expansion branches, which already used
  `uninitialized_value_construct_n`.

### Equivalence

* **Same algorithm as before.** W9a called `resize_construct(count, λ)` with
  λ = `default_construct(dst, binding())`. For a scalar T, `resize_construct`
  takes its relocation branch: `can_relocate_without_allocation` holds and a
  scalar is never a custom-lifetime type.
* **Re-done checks are no-ops.** That branch re-checks `count > max_size()`,
  `ensure_bound()` and `binding()`, all already done in the caller. Nothing in
  between (`try_expand` returned false) changes `arena_delta` or the state's
  segment, so the results are the same.
* **Step order kept.** B2 performs the remaining steps in the original order:
  allocate → `set_rel32` (on throw: retire fresh and rethrow, same as the
  original catch with `tail_built = 0`) → build the tail → relocate the prefix
  → `destroy_n` old → publish delta/size/capacity → retire old.
* **The tail construction is identical.** For a scalar, `default_construct`
  reaches `std::construct_at(dst)`, which is value-initialization (zero), and
  cannot throw; the per-element `binding()` is pure. So the old loop and
  `uninitialized_value_construct_n` produce the same bytes and the same
  (no-throw) control flow. Initialization is never skipped: reused, dirty
  free-list storage is zeroed (tested).

### Tests

`scalar_reallocation_value_initializes`: 64 rounds, each over a dirty
free-list block of the target size class. The prefix is kept, the tail is
zero, and the verified roundtrip passes. `test_scalar_resize` and
`test_mutation_failures` also pass.

## A1 — R on top of MV

* `XFixedString::release` and `XFixedVector::release` now return early when
  `data_delta == 0`, after clearing size and capacity. In that state `data()`
  is null, and the original release only cleared the descriptor: no element
  destruction, no retire.
* The live case calls `release_bound(binding())`, the original body with the
  binding passed in.
* On the live path, MV's move assignment calls `release_bound(destination)`.
  `destination` was computed from `this->arena_delta`, which nothing has
  modified since (the owns checks and `set_rel32` are pure), so it equals
  `binding()`.
* The O part is not included: on W3 its codegen was identical.
* Tests: the second-member alias guards (both directions), and checks that
  moves and destruction of moved-from or empty in-arena heads never touch the
  allocator state. Both pass on W9a as well.

## B6 — inline owns for one-level leaf aggregates

### Justification

After A1, the -O3 driver's `XFixedVector<Attr>::operator=(&&)` still made two
out-of-line calls to `move_assignment_owns_address<XFixedVector<Attr>>`. In β
there are none; the only remaining call is the live-path `release_bound`.
* W9a: owns×2 plus `release`.
* A1: owns×2 plus `release_bound`.
* β: `release_bound` only.

### Definition

`owns_leaf_aggregate<E>` (reflection, consteval) holds when E is a class with
no bases, is byte-copy-safe, is not an owning head and is not trivially
copyable, and every non-bit-field member is either trivially copyable, a
string head, or a vector of trivially copyable elements.

### Equivalence

For `XFixedVector<E>` with such an E, `move_assignment_owns_address_fast`
contains the generic owning-vector branch verbatim (M2 form):
* one `data()`;
* the capacity containment test first, then `size`;
* a null-safe end;
* the per-element call `move_assignment_owns_address(child, address)`, left
  unqualified, inside the same `fixed_detail` function template.

Ordinary lookup at the definition point finds the same `fixed_detail`
template, and ADL contributes the same associated-namespace candidates, so
overload resolution, the result, the order and the early return are
identical. Only where the code is placed changes; the compiler then inlines
`owns<Attr>`. The top-level calls from `operator=` were already qualified
(MV), as before. All other types keep the existing paths.

### Tests

* Static checks: `owns_leaf_aggregate<LeafPair>` holds, and
  `!owns_leaf_aggregate<MoveRecord>` (whose `labels` member is
  `XVector<XString>`).
* The fast and generic owns are compared address by address within ±24 bytes
  of the element storage and of every key and value buffer.
* A leaf-aggregate vector transfer works, and a descendant destination placed
  inside an element's value buffer takes the copy fallback.

## B3 — NOT adopted: the required premise is false

B3 needs "`capacity_ > 0` ⇒ bound and arena non-null" in every reachable
state. Two public-API counterexamples break it:
1. `v.bind(fixed_detail::allocator_ref{})` on a vector that has storage
   leaves `arena_delta == 1` (segment tag, null arena) and keeps `capacity_`.
2. Binding to a state created by `initialize_allocator_state(state, nullptr)`
   gives `segment_delta == 0`, so `arena()` is null.

In both cases W9a's `push_back` throws "XFixedVector is not bound to an
allocator"; skipping `ensure_bound()` would instead write into the storage.

The exact alternative — keep `ensure_bound` and drop only the redundant
resolution — saves nothing: in the -O3 code `binding()` is already dead, and
`data()` is resolved once.

`beta-B3.patch` adds the test `in_capacity_push_back_keeps_bound_check`,
which pins the current first error for both counterexamples.
