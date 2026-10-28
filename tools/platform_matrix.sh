#!/usr/bin/env bash
# Builds one platform-matrix target (v1.1 O7): full CTest plus the cross-compiler wire
# corpus against a reference written by the Linux x86-64 GCC 16 build. Writes
# <build>/PLATFORM_RESULT.md saying what actually ran.
#
#   tools/platform_matrix.sh TARGET [--build DIR] [--ref DIR] [-j N] [--] [cmake args]
#
# TARGET:
#   mingw-x64   MinGW-w64 GCC 16 on Linux (cmake/toolchains/mingw-w64-x86_64.cmake);
#               CTest through wine
#   host        the host compiler, no toolchain file: Linux x86-64 or arm64 with GCC 16
#               (-DCMAKE_CXX_COMPILER=g++), P2996 Clang, macOS; also writes the reference
#
# --ref DIR: corpus from `test_cross_compiler_wire --write` on the reference build
# (Linux x86-64, GCC 16). This build must accept every reference wire (--read) and
# write the same bytes (diff -r). Without --ref only the corpus is written.
# Exit status: 0 when every step that ran passed.
set -uo pipefail
# Runs under macOS bash 3.2 too: no empty-array expansions under set -u.

REPO=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
TARGET=${1:-}
[[ -n "$TARGET" ]] || { sed -n '2,17p' "$0"; exit 2; }
shift
BUILD=""
REF=""
JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
EXTRA=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --build) BUILD=$2; shift 2 ;;
        --ref) REF=$(cd "$2" && pwd) || exit 2; shift 2 ;;
        -j) JOBS=$2; shift 2 ;;
        --) shift; EXTRA+=("$@"); break ;;
        *) EXTRA+=("$1"); shift ;;
    esac
done
BUILD=${BUILD:-$REPO/build/platform-$TARGET}
TC=$REPO/cmake/toolchains
ARGS=(-DCMAKE_BUILD_TYPE=Release)
command -v ninja > /dev/null && ARGS+=(-G Ninja)
case "$TARGET" in
    mingw-x64) ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$TC/mingw-w64-x86_64.cmake") ;;
    host) ;;
    *) echo "unknown target: $TARGET" >&2; exit 2 ;;
esac
mkdir -p "$BUILD"
BUILD=$(cd "$BUILD" && pwd)
LOG=$BUILD/platform-logs
mkdir -p "$LOG"
R=$BUILD/PLATFORM_RESULT.md
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
    skipped=$(grep -E '^[[:space:]]*[0-9]+ - .*\(Skipped\)' "$LOG/ctest.log" | sed -E 's/^[[:space:]]*[0-9]+ - ([^ ]+).*/\1/' | tr '\n' ' ')
    [[ -n "$skipped" ]] && rows+=("| ctest skipped | ${skipped% } | |")
fi

# Run the corpus program exactly as CTest runs it: with the toolchain's emulator
# (wine, qemu), which is not a cache variable.
CW_CMD=()
line=$(ctest --test-dir "$BUILD" -N -V -R '^test_cross_compiler_wire$' 2>/dev/null |
    sed -n 's/^[0-9]*: Test command: //p' | head -1)
[[ -n "$line" ]] && eval "CW_CMD=($line)"
CW=""
[[ ${#CW_CMD[@]} -gt 0 ]] && CW=${CW_CMD[${#CW_CMD[@]}-1]}
# The emulator prefix: the words before the program in the corpus test's command.
EMU=""
for word in ${CW_CMD[@]+"${CW_CMD[@]}"}; do
    [[ "$word" == "$CW" ]] && break
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
    echo "# Platform target $TARGET"
    echo
    if REV=$(git -C "$REPO" rev-parse HEAD 2>/dev/null); then
        git -C "$REPO" diff --quiet || REV="$REV (dirty)"
    elif [[ -s "$REPO/.xo-commit" ]]; then
        REV="$(tr -d ' \r\n' < "$REPO/.xo-commit") (exported source tree, from .xo-commit)"
    else
        REV="source tree without .git (exported)"
    fi
    echo "- $(date -u +%Y-%m-%dT%H:%M:%SZ), $REV"
    echo "- compiler: \`$CXX\` — $("${CXX:-false}" --version 2>/dev/null | head -1)"
    echo "- machine: $(uname -srm)"
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
