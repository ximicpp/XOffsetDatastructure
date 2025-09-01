cmake_minimum_required(VERSION 3.10)

# Set project name
project(XOffsetDatastructure)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add Boost as a subproject
add_subdirectory(external/boost EXCLUDE_FROM_ALL)

# Add demo subdirectories
add_subdirectory(demo)
add_subdirectory(demov2)