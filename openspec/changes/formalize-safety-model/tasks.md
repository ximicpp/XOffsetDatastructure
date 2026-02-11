## 1. Formal Definitions
- [ ] 1.1 Define Architecture Set A with mathematical notation
- [ ] 1.2 Define Safe Type Subset S inductively (base case + recursive rules)
- [ ] 1.3 Define semantic equivalence for byte-copied object graphs

## 2. Zero-Copy Theorem
- [ ] 2.1 State the theorem precisely
- [ ] 2.2 Prove base case: primitives (fixed-width + same endianness)
- [ ] 2.3 Prove enum case: underlying fixed-width type
- [ ] 2.4 Prove offset_ptr case: relative addressing survives byte-copy
- [ ] 2.5 Prove container case: XVector/XSet/XMap internal offset_ptr
- [ ] 2.6 Prove composite case: structural induction on members
- [ ] 2.7 Prove XString case: allocator-aware with internal offset_ptr

## 3. Invariant Catalog
- [ ] 3.1 Enumerate all invariants (I1–I6) with code cross-references
- [ ] 3.2 Map each static_assert to the invariant it enforces
- [ ] 3.3 Map each is_safe_leaf specialization to the type taxonomy

## 4. Boundary Analysis
- [ ] 4.1 Document when zero-copy breaks (cross-arch, ABI change)
- [ ] 4.2 Document TypeLayout signature-based migration path
- [ ] 4.3 Document XOffsetPtr opt-in boundary

## 5. Assembly
- [ ] 5.1 Assemble into docs/FORMAL_SAFETY_MODEL.md
- [ ] 5.2 Review for consistency with code
