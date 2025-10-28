# 📘 构建和运行指南

本指南详细说明如何编译和运行 XOffsetDatastructure2 项目的所有测试，包括反射测试。

---

## 📋 目录

1. [环境要求](#环境要求)
2. [快速开始](#快速开始)
3. [详细构建步骤](#详细构建步骤)
4. [运行测试](#运行测试)
5. [常见问题](#常见问题)

---

## 🔧 环境要求

### 基础要求（所有测试）

- **CMake**: 3.10 或更高
- **编译器**: 支持 C++20
  - GCC 10+
  - Clang 10+
  - MSVC 2019+
- **Boost**: 已包含在 `external/boost/`

### 反射测试额外要求

- **编译器**: Clang with P2996 支持
- **C++ 标准**: C++26
- **编译标志**: `-freflection`

> **注意**: 反射测试是可选的。如果不支持，测试会自动跳过，不影响构建。

---

## 🚀 快速开始

### 方式 1: 仅基础测试（推荐新手）

```bash
# 配置项目
cmake -B build

# 编译
cmake --build build

# 运行所有测试
cd build
ctest --verbose
```

### 方式 2: 包含反射测试（需要支持反射的编译器）

```bash
# 配置项目（启用反射）
cmake -B build -DENABLE_REFLECTION_TESTS=ON

# 编译
cmake --build build

# 运行所有测试
cd build
ctest --verbose
```

### 方式 3: 使用特定编译器

```bash
# 指定 Clang 编译器
cmake -B build \
    -DCMAKE_CXX_COMPILER=/path/to/clang++ \
    -DENABLE_REFLECTION_TESTS=ON

# 编译
cmake --build build
```

---

## 📝 详细构建步骤

### 步骤 1: 清理旧构建（可选）

```bash
# 删除旧的构建目录
rm -rf build

# 或在 Windows
rmdir /s /q build
```

### 步骤 2: 配置项目

#### 基础配置（C++20，无反射）

```bash
cmake -B build
```

#### 启用反射测试（C++26）

```bash
cmake -B build -DENABLE_REFLECTION_TESTS=ON
```

#### 高级配置选项

```bash
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DENABLE_REFLECTION_TESTS=ON \
    -DCMAKE_CXX_STANDARD=26
```

**配置选项说明**:
- `CMAKE_BUILD_TYPE`: `Debug` 或 `Release`
- `CMAKE_CXX_COMPILER`: 编译器路径
- `ENABLE_REFLECTION_TESTS`: `ON` 或 `OFF`（默认 OFF）
- `CMAKE_CXX_STANDARD`: C++ 标准版本

### 步骤 3: 编译项目

#### 编译所有目标

```bash
cmake --build build
```

#### 仅编译测试

```bash
cmake --build build --target test_basic_types
cmake --build build --target test_reflection_operators
# ... 等等
```

#### 并行编译（加速）

```bash
# Linux/macOS
cmake --build build -j$(nproc)

# Windows
cmake --build build -j%NUMBER_OF_PROCESSORS%
```

#### 编译特定配置（Windows）

```bash
# Debug 模式
cmake --build build --config Debug

# Release 模式
cmake --build build --config Release
```

### 步骤 4: 验证构建

构建成功后，你应该看到：

```
========================================
  XOffsetDatastructure2 Test Summary
========================================
C++ Standard: C++26
Compiler: Clang 18.0.0
Basic Tests: 6 tests
Reflection Tests: 8 tests
Total Tests: 14 tests
Reflection: ENABLED
Reflection Flag: -freflection
========================================
```

---

## 🏃 运行测试

### 方式 1: 使用 CTest（推荐）

#### 运行所有测试

```bash
cd build
ctest --verbose
```

#### 运行特定测试

```bash
# 运行基础测试
ctest -R "BasicTypes|VectorOps|MapSetOps"

# 运行反射测试
ctest -R "test_reflection"

# 运行单个测试
ctest -R "test_reflection_operators"
```

#### 显示详细输出

```bash
ctest --verbose --output-on-failure
```

### 方式 2: 直接运行可执行文件

#### Linux/macOS

```bash
# 基础测试
./build/bin/Release/test_basic_types
./build/bin/Release/test_vector
./build/bin/Release/test_map_set
./build/bin/Release/test_nested
./build/bin/Release/test_compaction
./build/bin/Release/test_modify

# 反射测试
./build/bin/Release/test_reflection_operators
./build/bin/Release/test_member_iteration
./build/bin/Release/test_reflection_type_signature
./build/bin/Release/test_splice_operations
./build/bin/Release/test_type_introspection
./build/bin/Release/test_reflection_compaction
./build/bin/Release/test_reflection_serialization
./build/bin/Release/test_reflection_comparison
```

#### Windows

```cmd
REM 基础测试
.\build\bin\Release\test_basic_types.exe
.\build\bin\Release\test_vector.exe
.\build\bin\Release\test_map_set.exe
.\build\bin\Release\test_nested.exe
.\build\bin\Release\test_compaction.exe
.\build\bin\Release\test_modify.exe

REM 反射测试
.\build\bin\Release\test_reflection_operators.exe
.\build\bin\Release\test_member_iteration.exe
.\build\bin\Release\test_reflection_type_signature.exe
.\build\bin\Release\test_splice_operations.exe
.\build\bin\Release\test_type_introspection.exe
.\build\bin\Release\test_reflection_compaction.exe
.\build\bin\Release\test_reflection_serialization.exe
.\build\bin\Release\test_reflection_comparison.exe
```

### 方式 3: 使用脚本批量运行

#### Linux/macOS

```bash
cd tests
chmod +x run_reflection_tests.sh
./run_reflection_tests.sh
```

#### Windows

```cmd
cd tests
run_reflection_tests.bat
```

---

## 🔍 预期输出

### 基础测试输出示例

```
[TEST] Basic Types Test
--------------------------------------------------
Test 1: int type... [OK]
Test 2: float type... [OK]
Test 3: double type... [OK]
Test 4: char type... [OK]
Test 5: bool type... [OK]
Test 6: long long type... [OK]
Test 7: Persistence test... [OK]
[PASS] All basic types tests passed!
```

### 反射测试输出（支持反射）

```
========================================
  Reflection Operators Test
========================================

[INFO] C++26 Reflection: ENABLED
[INFO] __cpp_reflection = 202306

[Test 1] Type Reflection
------------------------
  TestStruct: TestStruct
  int: int
  double: double
  XString: XString
[PASS] Type reflection

...

[SUCCESS] All reflection operator tests passed!
```

### 反射测试输出（不支持反射）

```
========================================
  Reflection Operators Test
========================================

[SKIP] C++26 Reflection not available
[INFO] Compile with -std=c++26 -freflection to enable
```

---

## 🛠️ 常见问题

### Q1: 编译错误 "experimental/meta not found"

**问题**: 编译器不支持 P2996 反射

**解决方案**:
```bash
# 选项 1: 禁用反射测试
cmake -B build -DENABLE_REFLECTION_TESTS=OFF

# 选项 2: 使用支持反射的 Clang
cmake -B build \
    -DCMAKE_CXX_COMPILER=/path/to/clang-reflection \
    -DENABLE_REFLECTION_TESTS=ON
```

### Q2: 所有反射测试都跳过

**问题**: 运行时检测到反射不可用

**诊断**:
```bash
# 检查编译器宏定义
echo | clang++ -std=c++26 -freflection -E -dM - | grep __cpp_reflection

# 应该输出:
# #define __cpp_reflection 202306L
```

**解决方案**: 确认使用了正确的编译器和标志

### Q3: Boost 相关错误

**问题**: 找不到 Boost 头文件

**解决方案**:
```bash
# 确认 Boost 在正确位置
ls external/boost/

# 重新配置 CMake
cmake -B build --fresh
```

### Q4: macOS 上的链接错误

**问题**: RPATH 相关错误

**解决方案**: CMakeLists.txt 已经配置了 macOS RPATH，如果仍有问题：
```bash
# 设置环境变量
export DYLD_LIBRARY_PATH=/usr/local/lib:$DYLD_LIBRARY_PATH

# 重新构建
cmake --build build --clean-first
```

### Q5: Windows 上找不到可执行文件

**问题**: 构建模式不匹配

**解决方案**:
```cmd
REM 检查 Debug 目录
dir build\bin\Debug\

REM 检查 Release 目录
dir build\bin\Release\

REM 或指定构建模式
cmake --build build --config Release
```

---

## 📊 构建配置对比

### 配置 1: 仅基础功能

```bash
cmake -B build
```

- **C++ 标准**: C++20
- **测试数量**: 6 个基础测试
- **编译器要求**: 任何支持 C++20 的编译器
- **构建时间**: ~30 秒
- **适用场景**: 生产环境、CI/CD

### 配置 2: 包含反射（可选）

```bash
cmake -B build -DENABLE_REFLECTION_TESTS=ON
```

- **C++ 标准**: C++26
- **测试数量**: 6 个基础测试 + 8 个反射测试
- **编译器要求**: Clang with P2996
- **构建时间**: ~60 秒
- **适用场景**: 开发环境、特性验证

---

## 🎯 推荐工作流

### 日常开发

```bash
# 1. 清理并重新配置
cmake -B build --fresh

# 2. 编译
cmake --build build -j$(nproc)

# 3. 运行测试
cd build && ctest --output-on-failure
```

### 测试反射特性

```bash
# 1. 启用反射并配置
cmake -B build -DENABLE_REFLECTION_TESTS=ON

# 2. 编译
cmake --build build

# 3. 只运行反射测试
cd tests
./run_reflection_tests.sh  # Linux/macOS
# 或
run_reflection_tests.bat    # Windows
```

### CI/CD 集成

```bash
# 标准构建（无反射）
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

# 如果支持反射
if command -v clang++ &> /dev/null; then
    cmake -B build-reflection \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DENABLE_REFLECTION_TESTS=ON
    cmake --build build-reflection
    ctest --test-dir build-reflection --output-on-failure
fi
```

---

## 📈 性能优化

### 加速编译

```bash
# 使用 ccache（如果已安装）
cmake -B build -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

# 并行编译
cmake --build build -j$(nproc)

# 仅编译测试（不编译示例）
cmake --build build --target test_basic_types
```

### 减少构建时间

```bash
# Release 模式（无调试信息）
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 禁用反射测试
cmake -B build -DENABLE_REFLECTION_TESTS=OFF
```

---

## ✅ 验证清单

构建成功后，请确认：

- [ ] 配置成功（无 CMake 错误）
- [ ] 编译成功（无编译错误）
- [ ] 6 个基础测试可运行
- [ ] 8 个反射测试可运行（或正确跳过）
- [ ] CTest 报告测试通过
- [ ] 所有可执行文件在 `build/bin/` 目录

---

## 📞 获取帮助

如果遇到问题：

1. **查看编译输出** - 仔细阅读错误信息
2. **检查环境** - 确认编译器版本和配置
3. **查看文档** - 参考 README 和其他文档
4. **运行诊断** - 使用上述诊断命令

---

## 🎉 总结

### 最简单的使用方式

```bash
# 三步完成
cmake -B build                  # 配置
cmake --build build             # 编译
cd build && ctest --verbose     # 测试
```

### 完整验证（包含反射）

```bash
# 四步验证所有功能
cmake -B build -DENABLE_REFLECTION_TESTS=ON
cmake --build build
cd build && ctest --verbose
cd ../tests && ./run_reflection_tests.sh
```

---

**构建愉快！** 🚀
