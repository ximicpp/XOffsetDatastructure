## MODIFIED Requirements

### Requirement: Core Framework Formal Model
The project SHALL provide a formal specification document that defines the complete
theoretical foundation of the zero-encoding serialization framework, covering:
the problem definition (why traditional serialization is redundant under constraints),
the Architecture Model (set A), the Memory Model (offset_ptr relative addressing,
segment manager, why byte-copy preserves pointer validity), the Type Model (safe type
subset S with inductive definition), the Zero-Encoding Correctness Theorem (with proof
by structural induction), the Enforcement Chain (mapping every invariant to code), and
the Boundary Analysis (when the model breaks and how to detect/migrate).

**Domain S 定义更新**：安全类型集合 S 的准入逻辑 SHALL 区分两类类型：

1. **标准类型（非 opaque）**: `T ∈ S ⟺ is_local_serialization_free_v<T>` — 即 `trivially_copyable(T) ∧ ¬has_pointer(T)`
2. **Opaque 类型（relocatable 容器）**: `T ∈ S ⟺ has_opaque_signature<T> ∧ pointer_free(T)` — 不要求 `trivially_copyable`，byte-copy safety 由 offset_ptr 重定位模型保证（C2 Lemma C2.1）

该二分法反映了 `trivially_copyable` 在标准 C++ 语义中的局限性：使用 `offset_ptr` 的容器有 non-trivial 析构函数和拷贝构造函数，但其内存布局在 byte-copy 后仍然语义等价。

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

#### Scenario: Developer understands opaque type safety model
- **WHEN** a developer reads the Domain S definition in §4
- **THEN** they understand that opaque types (XString, XVector, XSet, XMap) are admitted to S via `has_opaque_signature ∧ pointer_free`, not via `trivially_copyable`
- **AND** they understand that the byte-copy safety guarantee for these types comes from the offset_ptr relocation model (C2 Lemma C2.1), not from the C++ type system's `trivially_copyable` trait
