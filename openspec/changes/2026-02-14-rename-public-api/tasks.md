# Tasks: Rename Public API

## Phase 1: Core Header Rename (xoffsetdatastructure.hpp)

- [x] 1.1 Rename `XBuffer` → `XBufferCore` (using alias at line 360)
- [x] 1.2 Rename `XBufferBestFit` → `XBufferCoreBestFit` (line 363)
- [x] 1.3 Rename `XBufferExt` → `XBuffer` (class at line 1004)
- [x] 1.4 Rename `XBufferCompactor` → `XCompactor` (class at line 1132)
- [x] 1.5 Rename `XBufferVisualizer` → `XBufferStats` (class at line 658)
- [x] 1.6 Update all internal references within the header
- [x] 1.7 Update XOFFSET_REGISTER_* macros to use new names

## Phase 2: Tests Update

- [x] 2.1 Update all test files: `XBufferExt` → `XBuffer`
- [x] 2.2 Update all test files: `XBufferCompactor` → `XCompactor`
- [x] 2.3 Update all test files: `XBufferVisualizer` → `XBufferStats`
- [x] 2.4 Update `tests/CMakeLists.txt` test names
- [x] 2.5 Rename `test_xbufferext_api.cpp` → `test_xbuffer_api.cpp`

## Phase 3: Examples Update

- [x] 3.1 Update `examples/demo.cpp`
- [x] 3.2 Update `examples/helloworld.cpp`
- [x] 3.3 Update `examples/README.md`

## Phase 4: Documentation Update

- [x] 4.1 Update `README.md`
- [x] 4.2 Note: `docs/` analysis documents (MEMORY_LIFECYCLE_ANALYSIS.md etc.) are historical and use old names intentionally

## Phase 5: Verification

- [x] 5.1 Build and run all tests locally (native P2996 compiler)
- [x] 5.2 All 25 tests pass (24 passed + 1 skipped, 0 failed)
- [ ] 5.3 Commit, push, and verify CI

## Rename Summary

| Old Name | New Name | Files Changed |
|----------|----------|---------------|
| `XBuffer` (core) | `XBufferCore` | 1 (header) |
| `XBufferBestFit` | `XBufferCoreBestFit` | 1 (header) |
| `XBufferExt` | `XBuffer` | 26 test/example files + header |
| `XBufferCompactor` | `XCompactor` | 7 files |
| `XBufferVisualizer` | `XBufferStats` | 3 files |
| `test_xbufferext_api.cpp` | `test_xbuffer_api.cpp` | file renamed |