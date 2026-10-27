# Windows x64 cross build on Linux with MinGW-w64 GCC 16 (posix threads, libstdc++),
# the C++26 reflection library included. The top-level build links programs
# statically on MinGW, so CTest runs them through wine when it is installed.
#
#   cmake -S . -B build/mingw -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake
#
# The compiler is searched in $XOFFSET_MINGW_ROOT/bin, then /opt/mingw-gcc16/bin
# (the toolchain tools/build_mingw_toolchain.sh builds), then PATH. Debian and
# Ubuntu name their posix-thread compiler with a -posix suffix; the win32-thread
# one lacks std::thread. Distribution MinGW GCC older than 16 has no reflection.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(XOFFSET_MINGW_TRIPLE x86_64-w64-mingw32)
set(XOFFSET_MINGW_HINTS "")
if(DEFINED ENV{XOFFSET_MINGW_ROOT})
    list(APPEND XOFFSET_MINGW_HINTS "$ENV{XOFFSET_MINGW_ROOT}/bin")
endif()
list(APPEND XOFFSET_MINGW_HINTS /opt/mingw-gcc16/bin)
find_program(XOFFSET_MINGW_CXX NAMES ${XOFFSET_MINGW_TRIPLE}-g++-posix ${XOFFSET_MINGW_TRIPLE}-g++
    HINTS ${XOFFSET_MINGW_HINTS} REQUIRED)
set(CMAKE_CXX_COMPILER "${XOFFSET_MINGW_CXX}")
get_filename_component(XOFFSET_MINGW_BIN "${XOFFSET_MINGW_CXX}" DIRECTORY)
find_program(XOFFSET_MINGW_WINDRES NAMES ${XOFFSET_MINGW_TRIPLE}-windres HINTS "${XOFFSET_MINGW_BIN}")
if(XOFFSET_MINGW_WINDRES)
    set(CMAKE_RC_COMPILER "${XOFFSET_MINGW_WINDRES}")
endif()
get_filename_component(XOFFSET_MINGW_PREFIX "${XOFFSET_MINGW_BIN}" DIRECTORY)
set(CMAKE_FIND_ROOT_PATH "${XOFFSET_MINGW_PREFIX}/${XOFFSET_MINGW_TRIPLE}" /usr/${XOFFSET_MINGW_TRIPLE})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
find_program(XOFFSET_WINE NAMES wine wine64)
if(XOFFSET_WINE)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${XOFFSET_WINE}")
endif()
