# Change: Add XBufferExt API Test Coverage

## Why
`XBufferExt` has 7 public APIs. Some are well-tested indirectly (`make`, `save_to_string`,
`load_from_string`), but 3 have zero dedicated test coverage:
- `find_ex<T>()` — only used in `examples/helloworld.cpp`
- `find_or_make<T>()` — zero usage anywhere
- `stats()` — used in tests but never verified for correctness

## What Changes
- New test file: `tests/test_xbufferext_api.cpp`
- Add to `tests/CMakeLists.txt`

## Impact
- Affected specs: none (testing existing behavior)
- Affected code: no library changes
- New test: `test_xbufferext_api`
