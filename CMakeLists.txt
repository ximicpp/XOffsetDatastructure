cmake_minimum_required(VERSION 3.13)
project(XOffsetDatastructure)

enable_testing()

# This project requires C++26 with reflection support
# Note: we still set the language mode manually because reflection flags are custom.
message(STATUS "Using C++26 with reflection")

# Verify we're using Clang with P2996 support
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "This project requires Clang with P2996 reflection support")
endif()

# Get the clang install directory
get_filename_component(CLANG_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
get_filename_component(CLANG_INSTALL_DIR ${CLANG_BIN_DIR} DIRECTORY)

message(STATUS "Added -freflection flag")
message(STATUS "Clang install: ${CLANG_INSTALL_DIR}")

# macOS specific settings
if(APPLE)
    # Ensure we use the correct deployment target
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version" FORCE)
    endif()
endif()

# Set TypeLayout include directories
set(TYPELAYOUT_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/external/typelayout/include
)

# Active Boost header surface used by this repository. The vendored superproject
# is larger, but the build only exposes the subset required by xoffsetdatastructure.hpp.
set(BOOST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/external/boost/libs/container/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/core/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/config/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/throw_exception/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/static_assert/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/container_hash/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/type_traits/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/winapi/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/intrusive/include
    ${CMAKE_SOURCE_DIR}/external/boost/libs/predef/include
)

add_library(xoffset_common INTERFACE)
target_compile_options(xoffset_common INTERFACE
    -std=c++26
    -freflection
    -fexpansion-statements
    -stdlib=libc++)
target_link_options(xoffset_common INTERFACE
    -stdlib=libc++)
target_include_directories(xoffset_common SYSTEM INTERFACE
    ${BOOST_INCLUDE_DIRS}
    ${TYPELAYOUT_INCLUDE_DIRS}
    ${CLANG_INSTALL_DIR}/include/c++/v1)
target_include_directories(xoffset_common INTERFACE
    ${CMAKE_SOURCE_DIR})
target_link_directories(xoffset_common INTERFACE
    ${CLANG_INSTALL_DIR}/lib)

# Helper: configure a target with shared deps and output dir.
function(configure_xoffset_target target_name)
    target_link_libraries(${target_name} PRIVATE xoffset_common)
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
endfunction()

add_subdirectory(examples)
add_subdirectory(tools)
add_subdirectory(tests)
