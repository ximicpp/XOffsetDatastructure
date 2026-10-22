#!/usr/bin/env bash
# Smoke test for a packaged clang-p2996 toolchain. Runs inside the gcc:16
# runtime container (the same container the H1 timing uses), with the
# repository at /workspace and the toolchain unpacked at $TC.
#
#   1. identity: clang revision and the libc++ hardening default (must be none)
#   2. reflection probe (tools/p2996-toolchain/reflection_probe.cpp)
#   3. XOffset Release configure, build and full CTest
#   4. SLG verify (S and M, reuse and default protocol) if SLG_SRC is given
#
# Every stage must pass; nothing is skipped silently.
set -euo pipefail

TC=${TC:-/opt/p2996-toolchain}
OUT=${OUT:-/workspace/build/p2996-smoke}
JOBS=${JOBS:-$(nproc)}
EXPECT_COMMIT=${CLANG_P2996_COMMIT:?CLANG_P2996_COMMIT is required}
TRIPLE=$("$TC/bin/clang++" -print-target-triple)
LIBDIR="$TC/lib/$TRIPLE"
export LD_LIBRARY_PATH="$LIBDIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
mkdir -p "$OUT"

stage() { printf '\n=== %s ===\n' "$*"; }

stage "1. identity"
uname -a
"$TC/bin/clang++" --version | tee "$OUT/clang-version.txt"
grep -q "$EXPECT_COMMIT" "$OUT/clang-version.txt" || { echo "clang is not $EXPECT_COMMIT"; exit 1; }
config_site=$(find "$TC/include" -name __config_site | head -n 1)
test -n "$config_site" || { echo "no __config_site under $TC/include"; exit 1; }
echo "__config_site: $config_site"
grep -n 'HARDENING_MODE_DEFAULT\|_LIBCPP_ABI_VERSION\|_LIBCPP_ASSERTION' "$config_site" | tee "$OUT/config-site.txt"
# libc++ writes the mode as a number: none = (1 << 1) = 2 (libcxx/include/__config).
grep -Eq '^#define _LIBCPP_HARDENING_MODE_DEFAULT 2$' "$config_site" \
    || { echo "libc++ hardening default is not none (2)"; exit 1; }
"$TC/bin/clang++" -v -fsyntax-only -x c++ /dev/null 2>&1 | grep -i 'gcc installation' | tee "$OUT/gcc-installation.txt"
"$TC/bin/clang++" -### -std=c++26 -stdlib=libc++ -x c++ /dev/null -o /dev/null 2>&1 | tee "$OUT/driver.txt" | tail -n 3
ls -l "$LIBDIR"

stage "2. reflection probe"
"$TC/bin/clang++" -std=c++26 -freflection -fexpansion-statements -stdlib=libc++ -O2 \
    /workspace/tools/p2996-toolchain/reflection_probe.cpp -o "$OUT/reflection_probe"
"$OUT/reflection_probe" | tee "$OUT/reflection-probe.txt"
ldd "$OUT/reflection_probe" | tee "$OUT/reflection-probe-ldd.txt"
grep -q "$LIBDIR/libc++.so" "$OUT/reflection-probe-ldd.txt" || { echo "probe did not load the toolchain libc++"; exit 1; }

stage "3. XOffset Release CTest"
cmake -S /workspace -B "$OUT/xoffset-rel" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER="$TC/bin/clang++" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build "$OUT/xoffset-rel" --parallel "$JOBS"
ctest --test-dir "$OUT/xoffset-rel" --output-on-failure --no-tests=error \
    --parallel 2 --timeout 900 2>&1 | tee "$OUT/ctest.txt"

stage "4. SLG verify"
if [ -z "${SLG_SRC:-}" ]; then
    echo "SLG_SRC is not set: the SLG tree has not been frozen yet (see tools/p2996-toolchain/slg_source.env)."
    echo "SLG verify NOT RUN"
    exit 3
fi
DEPS="$OUT/slg-deps"
mkdir -p "$DEPS/src" "$DEPS/prefix"
CXXF="-stdlib=libc++"
fetch() { # name url tag commit-prefix
    git clone -q --depth 1 -b "$3" "$2" "$DEPS/src/$1"
    local head; head=$(git -C "$DEPS/src/$1" rev-parse HEAD)
    case "$head" in "$4"*) echo "$1 $3 $head" | tee -a "$OUT/slg-deps.txt" ;;
        *) echo "$1: $3 is $head, expected $4"; exit 1 ;; esac
}
fetch protobuf https://github.com/protocolbuffers/protobuf.git v21.12 f0dc78d7
fetch flatbuffers https://github.com/google/flatbuffers.git v25.12.19 7e163021
fetch cereal https://github.com/USCiLab/cereal.git v1.3.2 ebef1e92
cmake -S "$DEPS/src/protobuf" -B "$DEPS/build-protobuf" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER="$TC/bin/clang++" -DCMAKE_C_COMPILER="$TC/bin/clang" -DCMAKE_CXX_FLAGS="$CXXF" \
    -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_BUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_INSTALL_PREFIX="$DEPS/prefix" > "$OUT/protobuf-configure.txt"
cmake --build "$DEPS/build-protobuf" --parallel "$JOBS" --target install > "$OUT/protobuf-build.txt"
cmake -S "$DEPS/src/flatbuffers" -B "$DEPS/build-flatbuffers" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER="$TC/bin/clang++" -DCMAKE_C_COMPILER="$TC/bin/clang" -DCMAKE_CXX_FLAGS="$CXXF" \
    -DFLATBUFFERS_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX="$DEPS/prefix" > "$OUT/flatbuffers-configure.txt"
cmake --build "$DEPS/build-flatbuffers" --parallel "$JOBS" --target install > "$OUT/flatbuffers-build.txt"
cp -R "$DEPS/src/cereal/include/cereal" "$DEPS/prefix/include/"
export PATH="$DEPS/prefix/bin:$PATH"
cmake -S "$SLG_SRC" -B "$OUT/slg" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$TC/bin/clang++" \
    -DCMAKE_PREFIX_PATH="$DEPS/prefix" -DSLG_WITH_XOFFSET=ON -DXOFFSET_ROOT=/workspace \
    2>&1 | tee "$OUT/slg-configure.txt"
grep -q 'xoffset=ON' "$OUT/slg-configure.txt" || { echo "SLG: XOffset path not enabled"; exit 1; }
grep -q 'protobuf=1\|protobuf=TRUE\|protobuf=ON' "$OUT/slg-configure.txt" || { echo "SLG: protobuf not found"; exit 1; }
grep -q 'flatbuffers=/' "$OUT/slg-configure.txt" || { echo "SLG: flatc not found"; exit 1; }
grep -q 'cereal=/' "$OUT/slg-configure.txt" || { echo "SLG: cereal not found"; exit 1; }
cmake --build "$OUT/slg" --parallel "$JOBS" --target slg_bench
for scale in S M; do
    for reuse in 1 0; do
        log="$OUT/slg-verify-$scale-reuse$reuse.txt"
        if [ "$reuse" = 1 ]; then env SLG_REUSE=1 "$OUT/slg/slg_bench" --mode verify --scale "$scale" > "$log" 2>&1
        else env -u SLG_REUSE "$OUT/slg/slg_bench" --mode verify --scale "$scale" > "$log" 2>&1; fi
        tail -n 1 "$log"
        test "$(tail -n 1 "$log")" = "verify: all formats agree" || { cat "$log"; exit 1; }
        for f in xoffset cereal protobuf flatbuffers; do
            grep -q "^cycle 1 $f " "$log" || { echo "SLG $scale reuse=$reuse: format $f missing"; exit 1; }
        done
        grep '^cycle 1 ' "$log"
    done
done
echo
echo "smoke: all stages passed"
