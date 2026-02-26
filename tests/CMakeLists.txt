# C++ standard is inherited from parent CMakeLists.txt (C++26)

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
target_include_directories(test_basic_types PRIVATE ${BOOST_INCLUDE_DIRS} ${TYPELAYOUT_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_basic_types PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_basic_types)

add_executable(test_vector test_vector.cpp)
target_include_directories(test_vector PRIVATE ${BOOST_INCLUDE_DIRS} ${TYPELAYOUT_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_vector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_vector)

add_executable(test_map_set test_map_set.cpp)
target_include_directories(test_map_set PRIVATE ${BOOST_INCLUDE_DIRS} ${TYPELAYOUT_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_map_set PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_map_set)

add_executable(test_nested test_nested.cpp)
target_include_directories(test_nested PRIVATE ${BOOST_INCLUDE_DIRS} ${TYPELAYOUT_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_nested PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_nested)

add_executable(test_compaction test_compaction.cpp)
target_include_directories(test_compaction PRIVATE ${BOOST_INCLUDE_DIRS} ${TYPELAYOUT_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_compaction PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_compaction)

add_executable(test_modify test_modify.cpp)
target_include_directories(test_modify PRIVATE ${BOOST_INCLUDE_DIRS} ${TYPELAYOUT_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_modify PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_modify)

add_executable(test_xbuffer_api test_xbuffer_api.cpp)
target_include_directories(test_xbuffer_api PRIVATE ${BOOST_INCLUDE_DIRS} ${TYPELAYOUT_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
set_target_properties(test_xbuffer_api PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
)
configure_macos_target(test_xbuffer_api)

# Add CTest tests
add_test(NAME BasicTypes COMMAND test_basic_types)
add_test(NAME VectorOps COMMAND test_vector)
add_test(NAME MapSetOps COMMAND test_map_set)
add_test(NAME NestedStructures COMMAND test_nested)
add_test(NAME MemoryCompaction COMMAND test_compaction)
add_test(NAME DataModification COMMAND test_modify)
add_test(NAME XBufferAPI COMMAND test_xbuffer_api)

# ============================================================================
# C++26 Reflection Tests
# ============================================================================

message(STATUS "Adding C++26 reflection tests...")

# List of reflection test files
set(REFLECTION_TESTS
    test_reflection_operators
    test_member_iteration
    test_reflection_type_signature
    test_splice_operations
    test_type_introspection
    test_reflection_compaction
    test_reflection_serialization
    test_reflection_comparison
    test_field_limit_fix
    test_class_type_signatures
    test_type_safety
    test_type_safety_comprehensive
    test_vptr_layout
    test_type_erased_detection
    test_typelayout_integration
    test_enum_support
    test_xstring_direct_assign
    test_xhandle
    test_error_paths
    test_memory_efficiency
    test_zero_boilerplate
    test_zero_boilerplate_vector
    test_complex_nesting
    test_inheritance
    test_adaptive_reservation
    test_classify_safety
)

# Count for summary
list(LENGTH REFLECTION_TESTS REFLECTION_TEST_COUNT)
math(EXPR TOTAL_TEST_COUNT "7 + ${REFLECTION_TEST_COUNT}")

# Add each reflection test
foreach(test_name ${REFLECTION_TESTS})
    add_executable(${test_name} ${test_name}.cpp)
    target_include_directories(${test_name} PRIVATE ${BOOST_INCLUDE_DIRS} ${TYPELAYOUT_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR})
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
    )
    configure_macos_target(${test_name})
    
    # Add to CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()

message(STATUS "Added ${CMAKE_CURRENT_LIST_DIR} reflection tests")

# ============================================================================
# Negative Compile Test (expected to FAIL compilation)
# ============================================================================
# test_polymorphic_rejection.cpp should NOT compile — it validates that
# static_assert fires for polymorphic types.  We do NOT add it as a normal
# executable; instead we use try_compile so a successful build still passes.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_polymorphic_rejection.cpp")
    try_compile(POLYMORPHIC_COMPILES
        ${CMAKE_BINARY_DIR}/try_compile_polymorphic
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_polymorphic_rejection.cpp
        CMAKE_FLAGS
            "-DINCLUDE_DIRECTORIES=${BOOST_INCLUDE_DIRS};${TYPELAYOUT_INCLUDE_DIRS};${CMAKE_SOURCE_DIR}"
        COMPILE_DEFINITIONS -std=c++26 -freflection -fexpansion-statements -stdlib=libc++
    )
    if(NOT POLYMORPHIC_COMPILES)
        message(STATUS "✓ test_polymorphic_rejection correctly fails to compile (static_assert works)")
    else()
        message(WARNING "✗ test_polymorphic_rejection compiled successfully — static_assert may be broken!")
    endif()
endif()

# Display configuration summary
message(STATUS "")
message(STATUS "========================================")
message(STATUS "  XOffsetDatastructure Test Summary")
message(STATUS "========================================")
message(STATUS "C++ Standard: C++26")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Basic Tests: 7 tests (including XBufferAPI)")
message(STATUS "Reflection Tests: ${REFLECTION_TEST_COUNT} tests")
message(STATUS "Total Tests: ${TOTAL_TEST_COUNT} tests")
message(STATUS "Negative Compile Tests: 1 (polymorphic_rejection)")
message(STATUS "Reflection: ENABLED (C++26 required)")
message(STATUS "========================================")
message(STATUS "")
