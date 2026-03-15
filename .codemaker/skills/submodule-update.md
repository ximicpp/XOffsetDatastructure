# Skill: submodule-update

## Description
初始化和更新 Git 子模块。本项目使用 TypeLayout 作为子模块，位于 `external/typelayout`。

## When to Use
- 新克隆仓库后
- `git pull` 后子模块指针发生变化时
- 构建报错提示找不到 TypeLayout 头文件时

## Project Submodule Layout
```
external/
  typelayout/          # Git submodule -> https://github.com/ximicpp/TypeLayout
    include/
      boost/
        typelayout.hpp # 主头文件
```

## Steps

### Step 1: 初始化并递归更新子模块
```bash
git submodule update --init --recursive
```

### Step 2: 验证子模块状态
```bash
git submodule status
```
输出应显示子模块的 commit hash，前面没有 `-`（表示已初始化）或 `+`（表示本地有修改）。

### Step 3: 验证关键文件存在
```bash
ls external/typelayout/include/boost/typelayout.hpp
```

## Troubleshooting
```bash
# 如果子模块目录为空
rm -rf external/typelayout
git submodule update --init --recursive

# 如果子模块 URL 变更
git submodule sync --recursive
git submodule update --init --recursive
```

## Important Notes
- `.gitmodules` 文件定义了子模块的 URL 和路径
- 子模块版本由父仓库的 commit 指针控制
- 不要直接在子模块目录中修改代码（除非有意升级）
