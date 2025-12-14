# Compile-Time Layout Contracts: Guaranteeing Binary Compatibility with C++26 Static Reflection

## Abstract

Binary compatibility bugs are among the most insidious in systems programming. Two programs exchange raw bytes through shared memory or network protocols—both compile successfully, both pass unit tests, yet one reads garbage from the other. The culprit: invisible differences in struct layout caused by compiler variations, platform differences, or innocent refactoring.

This session introduces **TypeLayout**, a header-only C++26 library that leverages P2996 Static Reflection to generate **complete, semantic layout signatures** at compile time. The core guarantee is:

> **Identical signature ⟺ Identical memory layout**

With a single macro, developers can bind a type to its expected layout signature. If the layout ever differs—on any platform, with any compiler—**compilation fails immediately**:

```cpp
struct Player { uint64_t id; char name[32]; float health; };
TYPELAYOUT_BIND(Player, "struct[s:48,a:8]{@0[id]:u64[s:8,a:8],@8[name]:bytes[s:32,a:1],@40[health]:f32[s:4,a:4]}");
```

Unlike manual `sizeof()` checks that only verify total size, TypeLayout signatures capture the **complete internal layout**—field offsets, alignments, and nested structure details. The library also provides portability checking to detect platform-dependent types (`wchar_t`, `long`, `long double`) hidden in nested structures.

This talk covers the problem space, demonstrates the C++26 reflection APIs that make this possible, walks through the implementation, and shows practical integration patterns for IPC, serialization, and cross-platform development.

---

## Outline

### 1. The Problem: Silent Binary Incompatibility (8 min)

- What is binary compatibility and why does it matter?
  - The core problem: **same source code, different binary layout across platforms**
    - Different compilers may use different padding strategies
    - Different platforms have different type sizes (`long`: 4 bytes on Windows, 8 bytes on Linux)
    - Struct layout is implementation-defined, not guaranteed by the standard
- Real-world failure scenarios:
  - Cross-platform shared memory: data written on Linux, corrupted when read on Windows
  - Network protocols: sender and receiver disagree on field offsets
  - File formats: data files become unreadable after compiler upgrade
- Why traditional solutions fail:
  - Manual `static_assert(sizeof(...))` only checks size, not internal layout
  - `#pragma pack` is non-portable and error-prone
  - Runtime checks catch bugs too late—after deployment

```cpp
// The problem: same struct, different layout on Windows vs Linux
struct Record {
    int32_t id;
    long    timestamp;  // 4 bytes on Windows (LLP64), 8 bytes on Linux (LP64)!
    int32_t flags;
};
// Windows: sizeof = 12, offsets: id@0, timestamp@4, flags@8
// Linux:   sizeof = 24, offsets: id@0, timestamp@8, flags@16
// Cross-platform shared memory or network protocol = silent data corruption!

// TypeLayout solution: compile-time layout verification
TYPELAYOUT_BIND(Record, "struct[s:12,a:4]{@0[id]:i32[s:4,a:4],@4[timestamp]:long[s:4,a:4],@8[flags]:i32[s:4,a:4]}");
// Compiles on Windows, FAILS on Linux — catches the incompatibility at compile time!
```

---

### 2. C++26 Static Reflection Primer (10 min)

- Introduction to P2996 and `<experimental/meta>`
- Four key APIs used by TypeLayout:
  - `nonstatic_data_members_of(^^T)` — enumerate all fields at compile time
  - `identifier_of(member)` — extract field name as `std::string_view`
  - `offset_of(member).bytes` — compiler-verified byte offset
  - `type_of(member)` — get field type for recursive introspection
- The splice syntax: `obj.[:member:]` for programmatic member access
- Why this was impossible before C++26

```cpp
template<typename T>
consteval auto get_first_field_name() {
    constexpr auto members = std::meta::nonstatic_data_members_of(^^T);
    return std::meta::identifier_of(members[0]);  // Returns "id" for struct { int id; }
}
```

---

### 3. Anatomy of a Layout Signature (10 min)

- Design goals: human-readable, machine-comparable, semantically complete
- Signature format breakdown:
  ```
  struct[s:48,a:8]{@0[id]:u64[s:8,a:8],@8[name]:bytes[s:32,a:1],@40[health]:f32[s:4,a:4]}
  ```
  - `struct[s:48,a:8]` — type category, size 48 bytes, alignment 8
  - `@0[id]` — offset 0, field name "id"
  - `:u64[s:8,a:8]` — type signature with size/alignment
- Nested struct expansion (recursive signatures)
- Bit-field support: `@4.2[flags]:bits<3,u8>` — byte 4, bit offset 2, width 3 bits
- Type coverage: primitives, arrays, enums, unions, inheritance, polymorphic classes

---

### 4. The TYPELAYOUT_BIND Pattern (8 min)

- The "golden signature" workflow:
  1. Define your struct on a reference platform
  2. Generate its signature (runtime print or tooling)
  3. Add `TYPELAYOUT_BIND(Type, "signature")` next to the definition
  4. Compilation fails on any platform where layout differs
- What happens when layout changes: clear `static_assert` error message

```cpp
struct Point { int32_t x, y; };
TYPELAYOUT_BIND(Point, "struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}");

// If compiled on a platform where layout differs:
// error: static_assert failed: "Layout mismatch for Point"
```

---

### 5. Template Constraints with Layout Concepts (8 min)

- `LayoutMatch<T, Signature>` concept for generic programming
- `LayoutCompatible<T, U>` — verify two types share identical layout
- `Portable<T>` — verify no platform-dependent members
- Use case: compile-time validated IPC message types

```cpp
template<typename T>
    requires LayoutMatch<T, "struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}">
void send_point(const T& p) {
    send_raw_bytes(&p, sizeof(T));  // Safe: layout is guaranteed
}
```

---

### 6. Portability Checking (8 min)

- Hidden dangers of platform-dependent types:
  - `wchar_t`: 2 bytes (Windows) vs 4 bytes (Linux)
  - `long`: 4 bytes (Windows LLP64) vs 8 bytes (Linux LP64)
  - `long double`: 8/12/16 bytes depending on platform
- `is_portable<T>()` recursively checks all members and base classes
- Detection through nested struct hierarchies and inheritance chains

```cpp
struct BadMessage {
    int32_t id;
    wchar_t name[16];  // Platform-dependent!
};
static_assert(is_portable<BadMessage>());  // FAILS: wchar_t is not portable
```

---

### 7. Implementation Deep Dive (10 min)

- Challenge: building complex strings at compile time
- Solution: `CompileString<N>` template with:
  - `consteval` constructor
  - `operator+` for concatenation
  - `operator==` for comparison
- Recursive signature generation using fold expressions
- Zero runtime overhead: all work done at compile time

---

### 8. Conclusion and Future Directions (8 min)

- Summary: C++26 reflection enables proof-based binary compatibility
- TypeLayout provides:
  - Complete semantic signatures (field names + offsets + types)
  - Compile-time contract enforcement
  - Portability verification
  - Zero runtime cost
- Future work: signature diff tooling, serialization integration
- Q&A