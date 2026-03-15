# Skill: native-build-test

## Description
使用本地安装的 Clang P2996 编译器直接构建和测试项目，无需 Docker。

## When to Use
- 本地已安装 P2996 编译器时
- 需要更快的构建迭代时
- Docker 不可用时

## Prerequisites
- Clang P2996 编译器已安装在以下路径之一：
  - `/usr/local/bin/clang++`
  - `~/clang-p2996-install/bin/clang++`
  - `/opt/clang-p2996/bin/clang++`
  - `/opt/p2996-toolchain/bin/clang++`

## Steps

### Step 1: 检查编译器是否可用
```bash
ls ~/clang-p2996-install/bin/clang++ 2>/dev/null && echo "P2996 compiler found" || echo "P2996 compiler NOT found"
```

### Step 2: 执行构建
```bash
cd /path/to/XOffsetDatastructure
./build.sh
```

**带选项的构建：**
```bash
# Debug 模式
./build.sh --debug

# 不启用反射
./build.sh --no-reflection

# 指定并行数
./build.sh -j 8

# 不使用 P2996（系统编译器）
./build.sh --no-p2996
```

### Step 3: 验证输出
- 确认所有测试 PASS
- 确认无编译错误或警告

## Important Notes
- build.sh 会自动搜索 P2996 编译器路径
- 编译标志：`-std=c++26 -freflection -fexpansion-statements -stdlib=libc++`
- 仅支持 64 位小端架构

## Expected Output
```
[BUILD] Found Clang P2996 at: ~/clang-p2996-install/bin/clang++
[BUILD] All tests passed!
```
