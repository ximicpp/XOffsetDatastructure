# Change: Formalize Zero-Copy Safety Model

## Why
The library claims that "direct memory copy equals correct deserialization" for types in the
Safe Type Subset on the Target Architecture. This claim is currently implied by scattered code
comments and `static_assert` checks, but there is no single document that formally:
1. Defines the Architecture Set **A** and Type Set **S** with precise mathematical notation.
2. States the Safety Theorem: ∀T ∈ S, ∀arch ∈ A: `memcpy(dst, src, sizeof(T))` preserves
   semantic equivalence.
3. Proves the theorem by structural induction on the type taxonomy (primitives → enums →
   containers → composites).
4. Identifies the exact invariants that make this work (offset_ptr relative addressing,
   fixed-width types, deterministic layout, no hidden state).

Without this formalization, the safety guarantee is informal and fragile — any future type
addition could silently break it.

## What Changes
- Produce a formal specification document (`docs/FORMAL_SAFETY_MODEL.md`) containing:

  **§1 — Definitions**
  - Architecture Set A = { (pointer_size, endianness, sizeof_table, alignment_table) }
  - Current singleton: A = { Arch64LE }
  - Target Architecture predicate: `CurrentPlatform ∈ A` (enforced by static_assert)
  
  **§2 — Safe Type Subset S**
  - Inductive definition:
    - **Base case (LEAF)**: S₀ = { int8_t, ..., uint64_t, float, double, bool, char } ∪ { enum E : fixed-width }
    - **String**: XString ∈ S (allocator-aware, internal offset_ptr)
    - **Container**: XVector<T> ∈ S ⟺ T ∈ S; XSet<T> ∈ S ⟺ T ∈ S; XMap<K,V> ∈ S ⟺ K ∈ S ∧ V ∈ S
    - **Composite**: struct C ∈ S ⟺ ¬polymorphic(C) ∧ ¬has_bases(C) ∧ ¬union(C) ∧ ∀m ∈ members(C): type(m) ∈ S
  - **Exclusion**: raw pointers, references, std containers, virtual types, XOffsetPtr (opt-in only)
  
  **§3 — The Zero-Copy Theorem**
  - Statement: If `Platform ∈ A` and `T ∈ S`, then for any buffer B containing a valid object
    graph of type T, `byte_copy(B) → B'` produces a semantically equivalent object graph.
  - Proof sketch by induction on type structure:
    - Primitives: fixed-width, same endianness → byte-identical
    - Enums: underlying type is primitive → same argument
    - offset_ptr: relative addressing → self-relocated after copy
    - Containers: backing store uses offset_ptr → internal pointers survive copy
    - Composites: all members satisfy above → whole struct satisfies
  
  **§4 — Invariant Catalog**
  - I1: All pointers inside managed memory are `offset_ptr` (relative, not absolute)
  - I2: No hidden mutable state (no vtable, no type-erased members)
  - I3: All integer types are fixed-width (no `int`, `long`, `size_t`)
  - I4: Endianness is fixed per architecture set
  - I5: Alignment is deterministic per architecture set
  - I6: Named object index (iset_index) uses offset_ptr internally
  
  **§5 — Boundary Conditions**
  - When zero-copy breaks: cross-architecture, cross-endian, ABI change
  - Migration path: type signature comparison via TypeLayout
  - XOffsetPtr opt-in: why excluded, when safe to include

- Map the formal model to code: annotate which `static_assert` / `is_safe_leaf` / `is_safe_type`
  enforces which invariant.
- No code changes required — this is a specification/proof proposal.

## Impact
- Affected specs: `safety-model` (new capability)
- Affected code: none (read-only analysis)
- New artifact: `docs/FORMAL_SAFETY_MODEL.md`
