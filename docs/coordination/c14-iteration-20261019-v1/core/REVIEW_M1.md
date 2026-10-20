# M1 independent source and lifetime review

Date: 2026-10-19 (Asia/Shanghai). Reviewer: `m1_safety_review`, independent read-only review agent. No compiler, native executable, sanitizer, business run, timing, or archive was executed by this reviewer.

## Decision and reviewed identity

**No blocking source defect found in the actual two-file patch. Admit to the registered mechanism and correctness checks only.** This is neither platform validation nor evidence of an improvement, and does not authorize integration into the retained C14 source.

Reviewed candidate: `build/c14-iteration-20261019-v1/candidates/m1-default-scalar-v1/source`, derived from baseline commit `1904b9c71230026ac6f2d58f919a9ab76447d264`. Read the actual baseline/candidate diffs and candidate identity, rather than only the proposal. The patch hash was independently read using `Get-FileHash`:

| Item | SHA-256 |
| --- | --- |
| `M1.patch` | `072f3770deeb00c14ad2671c3b25b4e2439edb8129ce85db2c8830326e2a6f09` |
| candidate `xoffsetdatastructure.hpp` | `50fc517ad0978b2a4961848870a7d7f1f2d670e6a9e7b9468b687d5622fc9280` |
| candidate `xoffset_core_lifetime.hpp` | `125293c8919dc2b360a83fb8dd06f1b860f98495f93f5a6f76c156f64d3c7fc3` |

The baseline main-header identity in `IDENTITY.json` is `66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3`. The identity records 154 members and precisely these two changed production files. This review independently hashes the patch and changed headers; it does not claim a separate complete 154-member materialization audit.

## Findings

### Declaration order and customization gate

The new `detail::default_scalar_bulk_supported<T>()` declaration is visible before `XFixedVector<T>`. Its definition is in the existing end-of-main-header `xoffset_core_lifetime.hpp` seam, after `receive_raw.hpp` makes both customization templates complete. The calls depend on `T` and follow the existing `future_custom_lifetime_supported<T>()` pattern. There is no new nondependent call requiring the definition before the class-template definition and no new recursion through `can_relocate_without_allocation` or the receive future gate.

The predicate requires scalar `T` and both `wire_activation<T>::enabled` and `wire_future_lifetime<T>::enabled` to be false. Checking both flags directly is appropriately conservative: an incomplete or one-sided enabled policy does not accidentally become eligible merely because `future_custom_lifetime_supported<T>()` rejects it. The static assertion on `ValueInitializeTail=true` prevents an internal caller from bypassing this restriction.

Only the public default scalar resize selects `resize_construct<true>`. All existing general, fill, argument-construction, and custom-frontier calls keep the default false template argument and original lambda execution. No current in-tree explicit type-argument invocation of `resize_construct` was found. Layout, wire descriptors, public resize overloads, and allocator representation are unchanged.

### Required values and object lifetime

For an eligible scalar, `fixed_detail::default_construct(dst, binding())` reaches `std::construct_at(dst)` in the old code. Its empty-argument construction value-initializes `T`; `allocator_ref` has no scalar conversion or conversion operator that would select the allocator-aware branch. The new `std::uninitialized_value_construct_n` preserves that value initialization for each new suffix element.

This matters beyond `uint8_t` and `uint32_t`: false bool, zero floating value, enum zero value, null ordinary pointer, null member pointer, and `nullptr_t` must be checked as values. A null member-pointer representation in particular need not be all zero bytes. The patch correctly delegates to the standard typed algorithm instead of using a blanket `memset`. Pointer/member-pointer native construction checks do not imply V1 admission of those types.

The existing `begin_dynamic_array<T>(..., count)` call is unchanged and continues to establish the whole allocation's array provenance before suffix pointer arithmetic and element construction. The patch does not replace it with a typed cast, resize only the live prefix, read dirty spare slots, or alter same-address `rebuild_dynamic_array` handling. Fresh suffix construction still finishes before old prefix relocation.

Eligible scalars have no user constructor/destructor callback and their value initialization cannot throw. Setting `tail_built` only after the complete typed algorithm succeeds is therefore safe in this restricted branch. In the original false branch, per-element progress tracking and unwind destruction are unchanged. The count is positive on this fresh growth branch, `old_size < count`, and the existing maximum-size check still establishes the allocation-byte bound.

### Transaction, aliases, callback, move, and retirement order

The actual diff retains: `ensure_bound`; local allocator resolution; old size/capacity/data capture; allocate; whole-array creation; `set_rel32`; complete fresh tail construction; old prefix relocation; old prefix destruction; descriptor publication; old allocation retirement. `set_rel32` failure still retires the fresh allocation before touching the old descriptor. Allocation/VM failures and their possible host allocation/new-handler behavior remain before any new initialization or prefix commit.

The new branch removes binding/owner address resolution during ordinary scalar suffix initialization. It introduces no callback, cached owner pointer, allocation, or control block. Prefix relocation and final retirement use the original functions and original order. Existing reentrant Core-move and canonical-owner tests still need to run on this candidate; source review does not substitute for their actual outcome.

Self-alias fill retains `resize_construct<false>` and the original last-read-before-relocate ordering. Non-scalar custom whole-object hooks and custom scalar fresh hooks retain their original dispatch. The pre-existing scalar in-capacity and same-address tail paths already used typed bulk value initialization; **this patch neither changes nor certifies a different scheduling contract for custom scalar policies on those existing paths**. Any broader custom-path correction would require its own defined scope and evidence.

## Required independent validation and falsifiers

Before any screen, the sole heavy executor must bind this exact source identity and locked compiler/runtime, and execute the frozen request. The relevant existing tests supply semantic coverage without duplicating them:

- `tests/test_scalar_resize.cpp`: blocked-tail fresh versus physical-tail/in-capacity behavior, initialized values, exact allocate/retire events, overflow/zero, self-alias fill, true arena and VM failures, owner move/swap/replacement, and new-handler Core movement on a failed grow.
- `tests/test_resize_growth.cpp`: fresh transaction, nested last-read aliases, custom throwing construction, host/arena/VM failures, original fallback, and wire fixture producer/consumer modes.
- `tests/test_custom_future_lifetime.cpp`: complete-T default/copy/relocation hooks, source preservation, unwind, custom prefix lifetime, self fill, and post-reload mutation.
- `tests/test_core_lifetime.cpp`, `tests/test_canonical_registry_v2.cpp`, `tests/test_canonical_cold_no_callback.cpp`, and `tests/test_verified_receive.cpp`: the related array/canonical lifetime and reentrant callback domains as selected by the frozen matrix.

Add the promised narrowly scoped semantic source for both integer widths on fresh/capacity/tail and general scalar value checks, including a member pointer whose required null value disproves a byte-zero shortcut. A scalar customization gate check should explicitly reject activation-only, future-only, and fully enabled scalar policies; custom fresh construction must retain its prior scheduling. The existing class custom tests alone do not exercise `std::is_scalar_v<T> && enabled`.

The actual `-O3 -DNDEBUG` driver/call closure must show that the baseline retains the targeted repeated binding/address-resolution work and the candidate removes it. A source-level lambda call or a diagnostic counter is insufficient. If baseline code generation already folds it to equivalent bulk initialization, or actual fixed-M input does not materially reach the fresh suffix path, reject this mechanism without timing. Full business conservation, V1, applicable consumers, normal/SAN, and the registered cost controls remain independent gates. No performance claim is made here.

## Scope limits

No production or candidate source was modified by the reviewer. Historical F, frozen C14, manifests, validation-owned files, queue/lease state, and the parent plan/task/state files were not written. Only this assigned review document was created. The optional new semantic test source and executable request were not yet available for this review; their final content and receipts require their own verification.

## Addendum: actual v2 review and scalar-policy test design

The execution candidate is now **`m1-default-scalar-v2`**; v1 is retained as an unexecuted draft. Reviewed the actual v2 patch and v1-to-v2 core-header diff, and independently hashed:

| Item | SHA-256 |
| --- | --- |
| v2 `M1.patch` | `921a5c292988720ed14695a8123555ca5b6a06c56e1116dbd18f5f4de2569527` |
| v2 `xoffsetdatastructure.hpp` | `50fc517ad0978b2a4961848870a7d7f1f2d670e6a9e7b9468b687d5622fc9280` |
| v2 `xoffset_core_lifetime.hpp` | `34d961bf7bb0cc4048f8403ef7b38618c9b124ba7d1866952aba638be6f32b09` |

The sole v1-to-v2 production change adds an explicit `!future_custom_lifetime_supported<T>()` conjunct before the two disabled-frontier checks. The original helper declaration is visible here; this call is dependent and is evaluated after the complete headers are available. `future_custom_lifetime_supported<T>()` calls the receive future gate only when both frontier flags are enabled, and that gate does not call `default_scalar_bulk_supported<T>()`. There is no cycle. The conjunct is semantically redundant when both frontier flags are false, but explicitly documents the construction-frontier exclusion required by the plan. The preceding source/lifetime decision applies to these exact v2 headers: **no blocking production-source defect found; normal/SAN/codegen/mechanism gates remain required**.

The newly available `tools/c14_iteration_20261019/core/test_m1_scalar_values.cpp` compares bool, double, enum, raw pointer, data-member pointer, and function-member pointer defaults by value. That correctly avoids treating null member pointers as all-zero byte strings. It also tests both integer widths and self-alias fill.

One fixture precondition needs correction before execution: the displayed version invokes all eight `paths` using one allocator, and each path retires allocations into its freelists. A later group's initial `resize(3)` can reuse a non-tail retired block. Its subsequent `resize(11)` may then correctly allocate fresh, violating the fixture's unconditional physical-tail address assertion. Use a separate newly created owner/arena for each `T` group, so the first live payload is demonstrably the physical tail before this assertion. This is a deterministic fixture-domain issue, not a reason to weaken the production requirement. It was reported to the writer; the final fixture revision must be reviewed/bound before running.

### A feasible complete scalar enum construction frontier

Use a local `enum class PolicyEnum : std::uint32_t { marker = 17, prefix = 29 };`. Fixed underlying uint32 gives valid underlying representations and ordinary scalar semantics without pointers or ownership. Define its policy specializations before any container/helper instantiation involving this enum. No TypeLayout specialization or relaxed receive gate should be needed for this enum.

Provide all actual surface members, using aliases `E=PolicyEnum`, `Alloc=fixed_detail::allocator_ref`, and the existing context types:

```cpp
// wire_activation<E>, enabled = true
static void validate_raw(const char*, detail::WireValidationContext&);
static void validate_raw(const char*, detail::WireValidationContext&,
                         const detail::CanonicalRawBytes*);
static void activate(char*) noexcept;
static void activate(char*, detail::RawActivationStats&,
                     const detail::CanonicalRawBytes*) noexcept;

// wire_future_lifetime<E>, enabled = true
using children = wire_future_children<>;
static void default_construct(void*, const Alloc&);
static void transfer_construct(void*, const E&, const Alloc&);
static void transfer_construct(void*, E&&, const Alloc&);
static void relocate(E*, E&) noexcept;
static void reactivate_inline(void*, const detail::CanonicalRawBytes*) noexcept;
```

Activation should read the saved uint32 with `memcpy`, then start the complete `E` with nonallocating placement construction from `static_cast<E>(saved)`, preserving representation. For this fixed unsigned enum all underlying values are valid, so raw validation may be a documented no-op after the caller's existing complete range/ownership validation. Both validate/activate overload pairs are necessary: the old two-argument raw surface and the canonical three-argument surface are checked separately. `reactivate_inline` delegates to the representation-preserving activation. Construction hooks can increment simple local counters; default construction creates `E::marker`, both transfer overloads nondestructively preserve the source value, and relocation placement-constructs the destination from the source while leaving the source live. There are no owned children to transfer or empty, so `children<>` is accurate.

Assert these prerequisites independently:

```cpp
static_assert(std::is_scalar_v<E>);
static_assert(detail::custom_future_contract::surface_complete<E>());
static_assert(detail::receive_raw::future_supported<E>());
static_assert(detail::future_custom_lifetime_supported<E>());
static_assert(!detail::default_scalar_bulk_supported<E>());
```

The actual `receive_raw_supported<T>()` checks an enabled `wire_activation` before its ordinary scalar/enum cases. `receive_raw_future_supported<T>()` likewise checks the complete custom surface before ordinary scalar acceptance. Thus a fully specified enum policy can exercise this exact scalar frontier; it need not be replaced by a class policy or bypassed through an internal raw pointer.

A minimal scheduling test should construct an arena-bound `XVector<E>` in a new owner, reset counters after fixture creation, and execute these public calls:

1. `resize(3)` from no payload. `try_expand_dynamic_bytes` rejects the null old pointer, so this is unambiguously fresh. Assert three default hooks and three `marker` values.
2. Set one prefix value to `prefix`, allocate a live blocker, capture the old data pointer, and `resize(7)`. Assert a different data address, four additional default hooks, three relocation hooks, preserved prefix values, and four new `marker` values. Keep the blocker live through the operation; this makes the ordinary typed-zero bulk path observably incorrect if the frontier gate is accidentally removed.
3. With sufficient new allocation size relative to any retired small block, `resize(11, v[0])` checks the original scalar self-alias fill route. Assert four nondestructive copy hooks, seven prefix relocation hooks, source/prefix preservation, and the four requested fill values. This overload already forces its original general fresh transaction when count exceeds capacity.

Do not assert default-hook dispatch for custom scalar in-capacity or physical-tail default resize: the unchanged baseline paths deliberately remain outside M1's fresh-only scheduling claim. Use separate enum types with just `wire_activation::enabled=true` or just `wire_future_lifetime::enabled=true` for two negative bulk-gate assertions; their incomplete surface must remain ineligible even when the complete future helper returns false. No execution or compiler check of this design was performed by the reviewer.
