#!/bin/bash
# H1 build entry: one script for perf-gcc16.yml, the Mac VM and the cloud.
#
#   tools/perf/h1_build.sh <gcc|clang> <out> [steps]
#
# steps (default "deps slg audit tests"): any of
#   deps   protobuf v21.12 (static, per toolchain), FlatBuffers v25.12.19 (library per toolchain;
#          flatc and the generated header are made once and shared, see FB_GEN), cereal v1.3.2 headers
#   slg    bench/slg slg_bench (SLG_WITH_XOFFSET=ON, this checkout as XOFFSET_ROOT)
#   audit  tools/perf/path_audit (gate V6) and tools/perf/slg_strlen (string lengths)
#   tests  the library's full Release CTest build (build only; run ctest yourself)
#
# Environment:
#   DEPS_SRC     directory with protobuf-v21.12, flatbuffers-v25.12.19, cereal-v1.3.2
#                (and optionally zpp_bits-v4.7.6, cista-v0.16); missing ones are cloned and their
#                commits checked
#   CLANG_ROOT   clang-p2996 install prefix (clang only; default /opt/p2996-toolchain)
#   CMAKE        cmake binary (default: cmake on PATH)
#   GENERATOR    CMake generator (default: Ninja if available, else Unix Makefiles)
#   JOBS         parallel jobs (default: nproc)
#   SLG_SRC      SLG tree (default: <checkout>/bench/slg)
#   FB_GEN       directory with a shared slg_generated.h (written by the first build if empty)
#   EXTRA_SLG    "zpp_cista" also builds the zpp_bits and cista columns (secondary, report only)
#   DEPS_PREFIX  dependency install prefix (default <out>/deps); point a second build at the first
#                build's prefix to reuse its libraries (the -O2 build does)
#   SDKROOT      macOS only (host mode for the Mac bridge's native build; steps "deps slg"):
#                the SDK path, passed as CMAKE_OSX_SYSROOT
#   OPT          optimisation flags replacing Release's default (-O3 -DNDEBUG), e.g. "-O2 -DNDEBUG"
#                for the secondary GCC -O2 vs -O3 run (H1 section 5.6)
#
# Fairness rules (H1 design section 4): CMake Release (-O3 -DNDEBUG), no -march/-mcpu, no LTO,
# identical CMake arguments for both toolchains except the compiler and -stdlib=libc++.
set -euo pipefail

TC=${1:?usage: h1_build.sh <gcc|clang> <out> [steps]}
OUT=$(mkdir -p "$2" && cd "$2" && pwd)
STEPS=${3:-"deps slg audit tests"}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
CMAKE=${CMAKE:-cmake}
JOBS=${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu)}
SLG_SRC=${SLG_SRC:-$ROOT/bench/slg}
DEPS_SRC=${DEPS_SRC:-$OUT/../deps-src}
if [ -z "${GENERATOR:-}" ]; then
  if command -v ninja >/dev/null; then GENERATOR=Ninja; else GENERATOR="Unix Makefiles"; fi
fi

case "$TC" in
  gcc)
    CC_=${CC_GCC:-gcc}; CXX_=${CXX_GCC:-g++}
    STDFLAGS=""
    ;;
  clang)
    CLANG_ROOT=${CLANG_ROOT:-/opt/p2996-toolchain}
    CC_=$CLANG_ROOT/bin/clang; CXX_=$CLANG_ROOT/bin/clang++
    STDFLAGS="-stdlib=libc++"
    # Runtime libc++ next to the toolchain (dynamic, H1 section 4.6).
    if [ "$(uname -s)" = Darwin ]; then SO=dylib; else SO=so; fi
    LIBCXX=$("$CXX_" $STDFLAGS -print-file-name=libc++.$SO)
    if [ ! -e "$LIBCXX" ]; then echo "libc++.$SO not found next to $CXX_ (got '$LIBCXX')" >&2; exit 1; fi
    LIBCXX_DIR=$(cd "$(dirname "$LIBCXX")" && pwd)
    # Runtime libraries for a runtime container without the toolchain: mount <out>/clang-rt at
    # the path in <out>/libcxx_dir.txt (the binaries' rpath).
    mkdir -p "$OUT/clang-rt"
    cp -a "$LIBCXX_DIR"/libc++.*$SO* "$LIBCXX_DIR"/libc++abi.*$SO* "$OUT/clang-rt/" 2>/dev/null ||
      cp -a "$LIBCXX_DIR"/libc++*.$SO* "$OUT/clang-rt/"
    cp -a "$LIBCXX_DIR"/libunwind*.$SO* "$OUT/clang-rt/" 2>/dev/null || true
    echo "$LIBCXX_DIR" > "$OUT/libcxx_dir.txt"
    ;;
  *) echo "unknown toolchain $TC" >&2; exit 2 ;;
esac

COMMON=(-G "$GENERATOR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$CC_" -DCMAKE_CXX_COMPILER="$CXX_"
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
# macOS host (the Mac bridge's native build, steps "deps slg"): SDK from SDKROOT.
if [ "$(uname -s)" = Darwin ]; then
  : "${SDKROOT:?set SDKROOT to the macOS SDK (e.g. MacOSX26.5.sdk) on macOS}"
  COMMON+=(-DCMAKE_OSX_SYSROOT="$SDKROOT")
fi
if [ -n "${OPT:-}" ]; then COMMON+=(-DCMAKE_CXX_FLAGS_RELEASE="$OPT" -DCMAKE_C_FLAGS_RELEASE="$OPT"); fi
if [ -n "$STDFLAGS" ]; then
  COMMON+=(-DCMAKE_CXX_FLAGS="$STDFLAGS" -DCMAKE_EXE_LINKER_FLAGS="$STDFLAGS -Wl,-rpath,$LIBCXX_DIR"
           -DCMAKE_SHARED_LINKER_FLAGS="$STDFLAGS")
fi
PREFIX=${DEPS_PREFIX:-$OUT/deps}

fetch() {  # fetch <dir> <url> <tag> <commit>
  local d=$DEPS_SRC/$1
  if [ ! -d "$d" ]; then
    mkdir -p "$DEPS_SRC"
    git clone -q --depth 1 --branch "$3" "$2" "$d"
  fi
  local got
  got=$(git -C "$d" rev-parse HEAD 2>/dev/null || cat "$d/.commit")
  if [ "$got" != "$4" ]; then echo "$1: commit $got, expected $4" >&2; exit 1; fi
}

step_deps() {
  fetch protobuf-v21.12 https://github.com/protocolbuffers/protobuf v21.12 f0dc78d7e6e331b8c6bb2d5283e06aa26883ca7c
  fetch flatbuffers-v25.12.19 https://github.com/google/flatbuffers v25.12.19 7e163021e59cca4f8e1e35a7c828b5c6b7915953
  fetch cereal-v1.3.2 https://github.com/USCiLab/cereal v1.3.2 ebef1e929807629befafbb2918ea1a08c7194554
  # protobuf keeps its own default language standard (H1 risk table).
  "$CMAKE" -S "$DEPS_SRC/protobuf-v21.12" -B "$OUT/b-protobuf" "${COMMON[@]}" -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_BUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON
  "$CMAKE" --build "$OUT/b-protobuf" --parallel "$JOBS"
  "$CMAKE" --install "$OUT/b-protobuf" >/dev/null
  "$CMAKE" -S "$DEPS_SRC/flatbuffers-v25.12.19" -B "$OUT/b-fb" "${COMMON[@]}" -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DFLATBUFFERS_BUILD_TESTS=OFF
  "$CMAKE" --build "$OUT/b-fb" --parallel "$JOBS"
  "$CMAKE" --install "$OUT/b-fb" >/dev/null
  mkdir -p "$PREFIX/include"
  rm -rf "$PREFIX/include/cereal"
  cp -R "$DEPS_SRC/cereal-v1.3.2/include/cereal" "$PREFIX/include/"
}

step_slg() {
  local extra=()
  if [ "${EXTRA_SLG:-}" = zpp_cista ]; then
    fetch zpp_bits-v4.7.6 https://github.com/eyalz800/zpp_bits v4.7.6 4a57ecd68a481c2cd6e7ee94aaf83282a735fded
    fetch cista-v0.16 https://github.com/felixguendling/cista v0.16 015b94e66c4c0d7c1ef29742383a024c84fbbba1
    extra=(-DZPP_BITS_INCLUDE="$DEPS_SRC/zpp_bits-v4.7.6" -DCISTA_INCLUDE="$DEPS_SRC/cista-v0.16/include")
  else
    extra=(-DZPP_BITS_INCLUDE=ZPP_BITS_INCLUDE-NOTFOUND -DCISTA_INCLUDE=CISTA_INCLUDE-NOTFOUND)
  fi
  # flatc from this toolchain's deps; the generated header is shared between toolchains when
  # FB_GEN is set (first build writes it, later builds check it is byte-identical).
  "$CMAKE" -S "$SLG_SRC" -B "$OUT/b-slg" "${COMMON[@]}" -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DSLG_WITH_XOFFSET=ON -DXOFFSET_ROOT="$ROOT" -DFLATC="$PREFIX/bin/flatc" \
    -DCEREAL_INCLUDE="$PREFIX/include" -DZSTD_LIB=ZSTD_LIB-NOTFOUND -DLZ4_LIB=LZ4_LIB-NOTFOUND "${extra[@]}"
  "$CMAKE" --build "$OUT/b-slg" --target slg_bench --parallel "$JOBS"
  if [ -n "${FB_GEN:-}" ]; then
    mkdir -p "$FB_GEN"
    if [ -f "$FB_GEN/slg_generated.h" ]; then
      cmp "$FB_GEN/slg_generated.h" "$OUT/b-slg/gen/slg_generated.h" || { echo "flatc output differs from $FB_GEN" >&2; exit 1; }
    else
      cp "$OUT/b-slg/gen/slg_generated.h" "$FB_GEN/"
    fi
  fi
  cp "$OUT/b-slg/slg_bench" "$OUT/slg_bench"
  "$OUT/slg_bench" --mode verify --scale S --cycles 1 --formats xoffset >/dev/null
}

step_audit() {
  "$CMAKE" -S "$ROOT/tools/perf" -B "$OUT/b-audit" "${COMMON[@]}" -DXOFFSET_ROOT="$ROOT" -DSLG_SRC="$SLG_SRC" \
    -DCEREAL_INCLUDE="$PREFIX/include"
  "$CMAKE" --build "$OUT/b-audit" --target path_audit slg_strlen --parallel "$JOBS"
  cp "$OUT/b-audit/path_audit" "$OUT/b-audit/slg_strlen" "$OUT/"
}

step_tests() {
  "$CMAKE" -S "$ROOT" -B "$OUT/b-tests" "${COMMON[@]}"
  "$CMAKE" --build "$OUT/b-tests" --parallel "$JOBS"
}

"$CXX_" --version > "$OUT/compiler_version.txt"
"$CXX_" -v -E -x c++ -std=c++26 $STDFLAGS /dev/null -o /dev/null > "$OUT/compiler_v.txt" 2>&1 || true

for s in $STEPS; do
  t0=$(date +%s)
  "step_$s"
  echo "h1_build: $TC $s $(( $(date +%s) - t0 )) s" | tee -a "$OUT/build_times.txt"
done
