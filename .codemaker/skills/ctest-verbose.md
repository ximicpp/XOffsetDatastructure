# Skill: ctest-verbose

## Description
使用 CTest 在构建目录中运行测试，支持按名称筛选、详细输出等功能。

## When to Use
- 需要按名称筛选运行特定测试时
- 需要查看所有测试的详细日志输出时
- 构建完成后批量验证测试时

## Prerequisites
- 项目已通过 CMake 构建
- `build/` 目录中存在已编译的测试

## Steps

### Step 1: 进入构建目录
```bash
cd build
```

### Step 2: 运行所有测试（详细模式）
```bash
ctest --verbose
```

### Step 3: 按名称筛选运行
```bash
# 按正则匹配测试名称
ctest -R "test_basic_types" --verbose
ctest -R "test_vector" --verbose
ctest -R "Enum" --verbose          # 匹配包含 "Enum" 的测试
ctest -R "Reflection" --verbose     # 匹配所有反射相关测试
```

### Step 4: 其他有用选项
```bash
# 只显示失败的测试
ctest --output-on-failure

# 并行运行测试
ctest -j 4 --verbose

# 列出所有可用测试（不运行）
ctest -N

# 排除某些测试
ctest -E "test_memory_efficiency" --verbose
```

### 在 Docker 中使用 CTest
```bash
docker run --rm -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash -c "bash ./build.sh && cd build && ctest --verbose"
```

## Important Notes
- CTest 测试名称在 `tests/CMakeLists.txt` 中通过 `add_test(NAME ...)` 定义
- 测试名称和可执行文件名可能不同（如 `BasicTypesTest` vs `test_basic_types`）
- 使用 `ctest -N` 先列出所有可用测试名称
- 返回码：0 = 全部通过，非 0 = 有失败

## Expected Output
```
Test project /workspace/build
    Start 1: BasicTypesTest
1/N Test #1: BasicTypesTest ........... Passed    0.01 sec
    ...
100% tests passed, 0 tests failed out of N
```
