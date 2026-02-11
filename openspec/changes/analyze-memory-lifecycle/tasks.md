## 1. Buffer Creation Analysis
- [ ] 1.1 Trace XManagedMemory(4096): vector<char> allocation + create_impl internals
- [ ] 1.2 Document segment manager header layout and overhead

## 2. Object Construction Analysis
- [ ] 2.1 Trace make<Player>("Hero"): iset_index entry + placement new in segment
- [ ] 2.2 Trace Player(allocator): XString/XVector default construction with allocator
- [ ] 2.3 Trace trivial scalar assignment (id, level) — direct segment memory write

## 3. Temporary Object & Move/Copy Semantics (CRITICAL)
- [ ] 3.1 Trace XString("Alice", allocator): stack temporary with segment-allocated char data
- [ ] 3.2 Trace allocator type conversion: allocator<XString,SM> → allocator<char,SM> (rebind)
- [ ] 3.3 Trace move-assignment player->name = <temporary>: offset_ptr recalculation
- [ ] 3.4 Trace temporary destruction: moved-from state, no deallocation
- [ ] 3.5 Document offset_ptr behavior across stack↔segment boundary (offset = target - this)

## 4. Data Mutation & Reallocation
- [ ] 4.1 Trace push_back(101): initial vector allocation from free-list
- [ ] 4.2 Trace push_back triggering reallocation: 1.1x growth, old block → free-list
- [ ] 4.3 Trace pop_back: size decrement only, no memory return
- [ ] 4.4 Document free-list fragmentation created by reallocation cycles

## 5. Serialization & Deserialization
- [ ] 5.1 Trace save_to_string: byte-for-byte copy, no encoding
- [ ] 5.2 Trace load_from_string: vector<char> construction → move into m_buffer → open_impl
- [ ] 5.3 Document open_impl segment rediscovery (header, free-list, iset_index)
- [ ] 5.4 Trace NRVO on load_from_string return path
- [ ] 5.5 Trace find_ex: iset_index lookup via offset_ptr chain

## 6. Compaction & Migration
- [ ] 6.1 Trace compact_automatic: new segment creation
- [ ] 6.2 Trace migrate_member TrivialCopy: id, level direct assignment
- [ ] 6.3 Trace migrate_member AllocatorAware: XString cross-segment copy construction
- [ ] 6.4 Trace migrate_container: XVector trivially_copyable fast path (container assignment)
- [ ] 6.5 Trace shrink_to_fit + update_after_shrink: buffer reallocation chain
- [ ] 6.6 Trace return value: NRVO or BOOST_MOVABLE_BUT_NOT_COPYABLE move

## 7. Memory Reclamation
- [ ] 7.1 Trace destructor chain: ~XManagedMemory → destroy_impl → vector swap
- [ ] 7.2 Document bulk deallocation: segment objects NOT individually destructed

## 8. Cross-cutting Analysis
- [ ] 8.1 Document allocator propagation in Boost.Container (operator= across segments)
- [ ] 8.2 Document x_seq_fit vs x_best_fit fragmentation characteristics
- [ ] 8.3 Draw ASCII memory layout diagrams for all 7 phases

## 9. Assembly
- [ ] 9.1 Assemble into docs/MEMORY_LIFECYCLE_ANALYSIS.md
- [ ] 9.2 Cross-reference with helloworld.cpp line numbers