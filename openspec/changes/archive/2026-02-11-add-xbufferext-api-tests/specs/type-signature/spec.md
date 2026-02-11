## MODIFIED Requirements

### Requirement: Type Safety Enforcement
The system SHALL enforce type safety for all XBufferExt convenience APIs including `find_ex`, `find_or_make`, and `stats`.

#### Scenario: find_ex returns found object
- **WHEN** an object of type T has been created with name "obj"
- **THEN** `find_ex<T>("obj")` SHALL return a non-null pointer and count

#### Scenario: find_ex returns null for missing object
- **WHEN** no object with name "missing" exists
- **THEN** `find_ex<T>("missing")` SHALL return a null pointer

#### Scenario: find_or_make creates if missing
- **WHEN** no object with name "new_obj" exists
- **THEN** `find_or_make<T>("new_obj")` SHALL create and return a valid pointer

#### Scenario: find_or_make finds if existing
- **WHEN** an object with name "existing" already exists
- **THEN** `find_or_make<T>("existing")` SHALL return the existing object pointer
