cmake_minimum_required(VERSION 3.10)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Get Boost include directories from parent
set(BOOST_INCLUDE_DIRS ${BOOST_INCLUDE_DIRS} PARENT_SCOPE)

# Test executables
add_executable(test_basic_types test_basic_types.cpp)
target_include_directories(test_basic_types PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_basic_types PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)

add_executable(test_vector test_vector.cpp)
target_include_directories(test_vector PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_vector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)

add_executable(test_map_set test_map_set.cpp)
target_include_directories(test_map_set PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_map_set PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)

add_executable(test_nested test_nested.cpp)
target_include_directories(test_nested PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_nested PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)

add_executable(test_compaction test_compaction.cpp)
target_include_directories(test_compaction PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_compaction PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)

# Add CTest tests
add_test(NAME BasicTypes COMMAND test_basic_types)
add_test(NAME VectorOps COMMAND test_vector)
add_test(NAME MapSetOps COMMAND test_map_set)
add_test(NAME NestedStructures COMMAND test_nested)
add_test(NAME MemoryCompaction COMMAND test_compaction)
