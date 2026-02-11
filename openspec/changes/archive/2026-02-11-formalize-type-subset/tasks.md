## 1. Conceptual Analysis
- [x] 1.1 Formalize the Safe Type Subset: define membership rules as a set theory model
- [x] 1.2 Build a unified Type Taxonomy (classification shared by Safety, Migration, Signature)
- [x] 1.3 Define the three-layer responsibility model (XOffset vs TypeLayout vs C++ standard)
- [x] 1.4 Gap analysis: current implementation vs ideal model

## 2. Design
- [x] 2.1 Design the unified Type Classifier interface
- [x] 2.2 Determine which classification capabilities should move to TypeLayout vs stay in XOffset
- [x] 2.3 Define the relationship between Type Classifier, Signature, and Migration

## 3. Implementation
- [x] 3.1 Implement `ArchSpec` + `TargetArchitecture` presets (P1)
- [x] 3.2 Refactor static_assert to reference TargetArchitecture
- [x] 3.3 Implement `is_safe_leaf<T>` whitelist replacing implicit rules (P2)
- [x] 3.4 Add safety gate to Compactor entry points (P0/F7)
- [x] 3.5 Replace sizeof heuristic with precise template matching (F6)
- [x] 3.6 Delete type-erased blacklist (~30 lines removed) (P5)

## 4. Testing & Documentation
- [x] 4.1 All 22 tests pass (Docker build verified)
- [x] 4.2 Write `docs/TYPE_SUBSET_MODEL.md`