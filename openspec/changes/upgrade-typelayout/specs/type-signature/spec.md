## MODIFIED Requirements
### Requirement: TypeLayout Integration
The system SHALL use TypeLayout as the type-signature engine via Git submodule
at `external/typelayout`, tracking the latest stable upstream version.

#### Scenario: Submodule upgrade
- **WHEN** TypeLayout upstream publishes a new version
- **THEN** the submodule is updated and all existing tests continue to pass
