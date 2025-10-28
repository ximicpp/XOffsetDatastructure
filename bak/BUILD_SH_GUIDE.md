# 🚀 build.sh 快速使用指南

## 📋 概述

`build.sh` 是为 Linux/WSL 环境设计的构建脚本，替代了在 WSL 内部运行 `build.bat` 时遇到的路径问题。

---

## 🎯 使用方法

### 在 WSL 中运行（推荐）

1. **打开 WSL 终端**
   ```bash
   # 在 Windows PowerShell 或 CMD 中
   wsl
   ```

2. **进入项目目录**
   ```bash
   cd /mnt/g/workspace/XOffsetDatastructure
   ```

3. **设置执行权限（仅需一次）**
   ```bash
   chmod +x build.sh
   ```

4. **运行构建脚本**
   ```bash
   ./build.sh
   ```

---

## ⚙️ 命令行选项

### 默认行为
```bash
./build.sh
# 等同于：
# - 使用 Clang P2996 (~/clang-p2996-install/bin/clang++)
# - 启用 C++26 反射测试
# - Release 模式
# - 使用所有可用 CPU 核心
```

### 自定义选项

#### 使用系统编译器（不使用 P2996）
```bash
./build.sh --no-p2996
```

#### 禁用反射测试
```bash
./build.sh --no-reflection
```

#### Debug 模式
```bash
./build.sh --debug
```

#### 详细输出
```bash
./build.sh --verbose
# 或
./build.sh -v
```

#### 指定并行任务数
```bash
./build.sh -j 8
```

#### 组合选项
```bash
./build.sh --debug --verbose -j 4
./build.sh --no-reflection --no-p2996
```

#### 显示帮助
```bash
./build.sh --help
# 或
./build.sh -h
```

---

## 🎨 功能特性

### ✅ 优势

1. **彩色输出** - 清晰的视觉反馈
2. **智能检测** - 自动检测 Clang P2996 是否存在
3. **灵活配置** - 丰富的命令行选项
4. **完整测试** - 运行所有 14 个测试（6 基础 + 8 反射）
5. **详细报告** - 显示通过/失败/跳过的测试统计
6. **错误处理** - 遇到错误立即停止

### 🎯 默认配置

| 配置项 | 默认值 | 说明 |
|-------|--------|------|
| 编译器 | Clang P2996 | `~/clang-p2996-install/bin/clang++` |
| 反射 | 启用 | `ENABLE_REFLECTION_TESTS=ON` |
| 构建类型 | Release | 优化编译 |
| 并行任务 | 自动检测 | 使用 `nproc` 检测 |

---

## 📊 输出示例

### 成功构建

```
======================================================================
  XOffsetDatastructure2 Build Script (with Reflection Support)
======================================================================

Configuration:
  Compiler: Clang P2996 (~/clang-p2996-install/bin/clang++)
  Reflection: ENABLED
  Build Type: Release
  Parallel Jobs: 8

Configuring CMake...
...

Building project...
...

======================================================================
Running Tests
======================================================================

=== Basic Tests ===

[1/14] Running test_basic_types...
✓ PASSED

[2/14] Running test_vector...
✓ PASSED

...

=== Reflection Tests ===

[7/14] Running test_reflection_operators...
✓ PASSED

...

======================================================================
  Build Summary
======================================================================

  Tests Run:    14
  Tests Passed: 14
  Tests Failed: 0

  Result: ALL TESTS PASSED

  Status: ✓ SUCCESS
======================================================================

Build, demo, and tests completed successfully!
```

### 失败情况

脚本会以红色显示失败的测试，并在最后显示失败统计：

```
======================================================================
  Build Summary
======================================================================

  Tests Run:    14
  Tests Passed: 12
  Tests Failed: 2

  Result: SOME TESTS FAILED

  Status: ✗ FAILED
======================================================================

Build and demo completed, but some tests FAILED
```

---

## 🔧 故障排除

### 问题 1: Clang P2996 未找到

**错误信息**:
```
Error: Clang P2996 not found at ~/clang-p2996-install/bin/clang++
Please install Clang P2996 or use --no-p2996 flag
```

**解决方案**:
```bash
# 方案 1: 使用系统编译器
./build.sh --no-p2996

# 方案 2: 安装 Clang P2996
# 参考 REFLECTION_QUICKSTART.md
```

### 问题 2: 权限被拒绝

**错误信息**:
```
bash: ./build.sh: Permission denied
```

**解决方案**:
```bash
chmod +x build.sh
./build.sh
```

### 问题 3: 行尾问题（Windows）

**错误信息**:
```
bash: ./build.sh: /bin/bash^M: bad interpreter
```

**解决方案**:
```bash
# 在 WSL 中运行
dos2unix build.sh
# 或
sed -i 's/\r$//' build.sh
```

---

## 📁 构建输出

### 目录结构

```
build/
├── bin/
│   └── Release/          # (或 Debug/)
│       ├── test_basic_types
│       ├── test_vector
│       ├── test_map_set
│       ├── test_nested
│       ├── test_compaction
│       ├── test_modify
│       ├── test_reflection_operators
│       ├── test_member_iteration
│       ├── test_reflection_type_signature
│       ├── test_splice_operations
│       ├── test_type_introspection
│       ├── test_reflection_compaction
│       ├── test_reflection_serialization
│       ├── test_reflection_comparison
│       └── xoffsetdatastructure2_demo
└── ...
```

---

## 🆚 build.sh vs build.bat

| 特性 | build.sh | build.bat |
|-----|----------|-----------|
| 环境 | Linux/WSL | Windows |
| 默认编译器 | Clang P2996 | WSL Clang P2996 |
| 彩色输出 | ✅ ANSI 颜色 | ✅ Windows 颜色 |
| 并行构建 | ✅ 自动检测核心数 | ✅ 自动检测核心数 |
| 系统编译器选项 | `--no-p2996` | `--no-wsl` |
| 路径问题 | ❌ 无路径问题 | ⚠️ WSL 内运行有问题 |

---

## 💡 使用场景

### 场景 1: 日常开发（推荐）

```bash
./build.sh
```

快速构建并测试，使用默认配置。

### 场景 2: 调试构建

```bash
./build.sh --debug --verbose
```

详细输出，便于调试问题。

### 场景 3: 不支持反射的环境

```bash
./build.sh --no-p2996 --no-reflection
```

使用系统编译器，只运行基础测试。

### 场景 4: 快速编译（低核心数机器）

```bash
./build.sh -j 2
```

限制并行任务数，避免系统过载。

### 场景 5: CI/CD 环境

```bash
./build.sh --no-p2996 --no-reflection -j 4
```

适合在没有 P2996 的 CI 环境中运行。

---

## 🔄 与其他工具的配合

### 1. 与 CMake 手动命令对比

**使用 build.sh**:
```bash
./build.sh
```

**等价的 CMake 命令**:
```bash
mkdir -p build
cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=~/clang-p2996-install/bin/clang++ \
    -DCMAKE_C_COMPILER=~/clang-p2996-install/bin/clang \
    -DENABLE_REFLECTION_TESTS=ON \
    -DCMAKE_CXX_FLAGS='-stdlib=libc++' \
    -DCMAKE_EXE_LINKER_FLAGS='-L$HOME/clang-p2996-install/lib -Wl,-rpath,$HOME/clang-p2996-install/lib'
cmake --build . --config Release -j$(nproc)
# 手动运行每个测试...
cd ..
```

显然，`build.sh` 更简洁！

### 2. 与 Makefile 配合

`build.sh` 独立于 Makefile，你也可以继续使用：

```bash
make all-reflection
```

但 `build.sh` 提供更多选项和更详细的输出。

### 3. 只运行测试

如果已经构建过，想只运行测试：

```bash
cd build
ctest --verbose
# 或只运行反射测试
ctest -R test_reflection --verbose
```

---

## 📝 脚本实现细节

### 关键特性

1. **错误处理**: `set -e` 确保任何命令失败时立即退出
2. **彩色输出**: 使用 ANSI 转义码
3. **智能检测**: 检测 Clang P2996 是否存在
4. **灵活配置**: 支持多种命令行选项
5. **详细报告**: 统计测试结果

### 核心逻辑

```bash
# 1. 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        --no-p2996) USE_CLANG_P2996=0 ;;
        --no-reflection) ENABLE_REFLECTION=0 ;;
        ...
    esac
done

# 2. 配置编译器
if [ $USE_CLANG_P2996 -eq 1 ]; then
    CMAKE_CXX_COMPILER="~/clang-p2996-install/bin/clang++"
    ...
fi

# 3. 运行 CMake
cmake .. -DCMAKE_CXX_COMPILER=$CMAKE_CXX_COMPILER ...

# 4. 构建
cmake --build . -j$NUM_JOBS

# 5. 运行测试
run_test "test_basic_types" 1 14
run_test "test_vector" 2 14
...

# 6. 显示摘要
echo "Tests Passed: $PASSED_COUNT"
```

---

## 🎓 学习资源

- **CMake 配置**: 参见 `CMakeLists.txt`
- **反射测试**: 参见 `REFLECTION_QUICKSTART.md`
- **测试文档**: 参见 `tests/README.md`
- **构建指南**: 参见 `BUILD_AND_RUN_GUIDE.md`

---

## 🚀 快速开始

**最简单的方式：**

```bash
# 1. 打开 WSL
wsl

# 2. 进入项目目录
cd /mnt/g/workspace/XOffsetDatastructure

# 3. 运行构建
chmod +x build.sh && ./build.sh
```

就这么简单！🎉

---

## 📞 获取帮助

遇到问题？

1. **查看帮助**:
   ```bash
   ./build.sh --help
   ```

2. **详细输出**:
   ```bash
   ./build.sh --verbose
   ```

3. **参考文档**:
   - `BUILD_AND_RUN_GUIDE.md`
   - `REFLECTION_QUICKSTART.md`
   - `session_context.md`

---

**build.sh 使用指南 - 让构建变得简单！** ✨
