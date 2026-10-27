# Windows x64 cross build on Linux with clang-cl + lld-link against the MSVC
# toolset (STL, vcruntime) and the Windows SDK: the same headers and libraries a
# Visual Studio clang-cl build uses. The static CRT (/MT) is linked, so CTest
# runs the programs through wine without any MSVC runtime DLL.
#
#   cmake -S . -B build/clang-cl -G Ninja -DXOFFSET_CXX_STANDARD=20 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/clang-cl-x86_64.cmake
#
# $XOFFSET_MSVC_ROOT (default /opt/msvc) holds VC/Tools/MSVC/<version> and
# Windows Kits/10, the layout of a Visual Studio installation; the newest
# version of each is used. On a case-sensitive file system the tree needs
# lowercase aliases of its mixed-case headers and libraries (xwin splat makes
# them). clang-cl, lld-link, llvm-lib, llvm-rc and llvm-mt are searched with
# the version suffixes Debian and Ubuntu use.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(CMAKE_SYSTEM_VERSION 10.0)

set(XOFFSET_MSVC_ROOT "$ENV{XOFFSET_MSVC_ROOT}")
if(NOT XOFFSET_MSVC_ROOT)
    set(XOFFSET_MSVC_ROOT /opt/msvc)
endif()
file(GLOB XOFFSET_MSVC_VERSIONS LIST_DIRECTORIES true "${XOFFSET_MSVC_ROOT}/VC/Tools/MSVC/*")
file(GLOB XOFFSET_WINSDK_VERSIONS LIST_DIRECTORIES true "${XOFFSET_MSVC_ROOT}/Windows Kits/10/Include/*")
if(NOT XOFFSET_MSVC_VERSIONS OR NOT XOFFSET_WINSDK_VERSIONS)
    message(FATAL_ERROR "No MSVC toolset or Windows SDK under ${XOFFSET_MSVC_ROOT}")
endif()
list(SORT XOFFSET_MSVC_VERSIONS COMPARE NATURAL ORDER DESCENDING)
list(SORT XOFFSET_WINSDK_VERSIONS COMPARE NATURAL ORDER DESCENDING)
list(GET XOFFSET_MSVC_VERSIONS 0 XOFFSET_MSVC_DIR)
list(GET XOFFSET_WINSDK_VERSIONS 0 XOFFSET_WINSDK_INCLUDE)
get_filename_component(XOFFSET_WINSDK_VERSION "${XOFFSET_WINSDK_INCLUDE}" NAME)
set(XOFFSET_WINSDK_LIB "${XOFFSET_MSVC_ROOT}/Windows Kits/10/Lib/${XOFFSET_WINSDK_VERSION}")

set(XOFFSET_LLVM_SUFFIXES "" -21 -20 -19)
macro(xoffset_find_llvm var name)
    set(names)
    foreach(suffix IN LISTS XOFFSET_LLVM_SUFFIXES)
        list(APPEND names ${name}${suffix})
    endforeach()
    find_program(${var} NAMES ${names} REQUIRED)
endmacro()
xoffset_find_llvm(XOFFSET_CLANG_CL clang-cl)
xoffset_find_llvm(XOFFSET_LLD_LINK lld-link)
xoffset_find_llvm(XOFFSET_LLVM_LIB llvm-lib)
xoffset_find_llvm(XOFFSET_LLVM_RC llvm-rc)
xoffset_find_llvm(XOFFSET_LLVM_MT llvm-mt)
set(CMAKE_C_COMPILER "${XOFFSET_CLANG_CL}")
set(CMAKE_CXX_COMPILER "${XOFFSET_CLANG_CL}")
set(CMAKE_LINKER "${XOFFSET_LLD_LINK}")
set(CMAKE_AR "${XOFFSET_LLVM_LIB}")
set(CMAKE_RC_COMPILER "${XOFFSET_LLVM_RC}")
set(CMAKE_MT "${XOFFSET_LLVM_MT}")

# _MSC_VER of the toolset the headers come from (14.44 -> 1944).
get_filename_component(XOFFSET_MSVC_VERSION "${XOFFSET_MSVC_DIR}" NAME)
string(REGEX MATCH "^14\\.([0-9]+)" _ "${XOFFSET_MSVC_VERSION}")
set(XOFFSET_MSC_VER "19.${CMAKE_MATCH_1}")

set(XOFFSET_MSVC_SYSTEM_FLAGS "--target=x86_64-pc-windows-msvc -fms-compatibility-version=${XOFFSET_MSC_VER}")
foreach(dir "${XOFFSET_MSVC_DIR}/include" "${XOFFSET_WINSDK_INCLUDE}/ucrt" "${XOFFSET_WINSDK_INCLUDE}/um"
            "${XOFFSET_WINSDK_INCLUDE}/shared" "${XOFFSET_WINSDK_INCLUDE}/winrt")
    string(APPEND XOFFSET_MSVC_SYSTEM_FLAGS " \"-imsvc${dir}\"")
endforeach()
set(CMAKE_C_FLAGS_INIT "${XOFFSET_MSVC_SYSTEM_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${XOFFSET_MSVC_SYSTEM_FLAGS}")
set(XOFFSET_MSVC_LINK_FLAGS "")
foreach(dir "${XOFFSET_MSVC_DIR}/lib/x64" "${XOFFSET_WINSDK_LIB}/ucrt/x64" "${XOFFSET_WINSDK_LIB}/um/x64")
    string(APPEND XOFFSET_MSVC_LINK_FLAGS " \"/libpath:${dir}\"")
endforeach()
set(CMAKE_EXE_LINKER_FLAGS_INIT "${XOFFSET_MSVC_LINK_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${XOFFSET_MSVC_LINK_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${XOFFSET_MSVC_LINK_FLAGS}")

# Static CRT and STL (/MT): see msvc-flags.cmake.
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${CMAKE_CURRENT_LIST_DIR}/msvc-flags.cmake")

set(CMAKE_FIND_ROOT_PATH "${XOFFSET_MSVC_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
find_program(XOFFSET_WINE NAMES wine wine64)
if(XOFFSET_WINE)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${XOFFSET_WINE}")
endif()
