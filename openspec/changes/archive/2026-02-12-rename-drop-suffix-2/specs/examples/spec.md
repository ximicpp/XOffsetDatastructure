## MODIFIED Requirements
### Requirement: Example Code Quality
Example code (helloworld.cpp, demo.cpp) SHALL correctly follow all safety rules
documented in the README, including pointer invalidation handling, buffer access
via public API (`root<T>()`, `has_root<T>()`), and consistent allocator-aware
type definitions. All references SHALL use the canonical namespace name
`XOffsetDatastructure` (without the "2" suffix).

#### Scenario: No legacy namespace in examples
- **WHEN** inspecting all files under `examples/`
- **THEN** zero occurrences of `XOffsetDatastructure2` SHALL be found

#### Scenario: Include path uses canonical header name
- **WHEN** inspecting `#include` directives in example files
- **THEN** all references SHALL use `xoffsetdatastructure.hpp` (not `xoffsetdatastructure2.hpp`)
