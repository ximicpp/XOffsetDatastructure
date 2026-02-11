# Change: Analyze Memory Lifecycle via Demo Walkthrough

## Why
The library's core value — zero-encoding serialization — depends on a non-trivial memory model
(Boost.Interprocess segment manager on a `std::vector<char>` backing store). Currently there is
no documentation that traces what actually happens at the byte level when user code runs. Without
this, contributors and users cannot reason about allocation cost, fragmentation patterns, pointer
stability, or the subtle interactions between stack temporaries and segment memory.

## What Changes
- Produce a detailed technical document (`docs/MEMORY_LIFECYCLE_ANALYSIS.md`) that traces every
  memory operation in the `helloworld.cpp` demo, step by step. The analysis MUST NOT omit any
  category of operation. Specifically:

  ### Phase 1 — Buffer Creation (line 17)
  1. `XBufferExt xbuf(4096)`:
     - `std::vector<char>` heap allocation (4096 bytes)
     - `create_impl()`: segment manager header layout, free-list initialization, iset_index bootstrap
     - How much overhead the segment manager consumes (header + free-list node + index root)

  ### Phase 2 — Object Construction (lines 21–24)
  2. `xbuf.make<Player>("Hero")` — named object construction:
     - `validate_xbuffer_type<Player>()` compile-time check
     - `construct<Player>("Hero")(segment_manager)` internal path:
       a. iset_index allocates a name→offset entry (string "Hero" stored in segment)
       b. 72 bytes allocated from free-list for the Player object
       c. Player(allocator) constructor called **in-place** (placement new) inside the segment
       d. XString `name` member default-constructed with allocator (empty, no char allocation)
       e. XVector<int32_t> `items` member default-constructed with allocator (empty, capacity=0)
     - Returns `Player*` — an offset_ptr-derived address into the segment

  3. `player->id = 1; player->level = 10;` — trivial scalar writes:
     - Direct store to segment memory, no allocation

  4. **`player->name = XString("Alice", xbuf.allocator<XString>())`** — CRITICAL temporary path:
     - **Allocator type conversion**: `xbuf.allocator<XString>()` returns
       `allocator<XString, segment_manager>`, which is **implicitly converted** to
       `allocator<char, segment_manager>` by Boost.IPC's rebind mechanism when passed
       to the XString constructor
     - **Temporary construction on stack**: `XString("Alice", alloc)` constructs a temporary
       XString object on the **call stack**. The XString control block (offset_ptr to chars,
       size, capacity) lives on the stack, but the actual char data ("Alice\0") is allocated
       **inside the segment** via the segment manager allocator
     - **Move assignment** `player->name = <temporary>`: The XString move-assignment operator
       transfers ownership. The segment-resident char data is NOT copied — only the control
       block (offset_ptr, size, capacity) is updated in-place inside the segment. The
       **offset_ptr value changes** because `&(player->name)` is at a different address than
       the stack temporary — offset_ptr stores `target - this`, so the stored offset is
       recalculated during assignment
     - **Temporary destruction**: The stack temporary's destructor runs, but since ownership
       was moved, it's a no-op (no deallocation)

  ### Phase 3 — Data Mutation (lines 28–30, 74–78)
  5. `player->items.push_back(101)` — vector element insertion:
     - Vector has capacity=0, so first push_back triggers initial allocation:
       a. Allocator requests N bytes from segment manager free-list
       b. Free-list splits a block, returns pointer (as offset_ptr)
       c. `int32_t(101)` is trivially copied into the allocated slot
     - Subsequent push_back(102), push_back(103): may fit in existing capacity or trigger
       **reallocation with 1.1x growth factor** (detail::growth_factor_custom)
     - On reallocation:
       a. New larger block allocated from segment free-list
       b. Existing elements **trivially copied** (int32_t is trivially copyable) to new block
       c. Old block **returned to free-list** (becomes a free node — potential fragmentation source)
       d. Vector's internal offset_ptr updated to point to new block

  6. `player->items.pop_back()` — element removal:
     - Decrements size only; does NOT free memory or return capacity to free-list
     - The unused slots remain allocated → internal fragmentation within the vector
     - Combined with push_back-then-pop_back cycles → segment-level fragmentation (old
       vector blocks in free-list, new larger blocks in use)

  ### Phase 4 — Serialization (line 44)
  7. `xbuf.save_to_string()`:
     - `get_buffer()` returns `&m_buffer` (the `vector<char>`)
     - `std::string(begin, end)` performs a **byte-for-byte copy** of the entire 4096-byte buffer
     - **No encoding, no transformation** — this IS the serialization
     - Why it works: all internal pointers are `offset_ptr` (relative to their own address),
       so they remain valid in any copy at any address

  ### Phase 5 — Deserialization (lines 49–50)
  8. `XBufferExt::load_from_string(data)`:
     - `std::vector<char> buffer(data.begin(), data.end())` — heap allocation, byte copy
     - `XBufferExt xbuf(buffer)` — constructor takes `vector<char>&`:
       a. `m_buffer = std::move(externalBuffer)` — **vector move**: transfers heap ownership,
          no byte copy, O(1)
       b. `open_impl(addr, size)` — segment manager **rediscovers** existing structures:
          - Reads header at known offset to find free-list root
          - Rebuilds iset_index navigation from stored offset_ptr chains
          - Does NOT reconstruct objects — they're already there in the bytes
     - Returns XBufferExt — **NRVO** (Named Return Value Optimization) elides the move

  9. `loaded.find_ex<Player>("Hero")`:
     - iset_index lookup: walks the intrusive set using offset_ptr links
     - Returns `pair<Player*, bool>` — the Player* points into the loaded segment
     - **Structured binding** `auto [loaded_player, found]` decomposes the pair on the stack

  ### Phase 6 — Compaction (line 87)
  10. `XBufferCompactor::compact_automatic<Player>(xbuf, "Hero")`:
      - **New segment creation**: `XBuffer new_xbuf(new_size)` — fresh vector<char>, clean free-list
      - **Old object lookup**: `old_xbuf.find<Player>("Hero")`
      - **New object construction**: `new_xbuf.construct<Player>("Hero")(new_segment_manager)`
        — allocates 72 bytes in new segment, constructs empty Player
      - **Reflection-driven member migration** (`migrate_members`):
        - `id` (int32_t): `MigrateStrategy::TrivialCopy` → direct assignment `new.id = old.id`
        - `level` (int32_t): same as above
        - `name` (XString): `MigrateStrategy::AllocatorAware` →
          `XString(old_name, new_segment_manager)` — **allocator-aware copy construction**:
          constructs new XString in new segment, **copies char data** from old segment to new
          segment (cross-segment copy, not move)
        - `items` (XVector<int32_t>): `MigrateStrategy::Container` →
          `migrate_container()`: elements are `trivially_copyable`, so executes
          `new_container = old_container` — **container copy assignment** which allocates
          in new segment and copies all int32_t elements
      - **`shrink_to_fit()`**: reduces new segment to minimum size, triggers buffer reallocation
        and `update_after_shrink()` which creates a fresh segment from the compacted bytes
      - **Return value**: `XBuffer` returned — uses **NRVO** or **move construction**
        (`BOOST_MOVABLE_BUT_NOT_COPYABLE` ensures move, not copy)

  ### Phase 7 — Memory Reclamation (end of main)
  11. Destructor chain (reverse order of construction):
      - `compacted` (XBuffer): `~XManagedMemory()` → `priv_close()` →
        `destroy_impl()` (releases segment manager) → `vector<char>().swap(m_buffer)` (frees heap)
      - `loaded` (XBufferExt): same chain
      - `data` (std::string): standard heap deallocation
      - `xbuf` (XBufferExt): same chain as compacted
      - All segment-internal objects (Player, XString chars, XVector data) are **NOT individually
        destructed** — the entire segment is freed as a single heap block

  ### Cross-cutting Concerns
  12. **offset_ptr mechanics**: How the stored offset changes when the "this" location changes
      (stack vs segment, old segment vs new segment)
  13. **Allocator propagation in Boost.Container**: How `vector::operator=` across segments works
      (allocator-aware assignment: allocate in target's segment, copy data)
  14. **Free-list fragmentation patterns**: Visual diagram of segment state after push_back cycles
  15. **x_seq_fit vs x_best_fit**: When sequential fit creates more fragmentation than red-black tree

- Add annotated ASCII memory diagrams showing segment layout at each stage.
- No code changes required — this is a documentation/analysis proposal.

## Impact
- Affected specs: none (new standalone document)
- Affected code: none (read-only analysis)
- New artifact: `docs/MEMORY_LIFECYCLE_ANALYSIS.md`