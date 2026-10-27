include_guard(GLOBAL)

function(xoffset_enable_test_assertions target_name)
    if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        target_compile_options(${target_name} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${target_name} PRIVATE -UNDEBUG)
    endif()
endfunction()

# Oldest compilers the C++20/C++23 builds accept. They are the planned floor
# (design: GCC 12, Clang 16, Apple clang 15, MSVC 19.38); the default
# constant-evaluation limits measured on GCC 12/13 and Clang 16/18 decide it.
set(XOFFSET_FIELD_LIST_MIN_GNU 12)
set(XOFFSET_FIELD_LIST_MIN_CLANG 16)
set(XOFFSET_FIELD_LIST_MIN_APPLECLANG 15)
set(XOFFSET_FIELD_LIST_MIN_MSVC 19.38)

function(xoffset_check_full_toolchain)
    if(NOT XOFFSET_CXX_STANDARD MATCHES "^(20|23|26)$")
        message(FATAL_ERROR "XOFFSET_CXX_STANDARD must be 20, 23 or 26 (got '${XOFFSET_CXX_STANDARD}')")
    endif()
    if(NOT XOFFSET_SCHEMA_BACKEND MATCHES "^(auto|describe)$")
        message(FATAL_ERROR "XOFFSET_SCHEMA_BACKEND must be auto or describe (got '${XOFFSET_SCHEMA_BACKEND}')")
    endif()
    if(XOFFSET_CXX_STANDARD STREQUAL "26")
        if(CMAKE_VERSION VERSION_LESS 3.30)
            message(FATAL_ERROR "The full XOffset C++26 target requires CMake 3.30 or newer")
        endif()
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
                message(FATAL_ERROR "The full XOffset target requires GCC 16 or newer for P2996 reflection")
            endif()
        elseif(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            message(FATAL_ERROR "This project requires GCC 16+ or Clang with P2996 reflection support")
        endif()
        message(STATUS "Using C++26 with reflection (schema backend: ${XOFFSET_SCHEMA_BACKEND})")
        return()
    endif()
    # cxx_std_23 is known from CMake 3.20.
    if(CMAKE_VERSION VERSION_LESS 3.20)
        message(FATAL_ERROR "The full XOffset C++${XOFFSET_CXX_STANDARD} target requires CMake 3.20 or newer")
    endif()
    set(id "${CMAKE_CXX_COMPILER_ID}")
    if(id STREQUAL "GNU" OR id STREQUAL "Clang" OR id STREQUAL "AppleClang" OR id STREQUAL "MSVC")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "${XOFFSET_FIELD_LIST_MIN_${id}}")
            message(FATAL_ERROR "The full XOffset C++${XOFFSET_CXX_STANDARD} target requires "
                "${id} ${XOFFSET_FIELD_LIST_MIN_${id}} or newer (found ${CMAKE_CXX_COMPILER_VERSION})")
        endif()
    else()
        message(FATAL_ERROR "The full XOffset C++${XOFFSET_CXX_STANDARD} target supports GCC, Clang, "
            "Apple clang and MSVC (found ${id})")
    endif()
    message(STATUS "Using C++${XOFFSET_CXX_STANDARD} with XOFFSET_RECORD field lists")
endfunction()

function(xoffset_configure_full_interface target_name)
    target_link_libraries(${target_name} INTERFACE Threads::Threads)
    if(XOFFSET_SCHEMA_BACKEND STREQUAL "describe")
        target_compile_definitions(${target_name} INTERFACE XOFFSET_SCHEMA_BACKEND_DESCRIBE=1)
    endif()
    if(NOT XOFFSET_CXX_STANDARD STREQUAL "26")
        # No reflection flags, no <meta> and no TypeLayout include path: a
        # header that still reaches for either fails to compile here.
        target_compile_features(${target_name} INTERFACE cxx_std_${XOFFSET_CXX_STANDARD})
        if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            # __VA_OPT__ in XOFFSET_RECORD needs the conforming preprocessor.
            target_compile_options(${target_name} INTERFACE /Zc:preprocessor /Zc:__cplusplus /permissive-)
        elseif(CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
            # clang-cl: its preprocessor already conforms (/Zc:preprocessor is unused).
            target_compile_options(${target_name} INTERFACE /Zc:__cplusplus /permissive-)
        else()
            target_compile_options(${target_name} INTERFACE -std=c++${XOFFSET_CXX_STANDARD})
        endif()
        foreach(include_dir IN LISTS BOOST_INCLUDE_DIRS)
            target_include_directories(${target_name} SYSTEM INTERFACE
                "$<BUILD_INTERFACE:${include_dir}>")
        endforeach()
        target_include_directories(${target_name} INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>")
        return()
    endif()
    target_compile_features(${target_name} INTERFACE cxx_std_26)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC 16+ implements P2996 and expansion statements with libstdc++.
        target_compile_options(${target_name} INTERFACE -std=c++26 -freflection)
    else()
        target_compile_options(${target_name} INTERFACE
            -std=c++26 -freflection -fexpansion-statements -stdlib=libc++)
        target_link_options(${target_name} INTERFACE -stdlib=libc++)
    endif()
    foreach(include_dir IN LISTS BOOST_INCLUDE_DIRS TYPELAYOUT_INCLUDE_DIRS)
        target_include_directories(${target_name} SYSTEM INTERFACE
            "$<BUILD_INTERFACE:${include_dir}>")
    endforeach()
    # SDK-based Clang installations do not necessarily keep libc++ here.
    # Let the actual compile/link probe report unsupported reflection flags.
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND IS_DIRECTORY "${CLANG_INSTALL_DIR}/include/c++/v1")
        target_include_directories(${target_name} SYSTEM INTERFACE
            "$<BUILD_INTERFACE:${CLANG_INSTALL_DIR}/include/c++/v1>")
    endif()
    target_include_directories(${target_name} INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>")
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_directories(${target_name} INTERFACE "$<BUILD_INTERFACE:${CLANG_INSTALL_DIR}/lib>")
    endif()
endfunction()

function(xoffset_compile_check check_name source_file result_variable output_variable)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${source_file}" "${PROJECT_SOURCE_DIR}/xoffsetdatastructure.hpp"
        "${PROJECT_SOURCE_DIR}/xoffset_virtual_memory.hpp"
        "${PROJECT_SOURCE_DIR}/xoffset_slot_storage.hpp")
    # A toolchain's STATIC_LIBRARY override would bypass the link check.
    set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
    # Do not reuse a cached result after the source, dependencies, or flags change.
    unset(XOFFSET_CHECK_COMPILES CACHE)
    try_compile(XOFFSET_CHECK_COMPILES
        "${PROJECT_BINARY_DIR}/compile-checks/${check_name}"
        SOURCES "${source_file}"
        LINK_LIBRARIES xoffset_probe_settings
        OUTPUT_VARIABLE check_output)
    file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/compile-checks")
    file(WRITE "${PROJECT_BINARY_DIR}/compile-checks/${check_name}.log" "${check_output}")
    set(${result_variable} "${XOFFSET_CHECK_COMPILES}" PARENT_SCOPE)
    set(${output_variable} "${check_output}" PARENT_SCOPE)
    unset(XOFFSET_CHECK_COMPILES CACHE)
endfunction()

function(xoffset_require_reflection)
    xoffset_compile_check(reflection
        "${PROJECT_SOURCE_DIR}/cmake/reflection_probe.cpp" compiles output)
    if(NOT compiles)
        message(FATAL_ERROR
            "Full-library reflection compile/link probe failed. Use a P2996 Clang "
            "toolchain with matching libc++, Boost and TypeLayout, or configure "
            "-DXOFFSET_STORAGE_ONLY=ON for the independent C++17 OS layer.\n"
            "See ${PROJECT_BINARY_DIR}/compile-checks/reflection.log\n${output}")
    endif()
    set_property(GLOBAL APPEND PROPERTY XOFFSET_CONFIGURE_CHECKS reflection_compile_link)
    message(STATUS "Full-library reflection compile/link probe passed")
endfunction()

function(xoffset_require_field_lists)
    xoffset_compile_check(field_lists
        "${PROJECT_SOURCE_DIR}/cmake/field_list_probe.cpp" compiles output)
    if(NOT compiles)
        message(FATAL_ERROR
            "Full-library C++${XOFFSET_CXX_STANDARD} compile/link probe failed: the headers "
            "must build without reflection, <meta> or TypeLayout, with records described "
            "by XOFFSET_RECORD.\n"
            "See ${PROJECT_BINARY_DIR}/compile-checks/field_lists.log\n${output}")
    endif()
    set_property(GLOBAL APPEND PROPERTY XOFFSET_CONFIGURE_CHECKS field_list_compile_link)
    message(STATUS "Full-library C++${XOFFSET_CXX_STANDARD} field-list compile/link probe passed")
endfunction()

function(xoffset_check_polymorphic_rejection)
    xoffset_compile_check(polymorphic_positive
        "${PROJECT_SOURCE_DIR}/tests/test_polymorphic_positive.cpp" positive_compiles positive_output)
    if(NOT positive_compiles)
        message(FATAL_ERROR
            "Polymorphic rejection positive control failed; a broken toolchain or "
            "missing dependency must not count as an expected rejection.\n"
            "See ${PROJECT_BINARY_DIR}/compile-checks/polymorphic_positive.log\n${positive_output}")
    endif()
    xoffset_compile_check(polymorphic_rejection
        "${PROJECT_SOURCE_DIR}/tests/test_polymorphic_rejection.cpp" negative_compiles negative_output)
    if(negative_compiles)
        message(FATAL_ERROR
            "Polymorphic rejection unexpectedly compiled and linked; "
            "the byte-copy safety guard is no longer enforced.")
    endif()
    if(NOT negative_output MATCHES "static(_assert| assertion) failed[^\n]*Type is not byte-copy safe")
        message(FATAL_ERROR
            "Polymorphic rejection failed for an unexpected reason; required diagnostic "
            "'Type is not byte-copy safe' was not emitted by a static assertion.\n"
            "See ${PROJECT_BINARY_DIR}/compile-checks/polymorphic_rejection.log\n${negative_output}")
    endif()
    set_property(GLOBAL APPEND PROPERTY XOFFSET_CONFIGURE_CHECKS
        polymorphic_positive_compile_link polymorphic_expected_rejection)
    message(STATUS "Polymorphic positive control passed; expected rejection diagnostic verified")
endfunction()
