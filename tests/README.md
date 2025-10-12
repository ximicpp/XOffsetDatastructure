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

## Contributing

To add new tests:
1. Create `test_feature.cpp` in this directory
2. Follow the existing test structure
3. Add to CMakeLists.txt
4. Document in this README
