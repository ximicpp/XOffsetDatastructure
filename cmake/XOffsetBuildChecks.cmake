include_guard(GLOBAL)

function(xoffset_enable_test_assertions target_name)
    if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        target_compile_options(${target_name} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${target_name} PRIVATE -UNDEBUG)
    endif()
endfunction()

function(xoffset_configure_full_interface target_name)
    target_link_libraries(${target_name} INTERFACE Threads::Threads)
    target_compile_features(${target_name} INTERFACE cxx_std_26)
    target_compile_options(${target_name} INTERFACE
        -std=c++26 -freflection -fexpansion-statements -stdlib=libc++)
    target_link_options(${target_name} INTERFACE -stdlib=libc++)
    foreach(include_dir IN LISTS BOOST_INCLUDE_DIRS TYPELAYOUT_INCLUDE_DIRS)
        target_include_directories(${target_name} SYSTEM INTERFACE
            "$<BUILD_INTERFACE:${include_dir}>")
    endforeach()
    # SDK-based Clang installations do not necessarily keep libc++ here.
    # Let the actual compile/link probe report unsupported reflection flags.
    if(IS_DIRECTORY "${CLANG_INSTALL_DIR}/include/c++/v1")
        target_include_directories(${target_name} SYSTEM INTERFACE
            "$<BUILD_INTERFACE:${CLANG_INSTALL_DIR}/include/c++/v1>")
    endif()
    target_include_directories(${target_name} INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>")
    target_link_directories(${target_name} INTERFACE "$<BUILD_INTERFACE:${CLANG_INSTALL_DIR}/lib>")
endfunction()

function(xoffset_compile_check check_name source_file result_variable output_variable)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${source_file}" "${PROJECT_SOURCE_DIR}/xoffsetdatastructure.hpp"
        "${PROJECT_SOURCE_DIR}/xoffset_virtual_memory.hpp")
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
    if(NOT negative_output MATCHES "static assertion failed[^\n]*Type is not byte-copy safe")
        message(FATAL_ERROR
            "Polymorphic rejection failed for an unexpected reason; required diagnostic "
            "'Type is not byte-copy safe' was not emitted by a static assertion.\n"
            "See ${PROJECT_BINARY_DIR}/compile-checks/polymorphic_rejection.log\n${negative_output}")
    endif()
    set_property(GLOBAL APPEND PROPERTY XOFFSET_CONFIGURE_CHECKS
        polymorphic_positive_compile_link polymorphic_expected_rejection)
    message(STATUS "Polymorphic positive control passed; expected rejection diagnostic verified")
endfunction()
