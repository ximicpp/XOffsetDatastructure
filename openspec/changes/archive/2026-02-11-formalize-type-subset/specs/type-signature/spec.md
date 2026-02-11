## ADDED Requirements
### Requirement: Formalized Type Subset Model
The project SHALL maintain a formal model of the Safe Type Subset in `docs/TYPE_SUBSET_MODEL.md`, defining the mathematical membership rules, type taxonomy, and responsibility boundaries between XOffsetDatastructure and TypeLayout.

#### Scenario: Type subset membership is unambiguous
- **WHEN** a developer needs to determine if a C++ type can be stored in XBuffer
- **THEN** the Type Subset Model document provides a clear classification tree
- **AND** every leaf category maps to exactly one of: accept, reject, or recurse

#### Scenario: Safety, Migration, and Signature share the same type classification
- **WHEN** a type is classified by `is_xbuffer_safe<T>`
- **THEN** the same classification categories are used by `XBufferCompactor` for migration dispatch
- **AND** the same classification categories determine which TypeLayout signature mode is used
