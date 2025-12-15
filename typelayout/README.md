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

### Signature Functions

| Macro/Function | Description |
|----------------|-------------|
| `TYPELAYOUT_BIND(T, Sig)` | Bind type to signature |
| `get_layout_signature<T>()` | Get type's layout signature |
| `get_layout_signature_cstr<T>()` | Get signature as C-string (runtime) |
| `signatures_match<T, U>()` | Check if two types have same layout |
| `is_portable<T>()` | Check for platform-dependent members |
| `is_platform_dependent_v<T>` | Check if type is platform-dependent |

### Hash Functions (for runtime/protocol use)

| Function | Description |
|----------|-------------|
| `get_layout_hash<T>()` | Get 64-bit FNV-1a hash of layout signature |
| `hashes_match<T, U>()` | Check if two types have same layout hash |

```cpp
// Compile-time hash for protocol headers
constexpr uint64_t MESSAGE_HASH = get_layout_hash<Message>();

// Runtime validation
if (received_hash != MESSAGE_HASH) { /* layout mismatch */ }
```

### Concepts

| Concept | Description |
|---------|-------------|
| `LayoutMatch<T, Sig>` | Layout matches signature |
| `LayoutCompatible<T, U>` | Identical layout |
| `LayoutHashMatch<T, Hash>` | Layout hash matches expected value |
| `LayoutHashCompatible<T, U>` | Identical layout hash |
| `Portable<T>` | No platform-dependent members |

## Build

```bash
./build_and_run.sh
```

## License

MIT
