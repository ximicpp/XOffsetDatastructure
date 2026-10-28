# xoffset_zstd_dict: optional zstd dictionary compression for small messages.
# Not part of the XOffset library; a standalone project (README.md).
#
#   cmake -S contrib/zstd_dict -B build/zstd_dict -DXOZD_ZSTD_SOURCE_DIR=<zstd-1.5.x>
#   cmake --build build/zstd_dict && ctest --test-dir build/zstd_dict
#
# XOZD_ZSTD_SOURCE_DIR: an unpacked zstd 1.5.x release; its lib/ sources are
# compiled here (dictBuilder included for training). Without it, an installed
# zstd is used (find_package(zstd) or libzstd + zdict.h on the search path).
cmake_minimum_required(VERSION 3.16)
project(xoffset_zstd_dict LANGUAGES C CXX)

set(XOZD_ZSTD_SOURCE_DIR "" CACHE PATH "Unpacked zstd 1.5.x release to build from source")

if(XOZD_ZSTD_SOURCE_DIR)
    file(GLOB XOZD_ZSTD_SOURCES
        "${XOZD_ZSTD_SOURCE_DIR}/lib/common/*.c"
        "${XOZD_ZSTD_SOURCE_DIR}/lib/compress/*.c"
        "${XOZD_ZSTD_SOURCE_DIR}/lib/decompress/*.c"
        "${XOZD_ZSTD_SOURCE_DIR}/lib/dictBuilder/*.c")
    add_library(xozd_zstd STATIC ${XOZD_ZSTD_SOURCES})
    target_include_directories(xozd_zstd SYSTEM PUBLIC "${XOZD_ZSTD_SOURCE_DIR}/lib")
    target_compile_definitions(xozd_zstd PRIVATE ZSTD_DISABLE_ASM ZSTD_MULTITHREAD=0)
    set_target_properties(xozd_zstd PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
    set(XOZD_ZSTD xozd_zstd)
else()
    find_package(zstd CONFIG QUIET)
    if(TARGET zstd::libzstd_static)
        set(XOZD_ZSTD zstd::libzstd_static)
    elseif(TARGET zstd::libzstd_shared)
        set(XOZD_ZSTD zstd::libzstd_shared)
    else()
        find_library(XOZD_ZSTD_LIBRARY NAMES zstd REQUIRED)
        find_path(XOZD_ZSTD_INCLUDE zdict.h REQUIRED)
        add_library(xozd_zstd UNKNOWN IMPORTED)
        set_target_properties(xozd_zstd PROPERTIES IMPORTED_LOCATION "${XOZD_ZSTD_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${XOZD_ZSTD_INCLUDE}")
        set(XOZD_ZSTD xozd_zstd)
    endif()
endif()

# Receivers link xoffset_zstd_dict only; senders that train also link
# xoffset_zstd_dict_train (zstd's dictBuilder).
add_library(xoffset_zstd_dict STATIC xoffset_zstd_dict.cpp)
target_include_directories(xoffset_zstd_dict PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(xoffset_zstd_dict PUBLIC ${XOZD_ZSTD})
target_compile_features(xoffset_zstd_dict PUBLIC cxx_std_17)

add_library(xoffset_zstd_dict_train STATIC xoffset_zstd_dict_train.cpp)
target_link_libraries(xoffset_zstd_dict_train PUBLIC xoffset_zstd_dict)

add_executable(xoffset_zstd_dict_tool xoffset_zstd_dict_tool.cpp)
set_target_properties(xoffset_zstd_dict_tool PROPERTIES OUTPUT_NAME xoffset_zstd_dict)
target_link_libraries(xoffset_zstd_dict_tool PRIVATE xoffset_zstd_dict_train)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_zstd_dict test_zstd_dict.cpp)
    target_link_libraries(test_zstd_dict PRIVATE xoffset_zstd_dict_train)
    add_test(NAME test_zstd_dict COMMAND test_zstd_dict)
endif()
