# Skill: git-status-log

## Description
查看 Git 仓库的当前状态和提交历史，了解工作目录和分支情况。

## When to Use
- 了解当前仓库状态时
- 查看最近的提交历史时
- 确认当前分支和远程同步状态时

## Steps

### Step 1: 查看工作目录状态
```bash
git status
```

### Step 2: 查看提交历史
```bash
# 最近 N 条提交（简洁模式）
git log --oneline -5

# 带图形的分支历史
git log --oneline --graph -10

# 详细历史（含作者和日期）
git log -3
```

### Step 3: 查看当前分支信息
```bash
# 当前分支名
git branch --show-current

# 所有本地分支
git branch

# 包含远程分支
git branch -a
```

### Step 4: 查看远程同步状态
```bash
# 查看领先/落后远程的提交数
git status -sb

# 查看远程仓库信息
git remote -v
```

## Common Queries
```bash
# 查看某个文件的修改历史
git log --oneline -- xoffsetdatastructure.hpp

# 查看某次提交的详情
git show <commit-hash> --stat

# 查看谁最后修改了某一行
git blame <file> -L <start>,<end>
```

## Important Notes
- 所有 git 输出命令应加 `| cat` 管道避免进入 pager 模式
- `git status -sb` 是最简洁的状态查看方式
