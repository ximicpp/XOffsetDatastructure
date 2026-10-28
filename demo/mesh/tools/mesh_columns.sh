#!/usr/bin/env bash
# Device mesh demo: the cloud device columns, sourced by run_mesh.sh and
# run_mailbox.sh (REPO, NDK and MSVC_ROOT set by the caller). OWNER: network
# thread.
#
#   available COLUMN   true when this host can build and run the column
#   cmake_args COLUMN  its configure arguments (COMMON and PEER go first)
#   runner COLUMN      the command prefix that runs its programs here
#   exe COLUMN         the program suffix (.exe for Windows)
#   path_for COLUMN P  a path its programs understand (wine: the Z: drive)
ALL_COLUMNS="linux-x64-gcc linux-x64-clang linux-arm64-gcc windows-mingw windows-msvc windows-clang-cl android-arm64 android-x64"

have() { command -v "$1" >/dev/null 2>&1; }

# available COLUMN: true when this host can build and run the column.
available() {
    case "$1" in
        linux-x64-gcc) have "${XMESH_GCC:-g++}" ;;
        linux-x64-clang) have "${XMESH_CLANG:-clang++}" ;;
        linux-arm64-gcc) have aarch64-linux-gnu-g++ && have qemu-aarch64-static ;;
        windows-mingw) have wine && { have x86_64-w64-mingw32-g++-posix || have x86_64-w64-mingw32-g++ || [ -x /opt/mingw-gcc16/bin/x86_64-w64-mingw32-g++ ]; } ;;
        windows-msvc) have wine && [ -d "$MSVC_ROOT/VC" ] ;;
        windows-clang-cl) have wine && [ -d "$MSVC_ROOT/VC" ] && have clang-cl && { have lld-link || have lld-link-19 || [ -x /usr/lib/llvm-19/bin/lld-link ]; } ;;
        android-arm64) [ -f "$NDK/build/cmake/android.toolchain.cmake" ] && have qemu-aarch64-static ;;
        android-x64) [ -f "$NDK/build/cmake/android.toolchain.cmake" ] ;;
        *) return 1 ;;
    esac
}

COMMON="-G Ninja -DCMAKE_BUILD_TYPE=Release -DXOFFSET_BUILD_MESH_DEMO=ON -DXOFFSET_BUILD_TESTS=OFF -DXOFFSET_BUILD_TOOLS=OFF -DXOFFSET_BUILD_EXAMPLES=OFF"
PEER="-DXMESH_PEER_ONLY=ON -DXOFFSET_CXX_STANDARD=20"
TC=$REPO/cmake/toolchains

# cmake_args COLUMN: configure arguments of a device column.
cmake_args() {
    case "$1" in
        linux-x64-gcc) echo "-DCMAKE_CXX_COMPILER=${XMESH_GCC:-g++}" ;;
        linux-x64-clang) echo "-DCMAKE_CXX_COMPILER=${XMESH_CLANG:-clang++}" ;;
        linux-arm64-gcc) echo "-DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ -DCMAKE_EXE_LINKER_FLAGS=-static" ;;
        windows-mingw) echo "-DCMAKE_TOOLCHAIN_FILE=$TC/mingw-w64-x86_64.cmake" ;;
        windows-msvc) echo "-DCMAKE_TOOLCHAIN_FILE=$TC/msvc-wine-x86_64.cmake" ;;
        windows-clang-cl) echo "-DCMAKE_TOOLCHAIN_FILE=$TC/clang-cl-x86_64.cmake" ;;
        android-arm64) echo "-DCMAKE_TOOLCHAIN_FILE=$TC/android-arm64.cmake -DANDROID_NDK=$NDK -DXOFFSET_ANDROID_QEMU=ON" ;;
        android-x64) echo "-DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=x86_64 -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_static -DCMAKE_EXE_LINKER_FLAGS=-static" ;;
    esac
}

# runner COLUMN: the command prefix that runs the column's programs here.
runner() {
    case "$1" in
        linux-arm64-gcc|android-arm64) echo "qemu-aarch64-static" ;;
        windows-*) echo "wine" ;;
        *) echo "" ;;
    esac
}

exe() { case "$1" in windows-*) echo ".exe" ;; *) echo "" ;; esac; }

# path_for COLUMN PATH: a path the column's programs understand (wine: Z: drive).
path_for() { case "$1" in windows-*) echo "Z:$2" ;; *) echo "$2" ;; esac; }
