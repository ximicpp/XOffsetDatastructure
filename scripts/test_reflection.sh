#!/bin/bash
# Test reflection

CLANG=~/clang-p2996-install/bin/clang++

# Test 1: Check __cpp_reflection macro
echo "Test 1: Check __cpp_reflection macro"
cat > /tmp/t1.cpp << 'EOF'
#include <iostream>
int main() {
#ifdef __cpp_reflection
    std::cout << "Reflection defined: " << __cpp_reflection << std::endl;
    return 0;
#else
    std::cout << "Reflection NOT defined" << std::endl;
    return 1;
#endif
}
EOF

echo "Compiling with -freflection..."
$CLANG -std=c++26 -freflection /tmp/t1.cpp -o /tmp/t1 2>&1
echo "Running..."
/tmp/t1
echo

# Test 2: Try actual reflection syntax
echo "Test 2: Try reflection syntax"
cat > /tmp/t2.cpp << 'EOF'
struct S { int x; };
int main() {
    constexpr auto m = ^S::x;
    return 0;
}
EOF

echo "Compiling reflection code..."
$CLANG -std=c++26 -freflection /tmp/t2.cpp -o /tmp/t2 2>&1
if [ $? -eq 0 ]; then
    echo "SUCCESS: Reflection syntax compiled!"
else
    echo "FAILED: Reflection syntax did not compile"
fi
