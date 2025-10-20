# XOffsetDatastructure2 - Architecture Summary

## One-Sentence Description
A **zero-encoding/zero-decoding** high-performance serialization library using offset pointers and compile-time type signature verification.

---

## Core Architecture (3 Pillars)

### 1. Offset-Based Memory Management
```
┌─────────────────────────┐
│ XBuffer (std::vector)   │
│ ┌─────────────────────┐ │
│ │ Boost Managed Mem   │ │
│ │ Uses offset_ptr<T>  │ │ ← Relocatable pointers
│ └─────────────────────┘ │
└─────────────────────────┘
```
**Key**: Data can be copied/moved without breaking pointers

### 2. Dual Type System
```cpp
// Runtime (user-facing)
struct Player {
    template <typename Allocator>
    Player(Allocator a) : name(a), items(a) {}
    int id{0};
    XString name;
};

// Reflection (compile-time analysis)  
struct PlayerReflectionHint {
    int32_t id;      // ← Explicit size
    XString name;
};

static_assert(sizeof(Player) == sizeof(PlayerReflectionHint));
static_assert(layout_identical);
```
**Key**: Identical memory layout, different purposes

### 3. Compile-Time Type Signatures
```cpp
// Python calculates expected signature
"struct[s:72,a:8]{@0:i32[s:4,a:4],@8:string[s:32,a:8],...}"

// C++ calculates actual signature (using boost::pfr)
XTypeSignature::get_XTypeSignature<PlayerReflectionHint>()

// Verify they match at compile-time
static_assert(actual == expected, "Mismatch!");
```
**Key**: Any layout change caught at compile-time

---

## Data Flow

### Creation
```
YAML Schema → Python Generator → C++ Header → Compiler Validation
  player.xds.yaml                   player.hpp      static_assert
```

### Serialization (Zero-Encoding)
```
XBuffer → Just copy bytes → Network/Disk
  ↑
Data already in portable binary format!
```

### Deserialization (Zero-Decoding)
```
Bytes → XBuffer → Use immediately
         ↑
    offset_ptr resolves automatically
```

---

## Key Technologies

| Technology | Purpose |
|------------|---------|
| Boost.Interprocess | `offset_ptr`, managed memory |
| Boost.PFR | Compile-time reflection |
| Boost.Container | Allocator-aware containers |
| Python | Code generation from YAML |
| Template Metaprogramming | `CompileString` for compile-time signatures |

---

## Performance Profile

| Operation | Traditional | XOffsetDatastructure2 |
|-----------|-------------|----------------------|
| Serialize 1MB | ~100ms (encoding) | ~1ms (memcpy) |
| Deserialize 1MB | ~100ms (parsing) | ~0.1ms (mmap) |
| Access field | O(1) | O(1) |
| Container ops | O(log n) | O(log n) |

**Speedup**: 100x for serialization, comparable runtime performance

---

## Type Signature Format

```
Type Format: <kind>[s:<size>,a:<align>]<template>

Examples:
  i32[s:4,a:4]                    - int32_t
  string[s:32,a:8]                - XString
  vector[s:32,a:8]<i32>           - XVector<int>
  struct[s:72,a:8]{...}           - Custom struct
  
Struct Format:
  struct[s:<size>,a:<align>]{
    @<offset>:<field_type>,
    ...
  }
```

---

## Code Organization

```
├── xoffsetdatastructure2.hpp  # Core: XBuffer, containers
├── xtypesignature.hpp         # Compile-time signatures
├── tools/xds_generator.py     # YAML → C++ generator
├── schemas/*.xds.yaml         # Type definitions
├── generated/*.hpp            # Generated code
├── examples/                  # Usage examples
└── tests/                     # Comprehensive tests
```

---

## Safety Mechanisms

### Compile-Time
- ✅ Type signature mismatch → compilation error
- ✅ Size mismatch → compilation error  
- ✅ Alignment mismatch → compilation error
- ✅ Platform requirements → static_assert

### Runtime
- ✅ Memory bounds checking (Boost)
- ✅ Allocator safety
- ✅ Container invariants

---

## Usage Pattern

### 1. Define Schema (YAML)
```yaml
types:
  - name: Player
    fields:
      - name: id
        type: int
      - name: name
        type: XString
```

### 2. Generate Code
```bash
python tools/xds_generator.py player.xds.yaml
# → generated/player.hpp
```

### 3. Use in C++
```cpp
#include "player.hpp"

XBufferExt xbuf(4096);
auto* p = xbuf.make<Player>("hero");
p->id = 1;
p->name = xbuf.make<XString>("Alice");

// Serialize
auto data = xbuf.save_to_string();

// Deserialize  
XBufferExt loaded = XBufferExt::load_from_string(data);
auto [player, found] = loaded.find_ex<Player>("hero");
```

---

## Design Highlights

### 1. Zero Overhead Abstraction
- All type checking at compile-time
- No runtime overhead
- Template metaprogramming eliminates runtime cost

### 2. Memory Efficiency
- Flat containers (better cache locality)
- All containers: 32 bytes on 64-bit
- Minimal overhead

### 3. Safety
- Python and C++ must agree on layout
- Any discrepancy → compile error
- Can't serialize broken data

### 4. Developer Experience
- YAML schemas (declarative)
- Generated code (no manual writing)
- Compile-time errors (not runtime bugs)

---

## Limitations & Trade-offs

### Current Limitations
- 64-bit, little-endian only
- No virtual functions in serialized types
- Must use XOffsetDatastructure containers
- Code generation required

### Trade-offs
| Gain | Cost |
|------|------|
| 100x serialization speed | Setup complexity |
| Compile-time safety | Platform restrictions |
| Zero-copy deserialization | Must use offset_ptr |
| Binary compatibility | Learning curve |

---

## Future Work

- **C++26 Reflection**: Eliminate ReflectionHint types
- **Cross-platform**: Big-endian, 32-bit support
- **Versioning**: Schema evolution
- **Shared Memory**: IPC support

---

## Conclusion

XOffsetDatastructure2 achieves extreme performance through:

1. **Smart Design**: Data always in binary format
2. **Offset Pointers**: Enable zero-copy operations  
3. **Dual Verification**: Python pre-calculation + C++ validation
4. **Compile-Time Safety**: Catch all errors at compile-time

**Result**: ~100x faster serialization with compile-time guarantees

---

## Quick Links

- Full Analysis: [ARCHITECTURE_ANALYSIS.md](ARCHITECTURE_ANALYSIS.md)
- Type Signatures: [TYPE_SIGNATURE.md](TYPE_SIGNATURE.md)
- Schema Guide: [../schemas/README.md](../schemas/README.md)
- CppCon 2024: [Presentation PDF](XOffsetDatastructure_CppCon2024.pdf)
- CppCon 2025: [Compile-time Type Signatures PDF](Compile-timeTypeSignatures.pdf)
