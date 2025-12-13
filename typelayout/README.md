# TypeLayout

Compile-time type layout signature generator using C++26 static reflection (`std::meta`) for binary compatibility verification.

## Purpose

For zero-copy data transfer scenarios. Compile-time signature verification ensures memory layout consistency between sender and receiver.

Core guarantee: **Same signature = Same layout**

## Platform Requirements

Compile-time checks enforce these conditions (build fails if not met):

- 64-bit architecture (`sizeof(void*) == 8`)
- Little-endian byte order
- IEEE 754 floating-point format

Supported: x86-64/ARM64 Linux, Windows, macOS

## Signature Format

```
# Primitives
i32[s:4,a:4]     # int32_t, size=4, align=4
f64[s:8,a:8]     # double
ptr[s:8,a:8]     # pointer

# Arrays
bytes[s:32,a:1]                   # char[32]
array[s:16,a:4]<i32[s:4,a:4],4>  # int32_t[4]

# Struct
struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}

# Bit-field
@0.0[flag]:bits<1,u8[s:1,a:1]>   # byte.bit[name]:bits<width,type>

# Inheritance
class[s:24,a:8,inherited]{@0[base]:...,@16[value]:...}
class[s:32,a:8,polymorphic]{...}  # has virtual functions
```

## Usage

```cpp
#include <typelayout.hpp>
using namespace typelayout;

struct Point { int32_t x, y; };
struct Vector2D { int32_t x, y; };

// Get signature
constexpr auto sig = get_layout_signature<Point>();
const char* str = get_layout_signature_cstr<Point>();

// Compile-time verification
TYPELAYOUT_ASSERT_MATCH(Point, "struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}");
TYPELAYOUT_ASSERT_COMPATIBLE(Point, Vector2D);
```

## Build

Requires [Bloomberg Clang P2996 Fork](https://github.com/bloomberg/clang-p2996).

```bash
./build_and_run.sh
```

## API

| Function/Macro | Description |
|----------------|-------------|
| `get_layout_signature<T>()` | Get signature at compile-time |
| `get_layout_signature_cstr<T>()` | Get C-string signature |
| `signatures_match<T1, T2>()` | Compare two type signatures |
| `TYPELAYOUT_ASSERT_MATCH(T, Sig)` | Assert signature matches |
| `TYPELAYOUT_ASSERT_COMPATIBLE(T1, T2)` | Assert layout compatible |

## Type Support

Full support: integers, floats, chars, pointers, references, arrays, structs, classes, inheritance, virtual inheritance, polymorphism, bit-fields, enums, member pointers

Partial support: unions (size/align only)

## License

MIT
