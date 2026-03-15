## MODIFIED Requirements

### Requirement: Core Framework Formal Model
The project SHALL provide a formal specification document that defines the complete
theoretical foundation of the zero-encoding serialization framework, covering:
the problem definition (why traditional serialization is redundant under constraints),
the Architecture Model (set A), the Memory Model (offset_ptr relative addressing,
segment manager, why byte-copy preserves pointer validity), the Type Model (safe type
subset S defined by TypeLayout's `is_local_serialization_free_v<T>`), the Zero-Encoding
Correctness Theorem (with proof by structural induction), the Enforcement Chain
(mapping every invariant to code), and the Boundary Analysis (when the model breaks
and how to detect/migrate).

**Key change**: Domain S is no longer defined by hand-enumerated leaf types S₀ with
explicit exclusion rules. Instead, Domain S is unified with TypeLayout's serialization-free
predicate: `S = { T | is_local_serialization_free_v<T> } ∪ { registered opaque types }`.
This means:
- C2 (referential integrity) is fully captured by `trivially_copyable(T) && !has_pointer(T)`
- C1 (layout determinism) is enforced by TypeLayout signature comparison, not by type exclusion
- `union`, `int`, `long`, `wchar_t`, `long double` are no longer excluded from S
- The union exclusion rationale ("active member ambiguity") is reclassified as an application-layer semantic concern, parallel to enum value-domain validity

#### Scenario: Developer understands why the framework works
- **WHEN** a developer reads docs/CORE_FORMAL_MODEL.md from §1 to §5
- **THEN** they can explain why `byte_copy(Buffer)` produces a semantically equivalent
  object graph, from first principles (architecture constraints → offset_ptr relative
  addressing → type subset → structural induction)

#### Scenario: Developer traces enforcement to code
- **WHEN** a developer looks up invariant I1 (all internal pointers are offset_ptr)
- **THEN** §6 maps this to specific code locations (`is_local_serialization_free_v<T>`
  check in DefaultPolicy, Boost.IPC offset_ptr usage, validate_xbuffer_type error messages)

#### Scenario: Developer identifies framework boundaries
- **WHEN** a developer reads §7
- **THEN** they know exactly when zero-encoding breaks (cross-architecture layout mismatch)
  and how to detect it (TypeLayout signature comparison via StrictPolicy or CI Phase 2)
- **AND** they understand that platform-variant types (`long`, `wchar_t`, `long double`)
  are locally safe but require C1 signature verification for cross-platform transfer

#### Scenario: Developer understands offset_ptr as the key enabler
- **WHEN** a developer reads §3
- **THEN** they can explain why offset_ptr (stored = target - this) survives buffer
  copy while raw pointers do not, and why all internal structures (iset_index,
  free-list, container backing stores) use offset_ptr

#### Scenario: Developer understands C1/C2 separation
- **WHEN** a developer reads §2
- **THEN** they understand that C2 (no absolute address) is checked locally via
  `is_local_serialization_free_v<T>`, and C1 (layout match) is checked via
  TypeLayout signature comparison
- **AND** they understand that Domain S only enforces C2, while C1 is a separate
  verification layer