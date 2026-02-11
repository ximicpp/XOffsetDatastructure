## 1. Analysis
- [ ] 1.1 Inventory all 11 Safety rules in `detail::is_safe_type<T>()`
- [ ] 1.2 Inventory all 4 Compactor migration categories in `migrate_member<T>()`
- [ ] 1.3 Build type classification alignment matrix (Safety vs Compactor)
- [ ] 1.4 Identify gap: types that pass Safety but Compactor can't handle
- [ ] 1.5 Identify gap: enum migration missing in Compactor
- [ ] 1.6 Evaluate `sizeof==32 && alignof==8` container detection correctness
- [ ] 1.7 Analyze extensible safety trait design options

## 2. Implementation
- [ ] 2.1 Add `validate_xbuffer_type<T>()` to `compact_automatic` and `compact_automatic_all` (F7)
- [ ] 2.2 Add enum handling to Compactor's `migrate_member` (trivially copyable, same as POD)
- [ ] 2.3 Unify type classification: align Compactor dispatch with Safety categories (F8)
- [ ] 2.4 Evaluate and optionally implement extensible `is_safe_container<T>` trait (F6)

## 3. Testing
- [ ] 3.1 Add test for Compactor safety gate (verify it rejects unsafe types at compile time)
- [ ] 3.2 Add test for enum migration through Compactor
- [ ] 3.3 Verify all existing 25 tests pass

## 4. Documentation
- [ ] 4.1 Write `docs/SAFETY_DETECTION_ANALYSIS.md`
