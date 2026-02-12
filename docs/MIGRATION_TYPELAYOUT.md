# Migration Guide: XTypeSignature → TypeLayout

This guide covers the migration from the legacy `XTypeSignature` API (removed) to the
`boost::typelayout` library, which is now the sole type-signature engine in XOffsetDatastructure.

> **Note**: The `XTypeSignature` compatibility namespace has been fully removed.
> All code must use `boost::typelayout` directly.

## Overview

| Aspect | Old (XTypeSignature) — REMOVED | New (boost::typelayout) |
|--------|-------------------------------|------------------------|
| Namespace | `XTypeSignature` | `boost::typelayout` |
| String type | `CompileString<N>` | `FixedString<N>` |
| Signature function | `get_XTypeSignature<T>()` | `get_definition_signature<T>()` |
| Struct prefix | `struct[...]` | `record[...]` |
| Platform prefix | _(none)_ | `[64-le]` |
| Signature layers | 1 (definition-like) | 2 (Layout + Definition) |
| Match check | Manual `operator==` | `definition_signatures_match<T1,T2>()` |
| Layout match | _(not available)_ | `layout_signatures_match<T1,T2>()` |
| Alignment constant | `XTypeSignature::BASIC_ALIGNMENT` | Use literal `8` or define your own |

## Quick Migration

### Before (old API — no longer compiles)

```cpp
// ❌ These no longer exist:
constexpr auto sig = XTypeSignature::get_XTypeSignature<MyStruct>();
XTypeSignature::CompileString<6> cs("hello");
class alignas(XTypeSignature::BASIC_ALIGNMENT) MyClass { ... };
```

### After (current API)

```cpp
#include "xoffsetdatastructure.hpp"
// boost::typelayout is already included via xoffsetdatastructure.hpp

// Generate signature (definition layer — includes field names)
constexpr auto sig = boost::typelayout::get_definition_signature<MyStruct>();

// Validate with static_assert (note: "record" instead of "struct", "[64-le]" prefix)
static_assert(sig == "[64-le]record[s:16,a:8]{@0[x]:i32[s:4,a:4],@8[y]:f64[s:8,a:8]}",
              "Layout changed!");

// Print (use operator<< instead of .print())
std::cout << sig << "\n";

// Check if two types have matching signatures
static_assert(boost::typelayout::definition_signatures_match<MyStruct, MyStruct>());

// Layout-only comparison (ignores field names, only compares byte layout)
static_assert(boost::typelayout::layout_signatures_match<StructA, StructB>());

// Alignment: use literal 8
class alignas(8) MyClass { ... };
```

## Signature Format Changes

### Old format (no longer generated)
```
struct[s:72,a:8]{@0[id]:i32[s:4,a:4],@4[level]:i32[s:4,a:4],@8[name]:string[s:32,a:8]}
```

### New format (Definition)
```
[64-le]record[s:72,a:8]{@0[id]:i32[s:4,a:4],@4[level]:i32[s:4,a:4],@8[name]:string[s:32,a:8]}
```

### New format (Layout)
```
[64-le]record[s:72,a:8]{@0:i32[s:4,a:4],@4:i32[s:4,a:4],@8:string[s:32,a:8]}
```

Key differences:
1. **Platform prefix** `[64-le]` — encodes architecture (64-bit) and endianness (little-endian)
2. **`record`** instead of **`struct`** — TypeLayout uses `record` for all class/struct types
3. **Layout layer** strips field names (only byte offsets and types remain)
4. **Inheritance support** — `~base<BaseClass>:` entries appear in definition signatures

## Two-Layer Signature System

TypeLayout provides two signature layers:

| Layer | API | Purpose |
|-------|-----|---------|
| **Definition** | `get_definition_signature<T>()` | Full structural comparison including field names, inheritance hierarchy. Use for strict type identity. |
| **Layout** | `get_layout_signature<T>()` | Pure byte-layout comparison. Ignores field names. Use for binary compatibility checks. |

### When to use which

- **Definition**: When you need to ensure the **exact same type** (same fields, same names, same hierarchy)
- **Layout**: When you need to ensure **binary compatibility** (same bytes at same offsets, regardless of names)

## Container Specializations

XOffsetDatastructure containers (XString, XVector, XSet, XMap) are registered
in `boost::typelayout` namespace. They produce opaque signatures with fixed size:

| Container | Signature |
|-----------|-----------|
| `XString` | `string[s:32,a:8]` |
| `XVector<T>` | `vector[s:32,a:8]<...>` |
| `XSet<T>` | `set[s:32,a:8]<...>` |
| `XMap<K,V>` | `map[s:32,a:8]<...,>` |

## FAQ

**Q: Will my existing code using `XTypeSignature` still compile?**
A: No. The `XTypeSignature` namespace has been completely removed. You must migrate
to `boost::typelayout`. See the "Quick Migration" section above.

**Q: Do I need to update my `static_assert` strings?**
A: Yes. The format changed from `struct[...]` to `[64-le]record[...]`.
Update the expected strings accordingly.

**Q: What about `CompileString::print()`?**
A: TypeLayout's `FixedString` uses `operator<<` instead of `.print()`.
Replace `sig.print()` with `std::cout << sig`.

**Q: What replaces `BASIC_ALIGNMENT` and `ANY_SIZE`?**
A: Use the literal value `8` for alignment (or define your own constant).
The `ANY_SIZE` constant (64) was rarely used; define it locally if needed.