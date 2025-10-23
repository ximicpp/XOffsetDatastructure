#!/bin/bash
# ============================================================================
# 在 WSL2 中使用 Clang P2996 编译 XOffsetDatastructure2
# ============================================================================

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  编译 XOffsetDatastructure2 (C++26)${NC}"
echo -e "${BLUE}================================================${NC}"
echo

CLANG_P2996="$HOME/clang-p2996-install/bin/clang++"
PROJECT_DIR="/mnt/g/workspace/XOffsetDatastructure"

# 检查编译器
if [ ! -f "$CLANG_P2996" ]; then
    echo -e "${RED}❌ 错误: Clang P2996 未找到${NC}"
    echo "   路径: $CLANG_P2996"
    echo
    echo "请先运行: wsl_build_clang_p2996.bat"
    exit 1
fi

echo -e "${GREEN}✅ 找到 Clang P2996${NC}"
echo "   $CLANG_P2996"
"$CLANG_P2996" --version | head -1
echo

# 检查项目目录
if [ ! -d "$PROJECT_DIR" ]; then
    echo -e "${RED}❌ 错误: 项目目录未找到${NC}"
    echo "   路径: $PROJECT_DIR"
    exit 1
fi

cd "$PROJECT_DIR"
echo -e "${GREEN}✅ 项目目录: $PROJECT_DIR${NC}"
echo

# 清理旧构建
if [ -d "build_cpp26" ]; then
    echo -e "${YELLOW}⚠️  删除旧的构建目录${NC}"
    rm -rf build_cpp26
fi

# 创建构建目录
mkdir build_cpp26
cd build_cpp26

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  步骤 1/3: 配置 CMake${NC}"
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
echo -e "${GREEN}✅ CMake 配置完成${NC}"
echo

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  步骤 2/3: 编译${NC}"
echo -e "${BLUE}================================================${NC}"
echo

CPU_CORES=$(nproc)
echo "使用 $CPU_CORES 个核心并行编译"
echo

cmake --build . -j$CPU_CORES

echo
echo -e "${GREEN}✅ 编译完成${NC}"
echo

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  步骤 3/3: 运行测试${NC}"
echo -e "${BLUE}================================================${NC}"
echo

# 运行基础示例
echo "运行 helloworld..."
./bin/helloworld
echo

# 运行反射测试
if [ -f "./bin/test_compaction" ]; then
    echo "运行 test_compaction..."
    ./bin/test_compaction
    echo
fi

# 运行完整示例
if [ -f "./bin/xoffsetdatastructure2_demo" ]; then
    echo "运行 demo..."
    ./bin/xoffsetdatastructure2_demo
    echo
fi

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  构建和测试总结${NC}"
echo -e "${BLUE}================================================${NC}"
echo

echo -e "${GREEN}✅ 编译成功！${NC}"
echo
echo "可执行文件位置:"
ls -lh bin/ 2>/dev/null || true
echo

echo "检查反射功能:"
echo

# 创建临时测试 - 使用实际的反射语法
cat > /tmp/quick_reflection_test.cpp << 'EOFCPP'
#include <iostream>

struct TestStruct {
    int x;
    double y;
};

int main() {
    // 测试反射 splice 语法
    constexpr auto refl = ^^TestStruct;
    
    // 测试成员指针
    int TestStruct::*ptr = &[:^^TestStruct::x:];
    
    TestStruct obj{42, 3.14};
    
    std::cout << "✅ C++26 Reflection: ENABLED\n";
    std::cout << "   Splice syntax works!\n";
    std::cout << "   Test value: " << obj.*ptr << "\n";
    
    return 0;
}
EOFCPP

echo "测试反射功能..."
if "$CLANG_P2996" -std=c++26 -freflection -stdlib=libc++ /tmp/quick_reflection_test.cpp -o /tmp/quick_test -L"$LIBCXX_LIB" -Wl,-rpath,"$LIBCXX_LIB" 2>/dev/null && LD_LIBRARY_PATH="$LIBCXX_LIB" /tmp/quick_test; then
    echo
    echo -e "${GREEN}🎉 C++26 反射功能已启用！${NC}"
else
    echo
    echo -e "${YELLOW}⚠️  反射功能检测失败${NC}"
    echo "   但编译可能仍然成功"
fi

rm -f /tmp/quick_reflection_test.cpp /tmp/quick_test

echo
echo "运行其他测试:"
echo "  cd $PROJECT_DIR/build_cpp26"
echo "  ./bin/test_basic_types"
echo "  ./bin/test_vector"
echo "  ./bin/test_nested"
echo "  ctest"
echo

echo -e "${GREEN}完成！${NC}"
