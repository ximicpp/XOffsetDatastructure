cmake_minimum_required(VERSION 3.10)
project(XOffsetDatastructure)

# Set C++ standard (default to C++20, can be overridden)
if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to enable C++26 reflection tests
option(ENABLE_REFLECTION_TESTS "Enable C++26 reflection tests (requires Clang with P2996 support)" OFF)

# If reflection tests are enabled, use C++26
if(ENABLE_REFLECTION_TESTS)
    set(CMAKE_CXX_STANDARD 26)
    message(STATUS "Reflection tests enabled - using C++26")
    
    # Add reflection flag for Clang
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-freflection)
        message(STATUS "Added -freflection flag for Clang")
    else()
        message(WARNING "Reflection tests require Clang with P2996 support")
    endif()
endif()

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
