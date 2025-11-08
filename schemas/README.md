# XOffsetDatastructure2 Schema Definitions

This directory contains YAML schema definitions for all data types used in the project.

## Schema Files

| Schema File | Description | Generated Types |
|------------|-------------|-----------------|
| `test_types.xds.yaml` | Comprehensive test types with nested structures | TestTypeInner, TestType |
| `basic_types.xds.yaml` | Basic POD types testing | BasicTypes |
| `vector_test.xds.yaml` | XVector container tests | VectorTest |
| `map_set_test.xds.yaml` | XMap and XSet container tests | MapSetTest |
| `nested_test.xds.yaml` | Nested structure hierarchy | InnerObject, MiddleObject, OuterObject |
| `modify_test.xds.yaml` | Data modification tests | ModifyTestData |
| `compaction_test.xds.yaml` | Memory compaction tests | MemoryTestType |
| `serialization_test.xds.yaml` | Serialization tests | SimpleData, ComplexData |
| `alignment_test.xds.yaml` | Memory alignment tests | AlignedStruct |
| `player.xds.yaml` | Player data structure (example) | Player |
| `game_data.xds.yaml` | Game data with inventory (example) | Item, GameData |

## Schema Format

Each schema file follows this format:

```yaml
schema_version: "1.0"

types:
  - name: TypeName
    type: struct
    fields:
      - name: fieldName
        type: fieldType
        default: defaultValue  # Optional
        description: "field description"  # Optional
```

### Supported Field Types

#### Basic Types
- `int`, `float`, `double`, `bool`, `char`
- `long long`
- `int32_t`, `int64_t`, `uint32_t`, `uint64_t`

#### XOffsetDatastructure Types
- `XString` - Offset-based string
- `XVector<T>` - Offset-based vector
- `XSet<T>` - Offset-based set
- `XMap<K, V>` - Offset-based map

#### Custom Types
- Any type defined in the same schema file
- Nested structures are supported

## Code Generation

Schemas are automatically converted to C++ header files during the build process.

### Build Integration

```bash
# Generate all schemas
cd build
make generate_schemas

# Generated headers appear in: build/generated/*.hpp
```

### CMake Configuration

The schemas are configured in the main `CMakeLists.txt`:

```cmake
set(SCHEMA_FILES
    ${CMAKE_SOURCE_DIR}/schemas/test_types.xds.yaml
    ${CMAKE_SOURCE_DIR}/schemas/basic_types.xds.yaml
    # ... more schemas
)
```

## Generated Code Structure

Each schema generates two types:

1. **Runtime Type**: Used for actual data storage
   - Has allocator constructor
   - Contains XOffset containers
   
2. **ReflectionHint Type**: Used for compile-time reflection
   - Aggregate type for boost::pfr
   - Converts runtime types to basic types

### Example

Schema:
```yaml
types:
  - name: Player
    type: struct
    fields:
      - name: id
        type: int
        default: 0
      - name: name
        type: XString
```

Generated:
```cpp
// Runtime type
struct alignas(XTypeSignature::BASIC_ALIGNMENT) Player {
    template <typename Allocator>
    Player(Allocator allocator) : name(allocator) {}
    int id{0};
    XString name;
};

// Reflection type
struct alignas(XTypeSignature::BASIC_ALIGNMENT) PlayerReflectionHint {
    int32_t id;
    XString name;
};
```

## Usage in Code

```cpp
#include "xoffsetdatastructure2.hpp"
#include "player.hpp"  // Generated from player.xds.yaml

XBufferExt xbuf(4096);
auto* player = xbuf.make<Player>("Player1");
player->id = 100;
player->name.assign("Alice");
```

## Adding New Schemas

1. Create a new `.xds.yaml` file in `schemas/` directory
2. Define your types following the schema format
3. Add the schema to `CMakeLists.txt` in the `SCHEMA_FILES` list
4. Run `make generate_schemas` to generate the header file
5. Include the generated header in your code

## Tools

- **Generator**: `tools/xds_generator.py`
- **Documentation**: `docs/DSL_GUIDE.md`
- **Migration Guide**: `docs/GENERATED_TYPES.md`
