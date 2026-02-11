# Change: Upgrade TypeLayout Submodule

## Status
**COMPLETE** — Upgraded and verified. Zero code changes needed.

## Why
TypeLayout (`external/typelayout`) upstream published a major refactor (13 commits) including:
- Header architecture refactored to Boost-style (`detail/` + root API)
- `core/` directory removed (Phase 2 deprecation)
- FixedString aligned with P2484 semantics
- Layout engine fixes for opaque base classes and specializations
- `classify_safety()` now detects f80 (long double) as Risk

## What Changes
- Updated `external/typelayout` submodule from `1df5cdd` to `bb90052`
- No XOffsetDatastructure2 code changes required — fully backward compatible
- All 19 tests pass with zero regressions

## Impact
- Affected specs: `type-signature` (submodule version)
- Affected code: none