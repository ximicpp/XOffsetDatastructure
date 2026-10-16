# Stable VM improvement requirements

## Lifecycle

The owner registry and synchronization state SHALL remain usable throughout
static/global buffer destruction. Local root identity SHALL normalize cv
qualifiers without changing schema names, wire type admission, or handle epochs.

## Transport

Fresh-owner load SHALL completely initialize the logical payload from its input
before publication; ordinary grow and shrink/regrow SHALL retain zeroing.
Verified save SHALL construct its final owning output without an intermediate
payload string and SHALL preserve the normalized v1 byte format. Existing graph,
type, schema, and corruption checks SHALL remain enabled.

## Owner index

Exact arena bases SHALL locate current owners through a synchronized process-local
index. Registration allocation failure SHALL preserve existing owners and release
the incomplete new owner. Move, swap, and destruction SHALL NOT allocate index
nodes. Same-buffer concurrency SHALL continue to require external synchronization.

## Tail expansion

Container reserve MAY expand a trusted live physical-tail allocation within the
current logical extent. It SHALL validate all Core and wire allocator bounds
before changing metadata, preserve oversized reused physical blocks, and leave
failed attempts unchanged. It SHALL NOT implicitly grow the arena or absorb free
neighbors. Non-tail reserve SHALL retain its original migration behavior.
Existing readers SHALL accept new frames and support further mutation/save/load,
though valid allocator layouts and resulting frame lengths MAY differ.

## Evidence and optional policies

Functional checks SHALL precede performance claims. Comparisons SHALL identify
frozen sources, use matched semantic operations, retain raw samples, and separate
logical/access/reservation metrics from RSS. Optional allocator splitting,
pooling, reclamation, and Automatic growth SHALL require explicit evidence and
independent contracts; evaluating them does not imply shipping those policies.
