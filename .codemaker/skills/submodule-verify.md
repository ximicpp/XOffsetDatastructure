# Skill: submodule-verify

## Description
检查子模块的版本状态，验证子模块是否正确初始化以及版本是否与父仓库预期一致。

## When to Use
- 排查构建问题时（找不到 TypeLayout 头文件）
- 确认子模块版本是否正确时
- CI 失败怀疑子模块问题时

## Steps

### Step 1: 检查子模块状态
```bash
git submodule status
```

**输出解读：**
- ` abc1234 external/typelayout (v1.0)` — 正常，hash 匹配
- `-abc1234 external/typelayout` — 未初始化，需要 `git submodule update --init`
- `+abc1234 external/typelayout` — 本地有修改，hash 不匹配父仓库记录

### Step 2: 查看子模块配置
```bash
cat .gitmodules
```

### Step 3: 查看子模块的当前 commit
```bash
cd external/typelayout && git log --oneline -3 && cd ../..
```

### Step 4: 验证关键文件完整性
```bash
# TypeLayout 主头文件
ls -la external/typelayout/include/boost/typelayout.hpp

# TypeLayout 的 include 目录
ls external/typelayout/include/boost/
```

### Step 5: 对比远程最新版本
```bash
cd external/typelayout
git fetch origin
git log --oneline origin/main -3
cd ../..
```

## Quick Health Check（一行命令）
```bash
git submodule status && ls external/typelayout/include/boost/typelayout.hpp && echo "Submodule OK"
```

## Important Notes
- 如果子模块状态前缀为 `-`，运行 `git submodule update --init --recursive`
- 如果前缀为 `+`，说明本地子模块版本与父仓库不一致
- CI 中子模块通过 `actions/checkout@v4` 的 `submodules: recursive` 自动初始化
