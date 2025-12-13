cmake_minimum_required(VERSION 3.20)

project(typelayout
    VERSION 1.0.0
    DESCRIPTION "C++26 Compile-Time Type Layout Signature Generator"
    LANGUAGES CXX
)

# Require C++26 for static reflection support
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific settings for Clang P2996
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Add reflection flags for Bloomberg Clang P2996 fork
    add_compile_options(
        -freflection
        -freflection-latest
        -Wall
        -Wextra
    )
    
    # Note: -stdlib=libc++ and linker flags are passed via command line
    # from build_and_run.sh to ensure correct library paths
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC (when reflection support becomes available)
    add_compile_options(
        -Wall
        -Wextra
    )
    message(WARNING "GCC may not fully support C++26 reflection yet")
else()
    message(WARNING "Compiler ${CMAKE_CXX_COMPILER_ID} may not support C++26 reflection")
endif()

# Header-only library
add_library(typelayout INTERFACE)
target_include_directories(typelayout INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Demo executable
add_executable(typelayout_demo demo/demo.cpp)
target_link_libraries(typelayout_demo PRIVATE typelayout)

# Test executable (compile-time static_assert tests)
add_executable(typelayout_test test/test_all_types.cpp)
target_link_libraries(typelayout_test PRIVATE typelayout)

# XOffsetDatastructure verification demo (requires Boost)
option(TYPELAYOUT_BUILD_XOFFSET_DEMO "Build XOffsetDatastructure verification demo" ON)
if(TYPELAYOUT_BUILD_XOFFSET_DEMO)
    # Use local Boost from external directory
    set(LOCAL_BOOST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../external/boost")
    if(EXISTS "${LOCAL_BOOST_PATH}")
        # Boost include directories (same as main project)
        set(BOOST_INCLUDE_DIRS
            ${LOCAL_BOOST_PATH}
            ${LOCAL_BOOST_PATH}/libs/any/include
            ${LOCAL_BOOST_PATH}/libs/container/include
            ${LOCAL_BOOST_PATH}/libs/interprocess/include
            ${LOCAL_BOOST_PATH}/libs/type_index/include
            ${LOCAL_BOOST_PATH}/libs/core/include
            ${LOCAL_BOOST_PATH}/libs/config/include
            ${LOCAL_BOOST_PATH}/libs/throw_exception/include
            ${LOCAL_BOOST_PATH}/libs/assert/include
            ${LOCAL_BOOST_PATH}/libs/static_assert/include
            ${LOCAL_BOOST_PATH}/libs/container_hash/include
            ${LOCAL_BOOST_PATH}/libs/pfr/include
            ${LOCAL_BOOST_PATH}/libs/move/include
            ${LOCAL_BOOST_PATH}/libs/type_traits/include
            ${LOCAL_BOOST_PATH}/libs/winapi/include
            ${LOCAL_BOOST_PATH}/libs/intrusive/include
            ${LOCAL_BOOST_PATH}/libs/predef/include
        )
        
        add_executable(typelayout_xoffset_demo demo/xoffset_demo.cpp)
        target_link_libraries(typelayout_xoffset_demo PRIVATE typelayout)
        target_include_directories(typelayout_xoffset_demo PRIVATE 
            ${CMAKE_CURRENT_SOURCE_DIR}/..
            ${BOOST_INCLUDE_DIRS}
        )
        message(STATUS "  XOffset Demo: Enabled (using local Boost)")
    else()
        # Fallback to system Boost
        find_package(Boost 1.74 QUIET)
        if(Boost_FOUND)
            add_executable(typelayout_xoffset_demo demo/xoffset_demo.cpp)
            target_link_libraries(typelayout_xoffset_demo PRIVATE typelayout)
            target_include_directories(typelayout_xoffset_demo PRIVATE 
                ${CMAKE_CURRENT_SOURCE_DIR}/..
                ${Boost_INCLUDE_DIRS}
            )
            message(STATUS "  XOffset Demo: Enabled (Boost ${Boost_VERSION} found)")
        else()
            message(STATUS "  XOffset Demo: Disabled (Boost not found)")
        endif()
    endif()
endif()

# Installation rules
include(GNUInstallDirs)

install(TARGETS typelayout
    EXPORT typelayoutTargets
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT typelayoutTargets
    FILE typelayoutTargets.cmake
    NAMESPACE typelayout::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/typelayout
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== TypeLayout Configuration ===")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  C++ Compiler Path: ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  CXX Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "")