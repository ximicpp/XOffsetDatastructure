# Skill: git-pull-latest

## Description
从远程仓库拉取最新代码，包括子模块更新。

## When to Use
- 开始新的工作会话前
- 用户明确要求 "拉取最新" 时
- 需要同步远程变更时

## Prerequisites
- 工作目录干净（无未提交的更改），或已暂存变更
- 网络可用

## Steps

### Step 1: 检查当前状态
```bash
git status
```
如果有未提交的更改，先决定是提交还是 stash。

### Step 2: 拉取最新代码
```bash
git pull origin <branch>
```

**常用分支：**
```bash
git pull origin next_cpp26   # 主开发分支
git pull origin main          # 稳定分支
```

### Step 3: 验证拉取结果
```bash
git log --oneline -5
```

### Step 4: 更新子模块（如果需要）
如果拉取中包含子模块变更：
```bash
git submodule update --init --recursive
```

## Handling Conflicts
```bash
# 如果出现合并冲突
git status                    # 查看冲突文件
# 手动解决冲突后
git add <conflicted-files>
git commit -m "merge: resolve conflicts"
```

## Important Notes
- 使用 `| cat` 管道避免 git 命令进入 pager 模式
- 拉取后注意检查子模块是否需要更新（输出中会提示 `Fetching submodule`）
- Fast-forward 合并是最理想的情况，说明没有分叉
