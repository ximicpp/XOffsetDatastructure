# Skill: run-single-test

## Description
在 Docker 容器中运行单个测试可执行文件，用于调试特定测试失败或验证特定功能。

## When to Use
- 某个测试失败需要单独调试时
- 只修改了某个测试文件，想快速验证时
- 需要查看特定测试的详细输出时

## Prerequisites
- Docker 已安装并运行
- 项目已构建（或可以先构建再运行）

## Steps

### Step 1: 进入 Docker 交互 Shell
```bash
docker run --rm -it -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash
```

**Apple Silicon：**
```bash
docker run --rm -it --platform linux/amd64 -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash
```

### Step 2: 构建项目（如果尚未构建）
```bash
bash ./build.sh
```

### Step 3: 运行单个测试
```bash
# 在 Docker 交互 Shell 内
./build/bin/Release/test_basic_types
./build/bin/Release/test_vector
./build/bin/Release/test_reflection_operators
./build/bin/Release/test_complex_nesting
./build/bin/Release/test_compaction
# ... 等等
```

### 一步式运行（无需交互 Shell）
```bash
# 先构建再运行指定测试
docker run --rm -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash -c "bash ./build.sh && ./build/bin/Release/test_basic_types"
```

## Available Test Executables
测试文件位于 `tests/` 目录，编译后的可执行文件在 `build/bin/Release/`：
- `test_basic_types` — 基本类型测试
- `test_vector` — XVector 测试
- `test_complex_nesting` — 复杂嵌套结构测试
- `test_compaction` — 内存压缩测试
- `test_inheritance` — 继承测试
- `test_reflection_operators` — P2996 反射操作符测试
- `test_enum_support` — 枚举支持测试
- `test_type_safety_comprehensive` — 类型安全综合测试
- `test_memory_efficiency` — 内存效率测试
- `test_policy_trait` — 策略特征测试

## Important Notes
- 测试可执行文件路径：`build/bin/Release/<test_name>`
- 测试返回 0 表示成功，非 0 表示失败
- 使用 `bash -c` 组合命令可避免进入交互 Shell
