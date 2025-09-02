cmake_minimum_required(VERSION 3.10)
project(XOffsetDatastructure)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set Boost include directory
set(BOOST_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/boost_1_81_0)

# Add examples subdirectory
add_subdirectory(examples)