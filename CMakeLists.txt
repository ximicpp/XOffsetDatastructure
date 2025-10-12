cmake_minimum_required(VERSION 3.10)
project(XOffsetDatastructure)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Boost include directories
set(BOOST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/external/boost
    ${CMAKE_SOURCE_DIR}/external/boost/libs/any/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/container/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/interprocess/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/type_index/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/core/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/config/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/throw_exception/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/assert/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/static_assert/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/container_hash/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/pfr/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/move/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/type_traits/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/winapi/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/intrusive/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/predef/include
)

# Add examples subdirectory
add_subdirectory(examples)

# Enable testing
enable_testing()

# Test executables
add_executable(test_basic_types tests/test_basic_types.cpp)
target_include_directories(test_basic_types PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_basic_types PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)

add_executable(test_vector tests/test_vector.cpp)
target_include_directories(test_vector PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_vector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)

add_executable(test_map_set tests/test_map_set.cpp)
target_include_directories(test_map_set PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_map_set PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)

add_executable(test_nested tests/test_nested.cpp)
target_include_directories(test_nested PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_nested PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)

add_executable(test_compaction tests/test_compaction.cpp)
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
