#!/bin/bash
# 快速验证所有测试

set -e

echo "=========================================="
echo "  XOffsetDatastructure2 测试验证"
echo "=========================================="
echo ""

# 设置库路径
export LD_LIBRARY_PATH=~/clang-p2996-install/lib

# 进入测试目录
cd "$(dirname "$0")/build/tests"

# 运行 CTest
echo "📋 运行 CTest..."
echo ""
ctest --output-on-failure

echo ""
echo "=========================================="
echo "  验证完成"
echo "=========================================="
echo ""

# 显示反射测试的反射状态
echo "🔬 反射测试验证:"
cd ../bin
for test in test_reflection_*; do
    if [ -x "$test" ]; then
        status=$(./"$test" 2>&1 | grep -o "C++26 Reflection: ENABLED" || echo "")
        if [ -n "$status" ]; then
            echo "  ✅ $test - Reflection ENABLED"
        else
            echo "  ⚠️  $test - No reflection marker"
        fi
    fi
done

# 其他反射测试
for test in test_member_iteration test_splice_operations test_type_introspection; do
    if [ -x "$test" ]; then
        status=$(./"$test" 2>&1 | grep -o "C++26 Reflection: ENABLED" || echo "")
        if [ -n "$status" ]; then
            echo "  ✅ $test - Reflection ENABLED"
        else
            echo "  ⚠️  $test - No reflection marker"
        fi
    fi
done

echo ""
echo "✨ 所有测试验证完成！"
