cmake_minimum_required(VERSION 3.10)
project(XOffsetDatastructure)

# This project requires C++26 with reflection support
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
message(STATUS "Using C++26 with reflection")

# Verify we're using Clang with P2996 support
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "This project requires Clang with P2996 reflection support")
endif()

# Add reflection flag and standard library
add_compile_options(-freflection -fexpansion-statements -stdlib=libc++)
add_link_options(-stdlib=libc++)

# Get the clang install directory
get_filename_component(CLANG_BIN_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
get_filename_component(CLANG_INSTALL_DIR ${CLANG_BIN_DIR} DIRECTORY)
include_directories(SYSTEM ${CLANG_INSTALL_DIR}/include/c++/v1)
link_directories(${CLANG_INSTALL_DIR}/lib)

message(STATUS "Added -freflection flag")
message(STATUS "Clang install: ${CLANG_INSTALL_DIR}")

# macOS specific settings
if(APPLE)
    # Use the system C++ library
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    
    # Set RPATH settings globally
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "@executable_path;@loader_path")
    
    # Ensure we use the correct deployment target
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment version" FORCE)
    endif()
endif()

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

# Add tests subdirectory
add_subdirectory(tests)
