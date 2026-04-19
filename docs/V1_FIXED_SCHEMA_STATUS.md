# V1 Fixed-Schema Status

## Scope

This status file tracks what the repository now implements for the `v1 fixed-schema`
design and what remains outside the current local codebase closure.

`v1` explicitly means:

- exact schema match only
- no field evolution
- zero encode/decode transport for admitted types
- frozen XOffset-owned container ABI

## Implemented

### Wire contract

- `XWireHeaderV1` frozen verified header
- explicit root schema naming via `XOFFSET_REGISTER_SCHEMA_NAME`
- compile-time `wire_root_type_id_v<T>()`
- compile-time `wire_schema_hash_v<T>()`
- little-endian marker and header integrity checks

### Type admission

- `is_v1_wire_admitted_v<T>`
- explicit rejection of non-`v1` scalar families such as `long`, `wchar_t`, `long double`
- rejection of inheritance, pointers, and other non-admitted record shapes

### Frozen container ABI

- `XFixedString`
- `XFixedVector<T>`
- `XFixedFlatSet<T>`
- `XFixedFlatMap<K, V>`
- public main containers `XString`, `XVector<T>`, `XSet<T>`, `XMap<K, V>` now run on the frozen ABI path
- `XBlob`, `XFlatSet`, `XFlatMap` aliases

### Allocation and mutation

- in-buffer `XAllocatorStateV1`
- fixed allocator ABI frozen at `sizeof == 88`, `alignof == 8`
- custom payload-resident arena backend replaces the old Boost.Interprocess runtime
- relative allocator binding for frozen containers
- retired-block reuse via free-list allocator metadata
- `compact<T>()` wired to fixed main-container families

### Load / save semantics

- `save_verified<T>()`
- `load_verified<T>()`
- typed access gating after verified load
- `load_unverified()` remains as the explicit raw escape hatch
- `TypedXBuffer<T>::load(...)` now defaults to verified load

### Structural verification

- header validation
- root and allocator offset validation
- bounded graph walk for admitted fixed-layout objects
- allocator-state validation
- free-list overlap and accounting checks

### Convenience API

- `XBuffer::create<T>()`
- `TypedXBuffer<T>::create()`
- schema-aware `matches_schema()`
- `bytes()`

## Verified Locally

The repository currently passes:

- `ctest --test-dir build --output-on-failure`
- exact target-matrix baseline inventory under `tools/sigs/`
- cross-target signature comparison via `check_compat`

The normal `ctest` suite now includes the matrix inventory and compatibility
checks as tooling tests.

Current local result at the time of this status update:

- `27/27` tests passing

## Target Matrix Closure

The repository now commits and validates the exact `v1` target matrix baselines:

- `x86_64_windows_clang`
- `x86_64_linux_clang`
- `arm64_ios_clang`
- `arm64_android_clang`

Closure mechanics now implemented in-repo:

- `tools/export_signatures.cpp` rewrites the exact matrix baseline set
- `tools/check_signature_matrix.cpp` rejects missing or unexpected baseline files
- `tools/check_compat.cpp` compares the committed target baselines
- CI treats any `tools/sigs/*.sig.hpp` drift as a failure

For the current admitted fixed-schema catalog, these baselines are generated from the
exact target-matrix ABI metadata plus the exported type closure. This closes the
repository-level protocol support evidence path for the declared target set.

## Runtime TODO

Compared to the original Boost-based prototype, the current runtime does not need to
remain a general-purpose managed-memory system. It only needs to serve the admitted
`v1` wire model. That creates a concrete optimization backlog:

- remove the compatibility-only naming layer; completed in the current runtime.
  The public API is now natively
  `XException` / `XBadAlloc` / `XArenaAllocator<T>` and `arena()`
- remove `XArena*` construction paths from fixed containers and migration logic;
  completed in the current runtime. Public construction now goes through
  allocator-aware paths.
- replace the generic `construct/find(name)` compatibility layer with dedicated
  `root/allocator` fast paths; `v1` does not need arbitrary named objects
  (completed in the current runtime)
- specialize backend allocation for the constrained admitted domain:
  - single-threaded runtime
  - `alignof(T) <= 8`
  - one root object
  - one allocator state
- replace the backend `free_blocks_` linear scan with size-class buckets or another
  fixed-shape free-list tuned for `XString` / `XVector` growth patterns
- tune growth policies for `XFixedString`, `XFixedVector`, `XFixedFlatSet`, and
  `XFixedFlatMap` for expected XOffset workloads instead of preserving generic
  STL-style heuristics everywhere
- add focused microbenchmarks against the old Boost-based backend and use them to prune
  compatibility paths that no longer pay for themselves
- evaluate targeted specialized containers when workload-specific wins are clear
  (for example a dedicated tiny-string type instead of mutating `XString` ABI)

## Not In V1

The following remain intentionally out of scope for `v1`:

- field evolution
- `XOptional<T>`
- hash containers
- polymorphic wire objects
- public mixed-version compatibility
