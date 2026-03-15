# Skill: git-diff-review

## Description
查看代码变更差异，用于提交前审查或比较不同版本之间的区别。

## When to Use
- 提交前审查自己的变更时
- 比较两个提交或分支之间的差异时
- 确认暂存区内容是否正确时

## Steps

### Step 1: 查看未暂存的变更
```bash
# 完整 diff
git diff

# 仅看统计信息
git diff --stat
```

### Step 2: 查看已暂存的变更（即将提交的内容）
```bash
# 完整 diff
git diff --cached

# 仅看统计信息
git diff --cached --stat
```

### Step 3: 查看特定文件的变更
```bash
git diff -- xoffsetdatastructure.hpp
git diff --cached -- tests/test_basic_types.cpp
```

### Step 4: 比较提交之间的差异
```bash
# 最近一次提交与上一次的差异
git diff HEAD~1 HEAD --stat

# 两个特定提交之间
git diff <commit1> <commit2> --stat

# 两个分支之间
git diff main..next_cpp26 --stat
```

### Step 5: 查看某次提交引入的变更
```bash
git show <commit-hash>
git show <commit-hash> --stat   # 仅统计
```

## Advanced Usage
```bash
# 只看修改过的文件名
git diff --name-only

# 只看添加/删除的行数
git diff --shortstat

# Word-level diff（适合看注释变更）
git diff --word-diff

# 忽略空白变更
git diff -w
```

## Important Notes
- 所有 git diff 输出命令应加 `| cat` 管道避免进入 pager 模式
- `--stat` 选项最适合快速概览变更范围
- 提交前务必执行 `git diff --cached --stat` 确认即将提交的内容
