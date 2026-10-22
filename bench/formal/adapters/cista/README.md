# cista v0.16 adapter

The source pin is `015b94e66c4c0d7c1ef29742383a024c84fbbba1`; the separately
audited master revision is not substituted. P0 uses offset containers and
`mode::NONE` serialization/deserialization with default structural checks.
No fmt macro is defined. This is a trusted profile, not an untrusted-mutable
safety claim, and does not enable UNCHECKED or substitute a raw pointer cast.

Detached load copies immutable caller bytes into an owned private blob and
deserializes that copy. Native containers can later own heap allocations while
other containers still refer into the private blob. Queries operate on this
native graph; emit reserializes it into cista's native owning byte vector.

The state owns both forms of storage. In this release, vector `deallocate()`
does not descend into non-self-allocated blob elements, and `resize(smaller)`
does not destroy the removed elements. A loaded parent can therefore contain
newly self-allocated child strings/vectors that its ordinary parent destructor
would skip. The adapter explicitly releases children bottom-up, calls public
deallocate/reset, then lets owning parents and the private blob release.
Already-reset children make later ordinary destructors harmless.

Stage 7 preserves sequential status checks with a temporary removed-ID set,
cleans removed/destination records, uses legal member moves for stable survivor
compaction, and performs one public tail erase to destroy trailing elements.
It does not use bytewise relocation or v0.16's destructor-skipping shrink.
The increasing-ID append path avoids scanning all old records; insertion into
an earlier position similarly cleans destinations before member moves.

This ownership code requires the generic adapter contracts, per-stage fixture
digests and independent sanitizer/leak validation. Source inspection alone is
not passing runtime or performance evidence.
