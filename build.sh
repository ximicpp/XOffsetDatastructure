#!/usr/bin/env bash

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

BUILD_DIR="build"
BUILD_TYPE="Release"
VERBOSE=0
USER_COMPILER="${CXX:-}"

detect_platform() {
    case "${OSTYPE:-unknown}" in
        darwin*) echo "macos" ;;
        linux-gnu*) echo "linux" ;;
        *) echo "unknown" ;;
    esac
}

PLATFORM="$(detect_platform)"

get_cpu_count() {
    if [[ "$PLATFORM" == "macos" ]]; then
        sysctl -n hw.ncpu 2>/dev/null || echo 4
    else
        nproc 2>/dev/null || echo 4
    fi
}

NUM_JOBS="$(get_cpu_count)"

print_help() {
    cat <<'EOF'
Usage: ./build.sh [OPTIONS]

Options:
  --compiler PATH   Use a specific Clang P2996 compiler
  --debug           Configure a Debug build
  --verbose, -v     Print invoked commands and verbose CTest output
  -j N              Use N parallel build jobs
  --help, -h        Show this help

The script configures the project with CMake, builds it, runs CTest,
exports signatures into tools/sigs/, and runs the compatibility self-check.
EOF
}

run_cmd() {
    if [[ "$VERBOSE" -eq 1 ]]; then
        printf '+'
        printf ' %q' "$@"
        printf '\n'
    fi
    "$@"
}

die() {
    echo -e "${RED}Error:${NC} $*" >&2
    exit 1
}

section() {
    echo
    echo -e "${CYAN}======================================================================${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}======================================================================${NC}"
}

supports_reflection() {
    local compiler="$1"
    "$compiler" -freflection -x c++ -E - < /dev/null > /dev/null 2>&1
}

find_compiler() {
    local seen=""
    local candidate=""
    local candidates=(
        "$USER_COMPILER"
        "$(command -v clang++ 2>/dev/null || true)"
        "/usr/local/bin/clang++"
        "$HOME/clang-p2996-install/bin/clang++"
        "/opt/clang-p2996/bin/clang++"
        "/opt/p2996-toolchain/bin/clang++"
    )

    for candidate in "${candidates[@]}"; do
        [[ -n "$candidate" ]] || continue
        [[ -x "$candidate" ]] || continue
        case " $seen " in
            *" $candidate "*) continue ;;
        esac
        seen="$seen $candidate"
        if supports_reflection "$candidate"; then
            echo "$candidate"
            return 0
        fi
    done

    return 1
}

ensure_submodule() {
    local path="$1"
    local probe="$2"
    shift 2

    if [[ -e "$probe" ]]; then
        return 0
    fi

    echo -e "${YELLOW}Initializing missing submodule content for ${path}...${NC}"
    if [[ $# -gt 0 ]]; then
        run_cmd git submodule update --init "$@" "$path"
    else
        run_cmd git submodule update --init "$path"
    fi
}

find_binary() {
    local name="$1"
    local path=""
    local candidates=(
        "$BUILD_DIR/bin/$BUILD_TYPE/$name"
        "$BUILD_DIR/bin/$name"
    )

    for path in "${candidates[@]}"; do
        if [[ -x "$path" ]]; then
            echo "$path"
            return 0
        fi
    done

    return 1
}

reset_stale_cmake_cache() {
    local cache_file="$BUILD_DIR/CMakeCache.txt"
    local cached_source=""

    [[ -f "$cache_file" ]] || return 0

    cached_source="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache_file")"
    if [[ -n "$cached_source" && "$cached_source" != "$PWD" ]]; then
        echo -e "${YELLOW}Resetting stale CMake cache from ${cached_source}.${NC}"
        rm -f "$BUILD_DIR/CMakeCache.txt"
        rm -rf "$BUILD_DIR/CMakeFiles"
    fi
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --compiler)
            [[ $# -ge 2 ]] || die "--compiler requires a path"
            USER_COMPILER="$2"
            shift 2
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --verbose|-v)
            VERBOSE=1
            shift
            ;;
        -j)
            [[ $# -ge 2 ]] || die "-j requires a job count"
            NUM_JOBS="$2"
            shift 2
            ;;
        --help|-h)
            print_help
            exit 0
            ;;
        *)
            die "unknown option: $1"
            ;;
    esac
done

section "XOffsetDatastructure Build"

ensure_submodule "external/typelayout" "external/typelayout/include/boost/typelayout.hpp" --recursive
ensure_submodule "external/boost" "external/boost/CMakeLists.txt"

CXX_COMPILER="$(find_compiler)" || die "could not find a Clang compiler with -freflection support"
C_COMPILER=""
if [[ -x "$(dirname "$CXX_COMPILER")/clang" ]]; then
    C_COMPILER="$(dirname "$CXX_COMPILER")/clang"
fi

echo -e "${BLUE}Compiler:${NC} ${GREEN}${CXX_COMPILER}${NC}"
echo -e "${BLUE}Build type:${NC} ${GREEN}${BUILD_TYPE}${NC}"
echo -e "${BLUE}Jobs:${NC} ${GREEN}${NUM_JOBS}${NC}"

CMAKE_OSX_SYSROOT_VALUE=""

if [[ "$PLATFORM" == "macos" ]]; then
    MACOS_SDK="$(xcrun --show-sdk-path 2>/dev/null || true)"
    if [[ -n "$MACOS_SDK" ]]; then
        CMAKE_OSX_SYSROOT_VALUE="$MACOS_SDK"
    fi
fi

section "Configure"

reset_stale_cmake_cache

cmake_args=(
    -S .
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DCMAKE_CXX_COMPILER="$CXX_COMPILER"
    -DCMAKE_CXX_FLAGS=
    -DCMAKE_EXE_LINKER_FLAGS=
)

if [[ -n "$C_COMPILER" ]]; then
    cmake_args+=("-DCMAKE_C_COMPILER=$C_COMPILER")
fi
if [[ -n "$CMAKE_OSX_SYSROOT_VALUE" ]]; then
    cmake_args+=("-DCMAKE_OSX_SYSROOT=$CMAKE_OSX_SYSROOT_VALUE")
fi

run_cmd cmake "${cmake_args[@]}"

section "Build"
run_cmd cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j "$NUM_JOBS"

if [[ -d "/opt/clang-p2996/lib/x86_64-unknown-linux-gnu" ]]; then
    export LD_LIBRARY_PATH="/opt/clang-p2996/lib/x86_64-unknown-linux-gnu:${LD_LIBRARY_PATH:-}"
elif [[ -d "/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu" ]]; then
    export LD_LIBRARY_PATH="/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu:${LD_LIBRARY_PATH:-}"
fi

section "CTest"

ctest_args=(
    --test-dir "$BUILD_DIR"
    --output-on-failure
    -C "$BUILD_TYPE"
)
if ctest --help 2>&1 | grep -q -- "--no-tests"; then
    ctest_args+=(--no-tests=error)
fi
if [[ "$VERBOSE" -eq 1 ]]; then
    ctest_args+=(-V)
fi

run_cmd ctest "${ctest_args[@]}"

section "Signature Tools"

if export_bin="$(find_binary export_signatures)"; then
    run_cmd "$export_bin" tools/sigs
else
    echo -e "${YELLOW}export_signatures not found; skipping.${NC}"
fi

if compat_bin="$(find_binary check_compat)"; then
    run_cmd "$compat_bin"
else
    echo -e "${YELLOW}check_compat not found; skipping.${NC}"
fi

section "Done"
echo -e "${GREEN}Build and verification completed successfully.${NC}"
