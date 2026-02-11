## ADDED Requirements
### Requirement: Core Framework Formal Model
The project SHALL provide a formal specification document that defines the complete
theoretical foundation of the zero-encoding serialization framework, covering:
the problem definition (why traditional serialization is redundant under constraints),
the Architecture Model (set A), the Memory Model (offset_ptr relative addressing,
segment manager, why byte-copy preserves pointer validity), the Type Model (safe type
subset S with inductive definition), the Zero-Encoding Correctness Theorem (with proof
by structural induction), the Enforcement Chain (mapping every invariant to code), and
the Boundary Analysis (when the model breaks and how to detect/migrate).

#### Scenario: Developer understands why the framework works
- **WHEN** a developer reads docs/CORE_FORMAL_MODEL.md from §1 to §5
- **THEN** they can explain why `byte_copy(Buffer)` produces a semantically equivalent
  object graph, from first principles (architecture constraints → offset_ptr relative
  addressing → type subset → structural induction)

#### Scenario: Developer traces enforcement to code
- **WHEN** a developer looks up invariant I1 (all internal pointers are offset_ptr)
- **THEN** §6 maps this to specific code locations (is_safe_leaf exclusions, Boost.IPC
  offset_ptr usage, validate_xbuffer_type error messages)

#### Scenario: Developer identifies framework boundaries
- **WHEN** a developer reads §7
- **THEN** they know exactly when zero-encoding breaks (cross-architecture, ABI change,
  long double f80) and how to detect it (TypeLayout signature comparison)

#### Scenario: Developer understands offset_ptr as the key enabler
- **WHEN** a developer reads §3
- **THEN** they can explain why offset_ptr (stored = target - this) survives buffer
  copy while raw pointers do not, and why all internal structures (iset_index,
  free-list, container backing stores) use offset_ptr