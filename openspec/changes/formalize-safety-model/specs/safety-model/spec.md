## ADDED Requirements
### Requirement: Formal Architecture Set Definition
The project SHALL formally define the Architecture Set A as a set of tuples
(pointer_size, endianness, sizeof_table, alignment_table) with the current
singleton A = { Arch64LE }, enforced at compile time via static_assert.

#### Scenario: Platform validation at compile time
- **WHEN** a program includes xoffsetdatastructure2.hpp on a platform not in A
- **THEN** compilation fails with a descriptive static_assert message

### Requirement: Formal Safe Type Subset Definition
The project SHALL formally define the Safe Type Subset S inductively:
- S₀ (base): fixed-width primitives, fixed-underlying enums
- S_string: XString
- S_container: XVector<T>/XSet<T>/XMap<K,V> where element types ∈ S
- S_composite: structs where all nonstatic data members' types ∈ S,
  with no polymorphism, inheritance, or union

#### Scenario: Type accepted by safety check
- **WHEN** a type T satisfies the inductive definition of S
- **THEN** `is_xbuffer_safe<T>::value` evaluates to true at compile time

#### Scenario: Type rejected by safety check
- **WHEN** a type T contains a raw pointer, std::string, virtual function, or other excluded construct
- **THEN** `is_xbuffer_safe<T>::value` evaluates to false with a diagnostic message

### Requirement: Zero-Copy Safety Theorem
The project SHALL provide a documented proof that for all T ∈ S and all
architectures in A, byte-copying a buffer containing a valid object graph
of type T produces a semantically equivalent object graph. The proof SHALL
cover primitives, enums, offset_ptr, containers, and composite types.

#### Scenario: Developer verifies safety reasoning
- **WHEN** a developer reads docs/FORMAL_SAFETY_MODEL.md §3
- **THEN** they can follow the inductive proof from base cases to composite types

### Requirement: Invariant Catalog
The project SHALL enumerate and cross-reference the invariants that
underpin zero-copy safety (offset_ptr, no hidden state, fixed-width types,
fixed endianness, deterministic alignment, offset-based index).

#### Scenario: Invariant traceability
- **WHEN** a developer looks up invariant I1 (all internal pointers are offset_ptr)
- **THEN** they find the corresponding code enforcements (is_safe_leaf exclusions, Boost.IPC types)
