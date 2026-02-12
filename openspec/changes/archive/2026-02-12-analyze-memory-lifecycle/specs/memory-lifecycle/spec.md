## ADDED Requirements
### Requirement: Line-by-Line Memory Lifecycle Analysis
The project SHALL provide a technical document that performs line-by-line analysis
of the example programs, tracing every memory operation including: buffer creation,
named object construction, temporary object construction, allocator type conversion,
move/copy assignment, offset_ptr recalculation, vector reallocation, serialization,
deserialization, compaction migration, and destructor chains.

#### Scenario: Developer traces a specific line
- **WHEN** a developer looks up line 24 of helloworld.cpp (`player->name = XString(...)`)
- **THEN** the document explains the full chain: allocator rebind, stack temporary
  construction, segment char allocation, move-assignment with offset_ptr update,
  and temporary destruction

#### Scenario: ASCII memory diagrams per phase
- **WHEN** the document transitions between phases
- **THEN** an ASCII diagram shows the segment layout at that point

### Requirement: Correctness Audit Based on Lifecycle Analysis
The project SHALL audit the implementation for correctness issues discovered through
the lifecycle analysis, covering: pointer/reference validity after grow/shrink/compact,
exception safety of temporary construction and migration, destructor completeness
when segments are bulk-freed, cross-segment operation correctness, and concurrency
documentation for null_mutex_family.

#### Scenario: Correctness issue identified and classified
- **WHEN** a correctness issue is found during analysis
- **THEN** it is classified as Critical/Important/Nice-to-have with a fix recommendation

#### Scenario: Critical issues fixed
- **WHEN** a Critical correctness issue is identified
- **THEN** it is fixed in the same proposal and all tests pass

### Requirement: Usability Improvement Analysis
The project SHALL analyze usability issues discovered through the lifecycle analysis,
covering: API traps (allocator type confusion, raw pointer invalidation), missing
convenience interfaces (direct string assignment, range push_back, capacity estimation),
error diagnostics (segment-full messages, per-member safety errors), and documentation
gaps in the user mental model.

#### Scenario: Usability improvement proposed
- **WHEN** a usability issue is found during analysis
- **THEN** it is documented with a proposed solution and logged as a follow-up proposal
  or directly implemented based on severity