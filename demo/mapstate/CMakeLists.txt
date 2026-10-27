# Map state demo (demo/mapstate/INTERFACES.md). Built only with -DXOFFSET_BUILD_DEMOS=ON.
# OWNER: integration thread. Other threads ask before changing target lists.
#
# MAPDEMO_CLIENT_ONLY=ON builds mapdemo_client alone: the client targets
# (MinGW, MSVC, Android NDK, iOS Simulator, macOS) build it with
# XOFFSET_CXX_STANDARD=20 and the XOFFSET_RECORD field lists, while A and B
# run on Linux. MAPDEMO_WITH_ZSTD=ON (default) builds zstd 1.5.7 from its
# release tarball, pinned by SHA-256, for the compressed snapshot encoding;
# point MAPDEMO_ZSTD_URL at a local copy of the same tarball to build offline.

option(MAPDEMO_CLIENT_ONLY "Build only mapdemo_client" OFF)
option(MAPDEMO_WITH_ZSTD "zstd snapshot encoding (zstd 1.5.7, built from source)" ON)
set(MAPDEMO_ZSTD_URL "https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz"
    CACHE STRING "zstd 1.5.7 release tarball: URL or local path (SHA-256 checked)")

add_library(mapdemo_common STATIC src/net.cpp src/metrics.cpp src/checks.cpp src/codec.cpp)
target_include_directories(mapdemo_common PUBLIC include)
target_link_libraries(mapdemo_common PUBLIC xoffset_common)
if(WIN32)
    target_link_libraries(mapdemo_common PUBLIC ws2_32)
endif()

# Bitwise-identical floats between B (real state) and A's reference model.
# The servers are built by GCC or Clang; MSVC does not contract by default.
target_compile_options(mapdemo_common PUBLIC $<$<CXX_COMPILER_FRONTEND_VARIANT:GNU>:-ffp-contract=off>)

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
    target_link_libraries(mapdemo_common PUBLIC mapdemo_zstd)
    target_compile_definitions(mapdemo_common PUBLIC MAPDEMO_WITH_ZSTD=1)
    message(STATUS "mapdemo: zstd 1.5.7 snapshot encoding enabled")
else()
    message(STATUS "mapdemo: zstd disabled; snapshots are dense or compact only")
endif()

function(mapdemo_executable name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE mapdemo_common)
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin/$<CONFIG>)
endfunction()

if(NOT MAPDEMO_CLIENT_ONLY)
    mapdemo_executable(mapdemo_b src/server_b.cpp)
    mapdemo_executable(mapdemo_a src/server_a.cpp)
    mapdemo_executable(mapdemo_selftest src/selftest.cpp)
endif()
mapdemo_executable(mapdemo_client src/client_main.cpp src/client_core.cpp src/client_offline.cpp)

# Window client: raylib 5.5 (commit c1ab645ca298a2801097931d1079b10ff7eb9df8),
# prebuilt as a C library and found through CMAKE_PREFIX_PATH.
find_package(raylib 5.5 CONFIG QUIET)
if(raylib_FOUND)
    target_sources(mapdemo_client PRIVATE src/client_gui.cpp)
    target_link_libraries(mapdemo_client PRIVATE raylib)
    target_compile_definitions(mapdemo_client PRIVATE MAPDEMO_WITH_GUI=1)
    if(APPLE)
        target_link_libraries(mapdemo_client PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo" "-framework OpenGL")
    endif()
    message(STATUS "mapdemo: window client enabled (raylib ${raylib_VERSION})")
else()
    message(STATUS "mapdemo: raylib not found; mapdemo_client is --headless only")
endif()
