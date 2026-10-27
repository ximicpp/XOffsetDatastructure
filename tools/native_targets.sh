#!/usr/bin/env bash
# Builds and checks one native client target (K8): full CTest plus the cross-compiler
# wire corpus against a reference written by the Linux x86-64 server build. Writes
# <build>/NATIVE_RESULT.md saying what actually ran.
#
#   tools/native_targets.sh TARGET [--std 20|23] [--build DIR] [--ref DIR] [-j N] [--] [cmake args]
#
# TARGET (toolchain files in cmake/toolchains/):
#   macos          host Apple clang, system libc++ (deployment target 14.0)
#   ios-sim        Xcode iOS simulator SDK; CTest through `xcrun simctl spawn`
#                  (boot a device first; -DXOFFSET_IOS_SIM_DEVICE=<udid> picks one)
#   android-arm64  NDK Clang + static libc++ ($ANDROID_NDK); CTest through qemu-aarch64
#   mingw-x64      MinGW-w64 GCC on Linux; CTest through wine
#   host           the host compiler, no toolchain file (e.g. writing the reference)
#
# --ref DIR: corpus from `test_cross_compiler_wire --write` on the reference build
# (Linux x86-64, GCC 16, C++26 reflection). This build must accept every reference wire
# (--read) and write the same bytes (diff -r). Without --ref only the corpus is written.
# Exit status: 0 when every step that ran passed.
set -uo pipefail
# Runs under macOS bash 3.2 too: no empty-array expansions under set -u.

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TARGET=${1:-}
[[ -n "$TARGET" ]] || { sed -n '2,22p' "$0"; exit 2; }
shift
STD=20
BUILD=""
REF=""
JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
EXTRA=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --std) STD=$2; shift 2 ;;
        --build) BUILD=$2; shift 2 ;;
        --ref) REF=$(cd "$2" && pwd) || exit 2; shift 2 ;;
        -j) JOBS=$2; shift 2 ;;
        --) shift; EXTRA+=("$@"); break ;;
        *) EXTRA+=("$1"); shift ;;
    esac
done
BUILD=${BUILD:-$REPO/build/native-$TARGET-c++$STD}
TC=$REPO/cmake/toolchains
ARGS=(-G Ninja -DCMAKE_BUILD_TYPE=Release -DXOFFSET_CXX_STANDARD="$STD")
case "$TARGET" in
    macos) ;;
    ios-sim) ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$TC/ios-simulator.cmake") ;;
    android-arm64) ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$TC/android-arm64.cmake" -DXOFFSET_ANDROID_QEMU=ON) ;;
    mingw-x64) ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$TC/mingw-w64-x86_64.cmake") ;;
    host) ;;
    *) echo "unknown target: $TARGET" >&2; exit 2 ;;
esac
mkdir -p "$BUILD"
BUILD=$(cd "$BUILD" && pwd)
LOG=$BUILD/native-logs
mkdir -p "$LOG"
R=$BUILD/NATIVE_RESULT.md
STATUS=0
rows=()
step() {  # step NAME LOGFILE CMD...: run, record one result row
    local name=$1 log=$2; shift 2
    if "$@" > "$log" 2>&1; then
        rows+=("| $name | passed | \`${log#$BUILD/}\` |")
    else
        rows+=("| $name | **failed** (rc $?) | \`${log#$BUILD/}\` |")
        STATUS=1
    fi
}

step configure "$LOG/configure.log" cmake -S "$REPO" -B "$BUILD" "${ARGS[@]}" ${EXTRA[@]+"${EXTRA[@]}"}
if [[ $STATUS -eq 0 ]]; then
    step build "$LOG/build.log" cmake --build "$BUILD" --parallel "$JOBS"
fi
BUILT=$STATUS
if [[ $STATUS -eq 0 ]]; then
    step ctest "$LOG/ctest.log" ctest --test-dir "$BUILD" --output-on-failure --parallel "$JOBS" --timeout 1800
    summary=$(grep -E 'tests passed|tests failed' "$LOG/ctest.log" | tail -1)
    [[ -n "$summary" ]] && rows+=("| ctest summary | $summary | |")
fi

# Run the corpus program exactly as CTest runs it: with the toolchain's emulator
# (qemu, wine, simctl), which is not a cache variable.
CW_CMD=()
line=$(ctest --test-dir "$BUILD" -N -V -R '^test_cross_compiler_wire$' 2>/dev/null |
    sed -n 's/^[0-9]*: Test command: //p' | head -1)
[[ -n "$line" ]] && eval "CW_CMD=($line)"
CW=""
[[ ${#CW_CMD[@]} -gt 0 ]] && CW=${CW_CMD[${#CW_CMD[@]}-1]}
# The emulator prefix: the words before the first test's program in the build tree.
ANY_CMD=()
line=$(ctest --test-dir "$BUILD" -N -V 2>/dev/null | sed -n 's/^[0-9]*: Test command: //p' | head -1)
[[ -n "$line" ]] && eval "ANY_CMD=($line)"
EMU=""
for word in ${ANY_CMD[@]+"${ANY_CMD[@]}"}; do
    [[ "$word" == "$BUILD"/* ]] && break
    EMU="${EMU:+$EMU }$word"
done
# The corpus is checked whenever the build passed, also after a failed CTest.
if [[ $BUILT -eq 0 && -n "$CW" ]]; then
    rm -rf "$BUILD/corpus"
    step "corpus write" "$LOG/corpus-write.log" "${CW_CMD[@]}" --write "$BUILD/corpus"
    if [[ -n "$REF" ]]; then
        step "corpus read (reference wires accepted, same bytes)" "$LOG/corpus-read.log" "${CW_CMD[@]}" --read "$REF"
        step "corpus diff -r against reference" "$LOG/corpus-diff.log" diff -r "$REF" "$BUILD/corpus"
    else
        rows+=("| corpus read / diff | not run: no --ref | |")
    fi
elif [[ $BUILT -eq 0 ]]; then
    rows+=("| corpus | **not run**: test_cross_compiler_wire was not built | |")
    STATUS=1
fi

CXX=$(sed -n 's/^set(CMAKE_CXX_COMPILER "\(.*\)")$/\1/p' "$BUILD"/CMakeFiles/*/CMakeCXXCompiler.cmake 2>/dev/null | head -1)
{
    echo "# Native target $TARGET, C++$STD"
    echo
    echo "- $(date -u +%Y-%m-%dT%H:%M:%SZ), $(git -C "$REPO" rev-parse HEAD 2>/dev/null)$(git -C "$REPO" diff --quiet 2>/dev/null || echo ' (dirty)')"
    echo "- compiler: \`$CXX\` — $("${CXX:-false}" --version 2>/dev/null | head -1)"
    echo "- CTest runner: ${EMU:-native}"
    echo "- extra CMake arguments: ${EXTRA[*]+${EXTRA[*]}}"
    [[ -n "$REF" ]] && echo "- reference corpus: \`$REF\` ($(ls "$REF" | wc -l) files)"
    echo
    echo "| step | result | log |"
    echo "| --- | --- | --- |"
    printf '%s\n' "${rows[@]}"
} > "$R"
cat "$R"
exit $STATUS
