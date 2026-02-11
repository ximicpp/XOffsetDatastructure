## MODIFIED Requirements

### Requirement: Buffer Growth Safety
The system SHALL attempt to restore the buffer to its original state if growth fails partway through.

#### Scenario: grow fails after resize
- **WHEN** `grow()` succeeds in resizing the internal buffer but fails to re-open the managed memory
- **THEN** the system SHALL attempt to restore the buffer to its original size and return false
