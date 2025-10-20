# Type Signature Verification

## Overview

All generated type definitions now include compile-time type signature verification to ensure binary compatibility and detect layout changes.

## Generated Files

All generated headers are located in the project's `generated/` directory:

```
generated/
├── alignment_test.hpp
├── basic_types.hpp
├── compaction_test.hpp
├── game_data.hpp
├── map_set_test.hpp
├── modify_test.hpp
├── nested_test.hpp
├── player.hpp
├── serialization_test.hpp
├── test_types.hpp
└── vector_test.hpp
```

**Note**: These are source files that should be committed to version control.

## What's Generated

For each type definition in a YAML schema, the generator creates:

### 1. Runtime Type
```cpp
struct alignas(XTypeSignature::BASIC_ALIGNMENT) Player {
    template <typename Allocator>
    Player(Allocator allocator) : name(allocator), items(allocator) {}
    
    int id{0};
    int level{0};
    XString name;
    XVector<int> items;
};
```

### 2. Reflection Hint Type
```cpp
struct alignas(XTypeSignature::BASIC_ALIGNMENT) PlayerReflectionHint {
    int32_t id;
    int32_t level;
    XString name;
    XVector<int32_t> items;
};
```

### 3. Compile-Time Validation
```cpp
// Size and alignment validation
static_assert(sizeof(Player) == sizeof(PlayerReflectionHint),
              "Size mismatch: Player runtime and reflection types must have identical size");
static_assert(alignof(Player) == alignof(PlayerReflectionHint),
              "Alignment mismatch: Player runtime and reflection types must have identical alignment");

// Type signature validation (compile-time verification)
static_assert(XTypeSignature::get_XTypeSignature<PlayerReflectionHint>() ==
             "struct[s:72,a:8]{"
             "@0:i32[s:4,a:4],"
             "@4:i32[s:4,a:4],"
             "@8:string[s:32,a:8],"
             "@40:vector[s:32,a:8]<i32[s:4,a:4]>}",
              "Type signature mismatch for PlayerReflectionHint");
```

## Type Signature Format

Type signatures encode complete memory layout information:

### Basic Types
- `i32[s:4,a:4]` - int32_t, size 4, align 4
- `f32[s:4,a:4]` - float, size 4, align 4
- `i64[s:8,a:8]` - int64_t, size 8, align 8

### Container Types
- `string[s:32,a:8]` - XString
- `vector[s:32,a:8]<T>` - XVector<T>
- `set[s:32,a:8]<T>` - XSet<T>
- `map[s:32,a:8]<K,V>` - XMap<K,V>

### Struct Types
```
struct[s:<size>,a:<align>]{
    @<offset1>:<field1_type>,
    @<offset2>:<field2_type>,
    ...
}
```

## Example Signatures

### Simple Structure
```cpp
struct Player {
    int32_t id;        // @0, size 4
    int32_t level;     // @4, size 4
    XString name;      // @8, size 32
    XVector<int> items; // @40, size 32
};
// Total size: 72 bytes, alignment: 8

// Signature:
// struct[s:72,a:8]{
//   @0:i32[s:4,a:4],
//   @4:i32[s:4,a:4],
//   @8:string[s:32,a:8],
//   @40:vector[s:32,a:8]<i32[s:4,a:4]>
// }
```

### Nested Structure
```cpp
struct Item {
    int32_t item_id;    // @0
    int32_t item_type;  // @4
    int32_t quantity;   // @8
    XString name;       // @16
};

struct GameData {
    int32_t player_id;              // @0
    int32_t level;                  // @4
    float health;                   // @8
    XString player_name;            // @16
    XVector<Item> items;            // @48
    XSet<int> achievements;         // @80
    XMap<XString, int> quest_progress; // @112
};

// Item signature:
// struct[s:48,a:8]{
//   @0:i32[s:4,a:4],
//   @4:i32[s:4,a:4],
//   @8:i32[s:4,a:4],
//   @16:string[s:32,a:8]
// }

// GameData signature:
// struct[s:144,a:8]{
//   @0:i32[s:4,a:4],
//   @4:i32[s:4,a:4],
//   @8:f32[s:4,a:4],
//   @16:string[s:32,a:8],
//   @48:vector[s:32,a:8]<struct[s:48,a:8]{...Item...}>,
//   @80:set[s:32,a:8]<i32[s:4,a:4]>,
//   @112:map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>
// }
```

## Benefits

### 1. Compile-Time Safety
- Catches layout changes at compile time
- No runtime overhead
- Prevents silent ABI breaks

### 2. Binary Compatibility
- Verify serialized data compatibility
- Detect version mismatches
- Ensure cross-platform consistency

### 3. Documentation
- Self-documenting memory layout
- Clear field offsets
- Size and alignment visible

### 4. Debugging
- Easy to identify layout issues
- Compare signatures across versions
- Verify memory assumptions

## Usage

### View Type Signature
```cpp
#include "player.hpp"

// Print signature at runtime
auto sig = XTypeSignature::get_XTypeSignature<PlayerReflectionHint>();
sig.print();  // Output: struct[s:72,a:8]{...}

// Verify at compile-time (in generated code)
static_assert(sizeof(Player) == sizeof(PlayerReflectionHint));
```

### Debug Signature
Uncomment the pragma message in generated code to see the signature during compilation:
```cpp
// In generated header
// #pragma message(XTypeSignature::get_XTypeSignature<PlayerReflectionHint>().value)
```

### Run Demo
```bash
cd build
./bin/signature_demo
```

## Implementation Details

### Why Two Types?

1. **Runtime Type** (`Player`)
   - Has allocator constructor for XOffsetDatastructure containers
   - Used for actual data storage
   - Not an aggregate type (can't use boost::pfr)

2. **Reflection Hint Type** (`PlayerReflectionHint`)
   - Pure aggregate (no constructors)
   - Compatible with boost::pfr for reflection
   - Same memory layout as runtime type

### Validation
```cpp
// Generated for each type
static_assert(sizeof(Runtime) == sizeof(Reflection));
static_assert(alignof(Runtime) == alignof(Reflection));
```

This ensures both types have identical memory layout despite different constructors.

## Testing

All generated types are tested:
```bash
cd build
ctest

# Tests include:
# - BasicTypes
# - VectorOps
# - MapSetOps
# - NestedStructures
# - MemoryStats
# - DataModification
# - Comprehensive
# - Serialization
# - Alignment
# - TypeSignature ⭐
# - GeneratedTypes ⭐
```

## See Also

- [Schema Guide](../schemas/README.md) - How to define types in YAML
- [Generated Types Migration](GENERATED_TYPES.md) - Migration guide
- [DSL Guide](DSL_GUIDE.md) - Type system documentation
