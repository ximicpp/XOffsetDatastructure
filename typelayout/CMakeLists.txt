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