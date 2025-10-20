# Generated Type Definitions

All data type definitions have been extracted from test and example files into YAML schemas and generated C++ headers.

## Schema Files

Located in `schemas/` directory:

- `basic_types.xds.yaml` - Basic POD types (int, float, double, bool, char, long long)
- `vector_test.xds.yaml` - Vector container tests
- `map_set_test.xds.yaml` - Map and Set container tests  
- `nested_test.xds.yaml` - Nested structures (InnerObject, MiddleObject, OuterObject)
- `modify_test.xds.yaml` - Data modification tests
- `compaction_test.xds.yaml` - Memory compaction tests
- `serialization_test.xds.yaml` - Serialization tests (SimpleData, ComplexData)
- `alignment_test.xds.yaml` - Alignment tests
- `player.xds.yaml` - Player data structure (for examples)
- `game_data.xds.yaml` - Game data with items (for examples)
- `test_types.xds.yaml` - Comprehensive test types

## Generated Headers

Generated headers are located in `build/generated/` directory and follow the naming pattern `<schema_name>.hpp`.

Example:
- `schemas/basic_types.xds.yaml` → `build/generated/basic_types.hpp`

## Usage in Test Files

### Before (Manual Definition)

```cpp
#include "../xoffsetdatastructure2.hpp"

struct alignas(BASIC_ALIGNMENT) BasicTypes {
    template <typename Allocator>
    BasicTypes(Allocator allocator) {}
    
    int mInt;
    float mFloat;
    // ... more fields
};
```

### After (Generated from Schema)

```cpp
#include "../xoffsetdatastructure2.hpp"
#include "basic_types.hpp"  // Generated header

// BasicTypes is now available, generated from schema
```

## Migration Status

### ✅ Completed
- [x] `test_basic_types.cpp` - Using `basic_types.hpp`
- [x] All schemas created
- [x] CMake integration完成
- [x] Code generation working

### 📋 To Migrate

The following test files should be updated to use generated headers:

#### Test Files
- `test_vector.cpp` → Use `#include "vector_test.hpp"`
- `test_map_set.cpp` → Use `#include "map_set_test.hpp"`
- `test_nested.cpp` → Use `#include "nested_test.hpp"`
- `test_modify.cpp` → Use `#include "modify_test.hpp"`
- `test_compaction.cpp` → Use `#include "compaction_test.hpp"`
- `test_serialization.cpp` → Use `#include "serialization_test.hpp"`
- `test_alignment.cpp` → Use `#include "alignment_test.hpp"`
- `test_comprehensive.cpp` - Keep as-is (uses inline nested class)

#### Example Files
- `examples/helloworld.cpp` → Use `#include "player.hpp"`
- `examples/demo.cpp` → Use `#include "game_data.hpp"`

## Migration Steps

For each test file:

1. **Add generated header include**:
   ```cpp
   #include "generated_type.hpp"
   ```

2. **Remove manual struct definition**:
   Delete the `struct alignas(BASIC_ALIGNMENT) ...` definition

3. **Update CMakeLists.txt**:
   Add generated directory to include path:
   ```cmake
   target_include_directories(test_name PRIVATE 
       ${BOOST_INCLUDE_DIRS} 
       ${CMAKE_SOURCE_DIR}
       ${CMAKE_BINARY_DIR}/generated  # Add this
   )
   
   if(Python3_FOUND)
       add_dependencies(test_name generate_schemas)  # Add this
   endif()
   ```

## Benefits

1. ✅ **Single Source of Truth**: Type definitions in YAML schemas
2. ✅ **Automatic ReflectionHint Generation**: No manual duplication needed
3. ✅ **Type Safety**: Compile-time validation
4. ✅ **Documentation**: YAML schemas are self-documenting
5. ✅ **Consistency**: Generated code follows consistent patterns
6. ✅ **Easy Maintenance**: Change schema once, regenerate all code

## Example: Complete Migration

### Before
```cpp
// test_vector.cpp
#include "../xoffsetdatastructure2.hpp"

struct alignas(BASIC_ALIGNMENT) VectorTest {
    template <typename Allocator>
    VectorTest(Allocator allocator) 
        : intVector(allocator), 
          floatVector(allocator),
          stringVector(allocator) {}
    
    XVector<int> intVector;
    XVector<float> floatVector;
    XVector<XString> stringVector;
};
```

### After
```cpp
// test_vector.cpp
#include "../xoffsetdatastructure2.hpp"
#include "vector_test.hpp"

// VectorTest is now available from generated header
```

### Schema
```yaml
# schemas/vector_test.xds.yaml
schema_version: "1.0"

types:
  - name: VectorTest
    type: struct
    fields:
      - name: intVector
        type: XVector<int>
      - name: floatVector
        type: XVector<float>
      - name: stringVector
        type: XVector<XString>
```
