# Skill: git-commit-push

## Description
完整的 Git 提交和推送流程：暂存所有更改、提交并推送到远程仓库。

## When to Use
- 代码修改完成并通过构建验证后
- 用户明确要求 "提交和推送" 时

## Prerequisites
- 所有更改已验证通过（构建 + 测试）
- 工作目录中有未提交的更改

## Steps

### Step 1: 检查当前状态
```bash
git status
```

### Step 2: 查看变更详情
```bash
git diff --stat          # 未暂存的变更概要
git diff --cached --stat # 已暂存的变更概要
```

### Step 3: 暂存所有更改
```bash
git add -A
```

### Step 4: 提交
```bash
git commit -m "<type>: <简短描述>

<详细说明（可选）>"
```

**Commit 类型规范：**
- `feat:` — 新功能
- `fix:` — 修复 bug
- `refactor:` — 重构（不改变功能）
- `chore:` — 构建/CI/工具变更
- `docs:` — 文档更新
- `test:` — 测试相关

### Step 5: 推送到远程
```bash
git push origin <branch>
```

**常用分支：**
- `next_cpp26` — 主开发分支
- `main` — 稳定分支

## Commit Message Examples
```bash
# 功能添加
git commit -m "feat: integrate SigExporter into build pipeline and CI"

# Bug 修复
git commit -m "fix: DRY make_handle and eliminate redundant copy in TypedXBuffer::load"

# 重构
git commit -m "refactor: streamline xoffsetdatastructure.hpp (2162 -> 2007 lines)"

# 文档
git commit -m "docs: update technical overview for TypeLayout integration"
```

## Important Notes
- ⚠️ 提交前必须通过 `docker-build-test` 或 `native-build-test` 验证
- 使用 `| cat` 管道避免 git 命令进入 pager 模式
- 检查 `git status` 确认没有遗漏文件
