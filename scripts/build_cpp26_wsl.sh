#!/bin/bash
# ============================================================================
# Build XOffsetDatastructure2 with Clang P2996 in WSL2
# ============================================================================

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  Build XOffsetDatastructure2 (C++26)${NC}"
echo -e "${BLUE}================================================${NC}"
echo

CLANG_P2996="$HOME/clang-p2996-install/bin/clang++"
PROJECT_DIR="/mnt/g/workspace/XOffsetDatastructure"

# Check compiler
if [ ! -f "$CLANG_P2996" ]; then
    echo -e "${RED}[ERROR] Clang P2996 not found${NC}"
    echo "   Path: $CLANG_P2996"
    echo
    echo "Please run first: wsl_build_clang_p2996.bat"
    exit 1
fi

echo -e "${GREEN}[OK] Found Clang P2996${NC}"
echo "   $CLANG_P2996"
"$CLANG_P2996" --version | head -1
echo

# Check project directory
if [ ! -d "$PROJECT_DIR" ]; then
    echo -e "${RED}[ERROR] Project directory not found${NC}"
    echo "   Path: $PROJECT_DIR"
    exit 1
fi

cd "$PROJECT_DIR"
echo -e "${GREEN}[OK] Project directory: $PROJECT_DIR${NC}"
echo

# Clean old build
if [ -d "build_cpp26" ]; then
    echo -e "${YELLOW}[WARN] Removing old build directory${NC}"
    rm -rf build_cpp26
fi

# Create build directory
mkdir build_cpp26
cd build_cpp26

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  Step 1/3: Configure CMake${NC}"
echo -e "${BLUE}================================================${NC}"
echo

# Setup compiler with libc++
LIBCXX_INCLUDE="$HOME/clang-p2996-install/include/c++/v1"
LIBCXX_LIB="$HOME/clang-p2996-install/lib"

cmake \
    -DCMAKE_CXX_COMPILER="$CLANG_P2996" \
    -DCMAKE_CXX_STANDARD=26 \
    -DCMAKE_CXX_FLAGS="-std=c++26 -freflection -stdlib=libc++ -I$LIBCXX_INCLUDE -D__cpp_reflection=202306L" \
    -DCMAKE_EXE_LINKER_FLAGS="-L$LIBCXX_LIB -Wl,-rpath,$LIBCXX_LIB -lc++ -lc++abi" \
    ..

echo
echo -e "${GREEN}[OK] CMake configuration complete${NC}"
echo

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  Step 2/3: Build${NC}"
echo -e "${BLUE}================================================${NC}"
echo

CPU_CORES=$(nproc)
echo "Building with $CPU_CORES cores"
echo

cmake --build . -j$CPU_CORES

echo
echo -e "${GREEN}[OK] Build complete${NC}"
echo

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  Step 3/3: Run Tests${NC}"
echo -e "${BLUE}================================================${NC}"
echo

# Run basic example
echo "Running helloworld..."
./bin/helloworld
echo

# Run reflection test
if [ -f "./bin/test_compaction" ]; then
    echo "Running test_compaction..."
    ./bin/test_compaction
    echo
fi

# Run complete example
if [ -f "./bin/xoffsetdatastructure2_demo" ]; then
    echo "Running demo..."
    ./bin/xoffsetdatastructure2_demo
    echo
fi

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  Build and Test Summary${NC}"
echo -e "${BLUE}================================================${NC}"
echo

echo -e "${GREEN}[OK] Build successful!${NC}"
echo
echo "Executables location:"
ls -lh bin/ 2>/dev/null || true
echo

echo "Checking reflection features:"
echo

# Create temporary test - using actual reflection syntax
cat > /tmp/quick_reflection_test.cpp << 'EOFCPP'
#include <iostream>

struct TestStruct {
    int x;
    double y;
};

int main() {
    // Test reflection splice syntax
    constexpr auto refl = ^^TestStruct;
    
    // Test member pointer
    int TestStruct::*ptr = &[:^^TestStruct::x:];
    
    TestStruct obj{42, 3.14};
    
    std::cout << "[OK]C++26 Reflection: ENABLED\n";
    std::cout << "   Splice syntax works!\n";
    std::cout << "   Test value: " << obj.*ptr << "\n";
    
    return 0;
}
EOFCPP

echo "Testing reflection features..."
if "$CLANG_P2996" -std=c++26 -freflection -stdlib=libc++ /tmp/quick_reflection_test.cpp -o /tmp/quick_test -L"$LIBCXX_LIB" -Wl,-rpath,"$LIBCXX_LIB" 2>/dev/null && LD_LIBRARY_PATH="$LIBCXX_LIB" /tmp/quick_test; then
    echo
    echo -e "${GREEN}C++26 reflection features enabled!${NC}"
else
    echo
    echo -e "${YELLOW}[WARN] Reflection test failed${NC}"
    echo "   But build may still be successful"
fi

rm -f /tmp/quick_reflection_test.cpp /tmp/quick_test

echo
echo "Run other tests:"
echo "  cd $PROJECT_DIR/build_cpp26"
echo "  ./bin/test_basic_types"
echo "  ./bin/test_vector"
echo "  ./bin/test_nested"
echo "  ctest"
echo

echo -e "${GREEN}Done!${NC}"
