# Stable storage requirements

## Reservation ownership

All owning XBuffer creation and load paths SHALL use a VM reservation with no
relocating fallback. The reservation base SHALL remain unchanged for the lifetime
of that storage, including grow, logical shrink, and save. Move MAY transfer its
ownership; compaction MAY create a different storage.

## Extents and failure

The owner SHALL separately record logical length, accessible page extent,
actual reserved length, and logical maximum. Logical length SHALL not exceed
either the accessible extent or its maximum; page rounding SHALL not enlarge
the maximum. Growth beyond the limit SHALL fail without copying the old payload.
Recoverable enable/preparation failure SHALL retain the old logical arena and
allocator state. Additional enabled pages SHALL be recorded before returning.
Only newly exposed logical bytes SHALL be zeroed during growth.

## API and wire

Borrowed spans SHALL cover logical bytes only. Save/load SHALL preserve the v1
format and existing validation; OS metadata SHALL remain process-local. Save
and load MAY copy output/input, and SHALL not be advertised as universal
zero-copy operations. Existing reservation hints SHALL remain hints; explicit
maximum configuration SHALL define a hard limit.

## Pointers and handles

Arena base stability SHALL NOT imply permanent container element stability.
Move/swap/replacement SHALL maintain schema and access metadata with storage,
invalidate object-local handle caches, and prevent checked handles from
interpreting a replacement of another type or unverified raw bytes. Independent
buffer lifetimes SHALL NOT race in the shared registry; same-buffer concurrent
mutation remains externally synchronized.
