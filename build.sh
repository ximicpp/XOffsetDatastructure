#!/bin/bash

# XOffsetDatastructure2 Build Script (with Reflection Support)
# For Linux/WSL environments

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Default configuration
USE_CLANG_P2996=1
ENABLE_REFLECTION=1
BUILD_TYPE="Release"
SHOW_HELP=0
VERBOSE=0
NUM_JOBS=$(nproc 2>/dev/null || echo 4)

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
    echo "  -j N                Use N parallel jobs (default: $(nproc))"
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
echo -e "${CYAN}  XOffsetDatastructure2 Build Script (with Reflection Support)${NC}"
echo -e "${CYAN}======================================================================${NC}"
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
    CLANG_P2996_PATH="$HOME/clang-p2996-install/bin/clang++"
    
    if [ ! -f "$CLANG_P2996_PATH" ]; then
        echo -e "${RED}Error: Clang P2996 not found at $CLANG_P2996_PATH${NC}"
        echo -e "${YELLOW}Please install Clang P2996 or use --no-p2996 flag${NC}"
        exit 1
    fi
    
    CMAKE_CXX_COMPILER="$HOME/clang-p2996-install/bin/clang++"
    CMAKE_C_COMPILER="$HOME/clang-p2996-install/bin/clang"
    CMAKE_CXX_FLAGS="-stdlib=libc++"
    CMAKE_EXE_LINKER_FLAGS="-L$HOME/clang-p2996-install/lib -Wl,-rpath,$HOME/clang-p2996-install/lib"
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
    TOTAL_TESTS=18
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

# Reflection tests (12 tests) - only if enabled
if [ $ENABLE_REFLECTION -eq 1 ]; then
    echo -e "${YELLOW}=== Reflection Tests ===${NC}"
    echo ""
    
    run_test "test_reflection_operators" 7 $TOTAL_TESTS
    run_test "test_member_iteration" 8 $TOTAL_TESTS
    run_test "test_reflection_type_signature" 9 $TOTAL_TESTS
    run_test "test_splice_operations" 10 $TOTAL_TESTS
    run_test "test_type_introspection" 11 $TOTAL_TESTS
    run_test "test_reflection_compaction" 12 $TOTAL_TESTS
    run_test "test_reflection_serialization" 13 $TOTAL_TESTS
    run_test "test_reflection_comparison" 14 $TOTAL_TESTS
    run_test "test_field_limit_fix" 15 $TOTAL_TESTS
    run_test "test_class_type_signatures" 16 $TOTAL_TESTS
    run_test "test_type_safety" 17 $TOTAL_TESTS
    run_test "test_vptr_layout" 18 $TOTAL_TESTS
fi

# Run demo
echo -e "${CYAN}======================================================================${NC}"
echo -e "${CYAN}Running XOffsetDatastructure Demo v2${NC}"
echo -e "${CYAN}======================================================================${NC}"
echo ""

# Try both possible paths
DEMO_PATH="bin/xoffsetdatastructure2_demo"
if [ ! -f "$DEMO_PATH" ]; then
    DEMO_PATH="bin/$BUILD_TYPE/xoffsetdatastructure2_demo"
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
    echo -e "${YELLOW}Checked paths: bin/xoffsetdatastructure2_demo and bin/$BUILD_TYPE/xoffsetdatastructure2_demo${NC}"
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