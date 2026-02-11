# Change: Design XOffsetPtr Safety Model

## Why
`XOffsetPtr<T>` (alias for `boost::interprocess::offset_ptr<T>`) is defined in the library and
recommended in error messages as the safe alternative to raw pointers. However:

1. It is **not registered** in `is_safe_leaf`, so structs containing it are rejected
2. Its safety is **conditional** — depends on what `T` is and where the target lives
3. **Compaction migration** is non-trivial — the offset may need recalculation when objects move
4. There are **zero tests and zero examples** using it

This proposal designs the complete safety model for `XOffsetPtr<T>` covering:
- When it's safe (compile-time type check + runtime target constraint)
- How to migrate it during compaction
- How `offset_ptr`'s copy constructor interacts with buffer relocation

### Key Technical Insight
`offset_ptr`'s copy constructor calls `offset_ptr_to_offset_from_other(this, &ptr, ptr.offset)`,
which **recalculates** the relative offset from the new `this` to the same absolute target.
This means:
- If source and target `offset_ptr` are in the **same buffer**, copy automatically adjusts the offset ✅
- If the **pointed-to object** has been migrated to a new buffer but the pointer still references the old location, the copy will point to stale memory ❌

## What Changes
- Register `XOffsetPtr<T>` in `is_safe_leaf` whitelist (conditional on `T` being safe)
- Design and implement a `MigrateStrategy` for `XOffsetPtr` (or decide it's not migratable)
- Add tests exercising `XOffsetPtr` in XBuffer structs
- Update documentation and error messages

## Impact
- Affected specs: `type-signature` (new requirement for offset pointer safety)
- Affected code: `xoffsetdatastructure2.hpp` — safety whitelist, possibly Compactor
- Affected tests: New test file `tests/test_offset_ptr.cpp`
