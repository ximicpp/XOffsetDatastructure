# Map state demo (demo/mapstate/INTERFACES.md). Built only with -DXOFFSET_BUILD_DEMOS=ON.
# OWNER: integration thread. Other threads ask before changing target lists.
#
# MAPDEMO_CLIENT_ONLY=ON builds mapdemo_client alone: the client targets
# (MinGW, MSVC, Android NDK, iOS Simulator, macOS) build it with
# XOFFSET_CXX_STANDARD=20 and the XOFFSET_RECORD field lists, while A and B
# run on Linux. MAPDEMO_WITH_ZSTD=ON (default) builds zstd 1.5.7 from its
# release tarball, pinned by SHA-256, for the compressed snapshot encoding;
# point MAPDEMO_ZSTD_URL at a local copy of the same tarball to build offline.
#
# The client is built twice: mapdemo_client as the toolchain builds by
# default, and mapdemo_client_noexc without exceptions (-fno-exceptions;
# MSVC /EHs-c- with _HAS_EXCEPTIONS=0), the way engine clients build. Both
# come from the same sources, which report failures as values (result.hpp).
#
# mapdemo_client_v1 is the client of the previous release, schema version 1:
# the same sources built against include_v1/ (the frozen snapshot that A
# lists in its lineage), so A has an old-version client to convert for.

option(MAPDEMO_CLIENT_ONLY "Build only the clients (mapdemo_client, _noexc, _v1)" OFF)
option(MAPDEMO_NOEXC_CLIENT "Also build mapdemo_client_noexc, the client without exceptions" ON)
option(MAPDEMO_V1_CLIENT "Also build mapdemo_client_v1, the client of schema version 1" ON)
option(MAPDEMO_WITH_ZSTD "zstd snapshot encoding (zstd 1.5.7, built from source)" ON)
set(MAPDEMO_ZSTD_URL "https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz"
    CACHE STRING "zstd 1.5.7 release tarball: URL or local path (SHA-256 checked)")

# net, metrics, checks and the snapshot codec, shared by A, B and the client.
function(mapdemo_common_library name)
    add_library(${name} STATIC src/net.cpp src/metrics.cpp src/checks.cpp src/codec.cpp)
    target_include_directories(${name} PUBLIC include)
    target_link_libraries(${name} PUBLIC xoffset_common)
    if(WIN32)
        target_link_libraries(${name} PUBLIC ws2_32)
        # std::fopen is standard C++; the MSVC CRT deprecation warning does not apply.
        target_compile_definitions(${name} PUBLIC _CRT_SECURE_NO_WARNINGS)
    endif()
    # Bitwise-identical floats between B (real state) and A's reference model.
    # The servers are built by GCC or Clang; MSVC does not contract by default.
    target_compile_options(${name} PUBLIC $<$<CXX_COMPILER_FRONTEND_VARIANT:GNU>:-ffp-contract=off>)
    if(TARGET mapdemo_zstd)
        target_link_libraries(${name} PUBLIC mapdemo_zstd)
        target_compile_definitions(${name} PUBLIC MAPDEMO_WITH_ZSTD=1)
        # Event stream: contrib/zstd_dict (dictionary frames, batches) and the
        # event dictionary, embedded at configure time (below).
        target_sources(${name} PRIVATE "${MAPDEMO_ZSTD_DICT_DIR}/xoffset_zstd_dict.cpp" "${MAPDEMO_EVENT_DICT_CPP}")
        target_include_directories(${name} PUBLIC "${MAPDEMO_ZSTD_DICT_DIR}")
    endif()
endfunction()

if(MAPDEMO_WITH_ZSTD)
    # The MinGW toolchain file names only the C++ compiler; zstd is C.
    if(MINGW AND NOT CMAKE_C_COMPILER)
        string(REGEX REPLACE "g\\+\\+(-posix)?(\\.exe)?$" "gcc\\1\\2" MAPDEMO_MINGW_CC "${CMAKE_CXX_COMPILER}")
        set(CMAKE_C_COMPILER "${MAPDEMO_MINGW_CC}")
    endif()
    enable_language(C)
    include(FetchContent)
    FetchContent_Declare(mapdemo_zstd
        URL "${MAPDEMO_ZSTD_URL}"
        URL_HASH SHA256=eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3
        DOWNLOAD_EXTRACT_TIMESTAMP ON)
    # The tarball has no top-level CMakeLists.txt: this only unpacks it.
    FetchContent_MakeAvailable(mapdemo_zstd)
    # Compression and decompression only, single-threaded, portable C (no
    # x86-64 assembly), so every client compiler builds the same sources.
    file(GLOB MAPDEMO_ZSTD_SOURCES
        "${mapdemo_zstd_SOURCE_DIR}/lib/common/*.c"
        "${mapdemo_zstd_SOURCE_DIR}/lib/compress/*.c"
        "${mapdemo_zstd_SOURCE_DIR}/lib/decompress/*.c")
    add_library(mapdemo_zstd STATIC ${MAPDEMO_ZSTD_SOURCES})
    target_include_directories(mapdemo_zstd SYSTEM PUBLIC "${mapdemo_zstd_SOURCE_DIR}/lib")
    target_compile_definitions(mapdemo_zstd PRIVATE ZSTD_DISABLE_ASM)
    set_target_properties(mapdemo_zstd PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
    message(STATUS "mapdemo: zstd 1.5.7 snapshot encoding enabled")

    # The event dictionary (codec.hpp, event stream): dict/unit_event_v1.xozd,
    # trained offline from A's events with contrib/zstd_dict (README.md), is
    # compiled into every binary as a byte array. Read here, at configure
    # time, so cross builds (MinGW, MSVC, NDK, iOS) need no host tool.
    set(MAPDEMO_ZSTD_DICT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../contrib/zstd_dict")
    set(MAPDEMO_EVENT_DICT "${CMAKE_CURRENT_SOURCE_DIR}/dict/unit_event_v1.xozd")
    set(MAPDEMO_EVENT_DICT_CPP "${CMAKE_CURRENT_BINARY_DIR}/mapdemo_event_dict.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MAPDEMO_EVENT_DICT}")
    if(EXISTS "${MAPDEMO_EVENT_DICT}")
        file(READ "${MAPDEMO_EVENT_DICT}" MAPDEMO_EVENT_DICT_HEX HEX)
        string(LENGTH "${MAPDEMO_EVENT_DICT_HEX}" MAPDEMO_EVENT_DICT_HEX_LEN)
        math(EXPR MAPDEMO_EVENT_DICT_SIZE "${MAPDEMO_EVENT_DICT_HEX_LEN} / 2")
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," MAPDEMO_EVENT_DICT_BYTES "${MAPDEMO_EVENT_DICT_HEX}")
        message(STATUS "mapdemo: event dictionary ${MAPDEMO_EVENT_DICT_SIZE} bytes built in")
    else()
        set(MAPDEMO_EVENT_DICT_SIZE 0)
        set(MAPDEMO_EVENT_DICT_BYTES "0")
        message(STATUS "mapdemo: no dict/unit_event_v1.xozd; events go raw or zstd only")
    endif()
    file(CONFIGURE OUTPUT "${MAPDEMO_EVENT_DICT_CPP}" CONTENT
"// Generated by demo/mapstate/CMakeLists.txt from dict/unit_event_v1.xozd.
#include <cstddef>
namespace mapdemo::codec::detail {
extern const unsigned char kEventDictBytes[] = {@MAPDEMO_EVENT_DICT_BYTES@};
extern const std::size_t kEventDictSize = @MAPDEMO_EVENT_DICT_SIZE@;
}
" @ONLY)
else()
    message(STATUS "mapdemo: zstd disabled; snapshots are dense or compact only")
endif()

mapdemo_common_library(mapdemo_common)

function(mapdemo_executable name common)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE ${common})
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/$<CONFIG>)
endfunction()

if(NOT MAPDEMO_CLIENT_ONLY)
    mapdemo_executable(mapdemo_b mapdemo_common src/server_b.cpp)
    mapdemo_executable(mapdemo_a mapdemo_common src/server_a.cpp)
    mapdemo_executable(mapdemo_selftest mapdemo_common src/selftest.cpp)
    # Server side only (reflection): freeze a schema version, check the lock.
    mapdemo_executable(mapdemo_schema_freeze mapdemo_common src/schema_freeze.cpp)
endif()

# Window client: raylib 5.5 (commit c1ab645ca298a2801097931d1079b10ff7eb9df8),
# prebuilt as a C library and found through CMAKE_PREFIX_PATH.
find_package(raylib 5.5 CONFIG QUIET)
if(raylib_FOUND)
    message(STATUS "mapdemo: window client enabled (raylib ${raylib_VERSION})")
else()
    message(STATUS "mapdemo: raylib not found; mapdemo_client is --headless only")
endif()

function(mapdemo_client_target name common)
    mapdemo_executable(${name} ${common} src/client_main.cpp src/client_core.cpp src/client_offline.cpp)
    if(raylib_FOUND)
        target_sources(${name} PRIVATE src/client_gui.cpp)
        target_link_libraries(${name} PRIVATE raylib)
        target_compile_definitions(${name} PRIVATE MAPDEMO_WITH_GUI=1)
        if(APPLE)
            target_link_libraries(${name} PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo" "-framework OpenGL")
        endif()
    endif()
endfunction()

mapdemo_client_target(mapdemo_client mapdemo_common)
if(MAPDEMO_V1_CLIENT)
    mapdemo_common_library(mapdemo_common_v1)
    target_include_directories(mapdemo_common_v1 BEFORE PUBLIC include_v1)
    mapdemo_client_target(mapdemo_client_v1 mapdemo_common_v1)
    message(STATUS "mapdemo: mapdemo_client_v1 builds the schema version 1 client")
endif()
if(MAPDEMO_NOEXC_CLIENT)
    mapdemo_common_library(mapdemo_common_noexc)
    xoffset_disable_exceptions(mapdemo_common_noexc)
    mapdemo_client_target(mapdemo_client_noexc mapdemo_common_noexc)
    xoffset_disable_exceptions(mapdemo_client_noexc)
    message(STATUS "mapdemo: mapdemo_client_noexc builds without exceptions")
endif()
