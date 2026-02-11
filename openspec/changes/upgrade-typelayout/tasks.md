## 1. Analysis
- [x] 1.1 Fetch latest TypeLayout and diff against current submodule (13 new commits)
- [x] 1.2 Identify breaking changes: core/ removed, headers restructured to detail/
- [x] 1.3 Assess impact: umbrella header still works, all APIs preserved

## 2. Implementation
- [x] 2.1 Update submodule pointer to bb90052
- [x] 2.2 Fix compatibility issues — NONE needed (fully backward compatible)
- [x] 2.3 Verify all 19/19 tests pass