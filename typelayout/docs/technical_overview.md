# Compile-Time Layout Contracts: Guaranteeing Binary Compatibility with C++26 Static Reflection

## Abstract

**The nightmare of binary compatibility is subtle and catastrophic: two systems exchange raw bytes, both compile successfully, but the data is silently corrupted.** A struct compiled on Windows has different padding than the same struct on Linux. A field renamed from `price` to `cost` passes all type checks but reads garbage. These bugs are invisible at compile time and explosive at runtime.

Traditional solutions—manual `static_assert` chains, documentation contracts, version numbers—are tedious, error-prone, and fundamentally incomplete. They require human discipline to maintain and provide no semantic guarantees.

This talk introduces **TypeLayout**, a header-only C++26 library that leverages **Static Reflection (P2996)** to generate **complete, semantic layout signatures** at compile time. The core guarantee is simple and powerful:

> **Identical signature ⟺ Identical memory layout**

| Traditional Approach | TypeLayout Solution |
|---------------------|---------------------|
| Manual `offsetof()` checks | Automatic `offset_of(member).bytes` via reflection |
| No field name verification | `identifier_of(member)` captures semantic identity |
| Runtime-only validation | Compile-time `static_assert` failure |
| Per-platform maintenance | Single "golden signature" works everywhere |
| Hope-based compatibility | **Proof-based compatibility** |

The result: **one line of code** creates an unbreakable binary contract that catches layout mismatches *before* your code ships, not after your users report data corruption.

```cpp
TYPELAYOUT_BIND(Player, "struct[s:56,a:8]{@0[id]:u64[s:8,a:8],@8[name]:bytes[s:32,a:1],...}");
```

If the layout ever changes—different compiler, different platform, refactored field—**compilation fails immediately** with a clear error message.

---

## Outline

**1. The Problem: Silent Binary Incompatibility (5 min)**
- What is binary compatibility? Why does it matter?
- Real-world failure modes:
  - Cross-platform IPC with mismatched struct padding
  - Shared memory corruption after field reordering
  - Network protocol breakage after innocent refactoring
- Why existing tools fail: no semantic awareness, no compile-time guarantees
- *Takeaway: Binary compatibility bugs are silent, catastrophic, and preventable*

**2. The C++26 Reflection Foundation (8 min)**
- Introduction to P2996 Static Reflection (`<experimental/meta>`)
- Four key APIs that make TypeLayout possible:
  - `nonstatic_data_members_of(^^T)` — enumerate all fields
  - `identifier_of(member)` — extract field names as `string_view`
  - `offset_of(member).bytes` — compiler-verified byte offsets
  - `type_of(member)` — recursive type introspection
- Live code walkthrough: `typelayout.hpp:lines 244-314`
- *Takeaway: C++26 reflection provides complete structural AND semantic introspection*

**3. Anatomy of a Layout Signature (10 min)**
- Signature format design: human-readable, machine-comparable
- Example breakdown:
  ```
  struct[s:56,a:8]{@0[id]:u64[s:8,a:8],@8[name]:bytes[s:32,a:1],@40[pos]:struct[s:8,a:4]{...}}
  ```
  - `s:56` — total size in bytes
  - `a:8` — alignment requirement
  - `@0[id]` — offset 0, field name "id"
  - Nested structs expand recursively
- Bit-field support: `@4.2[flags]:bits<3,u8>` — byte 4, bit 2, width 3
- Type coverage: primitives, arrays, enums, unions, inheritance, polymorphic classes
- *Takeaway: Signatures capture everything that affects binary layout*

**4. The TYPELAYOUT_BIND Pattern (8 min)**
- The "golden signature" workflow:
  1. Define your struct
  2. Generate signature on reference platform
  3. Bind with `TYPELAYOUT_BIND(Type, Signature)`
  4. Compilation fails on any platform where layout differs
- Code example from `demo/demo.cpp:lines 22-25`:
  ```cpp
  struct Player { uint64_t id; char name[32]; Point pos; float health; };
  TYPELAYOUT_BIND(Player, "struct[s:56,a:8]{...}");
  ```
- What happens when layout changes: clear `static_assert` error
- *Takeaway: One line creates an unbreakable, self-documenting binary contract*

**5. Beyond Binding: Template Constraints with Concepts (7 min)**
- Using `LayoutMatch` concept for generic programming:
  ```cpp
  template<typename T>
      requires LayoutMatch<T, "struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}">
  void send_over_network(const T& data);
  ```
- `LayoutCompatible<T, U>` — verify two types share identical layout
- `Portable<T>` — verify no platform-dependent members (`wchar_t`, `long double`, etc.)
- Real-world use case: IPC message validation at compile time
- *Takeaway: Layout constraints enable safe generic binary interfaces*

**6. Portability Checking: Catching Platform-Dependent Types (7 min)**
- The hidden dangers: `wchar_t` (2 bytes Windows, 4 bytes Linux), `long` (4 vs 8 bytes)
- `is_portable<T>()` recursively checks all members and base classes
- Example: detecting `wchar_t` in deeply nested struct hierarchy
- Code walkthrough: `typelayout.hpp:lines 848-902`
- *Takeaway: Catch platform-dependent types before they cause cross-platform bugs*

**7. Implementation Deep Dive: CompileString and consteval (10 min)**
- Challenge: building complex strings at compile time
- Solution: `CompileString<N>` with concatenation and comparison operators
- All signature generation happens in `consteval` context
- Zero runtime overhead: signatures exist only at compile time
- Template metaprogramming techniques: fold expressions, index sequences
- *Takeaway: Understand the compile-time string building that powers TypeLayout*

**8. Conclusion & Future Directions (5 min)**
- Summary: C++26 reflection enables proof-based binary compatibility
- TypeLayout provides:
  - Complete semantic signatures (names + offsets + types)
  - Compile-time contract enforcement
  - Portability verification
  - Zero runtime cost
- Future work: signature diffing tools, migration path generation
- Q&A

---

## Key Code References

| Feature | File | Lines |
|---------|------|-------|
| Reflection API usage | `include/typelayout.hpp` | 244-314 |
| Field signature generation | `include/typelayout.hpp` | 275-314 |
| Bit-field handling | `include/typelayout.hpp` | 285-303 |
| Portability checking | `include/typelayout.hpp` | 848-902 |
| TYPELAYOUT_BIND macro | `include/typelayout.hpp` | 936-938 |
| Concepts (LayoutMatch, etc.) | `include/typelayout.hpp` | 919-931 |
| Demo usage | `demo/demo.cpp` | 22-52 |
| Comprehensive tests | `test/test_all_types.cpp` | 1-680 |

---

## Requirements

- **Compiler**: [Bloomberg Clang P2996 fork](https://github.com/bloomberg/clang-p2996)
- **Platform**: 64-bit, little-endian (x86-64, ARM64)
- **Standard**: C++26 with `<experimental/meta>`
