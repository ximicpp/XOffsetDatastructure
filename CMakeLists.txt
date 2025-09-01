cmake_minimum_required(VERSION 3.10)

# Set project name
project(XOffsetDatastructure)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add Boost as a subproject
add_subdirectory(external/boost EXCLUDE_FROM_ALL)

# Add include directories
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/xoffsetdatastructure)

# Add executable target
add_executable(xdemo demo/xdemo.cpp)

# Link against Boost
target_link_libraries(xdemo Boost::headers Boost::interprocess)

# Set output directory
set_target_properties(xdemo
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)