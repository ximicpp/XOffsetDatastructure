# Skill: git-branch-manage

## Description
Git 分支的创建、切换、合并和删除操作。

## When to Use
- 需要创建新的功能分支时
- 切换到不同分支工作时
- 合并分支或清理旧分支时

## Project Branch Convention
- `main` — 稳定发布分支
- `next_cpp26` — 主开发分支（C++26 反射架构）
- `feature/*` — 功能分支（从 `next_cpp26` 分出）

## Steps

### 创建新分支
```bash
# 基于当前分支创建并切换
git checkout -b feature/new-feature

# 基于特定分支创建
git checkout -b feature/new-feature next_cpp26
```

### 切换分支
```bash
git checkout next_cpp26
git checkout main
git checkout feature/new-feature
```

### 查看分支
```bash
# 本地分支
git branch

# 包含远程分支
git branch -a

# 带最后提交信息
git branch -v
```

### 合并分支
```bash
# 切换到目标分支
git checkout next_cpp26

# 合并功能分支
git merge feature/new-feature

# 合并后推送
git push origin next_cpp26
```

### 删除分支
```bash
# 删除已合并的本地分支
git branch -d feature/old-feature

# 强制删除未合并的分支
git branch -D feature/abandoned-feature

# 删除远程分支
git push origin --delete feature/old-feature
```

### Rebase（变基）
```bash
# 将功能分支变基到最新的 next_cpp26
git checkout feature/new-feature
git rebase next_cpp26
```

## Important Notes
- ⚠️ 切换分支前确保工作目录干净（`git status`）
- 合并前建议先拉取最新代码（`git pull`）
- 使用 `| cat` 管道避免 git 命令进入 pager 模式
- 避免直接在 `main` 分支上开发
