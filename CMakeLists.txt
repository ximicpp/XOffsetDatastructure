cmake_minimum_required(VERSION 3.10)
project(XOffsetDatastructure)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
