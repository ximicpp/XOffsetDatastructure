# TypeLayout

Compile-time type layout signature generator using C++26 static reflection for binary compatibility verification.

## Purpose

For zero-copy data transfer scenarios. Compile-time signature verification ensures memory layout consistency.

Core guarantee: **Same signature = Same layout**

## Requirements

- [Bloomberg Clang P2996 Fork](https://github.com/bloomberg/clang-p2996)
- 64-bit little-endian platform (x86-64/ARM64)

## Usage

```cpp
#include <typelayout.hpp>
using namespace typelayout;

struct Point { int32_t x, y; };
struct Vector2D { int32_t x, y; };

// Get signature
constexpr auto sig = get_layout_signature<Point>();

// Compile-time verification
TYPELAYOUT_ASSERT_MATCH(Point, "struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}");
TYPELAYOUT_ASSERT_COMPATIBLE(Point, Vector2D);
TYPELAYOUT_ASSERT_PORTABLE(Point);
```

## Build

```bash
./build_and_run.sh
```

## API

| Function/Macro | Description |
|----------------|-------------|
| `get_layout_signature<T>()` | Get signature at compile-time |
| `signatures_match<T1, T2>()` | Compare two type signatures |
| `is_portable<T>()` | Check for platform-dependent members |
| `TYPELAYOUT_ASSERT_MATCH(T, Sig)` | Assert signature matches |
| `TYPELAYOUT_ASSERT_COMPATIBLE(T1, T2)` | Assert layout compatible |
| `TYPELAYOUT_ASSERT_PORTABLE(T)` | Assert type is portable |
| `Portable<T>` | Concept: type is portable |
| `LayoutCompatible<T, U>` | Concept: same layout |

## Portability

Detect platform-dependent types at compile time:

```cpp
struct Bad { long value; wchar_t name[32]; };  // Non-portable
struct Good { int64_t value; char16_t name[32]; };  // Portable

static_assert(is_portable<Good>());
static_assert(!is_portable<Bad>());
```

## Cross-Platform Tool

Use `tools/typelayout_tool` for CI/CD integration:

```bash
# Build
clang++ -std=c++26 -freflection -I../include typelayout_tool.cpp -o typelayout-tool

# Generate (on each platform)
./typelayout-tool generate -o sigs_linux.txt

# Compare
./typelayout-tool compare sigs_linux.txt sigs_windows.txt
```

Demo: `./tools/demo_tool.sh`

## Type Support

Full: integers, floats, chars, pointers, arrays, structs, classes, inheritance, bit-fields, enums

Partial: unions (size/align only)

## License

MIT