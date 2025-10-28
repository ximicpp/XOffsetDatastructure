#!/bin/bash
# ============================================================================
# Build Clang P2996 in WSL2
# ============================================================================

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}  Clang P2996 Build Script (WSL2)${NC}"
echo -e "${BLUE}=========================================${NC}"
echo

BUILD_DIR=$HOME/clang-p2996-build
INSTALL_DIR=$HOME/clang-p2996-install
REPO_URL='https://github.com/bloomberg/clang-p2996.git'
BRANCH='p2996'

echo "Configuration:"
echo "  Source dir: $BUILD_DIR/clang-p2996"
echo "  Install dir: $INSTALL_DIR"
echo "  Branch: $BRANCH"
echo

# ============================================================================
# Step 1: Clone source code
# ============================================================================

echo -e "${BLUE}[1/5] Cloning source code...${NC}"
echo

SKIP_CLONE=false

if [ -d "$BUILD_DIR/clang-p2996/.git" ]; then
    echo -e "${YELLOW}Source code already exists${NC}"
    read -p "Delete and re-clone? [y/n]: " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        rm -rf "$BUILD_DIR"
    else
        cd "$BUILD_DIR/clang-p2996"
        echo "Using existing source code"
        SKIP_CLONE=true
    fi
fi

if [ "$SKIP_CLONE" != "true" ]; then
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    echo "Cloning Bloomberg Clang P2996..."
    echo "This may take 5-15 minutes, please wait..."
    echo
    
    git clone -b "$BRANCH" --single-branch "$REPO_URL"
    
    cd clang-p2996
    echo
    echo -e "${GREEN}Clone completed${NC}"
else
    cd "$BUILD_DIR/clang-p2996"
fi

echo "  Branch:" $(git rev-parse --abbrev-ref HEAD)
echo "  Commit:" $(git log --oneline -1)
echo

# Check directory structure
if [ ! -d "llvm" ]; then
    echo -e "${RED}Error: llvm directory not found${NC}"
    exit 1
fi

# ============================================================================
# Step 2: Configure CMake
# ============================================================================

echo -e "${BLUE}[2/5] Configuring CMake...${NC}"
echo

rm -rf build
mkdir build
cd build

CPU_CORES=$(nproc)
echo "CPU cores: $CPU_CORES"
echo

cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_ENABLE_PROJECTS='clang' \
    -DLLVM_ENABLE_RUNTIMES='libcxx;libcxxabi;libunwind' \
    -DLLVM_TARGETS_TO_BUILD='X86' \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DLLVM_ENABLE_ASSERTIONS=OFF \
    -DLLVM_OPTIMIZED_TABLEGEN=ON \
    -DLLVM_BUILD_TESTS=OFF \
    -DLLVM_INCLUDE_TESTS=OFF \
    -DLIBCXX_ENABLE_EXPERIMENTAL_LIBRARY=ON \
    ../llvm

echo
echo -e "${GREEN}CMake configuration completed${NC}"
echo

# ============================================================================
# Step 3: Compile
# ============================================================================

echo -e "${BLUE}[3/5] Starting compilation...${NC}"
echo
echo -e "${YELLOW}This will take 1-3 hours${NC}"
echo "   Using $CPU_CORES cores for parallel compilation"
echo

START_TIME=$(date +%s)
echo "Start time:" $(date)
echo

ninja -j$CPU_CORES

END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))
MINUTES=$((ELAPSED / 60))
SECONDS=$((ELAPSED % 60))

echo
echo -e "${GREEN}Compilation completed!${NC}"
echo "   Time taken: ${MINUTES}m ${SECONDS}s"
echo

# ============================================================================
# Step 4: Install
# ============================================================================

echo -e "${BLUE}[4/5] Installing...${NC}"
echo

ninja install

echo -e "${GREEN}Installation completed${NC}"
echo "   Location: $INSTALL_DIR"
echo

# ============================================================================
# Step 5: Verify
# ============================================================================

echo -e "${BLUE}[5/5] Verifying installation...${NC}"
echo

CLANG_BIN="$INSTALL_DIR/bin/clang++"

if [ ! -f "$CLANG_BIN" ]; then
    echo -e "${RED}clang++ not found${NC}"
    exit 1
fi

echo "Compiler version:"
"$CLANG_BIN" --version | head -3
echo

# Test reflection support
echo "Testing C++26 reflection..."
cat > /tmp/test_reflection.cpp << 'EOFCPP'
#include <iostream>

struct TestStruct {
    int x;
    double y;
};

int main() {
#if __cpp_reflection >= 202306L
    std::cout << "[OK] C++26 Reflection: ENABLED\n";
    std::cout << "   __cpp_reflection = " << __cpp_reflection << "\n";
    
    // Test P2996R5 reflection syntax
    constexpr auto refl = ^TestStruct;
    std::cout << "   Reflection syntax test passed!\n";
    
    return 0;
#else
    std::cout << "[ERROR] C++26 Reflection: NOT ENABLED\n";
    return 1;
#endif
}
EOFCPP

REFLECTION_OK=false
LIBCXX_INCLUDE="$INSTALL_DIR/include/c++/v1"
LIBCXX_LIB="$INSTALL_DIR/lib"

echo "Compiling test program (P2996R5 syntax)..."
# Try new syntax first: -std=c++26 -freflection
if "$CLANG_BIN" -std=c++26 -freflection -stdlib=libc++ \
    -I"$LIBCXX_INCLUDE" -L"$LIBCXX_LIB" -Wl,-rpath,"$LIBCXX_LIB" \
    /tmp/test_reflection.cpp -o /tmp/test_reflection 2>/dev/null; then
    echo "Running test..."
    if LD_LIBRARY_PATH="$LIBCXX_LIB" /tmp/test_reflection 2>/dev/null; then
        REFLECTION_OK=true
        echo -e "${GREEN}[OK] Using modern syntax: -std=c++26 -freflection${NC}"
    fi
else
    # Fallback to old syntax if new syntax fails
    echo -e "${YELLOW}Modern syntax failed, trying legacy syntax...${NC}"
    if "$CLANG_BIN" -std=c++2c -freflection-latest -stdlib=libc++ \
        -I"$LIBCXX_INCLUDE" -L"$LIBCXX_LIB" -Wl,-rpath,"$LIBCXX_LIB" \
        /tmp/test_reflection.cpp -o /tmp/test_reflection 2>/dev/null; then
        echo "Running test..."
        if LD_LIBRARY_PATH="$LIBCXX_LIB" /tmp/test_reflection 2>/dev/null; then
            REFLECTION_OK=true
            echo -e "${YELLOW}[OK] Using legacy syntax: -std=c++2c -freflection-latest${NC}"
        fi
    fi
fi

rm -f /tmp/test_reflection.cpp /tmp/test_reflection

echo

# ============================================================================
# Summary
# ============================================================================

echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}  Build Summary${NC}"
echo -e "${BLUE}=========================================${NC}"
echo

echo "Installation info:"
echo "  Compiler: $CLANG_BIN"
if [ "$REFLECTION_OK" = "true" ]; then
    echo -e "  C++26 Reflection: ${GREEN}[OK] Enabled${NC}"
else
    echo -e "  C++26 Reflection: ${YELLOW}[WARN] Check failed${NC}"
fi
echo

echo "Usage:"
echo
echo "1. Add to PATH (in ~/.bashrc):"
echo "   export PATH=\"$INSTALL_DIR/bin:\$PATH\""
echo
echo "2. Compile C++26 code (P2996R5 syntax):"
echo "   # Modern syntax (recommended):"
echo "   clang++ -std=c++26 -freflection -stdlib=libc++ your_file.cpp"
echo
echo "   # Legacy syntax (if modern fails):"
echo "   clang++ -std=c++2c -freflection-latest your_file.cpp"
echo
echo "3. Build XOffsetDatastructure2:"
echo "   cd /mnt/g/workspace/XOffsetDatastructure"
echo "   ./build_cpp26_wsl.sh"
echo

if [ "$REFLECTION_OK" = "true" ]; then
    echo -e "${GREEN}Congratulations! Clang P2996 built successfully!${NC}"
else
    echo -e "${YELLOW}[WARN] Build completed, but reflection test failed${NC}"
    echo "Please check compiler version and flags"
fi

echo
echo "Completed at:" $(date)
echo -e "${BLUE}=========================================${NC}"
