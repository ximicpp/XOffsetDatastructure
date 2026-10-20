#!/bin/bash
# Configure + build the serialization_comparison bench natively on macOS arm64.
# NOT executed by the preparation agent; run by the single heavy executor after
# build_competitors.sh.  Every (source, mode) pair gets a fresh directory:
#   /Volumes/XOffsetCS/mac-validation/bench/{c14,r1m2}-{normal,san}
#
# Targets built (all of CMakeLists.txt): sc_xoffset sc_cista sc_flatbuffers
# sc_capnproto sc_cereal, sc_<lib>_contract, sc_<lib>_resource,
# sc_model_contract, sc_view_contract, sc_flatbuffers_capabilities,
# sc_cereal_capabilities, sc_capnproto_capabilities, sc_cista_capabilities
# (+ sc_sanitizer_controls in SAN mode).
#
# Mirrors the historical integration configure (Release => -O3 -DNDEBUG,
# CMAKE_CXX_FLAGS "-stdlib=libc++ -pthread", gnu++26 from the bench
# CMakeLists, SAN = same flags + -fsanitize=address,undefined -fno-omit-frame-pointer -g
# + SC_ENABLE_SANITIZERS=ON + ASan Cap'n Proto runtime, normal generators).
# macOS differences: -isysroot/CMAKE_OSX_SYSROOT, -L/-rpath to the P2996
# libc++ dylibs instead of an ld-linux host-runtime prefix, no -resource-dir
# override (the P2996 install ships lib/clang/21/lib/darwin sanitizer dylibs).
#
# usage: build_bench.sh <c14|r1m2|all> <normal|san|all> [--ctest]
set -euo pipefail

T=/Volumes/XOffsetCS/toolchain/p2996-d77eff1
SDK=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
CMAKE=/opt/homebrew/bin/cmake
CTEST=/opt/homebrew/bin/ctest
NINJA=/opt/homebrew/bin/ninja
COMP=/Volumes/XOffsetCS/competitors/sources
ROOT=/Volumes/XOffsetCS/mac-validation
BUSINESS=$ROOT/business
INSTALL=$ROOT/install
JOBS=${JOBS:-8}

export LC_ALL=C LANG=C TZ=UTC PATH=/usr/bin:/bin:/opt/homebrew/bin
unset CPATH LIBRARY_PATH CPLUS_INCLUDE_PATH DYLD_LIBRARY_PATH SDKROOT MACOSX_DEPLOYMENT_TARGET || true

source_for() {
  case "$1" in
    c14) echo /Volumes/XOffsetCS/results/full-native-c14-p2996d77-20261020/source ;;
    r1m2) echo /Volumes/XOffsetCS/candidate-r1m2-20261020 ;;
    w9a) echo /Volumes/XOffsetCS/candidates/w9a ;;
    *) echo "bad source $1" >&2; exit 2 ;;
  esac
}

build_one() {
  local id=$1 mode=$2 run_ctest=$3
  local src meta build logs flags capnp_prefix san_opt=()
  src=$(source_for "$id")
  meta=$BUSINESS/meta-$id/experiments/serialization_comparison
  build=$ROOT/bench/$id-$mode
  logs=$ROOT/bench/$id-$mode-logs
  if [ -e "$build" ] || [ -e "$logs" ]; then echo "refusing to reuse $build / $logs" >&2; exit 2; fi
  mkdir -p "$logs"
  flags="-stdlib=libc++ -pthread -isysroot $SDK"
  capnp_prefix=$INSTALL/capnproto
  if [ "$mode" = san ]; then
    flags="$flags -fsanitize=address,undefined -fno-omit-frame-pointer -g"
    capnp_prefix=$INSTALL/capnproto-asan
    san_opt=(-DSC_ENABLE_SANITIZERS=ON)
  fi
  # Record the exact argv for later binding.
  local argv=(
    $CMAKE -G Ninja -S "$meta" -B "$build"
    -DCMAKE_MAKE_PROGRAM=$NINJA
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_CXX_COMPILER=$T/bin/clang++
    -DCMAKE_OSX_SYSROOT=$SDK
    -DCMAKE_OSX_ARCHITECTURES=arm64
    "-DCMAKE_CXX_FLAGS=$flags"
    "-DCMAKE_EXE_LINKER_FLAGS=-L$T/lib -Wl,-rpath,$T/lib"
    -DSC_XOFFSET_SOURCE=$src
    -DSC_CISTA_SOURCE=$COMP/cista-v0.16
    -DSC_FLATBUFFERS_SOURCE=$COMP/flatbuffers-v25.12.19
    -DSC_CAPNPROTO_SOURCE=$COMP/capnproto-v1.5.0
    -DSC_CEREAL_SOURCE=$COMP/cereal-v1.3.2
    -DSC_CAPNPROTO_PREFIX=$capnp_prefix
    -DSC_CAPNP=$INSTALL/capnproto/bin/capnp
    -DSC_CAPNPC_CPP=$INSTALL/capnproto/bin/capnpc-c++
    -DSC_FLATC=$INSTALL/flatbuffers/bin/flatc
    ${san_opt[@]+"${san_opt[@]}"}
  )
  printf '%q ' "${argv[@]}" > "$logs/configure.argv"; echo >> "$logs/configure.argv"
  "${argv[@]}" > "$logs/configure.log" 2>&1 || { tail -60 "$logs/configure.log"; exit 1; }
  $CMAKE --build "$build" --parallel "$JOBS" > "$logs/build.log" 2>&1 || { tail -80 "$logs/build.log"; exit 1; }
  $NINJA -C "$build" -t deps > "$logs/ninja-deps.log"
  # Every xoffset production header that sc_xoffset includes must come from $src.
  grep -E '/(xoffsetdatastructure|xoffset_virtual_memory)\.hpp$' "$logs/ninja-deps.log" | sed 's/^ *//' | sort -u > "$logs/xoffset-header-deps.txt"
  test -s "$logs/xoffset-header-deps.txt" || { echo "no xoffset header dependency recorded" >&2; exit 1; }
  if grep -v "^$src/" "$logs/xoffset-header-deps.txt"; then echo "foreign xoffset header dependency" >&2; exit 1; fi
  for lib in xoffset cista flatbuffers capnproto cereal; do
    test -x "$build/bin/sc_$lib"
    /usr/bin/otool -L "$build/bin/sc_$lib" >> "$logs/otool-L.txt"
  done
  if [ "$mode" = san ]; then grep -q libclang_rt.asan_osx_dynamic "$logs/otool-L.txt"; fi
  shasum -a 256 "$build"/bin/sc_* > "$logs/binaries.sha256"
  if [ "$run_ctest" = yes ]; then
    # Contract/capability/resource tests (not part of the 80 functional children).
    # Darwin SAN caveat: sc_*_resource replaces operator new; Darwin ASan
    # interposes operator new for libc++-dylib callers, so a SAN resource-test
    # failure is a known platform deviation, not an adapter verdict.
    env -i PATH=/usr/bin:/bin LANG=C LC_ALL=C TZ=UTC \
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 MallocNanoZone=0 \
      $CTEST --test-dir "$build" --output-on-failure --timeout 120 --output-junit "$logs/ctest.xml" \
      > "$logs/ctest.log" 2>&1 || echo "ctest reported failures for $id-$mode (see $logs/ctest.log)" >&2
  fi
  echo "built $build"
}

ids=${1:?usage: build_bench.sh <c14|r1m2|all> <normal|san|all> [--ctest]}
modes=${2:?usage}
ctest=no; [ "${3:-}" = --ctest ] && ctest=yes
[ "$ids" = all ] && ids="c14 r1m2"
[ "$modes" = all ] && modes="normal san"
for id in $ids; do for mode in $modes; do build_one "$id" "$mode" "$ctest"; done; done
