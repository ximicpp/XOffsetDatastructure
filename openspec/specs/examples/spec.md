# examples Specification

## Purpose
TBD - created by archiving change analyze-examples-readiness. Update Purpose after archive.
## Requirements
### Requirement: Example Code Correctness
Example code (helloworld.cpp, demo.cpp) SHALL correctly follow all safety rules
documented in the README, including pointer invalidation handling, buffer access
patterns, and error checking.

#### Scenario: No dangling pointer usage after buffer resize
- **WHEN** an example calls grow(), shrink_to_fit(), or compact_automatic()
- **THEN** it SHALL NOT use any previously obtained raw pointers
- **AND** it SHALL re-acquire object references via root<T>() or find<T>()

#### Scenario: Compacted buffer accessed correctly
- **WHEN** an example creates a compacted buffer via compact_automatic<T>()
- **THEN** it SHALL access the compacted data using the public API (root<T>() or find<T>())
- **AND** it SHALL NOT expose internal implementation details like XBUFFER_ROOT_NAME to users

### Requirement: Example Code Usability
Example code SHALL be directly copyable and runnable by new users without
requiring knowledge of internal implementation details.

#### Scenario: Data structure definition is clear and consistent
- **WHEN** a user reads the example data structures (Player, Item, GameData)
- **THEN** all structures SHALL follow a consistent allocator-aware pattern
- **AND** no legacy or mixed API styles SHALL be present

#### Scenario: Container operations use simplified syntax
- **WHEN** a user reads container operations in examples
- **THEN** all operations SHALL use the simplified syntax (no manual allocator passing)
- **AND** comments SHALL explain the automatic allocator injection where appropriate

