## Context
XOffsetPtr<T> (alias for boost::interprocess::offset_ptr<T>) is a reference-semantic type
that stores relative offsets. It differs fundamentally from all current safe types (LEAF-1~4),
which are value-semantic and self-contained.

## Goals / Non-Goals
- Goal: Clarify XOffsetPtr's position in the type safety model
- Goal: Provide a documented opt-in path for users who need it
- Goal: Update error messages to accurately describe the situation
- Non-Goal: Automatically migrate XOffsetPtr during compaction
- Non-Goal: Add runtime buffer-bounds validation

## Decisions

### Decision 1: XOffsetPtr is NOT a safe leaf by default
- **What:** The library does not register `is_safe_leaf<XOffsetPtr<T>>`
- **Why:** XOffsetPtr is reference-semantic, not value-semantic. Its validity depends
  on external state (the pointed-to object). The Safe Type Subset S is defined as
  self-contained value types whose binary safety is fully determined at compile time.
  XOffsetPtr breaks this invariant.
- **Alternatives considered:**
  - Register as safe leaf with recursive T check → Rejected: stretches "leaf" semantics
  - New `is_safe_ref` trait category → Rejected: adds complexity for a niche use case

### Decision 2: User opt-in via is_safe_leaf specialization
- **What:** Users can opt-in by specializing `is_safe_leaf<XOffsetPtr<MyType>>`
- **Why:** Reuses existing extension point, no new API surface. Users who understand
  the reference semantics and lifetime management can explicitly enable it.
- **Constraint:** User must also provide `migrate_as` if they want compaction support.
  Without it, the Compactor's `resolve_strategy` will fall back to `Composite`,
  which will fail because offset_ptr has no reflectable nonstatic data members.

### Decision 3: Compaction migration is user responsibility
- **What:** The library provides no built-in migration for XOffsetPtr. Users who opt-in
  and need compaction must register their own `migrate_as` specialization.
- **Why:** The library cannot know where the pointer target will be in the new buffer.
  Only the user knows the pointer relationship semantics.

## Key Technical Facts
- `offset_ptr` is NOT trivially copyable (custom copy ctor/assignment)
- Copy/assignment recalculates offset to point to same absolute address
- `m_offset == 1` represents null (not 0, since 0 means "points to self")
- XString/XVector already use offset_ptr internally but encapsulate it

## Risks / Trade-offs
- Risk: Users opt-in without understanding → dangling pointers after compaction
  → Mitigation: Clear documentation with examples of correct and incorrect usage
- Risk: Error message says "use XOffsetPtr" but it's rejected by default
  → Mitigation: Update error message to explain opt-in requirement

## Open Questions
- None remaining
