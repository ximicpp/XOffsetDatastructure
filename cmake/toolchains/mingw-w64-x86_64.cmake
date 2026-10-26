# Windows x64 cross build on Linux with MinGW-w64 GCC (posix threads) and libstdc++.
# GCC 16 builds the C++26 library; older MinGW GCC (12 and newer) builds it with
# XOFFSET_CXX_STANDARD=20 or 23. The top-level build links programs statically on
# MinGW, so CTest runs them through wine when it is installed.
#
#   cmake -S . -B build/mingw -G Ninja -DXOFFSET_CXX_STANDARD=20 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(XOFFSET_MINGW_TRIPLE x86_64-w64-mingw32)
# Debian and Ubuntu install the posix-thread compiler under a -posix suffix;
# the win32-thread one lacks std::thread.
find_program(XOFFSET_MINGW_CXX NAMES ${XOFFSET_MINGW_TRIPLE}-g++-posix ${XOFFSET_MINGW_TRIPLE}-g++ REQUIRED)
set(CMAKE_CXX_COMPILER "${XOFFSET_MINGW_CXX}")
find_program(XOFFSET_MINGW_WINDRES NAMES ${XOFFSET_MINGW_TRIPLE}-windres)
if(XOFFSET_MINGW_WINDRES)
    set(CMAKE_RC_COMPILER "${XOFFSET_MINGW_WINDRES}")
endif()
set(CMAKE_FIND_ROOT_PATH /usr/${XOFFSET_MINGW_TRIPLE})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
find_program(XOFFSET_WINE NAMES wine wine64)
if(XOFFSET_WINE)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${XOFFSET_WINE}")
endif()
