# memory-lifecycle Specification

## Purpose
Defines how XBuffer containers (XVector, XMap, XSet) automatically propagate the buffer's
allocator to element construction, eliminating the need for users to manually pass
`get_segment_manager()` in container operations.
## Requirements
### Requirement: Simplified Container Element Insertion
The system SHALL allow inserting elements into XVector, XMap, and XSet without requiring
manual allocator or segment_manager propagation when the container already holds an allocator.

#### Scenario: XVector<XString> push_back with const char*
- **WHEN** user calls `vec.push_back("Alice")` on an XVector<XString>
- **THEN** the system constructs an XString using the vector's internal allocator
- **AND** the string data is allocated in the segment memory

#### Scenario: XMap<XString, V> lookup with const char*
- **WHEN** user calls `map["key"]` on an XMap<XString, V>
- **THEN** the system constructs an XString key using the map's internal allocator
- **AND** no temporary heap allocation occurs outside the segment

#### Scenario: XVector<CustomStruct> emplace without explicit allocator
- **WHEN** user adds a custom struct to XVector<CustomStruct>
- **THEN** the system automatically propagates the allocator to the struct constructor
- **AND** all nested containers within the struct use segment memory

