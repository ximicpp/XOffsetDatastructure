# Change: Optimize XBufferCompactor Design and Implementation

## Why
After formalizing the Safe Type Subset model (`ArchSpec` + `is_safe_leaf<T>`), the Compactor is the remaining component that hasn't been aligned with the new architecture. Current issues:

1. **Type dispatch not aligned with whitelist**: Compactor uses its own `is_xstring`, `is_simple_pod_v`, `SupportedContainer` concept — independent from `is_safe_leaf<T>`
2. **Redundant type traits**: `is_xstring` duplicates what `is_safe_leaf<XString>` already declares
3. **API design questions**: `compact_automatic` returns a new XBuffer but the old one is still alive; `compact_automatic_all` assumes all named objects have the same type T
4. **Memory sizing strategy**: hardcoded `used_size + 10%` with 4096 floor — no way to configure
5. **No extensibility**: migration strategy is hardcoded; users can't add custom migration for `is_safe_leaf`-registered types

## What Changes
**Phase 1 (Analysis)**: Full audit producing findings document
- Type dispatch alignment with `is_safe_leaf`
- API design review (signatures, error handling, naming)
- Memory strategy evaluation
- Extensibility assessment
- Redundant code identification

**Phase 2 (Implementation)**: Based on analysis
- Align Compactor dispatch with `is_safe_leaf` whitelist
- Remove redundant Compactor-internal type traits
- Improve API if warranted
- Optimize memory sizing if warranted

## Impact
- Affected specs: `type-signature`
- Affected code: `xoffsetdatastructure2.hpp` lines 442-610 (XBufferCompactor)
- Affected tests: compaction-related tests
