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
option(XOFFSET_INSTALL "Generate XOffset install/export rules" ${XOFFSET_TOP_LEVEL})

if(XOFFSET_BUILD_TESTS)
    enable_testing()
endif()
include(cmake/XOffsetBuildChecks.cmake)
include(cmake/XOffsetTestInventory.cmake)
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
    if(CMAKE_VERSION VERSION_LESS 3.30)
        message(FATAL_ERROR "The full XOffset C++26 target requires CMake 3.30 or newer")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "This project requires Clang with P2996 reflection support")
    endif()
    message(STATUS "Using C++26 with reflection")
    get_filename_component(CLANG_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(CLANG_INSTALL_DIR "${CLANG_BIN_DIR}" DIRECTORY)

    # Build-tree convenience only. Installed consumers locate their toolchain
    # explicitly; these directories are never exported.
    if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version")
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
    xoffset_require_reflection()

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
else()
    message(STATUS "Configuring standalone C++17 OS storage layer")
endif()

include(cmake/XOffsetFourPathHeaders.cmake)
include(cmake/XOffsetOptionalRuntimes.cmake)

if(XOFFSET_BUILD_TESTS)
    add_subdirectory(tests)
endif()
if(XOFFSET_INSTALL)
    include(cmake/XOffsetInstall.cmake)
endif()
