# Stable virtual-memory storage

## Baseline and report review

The starting checkout is `next_cpp26`, commit
`07d649909d7f43ca0d11b85accb6d9dab991e284`, with no local changes.
The implementation uses the custom `XBufferCore` arena and frozen XFixed
containers, not Boost.Interprocess. The payload maximum is `0x7ffffff8`;
relative container bindings remain signed 32-bit offsets. `get_size()` and
`segment_size()` both describe the logical arena extent.

The first report correctly identifies whole-buffer relocation, explicit growth,
and the unsynchronized owner registry. It omits the second, payload-resident
allocator free-list layer. It also overstates element stability, zeroing savings,
and opportunities for unconditional in-place container expansion.

The second report specifies replacement of the storage owner, preservation of
the allocator and wire ABI, and failure at the reservation limit. Explicit
growth remains the default to preserve existing high-level behavior. Automatic
growth is an optional proposal from the first report, not expressly prohibited
by the second; it is not part of the second report's requested A–E storage
deliverables. Allocator split/coalesce, aligned address-mask lookup, and pooling are
additional first-report proposals, rather than prerequisites for the
stable-storage contract. The subsequent reviewed improvement adds bounded
live-tail expansion without changing that contract. Physical tail-page reclamation is expressly
optional. A new segmented transmission protocol and moving growth beyond the
reservation limit conflict with explicit second-report instructions and are
not implemented.

## Storage contract

`VirtualMemoryStorage` owns one OS reservation. Its base remains unchanged until
release or transfer of ownership. It records a logical hard limit, the actual
OS reservation length, the read/write page extent, and the logical byte length.
The invariants are `logical <= accessible <= reserved` and `logical <= maximum`.
Page rounding never increases the logical limit. Accessible pages are not a
measurement of resident physical memory.

Arena allocations align the actual `base + offset` address, not merely the
relative offset: an OS page-aligned base need not satisfy larger requested
alignments. Address addition, alignment padding, and payload bounds are checked
before header writes or offset narrowing. Root lookup and free-block reuse also
validate the actual address. Larger local allocation alignment does not extend
v1's existing `alignof(record) <= 8` wire admission rule.

POSIX backends reserve anonymous `PROT_NONE` mappings, then open page-aligned
tails with `mprotect`. Windows reserves with `MEM_RESERVE`, enables with
`MEM_COMMIT`, and releases with `MEM_RELEASE` only at destruction. Page size
and Windows reservation granularity are distinct runtime values. Logical shrink
retains enabled pages and the complete reservation; no page-discard guarantees
are claimed on either Linux or macOS.

Growth validates the new length before enabling pages. A successful enable is
recorded even if a subsequent injected preparation failure prevents publication.
Only the newly exposed logical range is initialized, including old retained
bytes after shrink. The existing payload is never copied. Normal reported
failure leaves the old logical range and allocator state usable. OS physical
memory exhaustion on later page faults is outside this recoverable guarantee.

## Integration and API

All owning create, byte-copy load, and compactor output paths use VM storage.
There is no vector or malloc fallback. The default logical maximum is 64 MiB,
configurable using `XOFFSET_DEFAULT_MAXIMUM_CAPACITY` and per-buffer
`maximum_capacity(bytes)`. A default reservation is at least the initial length;
legacy `max_capacity(bytes)` and raw numeric reservation parameters remain
minimum reservation hints and may raise the default limit. Explicit
`maximum_capacity(bytes)` is a strict limit and rejects initial data that does
not fit. All limits remain bounded by `MAX_PAYLOAD`.

`get_buffer()` becomes a borrowed span over logical bytes, not a vector pointer.
`data()`, `bytes()`, `get_size()`/`segment_size()`, `maximum_capacity()`,
`reserved_capacity()`, `reserved_bytes()`, and `accessible_bytes()` provide
storage-neutral access and separate counters. Spans never expose an inaccessible
tail. `copy_bytes()` explicitly returns an owning copy.

The allocator sees only the logical length. It has no segment-manager reopen or
post-growth throwing update: a successful storage growth publishes the new
logical extent without changing root offsets, free lists, or `used_end`.
`shrink_to_fit()` reduces logical length to `used_end`, retains the reservation,
and does not change the epoch. Save remains a copying normalized transport API.
Its wire reservation field describes the normalized logical payload, never the
process-local VM reservation. Existing v1 layout and signatures remain unchanged.

Every owner holds one identity record (the current owner, used to re-resolve
the owner after a host callback). Only direct-receive owners, the ones that
`canonical_bytes_at` can answer for, also enter a process-wide ordered index
under one mutex; ordinary owners never touch the index or its mutex. The index
and mutex share process lifetime so global/static buffers can safely unregister
during shutdown. Registration allocates the record (and, for a direct-receive
owner, its index node) and reports `XBadAlloc` on allocation failure. Move/swap
update the existing record; destruction detaches it without allocating. A buffer's
own writes, moves, and destruction still require external synchronization.

Fresh-owner byte load enables its destination pages and completely copies input
before publishing logical length, avoiding a redundant full-range memset.
Ordinary grow and shrink/regrow continue to clear newly exposed bytes. Verified
save constructs the final owning string directly: the supported
`resize_and_overwrite` path fills it once, and the fallback uses reserve/append.
Neither transport path exports VM metadata or promises zero-copy.

Vector/string reserve first attempts to extend its trusted live physical tail
allocation inside existing logical space. All Core header, `used_end`, and wire
allocator `arena_end` changes are checked before publication. Oversized reused
blocks retain their physical size; a successful reserve changes only capacity
and the required allocation metadata. A non-tail block or insufficient logical
space takes the existing allocation/migration path. It does not enable pages,
grow the arena, consume adjacent free blocks, or change the wire layout.

## Lifecycle and pointers

Growth, logical shrink, and save preserve the base and the addresses of existing
live arena allocations. Container reserve/rebuild, insertion, erase, assignment,
and replacement can still move or destroy internal elements. Compaction creates
another reservation and is a separate storage identity.

Move transfers reservation ownership without copying payload. Handles bind to
the C++ buffer object, not to the transferred reservation: moved-from handles
resolve empty, existing destination handles resolve the new root, and handles
after swap follow their respective buffer objects. Object-local epochs change
when storage is replaced or moved out, avoiding accidental cache hits from
equal source/destination epoch values. Handles require their buffer object to
remain alive. Local root identity normalizes cv qualifiers, allowing a read-only
handle to an unregistered local root. Existing schema-name and wire cv rules
are unchanged.

## Verification plan

Establish the full P2996 baseline first. Test the independent OS layer using a
regular compiler and query real page permissions. Then run the full library
suite, including wire/signature checks, with the required reflection toolchain.
Exercise page and limit boundaries, overflow rejection, deterministic reserve,
enable, and post-enable preparation failures, retained-tail zeroing, live root
and allocator preservation, nested containers, save/load/regrow, move/swap,
handle cache identity, and destruction. Use sanitizers for the affected code.
Record the actual host/platform/compiler and distinguish unexecuted platforms.

## Platform sources

- [Windows VirtualAlloc](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc)
- [Linux mprotect](https://man7.org/linux/man-pages/man2/mprotect.2.html)
- [Apple mmap](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/mmap.2.html)

Actual validation results and migration examples are recorded in
`STABLE_VM_STORAGE_VALIDATION.md` and `STABLE_VM_STORAGE_MIGRATION.md`.

## Follow-up review

The 2026-10-13 review reproduced global-buffer shutdown and const-handle
regressions that the initial suite did not cover. Both are fixed, with natural
process-exit and type-access regressions. The reviewed sequence is preserved in
[VM_STORAGE_IMPROVEMENT_PLAN.md](VM_STORAGE_IMPROVEMENT_PLAN.md); implementation,
measurements, optional-policy decisions, and remaining platform limits are in
[VM_STORAGE_IMPROVEMENTS_RESULTS.md](VM_STORAGE_IMPROVEMENTS_RESULTS.md).
