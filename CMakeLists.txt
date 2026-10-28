cmake_minimum_required(VERSION 3.13)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/XOffsetVersion.cmake")
project(XOffsetDatastructure VERSION ${XOFFSET_VERSION_NUMERIC} LANGUAGES CXX)

# A dependency should contribute interface targets, not its developer programs.
set(XOFFSET_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(XOFFSET_TOP_LEVEL ON)
endif()
option(XOFFSET_STORAGE_ONLY "Configure only the independent C++17 OS layer" OFF)
option(XOFFSET_BUILD_TESTS "Build and register XOffset tests" ${XOFFSET_TOP_LEVEL})
option(XOFFSET_BUILD_TOOLS "Build XOffset signature and benchmark tools" ${XOFFSET_TOP_LEVEL})
option(XOFFSET_BUILD_EXAMPLES "Build XOffset examples" ${XOFFSET_TOP_LEVEL})
option(XOFFSET_BUILD_DEMOS "Build the map state demo (demo/mapstate)" OFF)
option(XOFFSET_INSTALL "Generate XOffset install/export rules" ${XOFFSET_TOP_LEVEL})
option(XOFFSET_BUILD_FUZZERS "Build libFuzzer targets (needs clang -fsanitize=fuzzer)" OFF)
option(XOFFSET_FUZZ_INJECT_CRASH "Fuzz drill builds only: inputs starting with XOFFSET-INJECT crash the fuzz harnesses" OFF)
# 26: P2996 reflection describes every record (no boilerplate). 20 and 23: one
# XOFFSET_RECORD line per record; no <meta>, no -freflection, no TypeLayout.
set(XOFFSET_CXX_STANDARD 26 CACHE STRING "C++ standard of the full library targets: 20, 23 or 26")
set_property(CACHE XOFFSET_CXX_STANDARD PROPERTY STRINGS 20 23 26)
# describe: XOFFSET_RECORD field lists under every standard, also C++26 (the
# same-compiler control M26D); auto: reflection whenever the compiler has it.
set(XOFFSET_SCHEMA_BACKEND auto CACHE STRING "Schema backend of the full library: auto or describe")
set_property(CACHE XOFFSET_SCHEMA_BACKEND PROPERTY STRINGS auto describe)

if(XOFFSET_BUILD_TESTS)
    enable_testing()
endif()
# MinGW: libstdc++ as a DLL keeps its own operator new, so std::pmr and other
# library-internal allocations bypass a test's replacement operator new. Link
# the runtime statically so replacements see every allocation (project-local;
# not part of the exported interface).
if(MINGW AND XOFFSET_TOP_LEVEL)
    add_link_options(-static)
endif()
# MSVC and clang-cl: exception handling (/EHsc) and the release configurations'
# NDEBUG per target instead of in CMAKE_CXX_FLAGS[_<CONFIG>], so
# xoffset_disable_exceptions() (/EHs-c-) and xoffset_enable_test_assertions()
# (/UNDEBUG) do not draw cl's D9025 "overriding '/EHsc' with '/EHs-c-'".
if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    string(REGEX REPLACE "(^| )/EHsc( |$)" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_compile_options("$<$<NOT:$<BOOL:$<TARGET_PROPERTY:XOFFSET_NO_EXCEPTIONS>>>:/EHsc>")
    foreach(config RELEASE RELWITHDEBINFO MINSIZEREL)
        string(REGEX REPLACE "(^| )[/-]DNDEBUG( |$)" " " CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_${config}}")
    endforeach()
    add_compile_definitions("$<$<AND:$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>,$<NOT:$<BOOL:$<TARGET_PROPERTY:XOFFSET_TEST_ASSERTIONS>>>>:NDEBUG>")
endif()
# MSVC reads sources in the system code page unless told otherwise; the tests
# and tools are UTF-8 (a non-ASCII literal broke the build on code page 936,
# 3.2.0-rc.1 on Windows). Project-local: the public headers are ASCII and the
# exported interface does not impose a source charset on consumers.
if(MSVC)
    add_compile_options(/utf-8)
endif()
include(cmake/XOffsetBuildChecks.cmake)
include(cmake/XOffsetTestInventory.cmake)
include(cmake/XOffsetTestHost.cmake)
if(XOFFSET_TEST_HOST)
    # the hosted program libraries link the programs' static libraries (demo)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
find_package(Threads REQUIRED)

add_library(xoffset_storage INTERFACE)
add_library(XOffset::storage ALIAS xoffset_storage)
set_target_properties(xoffset_storage PROPERTIES EXPORT_NAME storage)
target_compile_features(xoffset_storage INTERFACE cxx_std_17)
target_link_libraries(xoffset_storage INTERFACE Threads::Threads)
target_include_directories(xoffset_storage INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>)

# This branch precedes compiler-family checks, reflection flags and dependencies.
if(NOT XOFFSET_STORAGE_ONLY)
    xoffset_check_full_toolchain()
    get_filename_component(CLANG_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(CLANG_INSTALL_DIR "${CLANG_BIN_DIR}" DIRECTORY)

    # Build-tree convenience only. Installed consumers locate their toolchain
    # explicitly; these directories are never exported.
    # Apple clang builds use the system libc++, whose std::pmr symbols exist from
    # macOS 14; an older target links but fails at load. project() has already made
    # CMAKE_OSX_DEPLOYMENT_TARGET an empty cache entry, so the default needs FORCE.
    # The C++26 build keeps the compiler's default (P2996 Clang ships its own libc++).
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET
       AND NOT XOFFSET_CXX_STANDARD STREQUAL "26")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS deployment version" FORCE)
    endif()
    include(cmake/XOffsetDependencies.cmake)
    xoffset_validate_dependencies()
    add_library(xoffset_common INTERFACE)
    add_library(XOffset::xoffset ALIAS xoffset_common)
    set_target_properties(xoffset_common PROPERTIES EXPORT_NAME xoffset)
    xoffset_configure_full_interface(xoffset_common)

    # An imported interface transfers the real settings to try_compile().
    add_library(xoffset_probe_settings INTERFACE IMPORTED)
    xoffset_configure_full_interface(xoffset_probe_settings)
    if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        # try_compile() projects do not get this directory's /EHsc (see above).
        set_property(TARGET xoffset_probe_settings APPEND PROPERTY INTERFACE_COMPILE_OPTIONS /EHsc)
    endif()
    if(XOFFSET_CXX_STANDARD STREQUAL "26")
        xoffset_require_reflection()
    else()
        xoffset_require_field_lists()
    endif()

    function(configure_xoffset_target target_name)
        target_link_libraries(${target_name} PRIVATE xoffset_common)
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/$<CONFIG>)
    endfunction()

    if(XOFFSET_BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif()
    if(XOFFSET_BUILD_TOOLS)
        add_subdirectory(tools)
    endif()
    if(XOFFSET_BUILD_DEMOS)
        add_subdirectory(demo/mapstate)
    endif()
else()
    message(STATUS "Configuring standalone C++17 OS storage layer")
endif()

include(cmake/XOffsetFourPathHeaders.cmake)
include(cmake/XOffsetOptionalRuntimes.cmake)

if(XOFFSET_BUILD_TESTS)
    add_subdirectory(tests)
endif()
if((XOFFSET_TEST_HOST OR XOFFSET_PHONE_PROGRAMS) AND NOT XOFFSET_STORAGE_ONLY)
    add_subdirectory(tools/test_host)
endif()
if(XOFFSET_INSTALL)
    include(cmake/XOffsetInstall.cmake)
endif()
