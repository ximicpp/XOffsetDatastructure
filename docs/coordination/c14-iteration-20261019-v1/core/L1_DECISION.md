# L1 decision: no candidate admitted from the context-cache design

Decision: reject the proposed context-long validation address source. Do not create or screen an L1 candidate from it. This is a source safety/mechanism decision, not a native failure or a claim that every smaller local design is impossible. Independent review is in REVIEW_L1.md.

The original copy-load initializes storage with VirtualMemoryStorage::from_bytes; it does not install a canonical byte[MAX] origin. The typed context's normal address recovery therefore commonly costs just the global acquire count and branch. Replacing each individual helper's one recovery with record-owner acquisition plus payload/state/extent checking has no established eliminated work and can add work. A logical count of canonical calls is not evidence of map/mutex cost on this path.

The lifetime counterexample to context caching is concrete in source: WireValidationContext embeds two lazy PMR indexes, register_owned_range/register_free_range can request an upstream chunk, and an upstream/new_handler may reenter. Explicit detail graph verification accepts an XBufferCore and upstream, so a callback can hold/move the caller's Core. unregister_registry marks the record owner null before releasing its mapping; a retained shared_ptr keeps only that inert record. A cached ctx base/origin, root pointer or raw Core pointer is not refreshed by retaining the record. Set/map ordering can also enter user comparison operators; typed validate_wire_value does not itself directly call wire_activation/future hooks. The earlier design's generic custom-validation wording was too broad; the PMR boundary alone suffices.

The required state machine is:

| State | Permitted action |
|---|---|
| No trusted identity/domain proof was established | Retain the existing exact helper route for this public/detail caller. Do not invent a trusted domain from numeric bounds. |
| Trusted identity remains live, Core moved/swapped, same storage remains valid | Reacquire the current owner through the retained identity, then validate current storage, extent and required state/generation before deriving any pointer. Do not dereference the original Core object. |
| Previously identified record is now retired/null or its mapping/domain no longer matches | Safely reject before dereferencing any old ctx/root/header pointer. This is not an unknown-domain fallback; falling back through a stale raw pointer is unsafe. |
| Same numeric address now belongs to a replacement owner | It is a different identity; reject the old proof. Payload address equality alone cannot defeat ABA. |

A possible smaller unit remains checked_relative(data) through the original complete storage/alignment/header checks and header POD memcpy. On a successful path this can be before any scratch insertion. The proof must expire before register_allocation calls the owned/free index insertion, since that call itself can allocate; expiry merely at the return to the container validator is too late. A later terminator read or nested-element traversal cannot inherit it. Also preserve original first-error ordering, including the global owned-before-free priority and exact header-inclusive membership.

No implementation or bare-machine-code opportunity for that narrower unit is established. Introducing a new context-wide witness, changing PMR callbacks, restricting legal detail callers, or adding a mapping pin would expand the original design rather than prove it. Current action is to close this unsafe context-cache formulation and advance independent R1. L1's family allocation is not reassigned to R1; there is no L1 sample window or measured rejection.
