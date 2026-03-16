# XOffsetDatastructure — Technical Architecture Overview

This document describes the internal architecture of `xoffsetdatastructure.hpp` (~1480 lines, single-header library). It serves as a reference for developers who want to understand **how** the library works, complementing the user-facing `README.md` (which covers **what** and **how to use**).

> **Audience**: Contributors, code reviewers, and advanced users.
> For the formal correctness model, see [`CORE_FORMAL_MODEL.md`](CORE_FORMAL_MODEL.md).
> For the migration from `XTypeSignature` to TypeLayout, see [`MIGRATION_TYPELAYOUT.md`](MIGRATION_TYPELAYOUT.md).
> For the CppCon talk materials, see [`Compile-timeTypeSignatures.pdf`](Compile-timeTypeSignatures.pdf) and [`XOffsetDatastructure_CppCon2024.pdf`](XOffsetDatastructure_CppCon2024.pdf).

---

## 1. File Layout

The single header is organized into these major sections (approximate line ranges):

| Section | Description |
|---------|-------------|
| **Platform Guards** | Architecture (64-bit) and endianness (little-endian) enforcement via preprocessor `#error` and `static_assert` |
| **Includes & TypeLayout** | Boost.Interprocess, Boost.Container, and [TypeLayout](https://github.com/ximicpp/TypeLayout) headers |
| **Memory Allocator (`x_best_fit`)** | Custom allocator extending `rbtree_best_fit` |
| **XBufferCore** | Typedef for the managed memory segment (`managed_external_buffer`) |
| **Container Types** | `XString`, `XVector<T>`, `XSet<T>`, `XMap<K,V>` — Boost.Container types using `offset_ptr`-based allocators |
| **Type Safety** | Domain S admission, `DefaultPolicy`, `StrictPolicy`, diagnostics |
| **Reflect Construct & Transfer** | C++26 P2996 reflection-based zero-boilerplate construction and deep copy |
| **XHandle** | Stable handle abstraction surviving buffer reallocation |
| **XManagedMemory** | High-level memory manager with adaptive reservation and `grow()` |
| **XBuffer** | User-facing API: `make<T>()`, `root<T>()`, `save()`, `load()`, `compact<T>()` |
| **XCompactor** | Reflection-based automatic memory compaction (deep migration) |
| **Registration Macros** | `XOFFSET_REGISTER_TYPE`, `XOFFSET_REGISTER_CONTAINER`, `XOFFSET_REGISTER_MAP` |

---

## 2. Type Safety Model

### 2.1 Domain S — The Safe Type Set

The library defines a **Safe Type Set S** — only types in S can be stored in the buffer:

```
Domain S = { T | is_local_serialization_free_v<T> }
         ∪ { registered opaque types (XVector, XString, XSet, XMap, ...) }
```

Where `is_local_serialization_free_v<T>` (from TypeLayout) means:
- `std::is_trivially_copyable_v<T> == true`
- `!has_pointer<T>` (no raw pointers in the transitive closure of T's members)

### 2.2 DefaultPolicy — The Four-Branch Admission Gate

`DefaultPolicy::accept<T>()` is a `consteval` function that decides at compile time whether a type is safe. It uses four branches:

| Branch | Condition | Meaning |
|--------|-----------|---------|
| **1: Opaque** | `has_opaque_signature<T>` | Registered container types (XString, XVector, etc.). Shell safety guaranteed by the user via `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE` macro. Element safety is recursively verified via `opaque_element_types<T>::all_elements_safe()`. |
| **2: Leaf** | `is_local_serialization_free_v<T>` | Primitive and trivially-copyable pointer-free types. Fully verified by TypeLayout — no recursion needed. |
| **3: Composite** | `std::is_class_v<T> && !is_union_v && !is_polymorphic_v` | User-defined structs with opaque or mixed members. Uses C++26 P2996 reflection (`nonstatic_data_members_of`, `bases_of`) to recursively check each member and base class. |
| **4: Rejected** | Everything else | Raw pointers, references, polymorphic types (vtable), unions — rejected. |

### 2.3 StrictPolicy — Layout Signature Lock

`StrictPolicy<GoldSignature>` extends `DefaultPolicy` by additionally requiring the type's layout signature to match a compile-time gold value. This provides C1+C2 (value preservation + reference preservation) guarantees:

```cpp
template<auto GoldSignature>
struct StrictPolicy {
    template<typename T>
    static consteval bool accept() {
        if constexpr (!DefaultPolicy::accept<T>()) return false;
        return std::string_view(get_layout_signature<T>()) == std::string_view(GoldSignature);
    }
};
```

### 2.4 Public API

```cpp
// Check if a type is safe for XBuffer
is_xbuffer_safe<T>::value    // constexpr bool
is_xbuffer_safe<T>::reason() // human-readable diagnostic string

// Validate with static_assert (called internally by make<T>())
validate_xbuffer_type<T>();

// Per-member diagnostic (fires static_assert per unsafe member for precise error location)
diagnose_unsafe_members<T>();
diagnose_unsafe_members<T, MyPolicy>();  // with custom policy
```

### 2.5 Diagnostic Functions

When `validate_xbuffer_type<T>()` fails, it automatically calls `diagnose_unsafe_members<T>()`, which uses P2996 reflection to iterate every base class and member, firing individual `static_assert` per unsafe field. This gives the developer a precise error pointing to the exact problematic member.

`get_safety_error_message<T>()` returns a human-readable `const char*` describing **why** a type is unsafe (e.g., "UNSAFE: polymorphic type (vtable pointer)", "UNSAFE: std container (use XVector/XMap/XSet/XString)").

---

## 3. Registration Macros

Three macros register custom types into both TypeLayout (opaque signature) and XCompactor (migration strategy) in a single call:

| Macro | For | Example |
|-------|-----|---------|
| `XOFFSET_REGISTER_TYPE(Type, name, strategy)` | Non-template types | `XOFFSET_REGISTER_TYPE(XString, "string", AllocatorAware)` |
| `XOFFSET_REGISTER_CONTAINER(Template, name, strategy)` | Single-param templates `T<U>` | `XOFFSET_REGISTER_CONTAINER(XVector, "vector", Container)` |
| `XOFFSET_REGISTER_MAP(Template, name, strategy)` | Two-param templates `T<K,V>` | `XOFFSET_REGISTER_MAP(XMap, "map", Container)` |

Each macro performs three tasks:
1. **TypeLayout opaque signature** — calls `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE` (or container/map variant) to register the type in `boost::typelayout` namespace
2. **Element safety specialization** — for container/map macros, specializes `opaque_element_types<T>` to recursively verify element types via `DefaultPolicy::accept`
3. **Migration strategy** — specializes `XCompactor::migrate_as<T>` with the given strategy

**Namespace safety**: All three macros include a `_XOffset_NS_Sentinel` check that `static_assert` fails if the macro is accidentally used inside a namespace block.

Migration strategies:

| Strategy | Behavior |
|----------|----------|
| `TrivialCopy` | Direct `memcpy` — for POD types |
| `AllocatorAware` | Re-constructs with new allocator — for types like `XString` |
| `Container` | Iterates elements and recursively migrates each one |
| `Composite` | Reflects over members and recursively migrates — for user structs |

---

## 4. Reflection-Based Construction (Zero Boilerplate)

The library uses C++26 P2996 reflection to enable **zero-boilerplate** type definitions — users write plain structs, and the library automatically handles allocator injection.

### 4.1 Reflect Construct

`reflect_construct<T>(raw_ptr, allocator)` initializes each member of T via reflection:
- Members with `allocator_type` — constructed with the buffer's allocator
- Other members — value-initialized (zero)

This allows `XBuffer::make<T>()` to work with plain aggregate structs containing `XString`, `XVector`, etc., without requiring the user to write any allocator-aware constructor.

### 4.2 Reflect Transfer

`reflect_transfer<T>(src, dst, allocator)` performs a deep copy of all members:
- Opaque container members — re-constructed in the destination buffer
- Leaf members — direct copy
- Composite members — recursive reflection

Used internally by `XVector::push_back` / `emplace_back` during reallocation, and by `XCompactor` during memory compaction.

---

## 5. Memory Architecture

### 5.1 XBufferCore

A typedef for `boost::interprocess::basic_managed_external_buffer` using:
- `x_best_fit` (rbtree-based) memory allocator
- `offset_ptr<void>` for position-independent pointers
- `iset_index` for named object lookup

### 5.2 XManagedMemory

Wraps `XBufferCore` and a `std::vector<char>` backing store. Provides:
- **Adaptive reservation**: Geometric growth with configurable initial and max capacity
- **`grow(size)`**: Resizes the backing buffer, invalidating all existing pointers
- **RAII**: Memory is owned by the `XManagedMemory` instance

### 5.3 XBuffer

The user-facing API wrapping `XManagedMemory`:

| Method | Description |
|--------|-------------|
| `make<T>()` | Allocate and construct a root object |
| `root<T>()` | Access the root object by reference |
| `has_root<T>()` | Check if a root object exists |
| `save(path)` / `save_raw(ptr, size)` | Persist buffer to file or memory |
| `load(path)` / `load_raw(ptr, size)` | Load buffer from file or memory |
| `grow(size)` | Resize buffer (invalidates all pointers) |
| `compact<T>()` | Create a new compacted buffer via `XCompactor` |
| `make_handle<T>()` | Create an `XHandle` that survives reallocation |

### 5.4 XHandle

A stable handle that wraps `root<T>()` access. Unlike raw pointers, `XHandle` survives `grow()` because it re-acquires the root reference on each dereference.

---

## 6. Container Types

All containers use `boost::interprocess::offset_ptr`-based allocators, making them safe for position-independent memory (shared memory, memory-mapped files, serialized buffers).

| Type | Underlying | Notes |
|------|-----------|-------|
| `XString` | `boost::container::basic_string<char, ..., Allocator>` | 32 bytes, align 8 |
| `XVector<T>` | `boost::container::vector<T, Allocator>` | 32 bytes, align 8 |
| `XSet<T>` | `boost::container::flat_set<T, ..., Allocator>` | 32 bytes, align 8 |
| `XMap<K,V>` | `boost::container::flat_map<K, V, ..., Allocator>` | 32 bytes, align 8 |

All four are registered as opaque types in TypeLayout via `XOFFSET_REGISTER_*` macros.

---

## 7. Memory Compaction (XCompactor)

`XCompactor` performs deep migration from an old buffer to a new, compacted buffer. It uses reflection to:
1. **Resolve strategy** for each type (via `migrate_as<T>` specializations)
2. **Recursively migrate** each member:
   - `TrivialCopy` — `memcpy`
   - `AllocatorAware` — reconstruct with new allocator
   - `Container` — iterate elements, migrate each
   - `Composite` — reflect over members, migrate each

This is one of the features that was **impossible** without C++26 reflection — it requires the ability to iterate struct members at compile time and perform type-specific migration for each.

---

## 8. TypeLayout Integration

[TypeLayout](https://github.com/ximicpp/TypeLayout) is integrated as a Git submodule at `external/typelayout`. It serves as the authoritative engine for:

- **Type signatures** — `get_definition_signature<T>()`, `get_layout_signature<T>()`
- **Safety classification** — `is_local_serialization_free_v<T>`, `classify_v<T>`
- **Cross-platform verification** — `is_byte_copy_portable<T>(remote_sig)`
- **Opaque type registration** — `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE` macros

XOffset does **not** implement its own type introspection — all type safety and signature decisions are delegated to TypeLayout.

---

## 9. Platform Requirements

| Requirement | Enforcement |
|-------------|-------------|
| 64-bit architecture | `#error` + `static_assert(sizeof(void*) == 8)` |
| Little-endian | `#error` + `static_assert(XOFFSET_LITTLE_ENDIAN)` |
| C++26 P2996 reflection | `#include <experimental/meta>` (Clang P2996 fork required) |

---

## Related Documents

| Document | Purpose |
|----------|---------|
| [`CORE_FORMAL_MODEL.md`](CORE_FORMAL_MODEL.md) | Formal correctness theorem (C1+C2) and proof |
| [`MIGRATION_TYPELAYOUT.md`](MIGRATION_TYPELAYOUT.md) | Migration guide from legacy XTypeSignature API |
| [`ZERO_BOILERPLATE.md`](ZERO_BOILERPLATE.md) | Zero-boilerplate API architecture and examples |
| [`BUILD_AND_TEST_GUIDE.md`](BUILD_AND_TEST_GUIDE.md) | Build, test, and Docker setup guide |
| [`QUICK_REFERENCE.md`](QUICK_REFERENCE.md) | Quick command reference |