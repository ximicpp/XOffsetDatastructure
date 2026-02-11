## 1. Analysis ✅
- [x] 1.1 Study offset_ptr internals: NOT trivially copyable, custom copy ctor recalculates offset
- [x] 1.2 Analyze use cases: intra-buffer refs, cross-object refs, all reference-semantic
- [x] 1.3 Determine safety classification: NOT a safe leaf — reference type, not value type
- [x] 1.4 Analyze compaction migration: not automatable — library can't know target's new address
- [x] 1.5 Design decision: opt-in via user is_safe_leaf specialization, migration is user responsibility

## 2. Design ✅
- [x] 2.1 Decision: XOffsetPtr is NOT registered as safe leaf by default
- [x] 2.2 Decision: User opt-in via is_safe_leaf specialization (reuse existing extension point)
- [x] 2.3 Decision: Compaction migration is user responsibility (via migrate_as)
- [x] 2.4 Write design.md

## 3. Implementation ✅
- [x] 3.1 Update error message: "use XOffsetPtr<T> with opt-in, see docs"
- [x] 3.2 Add LEAF-5 comment block documenting opt-in mechanism and rationale
- [x] 3.3 Update error panel: remove misleading "use XOffsetPtr<T>" from NOT ALLOWED list

## 4. Testing ✅
- [x] 4.1 XOffsetPtr<T> is rejected by default (confirmed: existing behavior unchanged)
- [x] 4.2 All 22 existing tests pass (Docker linux/amd64, Clang P2996)

## 5. Documentation ✅
- [x] 5.1 Update TYPE_SUBSET_MODEL.md: add LEAF-5 (XOffsetPtr opt-in), renumber Composite to LEAF-6
- [x] 5.2 Update IMPLEMENTATION_REVIEW.md: mark H1 as RESOLVED with design rationale