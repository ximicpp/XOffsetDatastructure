#!/bin/bash

# ============================================================================
# build_and_run.sh - Build and run the typelayout demo
# For Linux/WSL environments with Clang P2996 (Bloomberg fork)
# ============================================================================

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default configuration
BUILD_TYPE="Release"
VERBOSE=0
NUM_JOBS=$(nproc 2>/dev/null || echo 4)
SHOW_HELP=0

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --verbose|-v)
            VERBOSE=1
            shift
            ;;
        -j)
            NUM_JOBS="$2"
            shift 2
            ;;
        --help|-h)
            SHOW_HELP=1
            shift
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            SHOW_HELP=1
            shift
            ;;
    esac
done

# Show help
if [ $SHOW_HELP -eq 1 ]; then
    echo ""
    echo "Usage: ./build_and_run.sh [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --debug             Build in Debug mode instead of Release"
    echo "  --verbose, -v       Show verbose build output"
    echo "  -j N                Use N parallel jobs (default: $(nproc))"
    echo "  --help, -h          Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./build_and_run.sh                - Build and run in Release mode"
    echo "  ./build_and_run.sh --debug        - Build and run in Debug mode"
    echo "  ./build_and_run.sh -j 8           - Build with 8 parallel jobs"
    echo ""
    exit 0
fi

# Print header
echo ""
echo -e "${CYAN}======================================================================${NC}"
echo -e "${CYAN}  TypeLayout - Build Script                                          ${NC}"
echo -e "${CYAN}  (Using Clang P2996 with Static Reflection Support)                 ${NC}"
echo -e "${CYAN}======================================================================${NC}"
echo ""

# Check for Clang P2996
CLANG_P2996_PATH="$HOME/clang-p2996-install/bin/clang++"
CLANG_P2996_C_PATH="$HOME/clang-p2996-install/bin/clang"

if [ ! -f "$CLANG_P2996_PATH" ]; then
    echo -e "${RED}Error: Clang P2996 not found at $CLANG_P2996_PATH${NC}"
    echo -e "${YELLOW}Please install Bloomberg Clang P2996 fork to ~/clang-p2996-install${NC}"
    echo -e "${YELLOW}See: https://github.com/bloomberg/clang-p2996${NC}"
    exit 1
fi

# Display configuration
echo -e "${BLUE}Configuration:${NC}"
echo -e "  Compiler:      ${GREEN}Clang P2996 ($CLANG_P2996_PATH)${NC}"
echo -e "  Build Type:    ${GREEN}$BUILD_TYPE${NC}"
echo -e "  Parallel Jobs: ${GREEN}$NUM_JOBS${NC}"
echo ""

# Set up compiler flags
CMAKE_CXX_COMPILER="$CLANG_P2996_PATH"
CMAKE_C_COMPILER="$CLANG_P2996_C_PATH"
CMAKE_CXX_FLAGS="-stdlib=libc++"
CMAKE_EXE_LINKER_FLAGS="-L$HOME/clang-p2996-install/lib -Wl,-rpath,$HOME/clang-p2996-install/lib"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo -e "${BLUE}Configuring CMake...${NC}"
echo ""

CMAKE_CMD="cmake .."
CMAKE_CMD="$CMAKE_CMD -DCMAKE_BUILD_TYPE=$BUILD_TYPE"
CMAKE_CMD="$CMAKE_CMD -DCMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER"
CMAKE_CMD="$CMAKE_CMD -DCMAKE_C_COMPILER=$CMAKE_C_COMPILER"
CMAKE_CMD="$CMAKE_CMD -DCMAKE_CXX_FLAGS='$CMAKE_CXX_FLAGS'"
CMAKE_CMD="$CMAKE_CMD -DCMAKE_EXE_LINKER_FLAGS='$CMAKE_EXE_LINKER_FLAGS'"

if [ $VERBOSE -eq 1 ]; then
    echo -e "${CYAN}Running: $CMAKE_CMD${NC}"
fi

eval $CMAKE_CMD

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed${NC}"
    cd ..
    exit 1
fi

# Build
echo ""
echo -e "${BLUE}Building project...${NC}"
echo ""

BUILD_CMD="cmake --build . --config $BUILD_TYPE -j$NUM_JOBS"

if [ $VERBOSE -eq 1 ]; then
    BUILD_CMD="$BUILD_CMD --verbose"
    echo -e "${CYAN}Running: $BUILD_CMD${NC}"
fi

eval $BUILD_CMD

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    cd ..
    exit 1
fi

echo ""
echo -e "${GREEN}[OK] Build completed successfully!${NC}"

# Run the demo
echo ""
echo -e "${CYAN}======================================================================${NC}"
echo -e "${CYAN}  Running TypeLayout Demo                                             ${NC}"
echo -e "${CYAN}======================================================================${NC}"
echo ""

# Find the demo executable
DEMO_PATH=""
if [ -f "./typelayout_demo" ]; then
    DEMO_PATH="./typelayout_demo"
elif [ -f "./bin/typelayout_demo" ]; then
    DEMO_PATH="./bin/typelayout_demo"
elif [ -f "./bin/$BUILD_TYPE/typelayout_demo" ]; then
    DEMO_PATH="./bin/$BUILD_TYPE/typelayout_demo"
fi

if [ -n "$DEMO_PATH" ] && [ -f "$DEMO_PATH" ]; then
    $DEMO_PATH
    
    if [ $? -eq 0 ]; then
        echo ""
        echo -e "${GREEN}[OK] Demo completed successfully!${NC}"
    else
        echo ""
        echo -e "${RED}[FAIL] Demo failed!${NC}"
        cd ..
        exit 1
    fi
else
    echo -e "${RED}Error: Demo executable not found${NC}"
    echo -e "${YELLOW}Searched paths:${NC}"
    echo -e "  - ./typelayout_demo"
    echo -e "  - ./bin/typelayout_demo"
    echo -e "  - ./bin/$BUILD_TYPE/typelayout_demo"
    cd ..
    exit 1
fi

cd ..

# Final summary
echo ""
echo -e "${CYAN}======================================================================${NC}"
echo -e "${CYAN}  Summary                                                             ${NC}"
echo -e "${CYAN}======================================================================${NC}"
echo ""
echo -e "  ${GREEN}[OK] Build successful${NC}"
echo -e "  ${GREEN}[OK] All static_assert checks passed at compile time${NC}"
echo -e "  ${GREEN}[OK] Demo executed successfully${NC}"
echo ""
echo -e "${CYAN}======================================================================${NC}"
echo ""
