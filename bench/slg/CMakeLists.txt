cmake_minimum_required(VERSION 3.20)
project(slg_prototype LANGUAGES CXX)

# XOffset needs a P2996 Clang with its libc++ and the XOffset checkout (next_cpp26).
option(SLG_WITH_XOFFSET "Build the XOffset V2 path (needs P2996 Clang)" OFF)
set(XOFFSET_ROOT "" CACHE PATH "XOffsetDatastructure checkout (next_cpp26)")

if(SLG_WITH_XOFFSET)
  set(CMAKE_CXX_STANDARD 26)
else()
  set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(slg_bench bench/slg_bench.cpp)
target_include_directories(slg_bench PRIVATE include adapters bench)

find_path(CEREAL_INCLUDE cereal/cereal.hpp)
find_package(Protobuf QUIET)
find_package(flatbuffers CONFIG QUIET)
find_program(FLATC flatc)
find_path(ZSTD_INCLUDE zstd.h)
find_library(ZSTD_LIB zstd)
find_path(LZ4_INCLUDE lz4.h)
find_library(LZ4_LIB lz4)

if(CEREAL_INCLUDE)
  target_include_directories(slg_bench PRIVATE ${CEREAL_INCLUDE})
  target_compile_definitions(slg_bench PRIVATE SLG_WITH_CEREAL=1)
endif()

if(Protobuf_FOUND)
  set(PB_OUT ${CMAKE_CURRENT_BINARY_DIR}/gen)
  file(MAKE_DIRECTORY ${PB_OUT})
  add_custom_command(OUTPUT ${PB_OUT}/slg.pb.cc ${PB_OUT}/slg.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE} --cpp_out=${PB_OUT} -I ${CMAKE_CURRENT_SOURCE_DIR}/schema
            ${CMAKE_CURRENT_SOURCE_DIR}/schema/slg.proto
    DEPENDS schema/slg.proto)
  target_sources(slg_bench PRIVATE ${PB_OUT}/slg.pb.cc)
  target_include_directories(slg_bench PRIVATE ${PB_OUT})
  target_link_libraries(slg_bench PRIVATE protobuf::libprotobuf)
  target_compile_definitions(slg_bench PRIVATE SLG_WITH_PROTOBUF=1)
endif()

if(FLATC)
  set(FB_OUT ${CMAKE_CURRENT_BINARY_DIR}/gen)
  file(MAKE_DIRECTORY ${FB_OUT})
  add_custom_command(OUTPUT ${FB_OUT}/slg_generated.h
    COMMAND ${FLATC} --cpp -o ${FB_OUT} ${CMAKE_CURRENT_SOURCE_DIR}/schema/slg.fbs
    DEPENDS schema/slg.fbs)
  add_custom_target(slg_fbs DEPENDS ${FB_OUT}/slg_generated.h)
  add_dependencies(slg_bench slg_fbs)
  target_include_directories(slg_bench PRIVATE ${FB_OUT})
  if(TARGET flatbuffers::flatbuffers)
    target_link_libraries(slg_bench PRIVATE flatbuffers::flatbuffers)
  endif()
  target_compile_definitions(slg_bench PRIVATE SLG_WITH_FLATBUFFERS=1)
endif()

# zpp_bits (single header) and cista (include/ of the cista checkout); pass
# -DZPP_BITS_INCLUDE=<dir with zpp_bits.h> -DCISTA_INCLUDE=<cista>/include if not found.
find_path(ZPP_BITS_INCLUDE zpp_bits.h)
find_path(CISTA_INCLUDE cista/serialization.h)
if(ZPP_BITS_INCLUDE)
  target_include_directories(slg_bench PRIVATE ${ZPP_BITS_INCLUDE})
  target_compile_definitions(slg_bench PRIVATE SLG_WITH_ZPP_BITS=1)
endif()
if(CISTA_INCLUDE)
  target_include_directories(slg_bench PRIVATE ${CISTA_INCLUDE})
  target_compile_definitions(slg_bench PRIVATE SLG_WITH_CISTA=1)
endif()

if(ZSTD_LIB AND LZ4_LIB)
  target_include_directories(slg_bench PRIVATE ${ZSTD_INCLUDE} ${LZ4_INCLUDE})
  target_link_libraries(slg_bench PRIVATE ${ZSTD_LIB} ${LZ4_LIB})
  target_compile_definitions(slg_bench PRIVATE SLG_WITH_COMPRESSION=1)
endif()

if(SLG_WITH_XOFFSET)
  if(NOT XOFFSET_ROOT)
    message(FATAL_ERROR "SLG_WITH_XOFFSET=ON needs -DXOFFSET_ROOT=<XOffsetDatastructure checkout>")
  endif()
  add_subdirectory(${XOFFSET_ROOT} xoffset EXCLUDE_FROM_ALL)
  target_link_libraries(slg_bench PRIVATE XOffset::xoffset)
  target_compile_definitions(slg_bench PRIVATE SLG_WITH_XOFFSET=1)
endif()

message(STATUS "slg_bench formats: xoffset=${SLG_WITH_XOFFSET} cereal=${CEREAL_INCLUDE} protobuf=${Protobuf_FOUND} flatbuffers=${FLATC} zpp_bits=${ZPP_BITS_INCLUDE} cista=${CISTA_INCLUDE} compression=${ZSTD_LIB}")
add_executable(erase_probe bench/erase_probe.cpp)
target_include_directories(erase_probe PRIVATE include)
if(SLG_WITH_XOFFSET)
  target_link_libraries(erase_probe PRIVATE XOffset::xoffset)
  target_compile_definitions(erase_probe PRIVATE SLG_WITH_XOFFSET=1)
endif()

# small-message probe (diagnostic, thread 小消息开销)
if(SLG_WITH_XOFFSET)
  add_executable(small_msg_probe bench/small_msg_probe.cpp)
  target_include_directories(small_msg_probe PRIVATE include adapters bench)
  target_link_libraries(small_msg_probe PRIVATE XOffset::xoffset)
endif()
