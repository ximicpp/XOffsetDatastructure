# Design and adoption gates

## Final adoption decision

The two-layer best-fit/split and Core coalescing implementation below describes
the measured E prototype, not the accepted production allocator. Correctness
passed, but Core reuse was 8.3x slower, cache reuse 2.4x slower, and incremental
interleaved extent increased 49%. Removing best-fit/coalescing still left Core
about 1.8x slower and the interleaved extent 24% larger. Preserve these snapshots
and negative results; final F retains D's first-fit/whole-block policy. Automatic,
explicit reclamation, bounded pooling, and the narrow fresh-load mapping path
are adopted independently. See docs/VM_MEMORY_REUSE_RESULTS.md for evidence.

## Starting point

Current production headers must match the previous D baseline:
`896ac72ba55564bfa6177df3b89d7c192e0b2584c092fdb409c63e828bfadfb0`
(main) and `af90d9ea4281cba703878a1042ec63aedeb3fa1807fbe8feacd4b6dfae72b186`
(VM owner). Preserve original signatures and all prior evidence. Use the actual
P2996 toolchain and frozen snapshots for comparisons.

## Coordinated freelist reuse

Core free blocks and the payload's 16 cache classes own distinct physical
allocations. Choose the smallest suitable Core block, and the smallest suitable
block in the first usable wire size class. Respect actual address alignment.

Physical size comes from AllocationHeader.user_bytes, not container capacity or
an older cache's advertised block_bytes. For a returned user address p, physical
size U, and requested bytes n, choose residual user q = align_up(p+n+16,
max_align_t). The returned block's physical user extent is q-16-p; the residual
extent is p+U-q. Split only if all bounds/header/alignment calculations pass and
the residual holds at least eight bytes. Otherwise reuse the original whole
block. Write two disjoint valid AllocationHeaders. The returned pointer and all
other live objects remain fixed; used_end does not decrease.

Core replaces the chosen free-block entry with its residual, requiring no new
host allocation during reuse. Wire reuse unlinks its chosen cached block,
subtracts the OLD advertised bytes, and inserts the residual in its appropriate
class with updated free_bytes/arena_end. A residual too large for cache requires
an explicit safe ownership disposition, without loss or duplicate ownership.
Retirement advertises the representable physical user size, rounded down to
8-byte cache units; oversized blocks go to Core. Preserve legacy frames where
advertised cache size is smaller than physical extent. Keep frozen structs and
wire validation rules unchanged; validate old/new mutation interchange.

All potentially failing arithmetic and metadata planning precedes mutation.
No splitting live allocations or cross-layer coalescing is introduced in that
stage. A second stage coalesces physically adjacent Core-owned free blocks;
wire-cache blocks first transfer ownership through the existing cache release
path. Gaps of unknown ownership are never swallowed. Existing oversized-block
tail tests must retain unsplittable-reuse coverage and add split-residual tails.

## Transport investigation

First compare full verified loads within one executable/process using isolated
version translation units; validate complete objects outside timing. Separate
reserve, page enable, copy, owner registration, graph validation and destruction
in diagnostic-only snapshots. Profiling instrumentation is excluded from final
end-to-end comparisons.

A candidate may combine reserve and initial page enable only when fresh input
covers every reserved page. Keep maximum capacity, logical length, page rounding,
zero-length behavior and all Reserve/Enable/Prepare fault semantics. Any unused
reserved pages must remain inaccessible. The destination still completely copies
input before logical publication; ordinary grow/regrow retains zeroing. No input
adoption, borrowed lifetime or unchecked typed access is introduced.

## Validation and measurement

New regressions must first fail against D. Test both freelist layers, exact fits,
tiny residuals, oversized legacy cache entries, odd-size Core blocks, over-aligned
requests, repeated split/reuse, release-cache transitions, host OOM, hard bounds,
nonoverlap, tail expansion, hostile frames and old/new reader mutation. Run full
native/Linux P2996, affected ASan/UBSan and independent-buffer TSan as appropriate.

A pressure workload must allocate again AFTER large-to-small reuse: splitting
cannot retroactively lower historical high-water extent. Measure successful
requests at fixed logical capacity, additional growth/failures and metadata
costs. Keep previous target workloads as regression guards. Retain full semantic
checks, source hashes, paired raw samples, platform/toolchain limits and negative
results. Schedule heavy compilation separately from timing.

## Explicit Automatic policy (approved extended scope)

Add process-local XGrowPolicy {Explicit, Automatic}; default Explicit. Extend the
existing MaximumCapacity wrapper with with_growth(policy) and with_pool(pool)
builders so constructors/create/raw/verified/Typed load paths share configuration.
A setter/getter supports changing the policy after safe construction/load. Policy
follows storage through move/swap, and moved-from objects reset to Explicit.
Compaction creates an independent default-configured owner unless a later API
explicitly requests otherwise; no policy is serialized.

Automatic uses bounded geometric growth to fit a checked physical end, never
beyond maximum_capacity. It applies to new Core allocations and live-tail
expansion. Arithmetic, alignment and wire metadata checks precede growth. Failed
enable/preparation does not publish logical length or allocation metadata;
OS-accessible pages may remain as already documented. A later element-constructor
exception may leave increased capacity, while existing values remain valid.
For wire allocations, attempt reuse/current logical capacity, release cached
blocks and retry, then permit Automatic growth: free storage must not be ignored
merely because reservation headroom exists. Explicit mode retains grow/retry.

## Explicit physical tail reclamation

Core reclaim_tail_pages() delegates to the OS owner without changing logical
length; callers can first request logical shrink/save. Operate only on complete
pages starting at ceil(logical/page) through accessible, retaining the partial
last page and full reservation. Return status and attempted/discarded/inaccessible
bytes, distinguishing no-op, success, failure, and Linux discard-success/protect-
failure. Retry remains possible with truthful accessible counters. Grow/regrow
always initializes newly exposed logical bytes.

Windows uses MEM_DECOMMIT. Linux discards private anonymous pages with
MADV_DONTNEED before PROT_NONE; a later protection failure leaves accessible
unchanged and reports the partial result. macOS atomically replaces only its own
dead tail using MAP_FIXED anonymous PROT_NONE mapping, with no preceding unmap;
Apple XNU's failure restoration contract is independently researched and tested.
Default shrink/save continue to retain pages.

## Explicit bounded pool

VmStoragePool is a shared process-local handle, public alias XBufferPool. An
empty default handle allocates nothing and disables pooling. Explicit Limits set
max_cached_blocks, max_reserved_bytes, max_accessible_bytes. Fixed slot capacity
is reserved once at pool construction. Cache only exact maximum-capacity matches;
return requires all budgets to fit, otherwise release the OS reservation. Pool
stats expose hits/misses and cached count/reserved/access bytes; clear releases
idle mappings while retaining slot capacity. Live owners keep the state alive;
cached owners contain no pool reference, preventing shared-pointer cycles.

Returning owners have already left the owner registry. Return, move and swap do
not allocate; cache return is noexcept. Reused normal creation clears the entire
new logical range; from_bytes copies it completely. Retained dirty tails remain
outside logical access and are cleared on subsequent growth. Failed creation or
load may return a valid unregistered reservation to its pool. Reserve injection
precedes both cache hits and cold allocation; Enable is consumed only when more
pages are needed; Prepare precedes logical publication. No implicit global pool,
fixed-address adoption of external data, or pooling of live buffers is allowed.

Pool configuration, reclaim operation and Automatic policy must all receive
separate correctness/failure/lifecycle/thread tests and targeted measurements.
The same-buffer concurrency exclusion remains unchanged.
