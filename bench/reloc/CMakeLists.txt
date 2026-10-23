cmake_minimum_required(VERSION 3.20)
project(reloc_bench LANGUAGES CXX)

# Relocation tax micro benchmark (PERF_GOAL G9). Needs P2996 Clang or GCC 16.
# In the repository the library is two levels up (bench/reloc -> root).
get_filename_component(_reloc_repo_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
if(EXISTS "${_reloc_repo_root}/xoffsetdatastructure.hpp")
  set(_reloc_default_root "${_reloc_repo_root}")
else()
  set(_reloc_default_root "")
endif()
set(XOFFSET_ROOT "${_reloc_default_root}" CACHE PATH "XOffsetDatastructure checkout (next_cpp26)")
if(NOT XOFFSET_ROOT)
  message(FATAL_ERROR "reloc_bench needs -DXOFFSET_ROOT=<XOffsetDatastructure checkout>")
endif()
# Instruction-count build for the CI performance gate (see README.md).
# OFF leaves the timing path unchanged.
option(RELOC_IR_COUNT "Toggle callgrind collection around the XOffset timed regions" OFF)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(${XOFFSET_ROOT} xoffset EXCLUDE_FROM_ALL)
add_executable(reloc_bench reloc_bench.cpp)
target_link_libraries(reloc_bench PRIVATE XOffset::xoffset)
if(RELOC_IR_COUNT)
  find_path(VALGRIND_INCLUDE valgrind/callgrind.h REQUIRED)
  target_include_directories(reloc_bench SYSTEM PRIVATE ${VALGRIND_INCLUDE})
  target_compile_definitions(reloc_bench PRIVATE RELOC_IR_COUNT=1)
endif()
message(STATUS "reloc_bench: XOFFSET_ROOT=${XOFFSET_ROOT} ir_count=${RELOC_IR_COUNT}")
