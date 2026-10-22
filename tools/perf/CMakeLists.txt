# H1 measurement tools that need the library: path_audit (gate V6) and slg_strlen
# (string-length distribution of the SLG trace). Configured on its own by
# tools/perf/h1_build.sh, not part of the library build.
cmake_minimum_required(VERSION 3.20)
project(h1_perf_tools LANGUAGES CXX)

set(XOFFSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." CACHE PATH "XOffsetDatastructure checkout")
set(SLG_SRC "${XOFFSET_ROOT}/bench/slg" CACHE PATH "SLG benchmark tree")
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(${XOFFSET_ROOT} xoffset EXCLUDE_FROM_ALL)

add_executable(path_audit path_audit.cpp)
set(FORMAL_SRC "${XOFFSET_ROOT}/bench/formal" CACHE PATH "formal acceptance harness tree")
target_include_directories(path_audit PRIVATE ${SLG_SRC}/include)
if(EXISTS "${FORMAL_SRC}/adapters/xoffset/adapter.hpp")
  target_include_directories(path_audit PRIVATE ${FORMAL_SRC}/include ${FORMAL_SRC}/adapters)
endif()
target_link_libraries(path_audit PRIVATE XOffset::xoffset)

find_path(CEREAL_INCLUDE cereal/cereal.hpp)
if(CEREAL_INCLUDE)
  add_executable(slg_strlen slg_strlen.cpp)
  target_include_directories(slg_strlen PRIVATE ${SLG_SRC}/include ${SLG_SRC}/adapters ${SLG_SRC}/bench ${CEREAL_INCLUDE})
  target_compile_definitions(slg_strlen PRIVATE SLG_WITH_CEREAL=1)
endif()
