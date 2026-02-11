## ADDED Requirements
### Requirement: Line-by-Line Memory Lifecycle Analysis
The project SHALL provide a technical document that performs line-by-line analysis
of the example programs (`examples/helloworld.cpp`, `examples/demo.cpp`), tracing
every memory operation including: buffer creation, named object construction,
temporary object construction on stack with segment-allocated internal data,
allocator type conversion (rebind), move/copy assignment across stack↔segment
boundary, offset_ptr recalculation, vector reallocation and free-list return,
serialization byte-copy, deserialization segment rediscovery, reflection-driven
cross-segment migration, NRVO/move return semantics, and destructor chains.

#### Scenario: Developer traces a specific line
- **WHEN** a developer looks up line 24 of helloworld.cpp (`player->name = XString(...)`)
- **THEN** the document explains the full chain: allocator rebind, stack temporary
  construction, segment char allocation, move-assignment with offset_ptr update,
  and temporary destruction

#### Scenario: Developer understands every memory operation
- **WHEN** a developer reads the document from start to end
- **THEN** every line of the example code that involves memory (allocation, construction,
  copy, move, assignment, deallocation) has a corresponding explanation with no gaps

#### Scenario: ASCII memory diagrams per phase
- **WHEN** the document transitions between phases (creation → mutation → serialization → compaction)
- **THEN** an ASCII diagram shows the segment layout at that point including header,
  free-list nodes, named objects, and their internal data blocks

#### Scenario: Fragmentation and compaction analysis
- **WHEN** the document describes vector push_back/pop_back cycles and compact_automatic
- **THEN** it explains free-list fragmentation patterns and how each member type (trivial,
  allocator-aware, container) is migrated between old and new segments