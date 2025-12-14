# TypeLayout

Compile-time type layout signature generator using C++26 static reflection (P2996).

**Core guarantee: Same signature = Same layout**

## Requirements

- [Bloomberg Clang P2996](https://github.com/bloomberg/clang-p2996)
- 64-bit little-endian (x86-64/ARM64)

## Usage

```cpp
#include <typelayout.hpp>

struct Point { int32_t x, y; };

// Bind to expected signature - compilation fails if layout differs
TYPELAYOUT_BIND(Point, "struct[s:8,a:4]{@0[x]:i32[s:4,a:4],@4[y]:i32[s:4,a:4]}");

// Check compatibility
static_assert(signatures_match<Point, Vec2>());  // Same layout as another type

// Template constraint
template<typename T>
    requires LayoutMatch<T, "struct[s:8,a:4]{...}">
void send(const T& data);
```

## API

| Macro/Function | Description |
|----------------|-------------|
| `TYPELAYOUT_BIND(T, Sig)` | Bind type to signature |
| `get_layout_signature<T>()` | Get type's layout signature |
| `signatures_match<T, U>()` | Check if two types have same layout |
| `is_portable<T>()` | Check for platform-dependent members |
| `is_platform_dependent_v<T>` | Check if type is platform-dependent |

| Concept | Description |
|---------|-------------|
| `LayoutMatch<T, Sig>` | Layout matches signature |
| `LayoutCompatible<T, U>` | Identical layout |
| `Portable<T>` | No platform-dependent members |

## Build

```bash
./build_and_run.sh
```

## License

MIT
