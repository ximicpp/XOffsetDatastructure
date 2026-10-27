# Windows x64 build on Linux with the real MSVC compiler (cl.exe, link.exe,
# lib.exe) running under wine, through tools/msvc-wine (which translates paths
# between the two sides). The static CRT (/MT) is linked, so CTest runs the
# programs through wine without any MSVC runtime DLL.
#
#   cmake -S . -B build/msvc -G Ninja -DXOFFSET_CXX_STANDARD=20 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/msvc-wine-x86_64.cmake
#
# $XOFFSET_MSVC_ROOT (default /opt/msvc) holds VC/Tools/MSVC/<version> and
# Windows Kits/10, the layout of a Visual Studio installation; the newest
# version of each is used.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_SYSTEM_VERSION 10.0)

get_filename_component(XOFFSET_MSVC_WINE "${CMAKE_CURRENT_LIST_DIR}/../../tools/msvc-wine" ABSOLUTE)
set(CMAKE_C_COMPILER "${XOFFSET_MSVC_WINE}/cl")
set(CMAKE_CXX_COMPILER "${XOFFSET_MSVC_WINE}/cl")
set(CMAKE_LINKER "${XOFFSET_MSVC_WINE}/link")
set(CMAKE_AR "${XOFFSET_MSVC_WINE}/lib")
find_program(XOFFSET_LLVM_RC NAMES llvm-rc llvm-rc-21 llvm-rc-20 llvm-rc-19 llvm-rc-18 REQUIRED)
set(CMAKE_RC_COMPILER "${XOFFSET_LLVM_RC}")
# No manifest step (mt.exe is not in the tree): console test programs need none.
set(CMAKE_EXE_LINKER_FLAGS_INIT "/MANIFEST:NO")

# Static CRT and STL (/MT): see msvc-flags.cmake.
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_LIST_DIR}/msvc-flags.cmake")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
find_program(XOFFSET_WINE NAMES wine wine64)
if(XOFFSET_WINE)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${XOFFSET_WINE}")
endif()
