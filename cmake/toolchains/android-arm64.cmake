# Android arm64-v8a with the NDK's Clang and libc++. NDK Clang has no P2996
# reflection, so this target builds with XOFFSET_CXX_STANDARD=20 or 23.
#
#   cmake -S . -B build/android -G Ninja -DXOFFSET_CXX_STANDARD=20 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/android-arm64.cmake -DANDROID_NDK=<ndk>
#
# ANDROID_NDK (cache or environment, also ANDROID_NDK_ROOT / ANDROID_NDK_HOME) is the
# NDK root. libc++ is linked statically into each program, so std::pmr does not
# depend on the device's Android version.
#
# XOFFSET_ANDROID_QEMU=ON links the programs fully static and runs CTest through
# qemu-aarch64 user mode, so the suite runs on a Linux host without a device or an
# emulator. Bionic's dynamic loader is then not exercised.
if(NOT ANDROID_NDK)
    foreach(var ANDROID_NDK ANDROID_NDK_ROOT ANDROID_NDK_HOME)
        if(DEFINED ENV{${var}} AND NOT "$ENV{${var}}" STREQUAL "")
            set(ANDROID_NDK "$ENV{${var}}")
            break()
        endif()
    endforeach()
endif()
if(NOT ANDROID_NDK OR NOT EXISTS "${ANDROID_NDK}/build/cmake/android.toolchain.cmake")
    message(FATAL_ERROR "Set ANDROID_NDK to the root of an Android NDK (r26 or newer)")
endif()
set(ANDROID_NDK "${ANDROID_NDK}" CACHE PATH "Android NDK root")
set(ANDROID_ABI arm64-v8a)
# API 28 (Android 9): the tests' failure-injection allocators call aligned_alloc.
if(NOT ANDROID_PLATFORM)
    set(ANDROID_PLATFORM android-28)
endif()
set(ANDROID_STL c++_static)
option(XOFFSET_ANDROID_QEMU "Run CTest through qemu-aarch64 user mode (static programs)" OFF)
# try_compile() rereads this file in a fresh project.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ANDROID_NDK ANDROID_PLATFORM XOFFSET_ANDROID_QEMU)

if(XOFFSET_ANDROID_QEMU)
    # Set before the NDK file: it creates this cache entry empty, then prepends its own flags.
    set(CMAKE_EXE_LINKER_FLAGS "-static" CACHE STRING "Flags used by the linker.")
endif()
include("${ANDROID_NDK}/build/cmake/android.toolchain.cmake")
# Programs (ninja, qemu) run on the host: without this, the Ninja generator
# could miss a ninja outside the NDK's find root.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

if(XOFFSET_ANDROID_QEMU)
    find_program(XOFFSET_QEMU_AARCH64 NAMES qemu-aarch64-static qemu-aarch64 REQUIRED)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${XOFFSET_QEMU_AARCH64}")
endif()
