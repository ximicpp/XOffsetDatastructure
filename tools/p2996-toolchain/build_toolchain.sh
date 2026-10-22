#!/usr/bin/env bash
# Build clang-p2996 from source with the recipe of the repository Dockerfile
# (stage clang-builder): ubuntu:22.04 host, Release, clang + libcxx/libcxxabi/
# libunwind, assertions off, tests off. Differences from the Dockerfile:
#   - LLVM_TARGETS_TO_BUILD is a parameter (AArch64 or X86);
#   - only the toolchain is built and installed (clang, its resource headers
#     and the runtimes), not every LLVM tool and library;
#   - the prefix is /opt/p2996-toolchain, the path the CI P2996 image uses;
#   - the source is fetched at one commit (shallow) instead of the whole branch.
# Run inside ubuntu:22.04 with $PREFIX and $LOGDIR bind-mounted from the host.
set -euo pipefail

COMMIT=${CLANG_P2996_COMMIT:?CLANG_P2996_COMMIT is required}
TARGET=${LLVM_TARGET:?LLVM_TARGET is required (AArch64 or X86)}
PREFIX=${PREFIX:-/opt/p2996-toolchain}
WORK=${WORK:-/work}
LOGDIR=${LOGDIR:-/logs}
JOBS=${JOBS:-$(nproc)}
LINK_JOBS=${LINK_JOBS:-2}
test "${#COMMIT}" -eq 40

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    git cmake ninja-build build-essential python3 ca-certificates file
cat /etc/os-release
gcc --version | head -n 1
cmake --version | head -n 1

mkdir -p "$WORK" "$LOGDIR"
cd "$WORK"
git init -q clang-p2996
cd clang-p2996
git remote add origin https://github.com/bloomberg/clang-p2996.git
git fetch -q --depth 1 origin "$COMMIT"
git checkout -q --detach FETCH_HEAD
test "$(git rev-parse HEAD)" = "$COMMIT"

mkdir -p build
cd build
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS='clang' \
    -DLLVM_ENABLE_RUNTIMES='libcxx;libcxxabi;libunwind' \
    -DLLVM_TARGETS_TO_BUILD="$TARGET" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DLLVM_ENABLE_ASSERTIONS=OFF \
    -DLLVM_OPTIMIZED_TABLEGEN=ON \
    -DLLVM_BUILD_TESTS=OFF \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLIBCXX_ENABLE_EXPERIMENTAL_LIBRARY=ON \
    -DLLVM_PARALLEL_LINK_JOBS="$LINK_JOBS" \
    ../llvm 2>&1 | tee "$LOGDIR/configure.log"

# Build first so a failure shows the compile error, not an install error.
ninja -j"$JOBS" clang runtimes
ninja install-clang-stripped install-clang-resource-headers install-runtimes-stripped
test -x "$PREFIX/bin/clang++"
"$PREFIX/bin/clang++" --version
grep -E 'CMAKE_BUILD_TYPE|LLVM_TARGETS_TO_BUILD|LLVM_ENABLE_ASSERTIONS|LIBCXX_HARDENING_MODE|LLVM_ENABLE_RUNTIMES|LLVM_DEFAULT_TARGET_TRIPLE' \
    CMakeCache.txt runtimes/runtimes-bins/CMakeCache.txt 2>/dev/null | sort -u > "$LOGDIR/cmake-cache.txt" || true
