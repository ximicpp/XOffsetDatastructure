# Change: Analyze and Improve Safety Type Detection

## Why
Architecture review (F6/F7/F8) identified three issues in the safety detection system:
1. **F6**: `is_xbuffer_safe` is not extensible — container detection is hardcoded via `sizeof==32 && alignof==8`
2. **F7**: `XBufferCompactor` does not call safety checks, allowing unsafe types to be compacted
3. **F8**: Compactor and Safety use duplicate but inconsistent type classification logic

Additionally, the Compactor's `migrate_member` does not handle enums (which Safety now supports via `is_fixed_enum`), creating a semantic gap.

This proposal performs a deep analysis of the safety detection system and proposes concrete improvements.

## What Changes
**Phase 1 (Analysis)**: Produce `docs/SAFETY_DETECTION_ANALYSIS.md` covering:
- Complete rule inventory (Safety 11 rules vs Compactor 4 categories)
- Gap analysis: types that pass Safety but Compactor can't migrate (and vice versa)
- Type classification alignment matrix
- Extension point design options

**Phase 2 (Implementation)**: Based on analysis findings:
- Add `validate_xbuffer_type<T>()` gate to Compactor entry points (F7)
- Unify type dispatch between Safety and Compactor (F8)
- Add enum migration support to Compactor
- Evaluate extensible safety trait design (F6)

## Impact
- Affected specs: `type-signature` (may produce MODIFIED requirements for safety)
- Affected code: `xoffsetdatastructure2.hpp` — Safety system (C6) and Compactor (C5)
- Affected tests: May need new tests for enum migration and safety gate
