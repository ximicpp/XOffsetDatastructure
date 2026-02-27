# `long` Type Portability Guide for XOffsetDatastructure

> **Rule #1**: Never use `long`, `unsigned long`, or `long long` in any struct
> intended for cross-platform zero-copy sharing via XBuffer.

## Table of Contents

1. [The Problem](#the-problem)
2. [ABI Data Model Comparison](#abi-data-model-comparison)
3. [Why This Matters for XBuffer](#why-this-matters-for-xbuffer)
4. [Why Reflection Cannot Help](#why-reflection-cannot-help)
5. [Recommended Replacements](#recommended-replacements)
6. [Static Assert Guards](#static-assert-guards)
7. [FAQ](#faq)

---

## The Problem

The C/C++ standard does **not** fix the byte-width of `long`. Its size depends
on the platform's **data model**:

| Type | Windows (LLP64) | Linux/macOS x86_64 (LP64) | Linux ARM64 (LP64) |
|------|:---:|:---:|:---:|
| `int` | 4 | 4 | 4 |
| **`long`** | **4** | **8** | **8** |
| `long long` | 8 | 8 | 8 |
| `int32_t` | 4 | 4 | 4 |
| `int64_t` | 4→8 (via `long long`) | 8 (via `long`) | 8 (via `long`) |

A struct containing `long` compiled on Windows will have a **different binary
layout** than the same struct compiled on Linux. This breaks zero-copy
deserialization, which is the fundamental premise of XOffsetDatastructure.

## ABI Data Model Comparison

```
LLP64 (Windows):
  char=1  short=2  int=4  long=4  long long=8  pointer=8

LP64 (Linux, macOS, most Unix):
  char=1  short=2  int=4  long=8  long long=8  pointer=8

ILP32 (32-bit systems):
  char=1  short=2  int=4  long=4  long long=8  pointer=4
```

### The Typedef Trap

On LP64 Linux, `int64_t` is **typedef'd** to `long`:

```cpp
// <stdint.h> on LP64 Linux (glibc)
typedef long int           int64_t;   // long IS int64_t

// <stdint.h> on LLP64 Windows (MSVC)
typedef long long          int64_t;   // long long IS int64_t
```

This means `std::is_same_v<long, int64_t>` is:
- **`true`** on Linux LP64
- **`false`** on Windows LLP64

The compiler treats them as **the same type** on Linux, making it impossible to
distinguish user intent ("did you write `long` or `int64_t`?") after typedef
resolution.

## Why This Matters for XBuffer

XOffsetDatastructure uses **TypeLayout signatures** to describe binary layouts.
These signatures encode types as fixed-width markers:

| TypeLayout Marker | Size | Maps From |
|---|---|---|
| `i32` | 4 bytes | `int`, `int32_t`, `long` (on Windows) |
| `i64` | 8 bytes | `int64_t`, `long` (on Linux), `long long` |

When a user writes:

```cpp
struct MyData {
    long value;  // 4 bytes on Windows, 8 bytes on Linux!
};
```

- **On Windows**: TypeLayout encodes this as `i32` (4 bytes)
- **On Linux**: TypeLayout encodes this as `i64` (8 bytes)

The resulting signatures are **incompatible**, and zero-copy sharing between
platforms will produce **silent data corruption**.

## Why Reflection Cannot Help

We conducted a thorough investigation using **P2996 (C++26 static reflection)**
to determine if compile-time reflection could detect `long` inside struct
members. The results were conclusive:

### What P2996 *Can* Do

```cpp
// Direct type reflection CAN distinguish them:
static_assert(^^long != ^^int64_t);          // ✅ Different reflection IDs
display_string_of(^^long)    → "long"        // ✅ Preserves source name
display_string_of(^^int64_t) → "int64_t"     // ✅ Preserves source name
```

### What P2996 *Cannot* Do (The Showstopper)

```cpp
struct ProbeStruct {
    int64_t b;  // User wrote int64_t
    long    c;  // User wrote long
};

// Member-level type_of() performs "desugaring":
display_string_of(type_of(mems[0]))  → "long"   // ⚠️ int64_t desugared!
display_string_of(type_of(mems[1]))  → "long"   // Same as above
type_of(mems[0]) == type_of(mems[1]) → true     // ⚠️ Indistinguishable!
```

**The compiler's `type_of()` for struct members resolves typedef aliases**,
making it impossible to tell whether the user wrote `long` or `int64_t`. Since
`int64_t` IS `long` on LP64, they become identical after desugaring.

This means:
- ❌ Cannot scan structs for "accidental `long` usage" via reflection
- ❌ Cannot build an automated compile-time guard for struct members
- ✅ CAN still guard against naked `long` types at the API boundary

> **Probe test**: See `tests/test_long_reflection_probe.cpp` for the full
> P2996 diagnostic program and its output.

## Recommended Replacements

**Always use fixed-width integer types from `<cstdint>`:**

| ❌ Do NOT Use | ✅ Use Instead | Size (all platforms) |
|---|---|---|
| `long` | `int32_t` or `int64_t` | 4 or 8 bytes |
| `unsigned long` | `uint32_t` or `uint64_t` | 4 or 8 bytes |
| `long long` | `int64_t` | 8 bytes |
| `unsigned long long` | `uint64_t` | 8 bytes |
| `size_t` | `uint32_t` or `uint64_t` | 4 or 8 bytes |
| `ptrdiff_t` | `int32_t` or `int64_t` | 4 or 8 bytes |
| `intptr_t` | ❌ Avoid (pointer-sized) | — |

### Example: Before and After

```cpp
// ❌ BAD: Platform-dependent layout
struct GameState {
    long           score;
    unsigned long  flags;
    long long      timestamp;
    size_t         count;
};

// ✅ GOOD: Fixed layout on all platforms
struct GameState {
    int64_t        score;
    uint64_t       flags;
    int64_t        timestamp;
    uint64_t       count;
};
```

## Static Assert Guards

XOffsetDatastructure provides `classify_for_xoffset<T>()` which checks
type-level safety. For **top-level** types, `long` is caught:

```cpp
// This will fail classification on Windows (long = 4 bytes ≠ expected 8)
// But on Linux, long == int64_t, so it silently passes! 
```

### Recommended: Add Your Own Guards

Since the library cannot detect `long` inside struct members, **you** must
ensure your structs are clean. Add these guards to your code:

```cpp
#include <cstdint>
#include <type_traits>

// Guard macro: place in your struct definition file
#define XBUFFER_ASSERT_NO_LONG(StructName) \
    static_assert(true, "Manual review required: ensure " #StructName \
                        " contains no long/unsigned long/long long members. " \
                        "Use int32_t/int64_t/uint32_t/uint64_t instead.")

// Usage:
struct MyData {
    int32_t  x;
    int64_t  y;
    uint64_t z;
    
    template<typename Allocator>
    MyData(Allocator) {}
};
XBUFFER_ASSERT_NO_LONG(MyData);  // Reminder for code reviewers
```

### Platform-Specific Compile-Time Check

On **Windows only**, you can catch `long` at the type level because `long ≠ int32_t`:

```cpp
// This check ONLY works on Windows (LLP64) where long is a distinct type
#ifdef _WIN32
template<typename T>
struct contains_no_long {
    static_assert(!std::is_same_v<T, long>,
                  "Do not use 'long' in XBuffer structs. Use int32_t or int64_t.");
    static_assert(!std::is_same_v<T, unsigned long>,
                  "Do not use 'unsigned long' in XBuffer structs. Use uint32_t or uint64_t.");
    static constexpr bool value = true;
};
#endif
```

## FAQ

### Q: If `long` == `int64_t` on Linux, is it actually safe there?

**On a single platform, yes.** If your data never leaves Linux x86_64, using
`long` produces the exact same binary layout as `int64_t`. The problem arises
when:

1. Sharing data files between Windows and Linux
2. Sending data over a network between different platforms
3. The code might be compiled on a different platform in the future

### Q: Why not just use `#pragma pack` or `__attribute__((packed))`?

Packing only controls **padding/alignment**. It does **not** change the
fundamental size of `long` (4 vs 8 bytes). A packed struct with `long` is
still platform-dependent.

### Q: What about `long double`?

`long double` is even worse:
- **Windows (MSVC)**: 8 bytes (same as `double`)
- **Linux x86_64 (GCC/Clang)**: 16 bytes (80-bit extended, padded)
- **Linux ARM64**: 16 bytes (128-bit quad precision)

**Never use `long double` in XBuffer structs.** Use `double` (8 bytes, IEEE 754)
for floating-point data.

### Q: Can I use `wchar_t`?

No. `wchar_t` is:
- **Windows**: 2 bytes (UTF-16)
- **Linux**: 4 bytes (UTF-32)

Use `char` (UTF-8), `char8_t`, `char16_t`, or `char32_t` instead.

---

*This guide is part of the XOffsetDatastructure documentation. Last updated
based on P2996 reflection probe results from `tests/test_long_reflection_probe.cpp`.*
