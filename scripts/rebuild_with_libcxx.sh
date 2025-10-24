#!/bin/bash
# Quick rebuild script for libc++ support

cd ~/clang-p2996-build/clang-p2996/build

echo "================================================"
echo "  Building Clang P2996 with libc++ Support"
echo "================================================"
echo
echo "Start time: $(date)"
echo "Using $(nproc) CPU cores"
echo

# Build
ninja -j$(nproc)

if [ $? -eq 0 ]; then
    echo
    echo "================================================"
    echo "  Build Successful! Installing..."
    echo "================================================"
    
    ninja install
    
    echo
    echo "================================================"
    echo "  Testing Reflection"
    echo "================================================"
    
    # Test reflection
    cat > /tmp/test_ref.cpp << 'EOF'
#include <iostream>
int main() {
#if __cpp_reflection >= 202306L
    std::cout << "[OK]Reflection ENABLED (" << __cpp_reflection << ")\n";
    return 0;
#else
    std::cout << "[ERROR] Reflection NOT enabled\n";
    return 1;
#endif
}
EOF
    
    ~/clang-p2996-install/bin/clang++ -std=c++26 -freflection-latest \
        -stdlib=libc++ /tmp/test_ref.cpp -o /tmp/test_ref 2>&1
    
    if [ $? -eq 0 ]; then
        echo "Compilation successful, running test..."
        /tmp/test_ref
    else
        echo "Test compilation failed"
    fi
    
    echo
    echo "================================================"
    echo "  Checking installed files"
    echo "================================================"
    
    echo "libc++ headers:"
    if [ -f ~/clang-p2996-install/include/c++/v1/meta ]; then
        echo "  [OK]<meta> header found"
    else
        echo "  [ERROR] <meta> header NOT found"
    fi
    
    echo
    echo "libc++ libraries:"
    ls -lh ~/clang-p2996-install/lib/libc++* 2>/dev/null | head -5
    
    echo
    echo "Build completed at: $(date)"
else
    echo
    echo "================================================"
    echo "  Build Failed"
    echo "================================================"
fi
