## 1. XString Direct Assignment (highest impact)
- [x] 1.1 Verify `XString::operator=(const char*)` — **已原生支持** by Boost.Container basic_string (string.hpp:947)
- [x] 1.2 Verify `XString::operator=(string_view)` — **已原生支持** (string.hpp:958). 无需新增代码
- [x] 1.3 Update examples to use simplified syntax — helloworld.cpp (1处) + demo.cpp (4处) 已更新为 `= "xxx"`
- [x] 1.4 Add test: `test_xstring_direct_assign.cpp` — 13 个测试用例覆盖段内分配、序列化、XVector<XString>

## 2. Safety Documentation (Critical Rules)
- [x] 2.1 Add "Critical Safety Rules" section to README.md
- [x] 2.2 Document pointer invalidation rules (grow/shrink/compact)
- [x] 2.3 Document bulk deallocation semantics (C3.1)
- [x] 2.4 Document "NOT thread-safe" warning (C5.1)

## 3. make() Safety Wrapper
- [x] 3.1 Design `XHandle<T>` — Epoch-cached pattern (方案A): buffer维护m_epoch, handle缓存{ptr, epoch}
- [x] 3.2 Implement: m_epoch in XManagedMemory + XHandle<T> class + make_handle/find_handle in XBufferExt
- [x] 3.3 Add test: `test_xhandle.cpp` — 8 tests: basic, grow, shrink, compact, epoch cache, find, multi-handle, serialization

## 4. Single-Object Buffer Refactor (§12, Breaking Change)

### Phase 4A: Core API — xoffsetdatastructure2.hpp
- [x] 4A.1 Add internal `XBUFFER_ROOT_NAME = "__root__"` constant
- [x] 4A.2 Refactor `XBufferExt::make<T>()` — no name param, uses ROOT_NAME internally
- [x] 4A.3 Add `XBufferExt::root<T>()` returns `T&` + `has_root<T>()` returns bool
- [x] 4A.4 Removed `find_ex<T>`, `find_or_make<T>` from public API
- [x] 4A.5 Simplified `XHandle<T>` — removed `name_` member, binds to ROOT_NAME
- [x] 4A.6 Updated `make_handle<T>()` / `handle<T>()` — no name param
- [x] 4A.7 Simplified `XBufferCompactor::compact_automatic<T>()` — no name param, removed `compact_automatic_all`

### Phase 4B: Tests — 26 files
- [x] 4B.1 Batch replace: `make<T>("xxx")` → `make<T>()` (13 files)
- [x] 4B.2 Batch replace: `find<T>("xxx").first` → `root<T>()` (11 files)
- [x] 4B.3 Batch replace: `construct<T>("xxx")(mgr)` → `make<T>()` (7 files: basic_types, compaction, enum, map_set, modify, nested, vector)
- [x] 4B.4 Replaced `find_ex<T>` → `has_root<T>()` + `root<T>()` (xstring_direct_assign, xbufferext_api)
- [x] 4B.5 Fixed multi-object: test_reflection_comparison → 2 separate buffers; rewrote test_xbufferext_api; rewrote test_xhandle
- [x] 4B.6 All `XBuffer xbuf` → `XBufferExt xbuf` for tests using make/root

### Phase 4C: Examples — 2 files
- [x] 4C.1 Updated examples/helloworld.cpp — make/root/compact without names
- [x] 4C.2 Updated examples/demo.cpp — make/root/compact without names

### Phase 4D: Documentation
- [x] 4D.1 Update README.md — Critical Safety Rules updated to new API (make/root/make_handle)
- [x] 4D.2 AGENTS.md — code samples are generic patterns, no update needed

### Phase 4E: Verification
- [x] 4E.1 Full build + 25 tests pass in Docker ✅
- [x] 4E.2 Demo + HelloWorld run correctly ✅

## 5. Remaining Tasks (post-refactor)
- [x] 5.1 Serialization: `used_size()` + `save_to_vector()` (shrink then copy) + `load_from_vector()`
- [x] 5.2 Buffer Capacity: `estimate_buffer_size()` using `segment_manager::get_min_size()` + 20% headroom
- [x] 5.3 Transparent Comparator: XMap/XSet changed to `std::less<void>` — enables `map.find("key")` without temp XString
- [x] 5.4 shrink_to_fit() optimization: eliminated 3rd copy by reusing close/open pattern from grow()
- [x] 5.5 Per-Member static_assert diagnostics: `diagnose_unsafe_members<T>()` uses `template for` to name exact unsafe fields
