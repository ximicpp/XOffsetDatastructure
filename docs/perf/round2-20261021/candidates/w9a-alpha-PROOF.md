# α group on W9a — B1 owner direct fetch, B4 allocator fast path (with A2 and A3), B5

* **Base:** W9a (`/Volumes/XOffsetCS/candidates/w9a`).
* **Candidate:** `/Volumes/XOffsetCS/candidates/w9a-alpha`.
* **Per-component patches** against W9a, for exploratory timing only:
  `alpha-B1.patch`, `alpha-B4.patch` (B4 + A2 + A3) and `alpha-B5.patch`.
* **Common promise:** no allocation decision changes, so the emitted wire is
  byte-identical. Checked on S/M/L with the b7 method (§4).

## 1. B1 — owner direct fetch from a host page

### Layout

* `VirtualMemoryStorage` maps `host + reserved` bytes and places
  `base_ = mapping + host`. `host` is one page on POSIX (mmap, then mprotect
  of the host page to read/write on the PROT_NONE path; the full-copy RW path
  already covers it). On Windows it is one allocation granule, so the payload
  keeps its 64 KiB alignment: MEM_RESERVE of the whole range plus MEM_COMMIT
  of the host granule.
* A `std::atomic<void*>` is begun at `base_ - 16` (`owner_slot(payload)`) once
  per mapping, holding null.
* `data()`, `size()`, `reserved_bytes()`, `accessible_bytes()`, pool limits
  and pool statistics are unchanged: they describe the payload only. The 64 MiB
  default maximum capacity is still pure payload; each reservation now spends
  one extra page of address space and, after registration, one resident page.
* `release_mapping` unmaps or releases from `base_ - host_bytes_`.
  `swap_storage` swaps `host_bytes_`, so the page travels with the reservation
  through move, swap and pool reuse.
* No copy, emit or wire path reads below `data()`, so the page is never in
  the wire and never leaks addresses.

### Lookup

`owner_from_payload_ptr(ptr)` performs the following steps, with no TLS
access, loop, registry or shared_ptr:
1. `rec = slot(ptr).load(acquire)`.
2. If `rec` is non-null, `rec->payload == ptr`, and
   `owner = rec->owner.load(acquire)` is non-null, return `*owner`.

Every other case calls the unchanged original search
(`owner_from_payload_ptr_search`). That search keeps the TLS cache, the
registry, the dead-record and new_handler rules, and every error ("arena owner
lookup failed").

### Premises and lifetime / ordering proof

* **L0 (precondition).** Every caller passes an arena header address. That is
  `arena_header::owner()` (behind `freelist_runtime_owner` and
  `reset_freelist_runtime_cache`), `allocate_dynamic_bytes`'s
  `alloc_ref.segment`, and `StableEraseOwnerWitness`. An arena header is
  always `buffer_.data()`, the payload base of a library `VirtualMemoryStorage`;
  wire validation binds `segment_delta` to exactly that segment. Every such
  storage was created by the constructor above (`for_receive`, `from_bytes`
  and the full-copy path all use it), so the host page exists and is mapped
  while the storage is live. A null pointer takes the original path.
* **L1 (publication).** The slot is written with release semantics only
  after the record is inserted into the registry and stored in
  `owner_record_`. This happens in `register_registry` and in the receive
  path's `publish_node`, which is the only other place that sets
  `owner_record_`. The record is fully constructed before the store
  (`make_shared`). A reader's acquire load therefore sees a complete record,
  and `record->payload` is const.
* **L2 (no dangling slot).** `unregister_registry` is the only place where a
  Core gives up `owner_record_`. It is reached from `~XBufferCore`, before
  `buffer_` is destroyed; from `move_from`'s retired Core; and from
  `rollback_storage`, before the storage is swapped back to its token. It
  moves the lease into a local, stores owner = null, and then clears the slot
  (compare-exchange from this record to null). Only after that does the local
  lease die. While the slot is non-null, the record is therefore kept alive
  by the Core's `owner_record_`.
* **L3 (storage still mapped at clearing).** In all three unregister sites
  the storage named by `record->payload` is still owned by the Core, or (for
  rollback) not yet swapped away, so the host page is mapped.
* **L4 (record follows storage).** `XBufferCore::swap` exchanges `buffer_`
  and `owner_record_` together. `move_from` is two such swaps. The only
  storage-only swaps are `adopt_storage`, which runs before registration with
  the slot null, and `rollback_storage`, which runs after unregistration.
  Hence the slot of a storage always names the record whose `payload` is that
  storage, and `update_registry_owner` (called on every swap) sets
  `record->owner` to the Core that now holds it. A lookup after an owner move
  returns the new Core. This is tested: swap, move-construct and pooled reuse.
* **L5 (pooled reuse).** A storage enters the pool only from
  `VirtualMemoryStorage::release` after its Core has unregistered (L2), so the
  slot is null. A new owner republishes it at registration (tested). A
  storage that never had an owner keeps a null slot.
* **L6 (unpublished receive owners).** `prepare_owner` creates the record
  with owner null; `publish_node` publishes the slot; the owner is stored
  last (`publish_received` → `update_registry_owner`). A lookup in between
  sees owner null and takes the original search. That search finds the
  registered record with owner null and throws "arena owner lookup failed",
  as before (`test_verified_receive`'s unpublished callback still passes).
  Before `publish_node` the slot is null and the registry misses, which is
  the same error.
* **Equivalence.** For a live hit, the original search returns the same
  `record->owner` (the registry maps each payload to its unique registered
  record). For every other state the same code runs. Results and errors are
  therefore identical whenever L0 holds.
* **Diagnostic-only change.** `OwnerLookupStats` gains `direct_hits`.
  `test_owner_identity` assertions that pinned TLS cache counters now accept
  "cold or direct", meaning "never the TLS cache". The two tests of TLS
  eviction and delete callbacks, and the late-thread-exit lease test, replay
  their former cold lookups through the search fallback (reflection access),
  so the fallback keeps its coverage. A new `host_page_owner_slot` test
  covers publication, swap, move, clearing before pooling and pooled
  republication.

**Platforms.** The Linux path is the same POSIX code as Darwin. The Windows
code was written but cannot be compiled here (no SDK); it uses
`reservation_granularity_` for the host size, commits the host granule and
releases from `base_ - host_bytes_`. The C++17 storage-only test was compiled
and run with both `/usr/bin/clang++` and the P2996 clang.

## 2. B4 — allocator fast path (includes A2 and A3)

### The walk W9a performs (call it **W**)

For a request (B = aligned bytes, a = alignment), with class
c = `size_class(B)` < 16, W visits classes i = c, c+1, …, 15:
* It optionally resets the cursor of class i: if (bytes, alignment, head)
  differ, cursor := {B, head_i, 0, 0, a}.
* It walks list i from its head. A node equal to the memo's `first` jumps to
  `last->next`; every other node is visited, and the first node with
  `block_bytes ≥ B` and address ≡ 0 (mod a) is unlinked and returned.
* After a full walk without a fit, the class's cursor records the rejected
  prefix.
* If no class yields a block, W falls through to the Core tail allocation.

### Fast path **F**

F computes `nonempty` = bit i set ⇔ head_i ≠ 0, masked to i ≥ c. Nothing is
written before one of the three branches below is chosen.

* **(b) `nonempty == 0`.** Every class i ≥ c is empty, so W visits them,
  writes their cursors and selects nothing. For an empty class, W's net cursor
  effect is always {B, 0, 0, 0, a}: whether or not it reset, it then sets
  first = last = 0 and head = 0. F writes exactly these values
  (`normalize_empty_freelist_cursor`) and jumps to the same Core tail code.
  The state, the cache and the next step are identical.
* **(a) j = lowest set bit, and the head h of class j fits and is not
  `cursor.first`.**
  * W visits classes c..j−1, all empty, and normalizes them; F does the same.
  * In class j, W applies the same reset rule (F computes `reset` with the
    same three comparisons).
  * W's first node is h. The memo jump applies only if
    `old_first ≠ 0 && h == old_first`, which F excludes.
  * So W visits h, which fits, and unlinks it with an empty prefix. Its
    bookkeeping is: head_j = h->next, `free_bytes -= h->block_bytes`,
    `++epoch`, `cache.epoch = epoch`, `cursor.first = old_first`,
    `cursor.last = old_last`, `cursor.head = head_j`. F performs exactly these
    writes, in the same order relative to `destroy_at(h)`.
  * Both return h.
* **Otherwise** (h does not fit, or h starts a memo interval). F has made no
  change, and the unchanged W runs from the beginning.

The selected block, the free list, `free_bytes`, `epoch` and every cursor are
therefore identical to W in every case. Since the persisted state is
identical, every later decision is identical too. The memo only affects which
nodes are skipped and never which node is chosen, because a memo interval
holds only nodes already rejected for the same (B, a, head); F never consults
a memo except to defer to W. No host cache was added: the 16 heads are one
cache line, and the mask is recomputed per call. This avoids a second
invalidation rule. The existing synchronize/epoch rules still govern the
cursors.

* **A3.** `freelist_size_class` uses `bit_width(bytes − 1) − 3` with the
  bounds ≤ 8 → 0 and > 8·2^15 → 16. It is equal to the doubling loop for every
  uint32: tested exhaustively for 0..2^20, at 2^k ± 1 for all k, at
  UINT32_MAX, and for 10^6 random values. It is also used by wire validation.
* **A2.** `note_allocator_block` returns inline when `arena_begin ≠ 0`,
  `offset ≥ arena_begin` and `offset + bytes ≤ arena_end`. In that case the
  original stores nothing and cannot throw (`delta ≤ UINT32_MAX` and
  `end ≤ arena_end`). Every other case runs the unchanged original body, so
  stores, error messages and their order are equal. Tested by a 4000-step
  model comparison plus the extend-end, lower-begin and zero-begin cases.
* **Tests.** The existing oracle in `test_freelist_index` checks first-fit
  pointer equality across the mixed, M-prefix, randomized, middle-removal,
  epoch and multi-state workloads. The new empty-class-run workload also
  checks, after every full miss, that each empty class holds the walk's exact
  final cursor. All of these pass on both W9a and α.

## 3. B5 — no shared_ptr copy on the tail path

The `owner_record_` copy (`identity`) moved into the `!raw` branch. The
probe `try_allocate_aligned_without_growth` is noexcept and runs no callback,
so `owner->owner_record_` names the same record after the probe that a
pre-probe copy would have held. Inside the branch, the revalidation logic is
unchanged. On success the copy only incremented and decremented a reference
count; the record stays alive through the Core. There is no observable
difference.

## 4. Byte identity (b7 method)

* `build_diag.sh alpha-w9a /Volumes/XOffsetCS/candidates/w9a-alpha`, then
  `sc_xoffset run` on the S/M/L fixtures, writing
  `/Volumes/XOffsetCS/diag/b7-alpha/{S,M,L}`.
* The final wire, canonical output and query files are `cmp`-identical to
  `/Volumes/XOffsetCS/diag/b7/w9a-{S,M,L}`.
* The SHA fields in `r.json` match: final_wire, final_canonical, queries,
  query_before, query_after, record_count.
