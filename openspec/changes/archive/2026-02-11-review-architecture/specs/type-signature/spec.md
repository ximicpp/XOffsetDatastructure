## ADDED Requirements
### Requirement: Architecture Review Document
The project SHALL maintain an architecture review document (`docs/ARCHITECTURE_REVIEW.md`) that maps component boundaries, dependency directions, and extension points for the XOffsetDatastructure ↔ TypeLayout integration.

#### Scenario: Architecture review covers all components
- **WHEN** a developer reads `docs/ARCHITECTURE_REVIEW.md`
- **THEN** every logical section of `xoffsetdatastructure2.hpp` is listed with line ranges and responsibilities
- **AND** the dependency graph between XOffset, TypeLayout, and Boost is documented
- **AND** each extension point (TypeSignature specialization, safety trait, compactor) is evaluated
