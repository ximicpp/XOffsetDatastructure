function(add_xoffset_test test_name test_label)
    add_executable(${test_name} ${test_name}.cpp)
    configure_xoffset_target(${test_name})
    add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_name}>)
    set_tests_properties(${test_name} PROPERTIES LABELS "${test_label}")
endfunction()

# Basic tests (no reflection required)
set(BASIC_TESTS
    test_basic_types
    test_vector
    test_map_set
    test_nested
    test_compaction
    test_modify
    test_xbuffer_api
)

foreach(test_name ${BASIC_TESTS})
    add_xoffset_test(${test_name} basic)
endforeach()

# Reflection tests (require P2996 Clang)
set(REFLECTION_TESTS
    test_reflection_core
    test_reflection_advanced
    test_type_signatures
    test_field_limit_fix
    test_type_safety
    test_wire_admission
    test_enum_support
    test_xstring_direct_assign
    test_xhandle
    test_schema_envelope
    test_fixed_layout_containers
    test_error_paths
    test_memory_efficiency
    test_zero_boilerplate
    test_complex_nesting
    test_inheritance
    test_adaptive_reservation
    test_main_containers
)

foreach(test_name ${REFLECTION_TESTS})
    add_xoffset_test(${test_name} reflection)
endforeach()

# Negative compile test — should NOT compile (validates static_assert)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_polymorphic_rejection.cpp")
    try_compile(POLYMORPHIC_COMPILES
        ${CMAKE_BINARY_DIR}/try_compile_polymorphic
        SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_polymorphic_rejection.cpp
        CMAKE_FLAGS
            "-DINCLUDE_DIRECTORIES=${BOOST_INCLUDE_DIRS};${TYPELAYOUT_INCLUDE_DIRS};${CMAKE_SOURCE_DIR}"
        COMPILE_DEFINITIONS -std=c++26 -freflection -fexpansion-statements -stdlib=libc++
    )
    if(NOT POLYMORPHIC_COMPILES)
        message(STATUS "✓ test_polymorphic_rejection correctly fails to compile")
    else()
        message(WARNING "✗ test_polymorphic_rejection compiled — static_assert may be broken!")
    endif()
endif()

# Summary
list(LENGTH BASIC_TESTS BASIC_COUNT)
list(LENGTH REFLECTION_TESTS REFLECTION_COUNT)
math(EXPR TOTAL_COUNT "${BASIC_COUNT} + ${REFLECTION_COUNT}")
message(STATUS "Tests: ${BASIC_COUNT} basic + ${REFLECTION_COUNT} reflection = ${TOTAL_COUNT} total")
