#!/bin/bash

# XOffsetDatastructure Build Script (with Reflection Support)
# For Linux/WSL environments

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Platform detection
detect_platform() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    else
        echo "unknown"
    fi
}

PLATFORM=$(detect_platform)

# Get CPU count (cross-platform)
get_cpu_count() {
    if [[ "$PLATFORM" == "macos" ]]; then
        sysctl -n hw.ncpu 2>/dev/null || echo 4
    else
        nproc 2>/dev/null || echo 4
    fi
}

# Find Clang P2996 compiler
find_clang_p2996() {
    # Search paths in order of preference
    local search_paths=(
        "/usr/local/bin/clang++"
        "$HOME/clang-p2996-install/bin/clang++"
        "/opt/clang-p2996/bin/clang++"
        "/opt/p2996-toolchain/bin/clang++"
    )
    
    for path in "${search_paths[@]}"; do
        if [[ -f "$path" ]]; then
            # Verify it supports reflection
            if "$path" --help 2>&1 | grep -q "freflection" || "$path" -freflection -x c++ -E - < /dev/null 2>&1 | head -1 > /dev/null; then
                echo "$path"
                return 0
            fi
        fi
    done
    
    return 1
}

# Default configuration
USE_CLANG_P2996=1
ENABLE_REFLECTION=1
BUILD_TYPE="Release"
SHOW_HELP=0
VERBOSE=0
NUM_JOBS=$(get_cpu_count)

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-p2996)
            USE_CLANG_P2996=0
            shift
            ;;
        --no-reflection)
            ENABLE_REFLECTION=0
            shift
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
    echo "Usage: ./build.sh [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --no-p2996          Use system Clang/GCC instead of Clang P2996"
    echo "  --no-reflection     Disable C++26 reflection tests"
    echo "  --debug             Build in Debug mode instead of Release"
    echo "  --verbose, -v       Show verbose build output"
    echo "  -j N                Use N parallel jobs (default: auto-detected)"
    echo "  --help, -h          Show this help message"
    echo ""
    echo "Default: Use Clang P2996 with reflection enabled in Release mode"
    echo ""
    echo "Examples:"
    echo "  ./build.sh                      - Build with Clang P2996 and reflection"
    echo "  ./build.sh --no-p2996           - Build with system compiler"
    echo "  ./build.sh --no-reflection      - Build without reflection tests"
    echo "  ./build.sh --debug              - Build in Debug mode"
    echo "  ./build.sh -j 8                 - Build with 8 parallel jobs"
    echo ""
    exit 0
fi

# Print header
echo ""
echo -e "${CYAN}======================================================================${NC}"
echo -e "${CYAN}  XOffsetDatastructure Build Script (with Reflection Support)${NC}"
echo -e "${CYAN}======================================================================${NC}"
echo ""

# Check submodules
echo -e "${BLUE}Checking submodules...${NC}"

if [ ! -f "external/typelayout/include/boost/typelayout.hpp" ]; then
    echo -e "${YELLOW}TypeLayout submodule not initialized. Initializing...${NC}"
    git submodule update --init --recursive external/typelayout
    if [ $? -ne 0 ]; then
        echo -e "${RED}Error: Failed to initialize TypeLayout submodule${NC}"
        echo -e "${YELLOW}Try: git submodule update --init --recursive${NC}"
        exit 1
    fi
fi

if [ ! -d "external/boost" ]; then
    echo -e "${YELLOW}Boost submodule not initialized. Initializing...${NC}"
    git submodule update --init external/boost
fi

echo -e "${GREEN}Submodules OK${NC}"
echo ""

# Display configuration
echo -e "${BLUE}Configuration:${NC}"
if [ $USE_CLANG_P2996 -eq 1 ]; then
    echo -e "  Compiler: ${GREEN}Clang P2996 (~/clang-p2996-install/bin/clang++)${NC}"
else
    echo -e "  Compiler: ${YELLOW}System default (clang++ or g++)${NC}"
fi

if [ $ENABLE_REFLECTION -eq 1 ]; then
    echo -e "  Reflection: ${GREEN}ENABLED${NC}"
else
    echo -e "  Reflection: ${YELLOW}DISABLED${NC}"
fi

echo -e "  Build Type: ${GREEN}$BUILD_TYPE${NC}"
echo -e "  Parallel Jobs: ${GREEN}$NUM_JOBS${NC}"
echo ""

# Set up compiler
CMAKE_CXX_COMPILER=""
CMAKE_C_COMPILER=""
CMAKE_CXX_FLAGS=""
CMAKE_EXE_LINKER_FLAGS=""

if [ $USE_CLANG_P2996 -eq 1 ]; then
    # Use dynamic compiler discovery
    CLANG_P2996_PATH=$(find_clang_p2996)
    
    if [ -z "$CLANG_P2996_PATH" ]; then
        echo -e "${RED}Error: Clang P2996 not found in standard locations${NC}"
        echo -e "${YELLOW}Searched: /usr/local/bin, ~/clang-p2996-install, /opt/clang-p2996${NC}"
        echo -e "${YELLOW}Please install Clang P2996 or use --no-p2996 flag${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Found Clang P2996 at: $CLANG_P2996_PATH${NC}"
    
    CLANG_P2996_DIR=$(dirname "$CLANG_P2996_PATH")
    CLANG_INSTALL_DIR=$(dirname "$CLANG_P2996_DIR")
    
    CMAKE_CXX_COMPILER="$CLANG_P2996_PATH"
    CMAKE_C_COMPILER="${CLANG_P2996_DIR}/clang"
    CMAKE_CXX_FLAGS="-stdlib=libc++"
    
    # macOS specific: add SDK path
    if [[ "$PLATFORM" == "macos" ]]; then
        MACOS_SDK=$(xcrun --show-sdk-path 2>/dev/null)
        if [ -n "$MACOS_SDK" ]; then
            CMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS -isysroot $MACOS_SDK"
            echo -e "${GREEN}Using macOS SDK: $MACOS_SDK${NC}"
        fi
        CMAKE_EXE_LINKER_FLAGS="-L${CLANG_INSTALL_DIR}/lib -Wl,-rpath,${CLANG_INSTALL_DIR}/lib -Wl,-rpath,/usr/lib"
    else
        CMAKE_EXE_LINKER_FLAGS="-L${CLANG_INSTALL_DIR}/lib -Wl,-rpath,${CLANG_INSTALL_DIR}/lib"
    fi
fi

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo -e "${BLUE}Configuring CMake...${NC}"
echo ""

CMAKE_CMD="cmake .."
CMAKE_CMD="$CMAKE_CMD -DCMAKE_BUILD_TYPE=$BUILD_TYPE"

if [ -n "$CMAKE_CXX_COMPILER" ]; then
    CMAKE_CMD="$CMAKE_CMD -DCMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER"
fi

if [ -n "$CMAKE_C_COMPILER" ]; then
    CMAKE_CMD="$CMAKE_CMD -DCMAKE_C_COMPILER=$CMAKE_C_COMPILER"
fi

if [ $ENABLE_REFLECTION -eq 1 ]; then
    CMAKE_CMD="$CMAKE_CMD -DENABLE_REFLECTION_TESTS=ON"
else
    CMAKE_CMD="$CMAKE_CMD -DENABLE_REFLECTION_TESTS=OFF"
fi

if [ -n "$CMAKE_CXX_FLAGS" ]; then
    CMAKE_CMD="$CMAKE_CMD -DCMAKE_CXX_FLAGS='$CMAKE_CXX_FLAGS'"
fi

if [ -n "$CMAKE_EXE_LINKER_FLAGS" ]; then
    CMAKE_CMD="$CMAKE_CMD -DCMAKE_EXE_LINKER_FLAGS='$CMAKE_EXE_LINKER_FLAGS'"
fi

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

# Ensure libc++ can be found at runtime (needed in Docker with P2996 Clang)
if [ -d "/opt/clang-p2996/lib/x86_64-unknown-linux-gnu" ]; then
    export LD_LIBRARY_PATH="/opt/clang-p2996/lib/x86_64-unknown-linux-gnu:${LD_LIBRARY_PATH:-}"
elif [ -d "/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu" ]; then
    export LD_LIBRARY_PATH="/opt/p2996-toolchain/lib/x86_64-unknown-linux-gnu:${LD_LIBRARY_PATH:-}"
fi

# Run tests
echo ""
echo -e "${CYAN}======================================================================${NC}"
echo -e "${CYAN}Running Tests${NC}"
echo -e "${CYAN}======================================================================${NC}"
echo ""

TEST_FAILED=0
TEST_COUNT=0
PASSED_COUNT=0
SKIPPED_COUNT=0

# Helper function to run a test
run_test() {
    local test_name=$1
    local test_num=$2
    local total_tests=$3
    
    TEST_COUNT=$test_num
    
    local test_path="bin/$BUILD_TYPE/$test_name"
    
    if [ -f "$test_path" ]; then
        echo -e "${CYAN}[$test_num/$total_tests]${NC} Running ${BLUE}$test_name${NC}..."
        
        if [ $VERBOSE -eq 1 ]; then
            ./$test_path
        else
            ./$test_path > /dev/null 2>&1
        fi
        
        local result=$?
        
        if [ $result -eq 0 ]; then
            echo -e "${GREEN}✓ PASSED${NC}"
            PASSED_COUNT=$((PASSED_COUNT + 1))
        else
            echo -e "${RED}✗ FAILED${NC}"
            TEST_FAILED=1
        fi
    else
        echo -e "${CYAN}[$test_num/$total_tests]${NC} ${YELLOW}$test_name not found (skipped)${NC}"
        SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
    fi
    
    echo ""
}

# Determine total test count
TOTAL_TESTS=6
if [ $ENABLE_REFLECTION -eq 1 ]; then
    TOTAL_TESTS=23
fi

# Basic tests (6 tests)
echo -e "${YELLOW}=== Basic Tests ===${NC}"
echo ""

run_test "test_basic_types" 1 $TOTAL_TESTS
run_test "test_vector" 2 $TOTAL_TESTS
run_test "test_map_set" 3 $TOTAL_TESTS
run_test "test_nested" 4 $TOTAL_TESTS
run_test "test_compaction" 5 $TOTAL_TESTS
run_test "test_modify" 6 $TOTAL_TESTS

# Reflection tests (21 tests) - only if enabled
if [ $ENABLE_REFLECTION -eq 1 ]; then
    echo -e "${YELLOW}=== Reflection Tests ===${NC}"
    echo ""
    
    run_test "test_reflection_core" 7 $TOTAL_TESTS
    run_test "test_reflection_advanced" 8 $TOTAL_TESTS
    run_test "test_type_signatures" 9 $TOTAL_TESTS
    run_test "test_field_limit_fix" 10 $TOTAL_TESTS
    run_test "test_type_safety" 11 $TOTAL_TESTS
    run_test "test_enum_support" 12 $TOTAL_TESTS
    run_test "test_xbuffer_api" 13 $TOTAL_TESTS
    run_test "test_xstring_direct_assign" 14 $TOTAL_TESTS
    run_test "test_xhandle" 15 $TOTAL_TESTS
    run_test "test_error_paths" 16 $TOTAL_TESTS
    run_test "test_memory_efficiency" 17 $TOTAL_TESTS
    run_test "test_zero_boilerplate" 18 $TOTAL_TESTS
    run_test "test_zero_boilerplate_vector" 19 $TOTAL_TESTS
    run_test "test_complex_nesting" 20 $TOTAL_TESTS
    run_test "test_inheritance" 21 $TOTAL_TESTS
    run_test "test_adaptive_reservation" 22 $TOTAL_TESTS
    run_test "test_policy_trait" 23 $TOTAL_TESTS
fi

# ============================================================================
# Signature Export & Compatibility Check
# ============================================================================
if [ $ENABLE_REFLECTION -eq 1 ]; then
    echo ""
    echo -e "${CYAN}======================================================================${NC}"
    echo -e "${CYAN}Signature Export & Compatibility Check${NC}"
    echo -e "${CYAN}======================================================================${NC}"
    echo ""

    SIG_EXPORT_PATH="bin/export_signatures"
    if [ ! -f "$SIG_EXPORT_PATH" ]; then
        SIG_EXPORT_PATH="bin/$BUILD_TYPE/export_signatures"
    fi

    SIG_CHECK_PATH="bin/check_compat"
    if [ ! -f "$SIG_CHECK_PATH" ]; then
        SIG_CHECK_PATH="bin/$BUILD_TYPE/check_compat"
    fi

    # Step 1: Export signatures for current platform
    if [ -f "$SIG_EXPORT_PATH" ]; then
        echo -e "${BLUE}Exporting type signatures...${NC}"
        SIG_OUTPUT_DIR="../tools/sigs"
        mkdir -p "$SIG_OUTPUT_DIR"

        if ./$SIG_EXPORT_PATH "$SIG_OUTPUT_DIR/"; then
            echo -e "${GREEN}✓ Signatures exported to tools/sigs/${NC}"

            # Show which files were generated
            for sig_file in "$SIG_OUTPUT_DIR"/*.sig.hpp; do
                if [ -f "$sig_file" ]; then
                    echo -e "  ${CYAN}$(basename "$sig_file")${NC}"
                fi
            done
        else
            echo -e "${YELLOW}⚠ Signature export failed (non-fatal)${NC}"
        fi
        echo ""
    else
        echo -e "${YELLOW}export_signatures not found (skipped)${NC}"
    fi

    # Step 2: Run compatibility self-check
    if [ -f "$SIG_CHECK_PATH" ]; then
        echo -e "${BLUE}Running compatibility self-check...${NC}"

        if ./$SIG_CHECK_PATH; then
            echo -e "${GREEN}✓ Compatibility check passed${NC}"
        else
            echo -e "${YELLOW}⚠ Compatibility check failed (non-fatal)${NC}"
        fi
        echo ""
    else
        echo -e "${YELLOW}check_compat not found (skipped)${NC}"
    fi
fi

# Run demo
echo -e "${CYAN}======================================================================${NC}"
echo -e "${CYAN}Running XOffsetDatastructure Demo v2${NC}"
echo -e "${CYAN}======================================================================${NC}"
echo ""

# Try both possible paths
DEMO_PATH="bin/xoffsetdatastructure_demo"
if [ ! -f "$DEMO_PATH" ]; then
    DEMO_PATH="bin/$BUILD_TYPE/xoffsetdatastructure_demo"
fi

if [ -f "$DEMO_PATH" ]; then
    ./$DEMO_PATH
    
    if [ $? -eq 0 ]; then
        echo ""
        echo -e "${GREEN}✓ Demo completed successfully!${NC}"
    else
        echo ""
        echo -e "${RED}✗ Demo failed!${NC}"
        TEST_FAILED=1
    fi
else
    echo -e "${YELLOW}Demo executable not found (skipped)${NC}"
    echo -e "${YELLOW}Checked paths: bin/xoffsetdatastructure_demo and bin/$BUILD_TYPE/xoffsetdatastructure_demo${NC}"
fi

echo ""

# Run HelloWorld Example
echo -e "${CYAN}======================================================================${NC}"
echo -e "${CYAN}Running HelloWorld Example (with Type Signature Validation)${NC}"
echo -e "${CYAN}======================================================================${NC}"
echo ""

# Try both possible paths
HELLOWORLD_PATH="bin/helloworld"
if [ ! -f "$HELLOWORLD_PATH" ]; then
    HELLOWORLD_PATH="bin/$BUILD_TYPE/helloworld"
fi

if [ -f "$HELLOWORLD_PATH" ]; then
    ./$HELLOWORLD_PATH
    
    if [ $? -eq 0 ]; then
        echo ""
        echo -e "${GREEN}✓ HelloWorld example completed successfully!${NC}"
    else
        echo ""
        echo -e "${RED}✗ HelloWorld example failed!${NC}"
        TEST_FAILED=1
    fi
else
    echo -e "${YELLOW}HelloWorld executable not found (skipped)${NC}"
    echo -e "${YELLOW}Checked paths: bin/helloworld and bin/$BUILD_TYPE/helloworld${NC}"
fi

# Return to original directory
cd ..

# Final summary
echo ""
echo -e "${CYAN}======================================================================${NC}"
echo -e "${CYAN}  Build Summary${NC}"
echo -e "${CYAN}======================================================================${NC}"
echo ""

echo -e "  Tests Run:    ${CYAN}$TEST_COUNT${NC}"
echo -e "  Tests Passed: ${GREEN}$PASSED_COUNT${NC}"

if [ $SKIPPED_COUNT -gt 0 ]; then
    echo -e "  Tests Skipped: ${YELLOW}$SKIPPED_COUNT${NC}"
fi

if [ $TEST_FAILED -eq 0 ]; then
    FAILED_COUNT=0
    echo -e "  Tests Failed: ${GREEN}$FAILED_COUNT${NC}"
    echo ""
    echo -e "  Result: ${GREEN}ALL TESTS PASSED${NC}"
    echo ""
    echo -e "  Status: ${GREEN}✓ SUCCESS${NC}"
else
    FAILED_COUNT=$((TEST_COUNT - PASSED_COUNT))
    echo -e "  Tests Failed: ${RED}$FAILED_COUNT${NC}"
    echo ""
    echo -e "  Result: ${RED}SOME TESTS FAILED${NC}"
    echo ""
    echo -e "  Status: ${RED}✗ FAILED${NC}"
fi

echo -e "${CYAN}======================================================================${NC}"
echo ""

if [ $TEST_FAILED -eq 0 ]; then
    echo -e "${GREEN}Build, demo, and tests completed successfully!${NC}"
else
    echo -e "${YELLOW}Build and demo completed, but some tests FAILED${NC}"
fi

echo ""

exit $TEST_FAILED