# 🚀 快速构建参考

## 📦 一键构建命令

### 基础构建（无反射）
```bash
cmake -B build && cmake --build build && cd build && ctest
```

### 完整构建（含反射）
```bash
cmake -B build -DENABLE_REFLECTION_TESTS=ON && cmake --build build && cd build && ctest
```

---

## 📋 常用命令速查

### 配置项目
```bash
# 默认配置（C++20）
cmake -B build

# 启用反射（C++26）
cmake -B build -DENABLE_REFLECTION_TESTS=ON

# 指定编译器
cmake -B build -DCMAKE_CXX_COMPILER=/path/to/clang++

# 指定构建类型
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 组合使用
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DENABLE_REFLECTION_TESTS=ON
```

### 编译项目
```bash
# 编译所有
cmake --build build

# 并行编译
cmake --build build -j$(nproc)         # Linux/macOS
cmake --build build -j%NUMBER_OF_PROCESSORS%  # Windows

# 指定配置（Windows）
cmake --build build --config Release

# 编译单个目标
cmake --build build --target test_reflection_operators
```

### 运行测试
```bash
# 使用 CTest
cd build
ctest                           # 简单运行
ctest --verbose                 # 详细输出
ctest --output-on-failure       # 失败时显示输出
ctest -R test_reflection        # 运行匹配的测试
ctest -R "test_reflection_operators"  # 运行特定测试

# 直接运行
./build/bin/Release/test_reflection_operators  # Linux/macOS
.\build\bin\Release\test_reflection_operators.exe  # Windows

# 使用脚本
cd tests
./run_reflection_tests.sh       # Linux/macOS
run_reflection_tests.bat        # Windows
```

### 清理构建
```bash
# 清理构建文件
cmake --build build --target clean

# 完全删除构建目录
rm -rf build                    # Linux/macOS
rmdir /s /q build              # Windows

# 重新配置
cmake -B build --fresh
```

---

## 🎯 场景示例

### 场景 1: 第一次构建
```bash
cmake -B build
cmake --build build
cd build && ctest --verbose
```

### 场景 2: 测试反射特性
```bash
cmake -B build -DENABLE_REFLECTION_TESTS=ON
cmake --build build
cd tests && ./run_reflection_tests.sh
```

### 场景 3: 快速迭代开发
```bash
# 仅重新编译
cmake --build build -j$(nproc)

# 运行特定测试
./build/bin/Release/test_reflection_operators
```

### 场景 4: CI/CD 部署
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

### 场景 5: 调试构建
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
gdb ./build/bin/Debug/test_reflection_operators
```

---

## 📊 构建选项对照表

| 选项 | 值 | 说明 |
|------|-----|------|
| `CMAKE_BUILD_TYPE` | `Debug` / `Release` | 构建类型 |
| `CMAKE_CXX_COMPILER` | 路径 | 编译器 |
| `CMAKE_CXX_STANDARD` | `20` / `26` | C++ 标准 |
| `ENABLE_REFLECTION_TESTS` | `ON` / `OFF` | 反射测试 |

---

## 🔍 诊断命令

### 检查编译器
```bash
# 检查编译器版本
clang++ --version
g++ --version

# 检查反射支持
echo | clang++ -std=c++26 -freflection -E -dM - | grep __cpp_reflection
```

### 检查构建状态
```bash
# 查看可执行文件
ls build/bin/Release/
ls build/bin/Debug/

# 查看 CMake 缓存
cat build/CMakeCache.txt | grep CMAKE_CXX
```

### 检查测试
```bash
# 列出所有测试
ctest --test-dir build --show-only

# 运行并查看输出
ctest --test-dir build --verbose
```

---

## ⚠️ 常见错误快速修复

### 错误: "experimental/meta not found"
```bash
# 解决方案：禁用反射测试
cmake -B build -DENABLE_REFLECTION_TESTS=OFF
```

### 错误: 找不到 Boost
```bash
# 解决方案：重新配置
cmake -B build --fresh
```

### 错误: 找不到可执行文件
```bash
# Windows：检查正确的配置
ls build/bin/Debug/     # 或 build/bin/Release/

# 解决方案：指定配置
cmake --build build --config Release
```

### 错误: 测试失败
```bash
# 查看详细输出
ctest --test-dir build --verbose --output-on-failure
```

---

## 📝 输出目录结构

```
build/
├── bin/
│   ├── Debug/          (Windows Debug 构建)
│   │   ├── test_reflection_operators.exe
│   │   └── ...
│   └── Release/        (Windows Release 或 Linux/macOS)
│       ├── test_reflection_operators
│       └── ...
├── CMakeCache.txt
└── ...
```

---

## 🎓 推荐阅读顺序

1. **本文档** - 快速参考命令
2. **BUILD_AND_RUN_GUIDE.md** - 详细构建指南
3. **REFLECTION_QUICKSTART.md** - 反射测试快速入门

---

## 📞 需要帮助？

- **详细文档**: 查看 `BUILD_AND_RUN_GUIDE.md`
- **反射说明**: 查看 `REFLECTION_QUICKSTART.md`
- **测试总结**: 查看 `REFLECTION_TESTS_SUMMARY.md`
