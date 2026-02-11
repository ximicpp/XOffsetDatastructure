## ADDED Requirements

### Requirement: XOffsetPtr Default Safety Classification
The system SHALL reject `XOffsetPtr<T>` as an unsafe type by default, because it is reference-semantic (not value-semantic) and its validity depends on external state.

#### Scenario: XOffsetPtr is rejected by default
- **WHEN** a struct contains `XOffsetPtr<int32_t>` as a member without user opt-in
- **THEN** `is_xbuffer_safe` SHALL return false

### Requirement: XOffsetPtr User Opt-In
The system SHALL allow users to opt-in to XOffsetPtr safety by specializing `is_safe_leaf<XOffsetPtr<T>>` for specific pointed-to types.

#### Scenario: User opts in for specific type
- **WHEN** a user specializes `detail::is_safe_leaf<XOffsetPtr<int32_t>>` as `std::true_type`
- **THEN** `is_xbuffer_safe` SHALL return true for structs containing `XOffsetPtr<int32_t>`

### Requirement: XOffsetPtr Compaction Responsibility
The system SHALL NOT provide built-in compaction migration for `XOffsetPtr<T>`. Users who opt-in and require compaction MUST register a custom `migrate_as` specialization.

#### Scenario: Compaction without custom migration
- **WHEN** `XBufferCompactor::compact_automatic` encounters an `XOffsetPtr<T>` member without a registered `migrate_as` specialization
- **THEN** the system SHALL use the default `resolve_strategy` fallback behavior