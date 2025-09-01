cmake_minimum_required(VERSION 3.10)

# Set project name
project(XOffsetDatastructure)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add include directories
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/xoffsetdatastructure)
include_directories(${CMAKE_SOURCE_DIR}/external/boost)

# Add executable target
add_executable(xdemo demo/xdemo.cpp)

# Set output directory
set_target_properties(xdemo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)