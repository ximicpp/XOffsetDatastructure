#!/bin/bash
# Native macOS arm64 build of the pinned code generators / runtimes used by the
# serialization_comparison bench.  NOT executed by the preparation agent; run by
# the single heavy executor.  Each step refuses to reuse an existing build or
# install directory.
#
# Outputs (all under /Volumes/XOffsetCS/mac-validation/install):
#   flatbuffers/       flatc + headers (+ libflatbuffers.a, unused by the bench)
#   capnproto/         capnp, capnpc-c++, libcapnp.a, libkj.a, headers   (normal)
#   capnproto-asan/    same, compiled with -fsanitize=address,undefined   (SAN link)
#
# Compiler choice: everything is built with the P2996 clang + its own libc++
# (/Volumes/XOffsetCS/toolchain/p2996-d77eff1), i.e. the same compiler/standard
# library as the sc_* drivers.  This is REQUIRED for libcapnp.a/libkj.a, which
# are statically linked into sc_capnproto and expose std:: types across the ABI.
# For the pure generators (flatc, capnp, capnpc-c++) it is not strictly
# required: they only emit text, so AppleClang + system libc++ would produce
# byte-identical generated sources.  We nevertheless use the P2996 toolchain for
# them too, so that the closure has one compiler, the capnp tool comes from the
# same build tree as its runtime (as in the historical "same-compiler installed
# Cap'n Proto runtime/tools"), and no second libc++ is loaded anywhere.
# The SAN bench links capnproto-asan/lib but (as historically) still runs the
# normal capnp/capnpc-c++ generators.
set -euo pipefail

T=/Volumes/XOffsetCS/toolchain/p2996-d77eff1
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
CMAKE=/opt/homebrew/bin/cmake
NINJA=/opt/homebrew/bin/ninja
SRC=/Volumes/XOffsetCS/competitors/sources
ROOT=/Volumes/XOffsetCS/mac-validation
BUILD=$ROOT/competitor-build
INSTALL=$ROOT/install
JOBS=${JOBS:-8}

# Deterministic, minimal environment for the builds.
export LC_ALL=C LANG=C TZ=UTC PATH=/usr/bin:/bin:/opt/homebrew/bin
unset CPATH LIBRARY_PATH CPLUS_INCLUDE_PATH DYLD_LIBRARY_PATH SDKROOT MACOSX_DEPLOYMENT_TARGET || true

COMMON=(
  -G Ninja
  -DCMAKE_MAKE_PROGRAM=$NINJA
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_C_COMPILER=$T/bin/clang
  -DCMAKE_CXX_COMPILER=$T/bin/clang++
  -DCMAKE_OSX_SYSROOT=$SDK
  -DCMAKE_OSX_ARCHITECTURES=arm64
  "-DCMAKE_EXE_LINKER_FLAGS=-L$T/lib -Wl,-rpath,$T/lib"
  "-DCMAKE_SHARED_LINKER_FLAGS=-L$T/lib -Wl,-rpath,$T/lib"
)
NORMAL_CXX="-stdlib=libc++ -isysroot $SDK"
ASAN_CXX="-stdlib=libc++ -isysroot $SDK -fsanitize=address,undefined -fno-omit-frame-pointer -g"

fresh() { for d in "$@"; do if [ -e "$d" ]; then echo "refusing to reuse $d" >&2; exit 2; fi; done; }

step_flatbuffers() {
  fresh "$BUILD/flatbuffers" "$INSTALL/flatbuffers"
  $CMAKE -S "$SRC/flatbuffers-v25.12.19" -B "$BUILD/flatbuffers" "${COMMON[@]}" \
    "-DCMAKE_CXX_FLAGS=$NORMAL_CXX" "-DCMAKE_C_FLAGS=-isysroot $SDK" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL/flatbuffers" \
    -DFLATBUFFERS_BUILD_TESTS=OFF -DFLATBUFFERS_BUILD_FLATC=ON -DFLATBUFFERS_BUILD_FLATLIB=ON \
    -DFLATBUFFERS_BUILD_FLATHASH=OFF -DFLATBUFFERS_BUILD_BENCHMARKS=OFF -DFLATBUFFERS_BUILD_SHAREDLIB=OFF \
    -DFLATBUFFERS_STATIC_FLATC=OFF -DFLATBUFFERS_INSTALL=ON -DFLATBUFFERS_STRICT_MODE=OFF \
    > "$BUILD/flatbuffers.configure.log" 2>&1 || { tail -50 "$BUILD/flatbuffers.configure.log"; exit 1; }
  $CMAKE --build "$BUILD/flatbuffers" --parallel "$JOBS" > "$BUILD/flatbuffers.build.log" 2>&1
  $CMAKE --install "$BUILD/flatbuffers" > "$BUILD/flatbuffers.install.log" 2>&1
  "$INSTALL/flatbuffers/bin/flatc" --version
  /usr/bin/otool -L "$INSTALL/flatbuffers/bin/flatc"
}

# $1 = install name (capnproto | capnproto-asan), $2 = CXX flags
step_capnproto() {
  local name=$1 flags=$2 extra_link=""
  [ "$name" = capnproto-asan ] && extra_link="-fsanitize=address,undefined"
  # capnp 1.5.0 kj/async.c++ references FiberStack::impl under ASan even with
  # KJ_USE_FIBERS=0 (compile error), so the ASan runtime keeps fibers enabled.
  local fibers=OFF; [ "$name" = capnproto-asan ] && fibers=ON
  fresh "$BUILD/$name" "$INSTALL/$name"
  ASAN_OPTIONS=detect_leaks=0 MallocNanoZone=0 \
  $CMAKE -S "$SRC/capnproto-v1.5.0" -B "$BUILD/$name" "${COMMON[@]}" \
    "-DCMAKE_CXX_FLAGS=$flags" \
    "-DCMAKE_EXE_LINKER_FLAGS=-L$T/lib -Wl,-rpath,$T/lib $extra_link" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL/$name" \
    -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=OFF \
    -DWITH_OPENSSL=OFF -DWITH_ZLIB=OFF -DWITH_FIBERS=$fibers \
    > "$BUILD/$name.configure.log" 2>&1 || { tail -50 "$BUILD/$name.configure.log"; exit 1; }
  ASAN_OPTIONS=detect_leaks=0 MallocNanoZone=0 \
  $CMAKE --build "$BUILD/$name" --parallel "$JOBS" > "$BUILD/$name.build.log" 2>&1
  $CMAKE --install "$BUILD/$name" > "$BUILD/$name.install.log" 2>&1
  ls -l "$INSTALL/$name/lib/libcapnp.a" "$INSTALL/$name/lib/libkj.a" "$INSTALL/$name/bin/capnp" "$INSTALL/$name/bin/capnpc-c++"
}

step_verify() {
  # Generators must run natively and the runtime archives must be arm64.
  "$INSTALL/capnproto/bin/capnp" --version
  /usr/bin/otool -L "$INSTALL/capnproto/bin/capnp" "$INSTALL/capnproto/bin/capnpc-c++"
  /usr/bin/lipo -info "$INSTALL/capnproto/lib/libcapnp.a" "$INSTALL/capnproto-asan/lib/libcapnp.a"
  # The ASan archive must carry ASan instrumentation symbols.
  $T/bin/llvm-nm "$INSTALL/capnproto-asan/lib/libkj.a" 2>/dev/null | grep -q '___asan_' && echo "capnproto-asan: instrumented"
  shasum -a 256 "$INSTALL"/flatbuffers/bin/flatc "$INSTALL"/capnproto/bin/capnp "$INSTALL"/capnproto/bin/capnpc-c++ \
    "$INSTALL"/capnproto/lib/libcapnp.a "$INSTALL"/capnproto/lib/libkj.a \
    "$INSTALL"/capnproto-asan/lib/libcapnp.a "$INSTALL"/capnproto-asan/lib/libkj.a > "$INSTALL/IDENTITIES.sha256"
}

mkdir -p "$BUILD" "$INSTALL"
case "${1:-all}" in
  flatbuffers) step_flatbuffers ;;
  capnproto) step_capnproto capnproto "$NORMAL_CXX" ;;
  capnproto-asan) step_capnproto capnproto-asan "$ASAN_CXX" ;;
  verify) step_verify ;;
  all)
    step_flatbuffers
    step_capnproto capnproto "$NORMAL_CXX"
    step_capnproto capnproto-asan "$ASAN_CXX"
    step_verify ;;
  *) echo "usage: $0 [all|flatbuffers|capnproto|capnproto-asan|verify]" >&2; exit 2 ;;
esac
