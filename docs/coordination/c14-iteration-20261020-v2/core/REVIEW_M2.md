# M2 independent exact-owns local-data review

Date: 2026-10-20 (Asia/Shanghai). Reviewer: `m1_safety_review`. Only this assigned review document is writable. No compiler, native executable, heavy test, timing process, candidate edit, or production edit is executed by this reviewer.

## Design decision (actual-patch check pending)

The proposed one-invocation data-pointer reuse has a sound differential lifetime argument for the retained C14 built-in owning-head path. It does not require a context-long owner lease or an ownership-result cache. The actual candidate patch and identity were not yet available at the initial review; final admission requires their appended exact check. Neither this design decision nor any later static-source approval establishes a performance improvement.

Reviewed the baseline `move_assignment_owns_address` implementation and its actual built-in data, iterator, reflection, and canonical-recovery closure. Baseline production source is the retained C14 main header; no M1 or R1 combination is assumed. The expected change is confined to the non-string owning-head branch: resolve `value.data()` once, perform the same full-capacity containment check, then on a miss iterate the same nontrivial live elements with a null-safe end derived from that pointer and `value.size()`.

## Exact scope and pointer lifetime

The `is_owning_head` specializations name the library's own `XFixedVector`, `XVector`, flat set/map and wrapper set/map types (plus the unchanged string types). Their data and iterator members are library-defined inherited implementations, not virtual or generic container customization calls. `XFixedVector::data() const` calls `resolve_rel32_const<T>(&data_delta, data_delta)`. That helper returns null for zero delta; otherwise it computes the existing numeric target, asks `XBufferCore::canonical_bytes_at` for canonical provenance, or uses the existing laundered legacy pointer fallback. No new range, admission, or owner proof is introduced by storing this result in an automatic variable.

`canonical_bytes_at` in C14 closed-v4 has the existing acquire count guard, non-evicting TLS reads, builtin numeric registry comparator, registry mutex lookup and retained shared record. It does not call `owner_from_payload_ptr`, does not install a cold result in TLS, and does not reset or evict a terminal cached record. On the successful cold route its local record remains also retained by the live registry/owner; its internal release does not itself create a last-owner delete callback. Ordinary valid use still requires external synchronization against simultaneous owner destruction. No new shared record, map node, allocation, comparator, wire-policy hook, or callback is added by this design.

The proposed reuse window begins before the unchanged containment check. `move_assignment_contains` only performs null/zero checks and builtin numeric address arithmetic. Capacity and size are scalar descriptor reads; `value.size()` is the builtin member returning `size_`. These operations neither mutate the descriptor nor invoke user code. Thus the extra interval from the first data recovery to the original iterator setup has no storage invalidation boundary in this closure.

The old range-for evaluates its library `begin()` and `end()` before the first child recursion. `begin()` returns `data()`, while `end()` performs `data() ? data() + size_ : nullptr`. It then stores those pointer iterators for the complete loop; it does not recompute them after child recursion. The proposed explicit loop stores exactly the same pointer/end pair, just reusing the earlier resolution. Recursion therefore does not extend the parent's pointer lifetime beyond what the original range-for already required. This differential argument is sufficient without claiming that arbitrary recursive generic calls can never execute user code.

Within the intended ordinary actual type closure, recursion only repeats this ownership predicate, visits builtin arrays, or performs P2996 base/member reflection. `static_for` invokes implementation lambdas; those directly access actual declared subobjects and recurse. It does not construct/copy/move the reflected value, call conversion operators, overloaded comparison, `operator<`, custom wire construction/activation/validation hooks, PMR resources, allocation, or retirement. A string leaf only uses its original data/capacity containment. This is a materially smaller closure than L1 graph validation: there are no range-index insertions or upstream scratch callbacks to import into this proof.

## Generic-call boundary: preserve, do not overclaim

The existing recursive calls are unqualified `move_assignment_owns_address(child, address)`. A user-defined element's associated namespace or hidden friend can, in principle, provide a better-matching same-name overload through ADL. That is not a documented wire-policy customization, but it prevents an unconditional statement that every possible generic instantiation is user-code-free. `noexcept` alone would not prove otherwise.

This proposal must preserve those existing recursive expressions and their order. Do not silently qualify them or add a trait rejecting these types as part of the data-reuse patch; either would alter the existing generic dispatch domain. The old loop already cached parent begin/end across such recursion, and the new code introduces no callback between its earlier pointer capture and the old capture point. Therefore an ADL overload does not by itself establish a new stale-pointer counterexample to M2. If it mutates/destroys the parent's elements while the traversal continues, the original range-for had the same invalidated iterators. A narrow regression test can safely observe ADL call order/counter behavior without mutating the active parent, and should continue to find the same dispatch.

The proof is for the library's built-in owning-head specializations and existing exact traversal. It does not endorse external specializations of the internal trait that add arbitrary data/size semantics. It also does not authorize general local-pointer caching across unknown operations, assign callbacks, owner retirement, PMR code, or multiple public operations. There is no persistent negative ownership witness, epoch shortcut, same-arena shortcut, or same-address identity inference.

## Semantics that the actual patch must retain

- Keep the initial trivially-copyable early return, builtin array branch, string containment, class reflection/base/member order, and unknown-type false fallback unchanged.
- Check the **entire capacity byte range** before live-element recursion, including spare capacity and independently placed descriptors. Containment must still precede even the new size read and return immediately on a hit.
- Recurse over precisely `[data, data + size)` for nontrivially-copyable element types only. A null pointer must produce a null end without `nullptr + 0` arithmetic; empty allocated storage must produce equal nonnull pointers and no dereference. No element loads or size-driven recursion may occur for trivially-copyable elements.
- Keep each child's order and early positive return. Do not skip siblings, switch to an ownership type guess, reinterpret spare slots as live elements, or remove a reflected/base edge.
- Preserve both assignment-site calls, `!owns(destination, &source)` and `!owns(source, &destination)`, their short circuit order, and all allocator/in-arena preconditions before them. Keep the returned bool invocation-local and recompute on each later operation.
- Use the existing pointer provenance from `data()`. Do not replace it with an integer-derived pointer or reuse a known-retired record after an owner transition.

Zero-data heads with size zero naturally terminate. Malformed size/capacity or null-data-plus-positive-size descriptors acquire no new validity guarantee; this predicate remains an exact helper operating on existing live container invariants, not a wire validator. The null guard should match the original iterator behavior rather than dereference malformed storage. Pointer arithmetic remains within the existing whole allocation/array lifetime already required by the old vector iterators.

## Required semantic checks and mechanism boundary

Reuse the current existing tests on this exact candidate instead of writing a mirrored implementation oracle:

- `tests/test_same_arena_move.cpp`: different elements, self moves, empty allocated and null heads, allocator/provenance fallback, unbound fallback, nested compaction, both parent/child assignment directions, post-reload moves, retirement metadata failure, and actual VM failures.
- `tests/test_locator_soundness.cpp`: ownership changes across operations despite unchanged local descriptors/epochs, and a legally placed head inside an opaque byte vector. These directly prevent an accidental persistent cache or removal of full-capacity byte containment.
- `tests/test_canonical_cold_no_callback.cpp`, `tests/test_canonical_registry_v2.cpp`, and `tests/test_owner_identity.cpp`: the actual closed-v4 address-recovery/liveness premises. Historical passes are not candidate receipts.
- Existing complete-custom tests may be reused for supported custom objects, but their presence does not mean owns invokes their wire hooks. Optional callback counters should explicitly verify no wire-hook/comparison invocation during this predicate, and an ADL dispatch probe should preserve the original same-name recursion rather than assert it is absent.

For any focused new predicate fixture, cover a nontrivial vector where the target is in a later live child's nested allocation (early children miss), an unrelated allocation/different element miss, direct capacity-hit including a spare/byte-placed descriptor, empty allocated storage, default null storage, ordinary array/reflection/base reachability, and an unknown nonowning type's original false result. Public ancestor/descendant assignments must retain their copy-before-retire fallback; a bool result alone does not prove assignment safety. After retirement/reuse or an intervening legal operation, recompute the traversal and verify fresh reachability rather than reusing a prior answer.

The parent reports actual baseline optimized Attr containment-miss code has up to four data recoveries. This reviewer does not rerun or independently certify that assembly. A candidate codegen check must show the expected single recovery per non-string owning node in the actual call closure, unchanged full recursion/containment checks, and any new register/stack costs. Source call counts are not CPU-cycle evidence. Normal/SAN and applicable business/V1/consumer gates remain necessary before any separately frozen screen.

## Initial write boundary and pending evidence

The proposed candidate directory is `build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1`. At the initial read neither that candidate nor the new v2 PLAN/TASKS paths existed yet, so no actual-patch identity or final acceptance is asserted above. This file will be appended when the patch is available. No other file is changed by this reviewer.

## Final actual-patch and focused-fixture addendum

The v2 PLAN/TASKS and the actual candidate have since been published. Read the new PLAN and actual `M2.patch`, independently diffed the retained C14 main header against the candidate, read `IDENTITY.json`, and independently hashed the patch, changed header, and focused test:

| Exact reviewed object | SHA-256 |
| --- | --- |
| `m2-exact-owns-v1/M2.patch` | `c5c3d7bf6e1993fae1bb129b58e918fa3f9bfa9777f17e1b668a966cf56b7497` |
| candidate `source/xoffsetdatastructure.hpp` | `3e347612c1a62299bb910c68dd6b1411663f3165e18e298a51e7ba2e062c201f` |
| `tools/c14_iteration_20261020_v2/core/test_m2_owns.cpp` | `83bd4f85106cdf929e6418d29fe3c00f2f4c8b04a66e7c59c977ed409e1d8e0d` |

The identity records the correct C14 base commit, 154 members, and only `xoffsetdatastructure.hpp` changed. The actual main-header diff has one hunk in the proposed branch: `const auto* elements = value.data()`, original capacity containment using `elements`, and on its miss the nontrivial-element `count`, null-safe `last`, raw-pointer loop and `const auto& child = *item`. The original recursive expression remains exactly `move_assignment_owns_address(child, address)`, so generic ADL dispatch and const lvalue binding are preserved. The trivial, string, array, reflection and unknown-type branches are unmodified, as are both assignment-site calls. There is no M1/R1 edit in this actual diff.

The final pointer/end sequence satisfies the design constraints above: no size read before a containment hit, no size read for trivial elements, no null-pointer arithmetic, no capacity-based recursion, no persistence, no changed child visit order or early return, and no omitted ancestor traversal. `const auto*` and the iterator copy retain the original const element type from the const `value` parameter; pointer dereference performs no user iterator operation. No new blocking source defect was found.

### Focused fixture preconditions

Read the entire focused test at the exact hash above. It verifies observable ownership behavior, without implementing a parallel copy of the predicate. Its fixture preconditions are consistent with the intended claims:

- The default unbound `XVector<Attr>` has zero delta/size/capacity. Its miss reaches equal null iterators with no arithmetic or dereference.
- `reserve(4)` establishes an actual four-element capacity array without live Attr values. The `data()+3` targets only form an in-array pointer and are checked numerically; they are not dereferenced as inactive elements. This tests full capacity containment independently from recursion. The later live-spare query has the same property.
- Two live Attr children have disjoint live nested key/value allocations. A target in the second child requires the first child's miss and the complete original recursive traversal. No comparison or custom hook is needed to establish that route.
- The null, unrelated stack target, and second-owner descendant are valid numeric containment misses. They do not require dereferencing the target. This is distinct from claiming a new unsupported-type recursion result.
- Before `reserve(17)`, the original four-Attr array is followed by live key/value allocations. It is therefore not the arena's physical tail; this fixture does not accidentally rely on a fresh address when same-address expansion could legally succeed. The new 17-Attr allocation is larger than the old four-Attr range. The old array address is checked after retirement as a numeric target, without dereferencing retired Attr objects; existing nested allocations remain distinct and live.
- Moving the owning XBuffer preserves its reservation and nested objects. The test reacquires `v` from the moved owner before querying, rather than dereferencing the moved-from Core.
- `clear()` retires the child key/value allocations while retaining the 17-Attr parent capacity. The released child address lies outside that retained parent array, and the negative query occurs before the subsequent trivial-vector allocation could reuse a freed child block. The `v.data()+16` target is within retained capacity and is never dereferenced as a live Attr. Thus this check does not demand a persistent negative answer after storage reuse.
- The final trivial-vector hit and miss keep the original capacity-only semantics; no child recursion is expected.

No false test precondition like the prior M1 shared-arena physical-tail assertion was found. This focused fixture is primarily an ordinary owner/predicate semantics check; it does not itself execute a canonical receive domain, real callback reentry, both public ancestor/descendant assignments, or ADL probes. Reuse the existing tests listed above for their actual separate coverage. Do not describe this one executable as the complete lifetime matrix.

**Final source verdict: admit the exact candidate and focused test to the registered targeted normal/SAN and candidate-only codegen checks.** Existing control assembly need not be rebuilt. No static finding blocks those checks. Their actual results, bounded fixed-M mechanism evidence, applicable complete gates and independent screen freeze remain outstanding. No execution, correctness pass, measured speedup, or screen authorization is claimed by this review.
