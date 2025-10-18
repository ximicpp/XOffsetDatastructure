# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable testing
enable_testing()

# Helper function for macOS RPATH settings
function(configure_macos_target target_name)
    if(APPLE)
        set_target_properties(${target_name} PROPERTIES
            SKIP_BUILD_RPATH TRUE
            BUILD_WITH_INSTALL_RPATH FALSE
            INSTALL_RPATH ""
        )
        target_link_options(${target_name} PRIVATE
            -Wl,-rpath,/usr/lib
            -Wl,-rpath,/usr/local/lib
        )
    endif()
endfunction()

# Test executables
add_executable(test_basic_types test_basic_types.cpp)
target_include_directories(test_basic_types PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_basic_types PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_basic_types)

add_executable(test_vector test_vector.cpp)
target_include_directories(test_vector PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_vector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_vector)

add_executable(test_map_set test_map_set.cpp)
target_include_directories(test_map_set PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_map_set PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_map_set)

add_executable(test_nested test_nested.cpp)
target_include_directories(test_nested PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_nested PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_nested)

add_executable(test_compaction test_compaction.cpp)
target_include_directories(test_compaction PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_compaction PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_compaction)

add_executable(test_modify test_modify.cpp)
target_include_directories(test_modify PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_modify PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_modify)

add_executable(test_comprehensive test_comprehensive.cpp)
target_include_directories(test_comprehensive PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_comprehensive PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_comprehensive)

add_executable(test_serialization test_serialization.cpp)
target_include_directories(test_serialization PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_serialization PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_serialization)

add_executable(test_alignment test_alignment.cpp)
target_include_directories(test_alignment PRIVATE ${BOOST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_alignment PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_alignment)

# Add CTest tests
add_test(NAME BasicTypes COMMAND test_basic_types)
add_test(NAME VectorOps COMMAND test_vector)
add_test(NAME MapSetOps COMMAND test_map_set)
add_test(NAME NestedStructures COMMAND test_nested)
add_test(NAME MemoryStats COMMAND test_compaction)
add_test(NAME DataModification COMMAND test_modify)
add_test(NAME Comprehensive COMMAND test_comprehensive)
add_test(NAME Serialization COMMAND test_serialization)
add_test(NAME Alignment COMMAND test_alignment)
