# Reuse requirements

The splitting/coalescing paragraphs specify the evaluated E prototype. Their
correctness tests passed, but the performance adoption gate failed; these
allocator changes are archived rather than included in final F. The transport,
Automatic, reclamation, and pool requirements below apply to production F.

A reused allocation SHALL have one owner: Core, one payload cache node, or a live
caller. Splitting SHALL create nonoverlapping valid physical headers and preserve
all unrelated live values, base addresses and wire layout. Both caches SHALL
account for their own residuals; legacy advertised sizes MAY be smaller than
physical sizes. Reuse SHALL NOT silently grow the logical arena. Failed planning
SHALL leave allocation metadata unchanged. Unrepresentable or tiny residuals MAY
remain part of the live allocation instead of being cached.

Transport optimizations SHALL retain complete copy initialization, type/schema/
graph verification, exact logical hard limits, inaccessible unused reservation
pages and deterministic failure recovery. Performance claims SHALL use measured
end-to-end workloads, not instrumented phase estimates.

Automatic growth SHALL be explicit opt-in, bounded by the reservation's fixed
logical maximum, and preserve base/epoch. Growth policy SHALL follow storage
ownership and reset on moved-from objects; it SHALL NOT enter wire data. A failed
OS growth attempt SHALL propagate without a silent allocation retry. Cache
reuse/release SHALL precede automatic growth of a new allocation. Default
constructors and loads SHALL retain Explicit policy and disabled pooling.

Reclamation SHALL affect only complete pages beyond the logical end and SHALL
preserve the reservation, partial last page, logical extent, and live values.
Its result SHALL distinguish completed discard from completed protection changes
and support retry after partial failure. Regrowth SHALL initialize all exposed
bytes. Save and logical shrink SHALL NOT implicitly reclaim pages.

A shared pool SHALL bound idle block count, reserved bytes and accessible bytes,
match exact capacity, retain state while live owners exist, and avoid ownership
cycles. Returning a buffer SHALL remove its owner-registry entry first. Pool
return/move/swap SHALL NOT allocate. A reused owner SHALL initialize every logical
byte before publication, including dirty-tail regrowth. Clearing the pool SHALL
release only idle mappings; shared pool operations SHALL be synchronized.
