# Change: Improve grow() Error Handling Safety

## Why
`XManagedMemory::grow()` has a `catch(...)` that silently swallows all exceptions.
If `resize()` succeeds but `close_impl()` or `open_impl()` fails, the buffer is in
an inconsistent state (resized but not re-opened). The caller only gets `false`.

## What Changes
- Save original size before resize
- On failure, attempt to restore original buffer size
- Provide basic rollback semantics (best-effort)

## Impact
- Affected code: `XManagedMemory::grow()` in `xoffsetdatastructure2.hpp`
- Affected tests: existing `test_reflection_compaction` (test_grow_and_verify)
