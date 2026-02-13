# Change: Unify Opaque Registration — Eliminate Triple-Registration Redundancy

## Problem

Each XOffset container type required synchronized registration at **three separate locations**:

1. `TYPELAYOUT_OPAQUE_*` — TypeLayout opaque signature (boost::typelayout namespace)
2. `is_safe_leaf<T>` — Safety whitelist (detail namespace)
3. `migrate_as<T>` — Migration strategy for compaction (XBufferCompactor private)

Additionally, `TYPELAYOUT_OPAQUE_*` macros required manually specified `sizeof`/`alignof`
literal values (e.g., `32, 8`), duplicating information the compiler already knows.

## Solution

### Part 1: TypeLayout `_AUTO` macros (opaque.hpp)

Added three new macro variants to TypeLayout that use `to_fixed_string()` inside
`consteval calculate()` to convert `sizeof(T)`/`alignof(T)` to `FixedString` at
compile time, bypassing the preprocessor `#` operator limitation:

- `TYPELAYOUT_OPAQUE_TYPE_AUTO(Type, name)`
- `TYPELAYOUT_OPAQUE_CONTAINER_AUTO(Template, name)`
- `TYPELAYOUT_OPAQUE_MAP_AUTO(Template, name)`

Original macros preserved for backward compatibility.

### Part 2: Unified `XOFFSET_REGISTER_*` macros (xoffsetdatastructure.hpp)

Three macros that perform all three registrations in one call:

```cpp
XOFFSET_REGISTER_TYPE(XString, "string", AllocatorAware)
XOFFSET_REGISTER_CONTAINER(XVector, "vector", Container)
XOFFSET_REGISTER_CONTAINER(XSet, "set", Container)
XOFFSET_REGISTER_MAP(XMap, "map", Container)
```

Each expands to:
1. `TYPELAYOUT_OPAQUE_*_AUTO(...)` — opaque signature with auto sizeof/alignof
2. `is_safe_leaf<T> : true_type` — safety whitelist entry
3. `migrate_as<T> { strategy }` — migration strategy

### Part 3: Access control adjustment

Moved `MigrateStrategy` enum and `migrate_as` template from `private` to `public`
in `XBufferCompactor`, enabling out-of-class specialization from the unified macros.

## Affected files

- `external/typelayout/include/boost/typelayout/opaque.hpp` — +3 AUTO macros
- `xoffsetdatastructure.hpp` — unified macros, removed 3 scattered registration sites

## Verification

- 25/25 tests passed
- Signature output verified: `string[s:32,a:8]`, `vector[s:32,a:8]<i32[s:4,a:4]>`
