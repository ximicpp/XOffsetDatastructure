# XOffsetDatastructure2 Tests

This directory contains comprehensive test cases for the XOffsetDatastructure2 library.

## Test Files

### 1. test_basic_types.cpp
**Purpose:** Test basic POD (Plain Old Data) types
- int, float, double, char, bool, long long
- Persistence and data integrity
- Memory serialization/deserialization

**Run:**
```bash
cd build
./test_basic_types
```

### 2. test_vector.cpp
**Purpose:** Test XVector container operations
- push_back, element access, iteration
- String vector operations
- Clear and empty operations
- Persistence across buffer serialization

**Run:**
```bash
cd build
./test_vector
```

### 3. test_map_set.cpp
**Purpose:** Test XMap and XSet containers
- Set insertion and uniqueness
- Map key-value operations
- String keys and values
- Find operations
- Iteration and persistence

**Run:**
```bash
cd build
./test_map_set
```

### 4. test_nested.cpp
**Purpose:** Test nested object structures
- Multi-level object hierarchies
- Nested containers
- Deep access patterns
- Complex data structure persistence

**Run:**
```bash
cd build
./test_nested
```

### 5. test_compaction.cpp
**Purpose:** Test memory compaction functionality
- Memory usage before/after compaction
- Data integrity after compaction
- Size reduction verification
- Migration function testing

**Run:**
```bash
cd build
./test_compaction
```

### 6. run_all_tests.cpp
**Purpose:** Run all tests in sequence and report results
- Executes all test cases
- Summary report with pass/fail status
- Exception handling

**Note:** This requires linking all test object files together.

## Building Tests

### Add to CMakeLists.txt

Add the following to your `CMakeLists.txt`:

```cmake
# Tests
enable_testing()

add_executable(test_basic_types tests/test_basic_types.cpp)
target_link_libraries(test_basic_types ${Boost_LIBRARIES})

add_executable(test_vector tests/test_vector.cpp)
target_link_libraries(test_vector ${Boost_LIBRARIES})

add_executable(test_map_set tests/test_map_set.cpp)
target_link_libraries(test_map_set ${Boost_LIBRARIES})

add_executable(test_nested tests/test_nested.cpp)
target_link_libraries(test_nested ${Boost_LIBRARIES})

add_executable(test_compaction tests/test_compaction.cpp)
target_link_libraries(test_compaction ${Boost_LIBRARIES})

# Add tests
add_test(NAME BasicTypes COMMAND test_basic_types)
add_test(NAME VectorOps COMMAND test_vector)
add_test(NAME MapSetOps COMMAND test_map_set)
add_test(NAME NestedStructures COMMAND test_nested)
add_test(NAME MemoryCompaction COMMAND test_compaction)
```

### Build and Run

```bash
# Configure
cmake -B build -DOFFSET_DATA_STRUCTURE_2_CUSTOM_CONTAINER_GROWTH_FACTOR=0

# Build
cmake --build build --config Release

# Run individual tests
cd build
./test_basic_types
./test_vector
./test_map_set
./test_nested
./test_compaction

# Or run all tests via CTest
ctest --verbose
```

## Test Coverage

The test suite covers:

1. **Basic Types** - POD types and simple data
2. **Containers** - Vector, Map, Set operations
3. **Strings** - XString handling and persistence
4. **Nested Structures** - Complex object hierarchies
5. **Memory Management** - Buffer growth and compaction
6. **Persistence** - Serialization and deserialization
7. **Data Integrity** - Verification after operations

## Expected Output

Each test should output:
```
[TEST] Test Name
--------------------------------------------------
Test 1: Description... [OK]
Test 2: Description... [OK]
...
[PASS] All tests passed!
```

## Troubleshooting

### Assertion Failures
If a test fails with an assertion error, it indicates a data integrity issue. Check:
- Memory buffer size
- Allocator usage
- Data persistence logic

### Exceptions
Common exceptions:
- `interprocess_exception`: Memory allocation or initialization error
- `bad_alloc`: Insufficient memory
- Check buffer size and growth settings

### Build Errors
If tests don't compile:
- Ensure `xoffsetdatastructure2.hpp` is in the parent directory
- Check Boost library paths
- Verify C++17 or later compiler support

---

## 🆕 C++26 Reflection Tests (NEW!)

### Purpose
Test C++26 reflection features (P2996) with XOffsetDatastructure2.

### Quick Start

**Run all reflection tests (Linux/macOS):**
```bash
cd tests
chmod +x run_reflection_tests.sh
./run_reflection_tests.sh
```

**Run all reflection tests (Windows):**
```cmd
cd tests
run_reflection_tests.bat
```

### Reflection Test Files

#### 🔴 High Priority (Core Features)

**7. test_reflection_operators.cpp** (5 tests)
- Test `^^` reflection operator
- Test `[::]` splice operator
- Type and member reflection
- Built-in and container types

**8. test_member_iteration.cpp** (6 tests)
- `nonstatic_data_members_of()` function
- Member filtering and iteration
- Member properties (`is_public()`, `is_static_member()`)

**9. test_reflection_type_signature.cpp** (6 tests)
- Integration with `XTypeSignature`
- Compile-time type validation
- Serialization with reflection

#### 🟡 Medium Priority (Utility Features)

**10. test_splice_operations.cpp** (6 tests)
- Direct member splice
- Member pointer splice
- Type splice and expressions

**11. test_type_introspection.cpp** (7 tests)
- Type name queries (`display_string_of()`)
- Member type analysis (`type_of()`)
- Type comparison and validation

**12. test_reflection_compaction.cpp** (5 tests)
- Reflection-assisted memory analysis
- Structure analysis with reflection
- Data integrity validation

#### 🟢 Low Priority (Advanced Applications)

**13. test_reflection_serialization.cpp** (6 tests)
- Auto-generated structure documentation
- Member listing and analysis
- Version compatibility checks

**14. test_reflection_comparison.cpp** (7 tests)
- Compile-time member counting
- Structure equality checking
- Version compatibility validation

### Reflection Test Statistics

- **Total Test Files**: 8
- **Total Individual Tests**: 48
- **P2996 Features Covered**: All core APIs

### Running Individual Reflection Tests

```bash
# Linux/macOS
./build/tests/test_reflection_operators
./build/tests/test_member_iteration
# ... etc

# Windows
.\build\tests\Release\test_reflection_operators.exe
.\build\tests\Release\test_member_iteration.exe
REM ... etc
```

### Reflection Test Requirements

**Compiler**: Clang with P2996 support  
**Standard**: `-std=c++26`  
**Flags**: `-freflection`  
**Header**: `<experimental/meta>`

### Test Behavior

**If reflection is supported:**
- Tests run and show detailed output
- All tests should pass

**If reflection is NOT supported:**
- Tests automatically skip with `[SKIP]` message
- Build still succeeds (optional feature)

### Reflection Documentation

See these files for more information:
- **REFLECTION_QUICKSTART.md** - Quick start guide
- **REFLECTION_TESTS_SUMMARY.md** - Detailed test summary
- **REFLECTION_TEST_RECOMMENDATIONS.md** - Full recommendations

---

## Contributing

To add new tests:
1. Create `test_feature.cpp` in this directory
2. Follow the existing test structure
3. Add to CMakeLists.txt
4. Document in this README
