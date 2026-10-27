#!/usr/bin/env bash
# Builds a MinGW-w64 GCC cross toolchain (Linux x86-64 host -> x86_64-w64-mingw32,
# posix threads, C and C++) from source tarballs, and packs it like the P2996
# toolchain: <name>.tar.xz + .sha256 + .BUILDINFO.txt.
#
#   tools/build_mingw_toolchain.sh --src DIR [--prefix DIR] [--work DIR] [--out DIR] [-j N]
#
# --src holds the unpacked sources: gcc-<ver>/, binutils-<ver>/, mingw-w64-v<ver>/
# (one of each). Ubuntu's pool serves all three when the GNU mirrors are blocked:
# gcc-16_<ver>.orig.tar.gz wraps the pristine gcc-<ver>.tar.xz; binutils_<ver>.orig.tar.xz
# and mingw-w64_<ver>.orig.tar.bz2 are the upstream tarballs. Host needs build-essential,
# libgmp-dev, libmpfr-dev, libmpc-dev, flex, bison, texinfo.
#
# The toolchain is relocatable: unpack anywhere and put <prefix>/bin on PATH.
set -euo pipefail

SRC=""
PREFIX=/opt/mingw-gcc16
WORK=""
OUT=""
JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
while [[ $# -gt 0 ]]; do
    case "$1" in
        --src) SRC=$(cd "$2" && pwd); shift 2 ;;
        --prefix) PREFIX=$2; shift 2 ;;
        --work) WORK=$2; shift 2 ;;
        --out) OUT=$2; shift 2 ;;
        -j) JOBS=$2; shift 2 ;;
        *) sed -n '2,15p' "$0"; exit 2 ;;
    esac
done
[[ -n "$SRC" ]] || { sed -n '2,15p' "$0"; exit 2; }
one() {  # one GLOB: the single directory in $SRC matching GLOB
    local m=("$SRC"/$1)
    [[ ${#m[@]} -eq 1 && -d ${m[0]} ]] || { echo "need exactly one $SRC/$1" >&2; exit 2; }
    echo "${m[0]}"
}
GCC_SRC=$(one 'gcc-[0-9]*')
BINUTILS_SRC=$(one 'binutils-[0-9]*')
MINGW_SRC=$(one 'mingw-w64-v*')
TARGET=x86_64-w64-mingw32
WORK=${WORK:-$SRC/build-$TARGET}
GCC_VER=$(cat "$GCC_SRC/gcc/BASE-VER")
NAME=mingw-w64-gcc-$GCC_VER-$TARGET-linux-x86_64
mkdir -p "$WORK" "$PREFIX"
export PATH=$PREFIX/bin:$PATH
log() { echo "[$(date -u +%H:%M:%S)] $*"; }

# Without a sysroot the target headers and libraries live in <prefix>/<target>,
# which GCC finds relative to its own location (keeps the tree relocatable).
log binutils
mkdir -p "$WORK/binutils" && cd "$WORK/binutils"
"$BINUTILS_SRC/configure" --target=$TARGET --prefix="$PREFIX" \
    --disable-multilib --disable-nls --disable-werror --disable-gdb --disable-gprofng
make -j"$JOBS" && make install

log mingw-w64 headers
mkdir -p "$WORK/headers" && cd "$WORK/headers"
"$MINGW_SRC/mingw-w64-headers/configure" --host=$TARGET --prefix="$PREFIX/$TARGET" \
    --enable-sdk=all
make install

log gcc stage 1
GCC_CONF=(--target=$TARGET --prefix="$PREFIX" --enable-languages=c,c++
    --enable-threads=posix --disable-multilib --disable-nls --disable-libstdcxx-pch
    --disable-werror --enable-checking=release --with-pkgversion="XOffset MinGW-w64 GCC $GCC_VER")
mkdir -p "$WORK/gcc" && cd "$WORK/gcc"
"$GCC_SRC/configure" "${GCC_CONF[@]}"
make -j"$JOBS" all-gcc && make install-gcc

log mingw-w64 crt
mkdir -p "$WORK/crt" && cd "$WORK/crt"
"$MINGW_SRC/mingw-w64-crt/configure" --host=$TARGET --prefix="$PREFIX/$TARGET" \
    --disable-lib32 --enable-lib64
make -j"$JOBS" && make install

log winpthreads
mkdir -p "$WORK/winpthreads" && cd "$WORK/winpthreads"
"$MINGW_SRC/mingw-w64-libraries/winpthreads/configure" --host=$TARGET \
    --prefix="$PREFIX/$TARGET" --enable-static --enable-shared
make -j"$JOBS" && make install

log gcc
cd "$WORK/gcc"
make -j"$JOBS" && make install-strip

log smoke test
cd "$WORK"
cat > smoke.cpp <<'EOF'
#include <cstdio>
#include <thread>
#include <version>
int main() {
    int v = 0;
    std::thread t([&] { v = 26; });
    t.join();
    std::printf("cplusplus %ld threads %d\n", (long)__cplusplus, v);
}
EOF
$TARGET-g++ -std=c++26 -O2 -static smoke.cpp -o smoke.exe
file smoke.exe

if [[ -n "$OUT" ]]; then
    mkdir -p "$OUT" && OUT=$(cd "$OUT" && pwd)
    log pack "$OUT/$NAME.tar.xz"
    tar -C "$(dirname "$PREFIX")" -cf - "$(basename "$PREFIX")" | xz -T"$JOBS" -6 > "$OUT/$NAME.tar.xz"
    (cd "$OUT" && sha256sum "$NAME.tar.xz" > "$NAME.tar.xz.sha256")
    {
        echo "name: $NAME"
        echo "built: $(date -u +%Y-%m-%dT%H:%M:%SZ) on $(uname -srm), $(. /etc/os-release && echo "$PRETTY_NAME")"
        echo "host compiler: $(gcc --version | head -1)"
        echo "script: tools/build_mingw_toolchain.sh"
        echo "prefix when built: $PREFIX (relocatable; unpacks to $(basename "$PREFIX")/)"
        echo "gcc: $GCC_VER ($GCC_SRC)"
        echo "binutils: $(basename "$BINUTILS_SRC")"
        echo "mingw-w64: $(basename "$MINGW_SRC") (headers, crt lib64, winpthreads)"
        echo "gcc configure: ${GCC_CONF[*]}"
        echo "version: $($TARGET-g++ --version | head -1)"
        echo "thread model: $($TARGET-g++ -v 2>&1 | sed -n 's/^Thread model: //p')"
    } > "$OUT/$NAME.BUILDINFO.txt"
    cat "$OUT/$NAME.BUILDINFO.txt" "$OUT/$NAME.tar.xz.sha256"
fi
log done
