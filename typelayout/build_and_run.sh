#!/bin/bash

# build_and_run.sh - Build and run typelayout demos
# For Linux/macOS/WSL with Clang P2996 (Bloomberg fork)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

BUILD_TYPE="Release"
NUM_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug) BUILD_TYPE="Debug"; shift ;;
        -j) NUM_JOBS="$2"; shift 2 ;;
        --help|-h)
            echo "Usage: ./build_and_run.sh [--debug] [-j N]"
            exit 0 ;;
        *) shift ;;
    esac
done

# Check for Clang P2996 in common locations
find_clang_p2996() {
    local candidates=(
        "/usr/local/bin/clang++"
        "$HOME/clang-p2996-install/bin/clang++"
        "/opt/clang-p2996/bin/clang++"
    )
    for path in "${candidates[@]}"; do
        if [ -f "$path" ]; then
            # Verify it's the P2996 fork
            if "$path" --version 2>/dev/null | grep -q "clang-p2996"; then
                echo "$path"
                return 0
            fi
        fi
    done
    return 1
}

CLANG_P2996=$(find_clang_p2996)
if [ -z "$CLANG_P2996" ]; then
    echo -e "${RED}Error: Clang P2996 (Bloomberg fork) not found${NC}"
    echo -e "Searched: /usr/local/bin, \$HOME/clang-p2996-install/bin, /opt/clang-p2996/bin"
    exit 1
fi

echo -e "${CYAN}=== TypeLayout Build (Clang P2996) ===${NC}"
echo -e "Build Type: ${GREEN}$BUILD_TYPE${NC}, Jobs: ${GREEN}$NUM_JOBS${NC}\n"

# Configure and build
mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"

# Get compiler install directory for library path
CLANG_DIR=$(dirname "$(dirname "$CLANG_P2996")")

cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_CXX_COMPILER=$CLANG_P2996 \
    -DCMAKE_CXX_FLAGS="-stdlib=libc++ -isysroot $(xcrun --show-sdk-path 2>/dev/null || echo '')" \
    -DCMAKE_EXE_LINKER_FLAGS="-L$CLANG_DIR/lib -Wl,-rpath,$CLANG_DIR/lib"

cmake --build . -j$NUM_JOBS

echo -e "\n${GREEN}[OK] Build completed${NC}\n"

# Run demos
run_demo() {
    local name=$1
    [ -f "./$name" ] && { ./$name; return 0; }
    [ -f "./bin/$name" ] && { ./bin/$name; return 0; }
    return 1
}

echo -e "${CYAN}=== Running Demos ===${NC}\n"
run_demo typelayout_demo
run_demo typelayout_xoffset_demo 2>/dev/null || true

echo -e "\n${GREEN}[OK] All demos completed${NC}"