# Skill: git-branch-manage

## Description
Git 分支的创建、切换、合并和删除操作。本项目维护两条并行主线，详见 `BRANCHING.md`。

## When to Use
- 需要切换主线（C++26 ↔ Practical）时
- 需要创建新的功能分支时
- 合并分支或清理旧分支时

## Project Branch Architecture

### 两条并行主线
```
main (稳定入口)
 ├── next_practical     ← C++17/20 Practical 主线 (Boost.PFR)
 │    └── release/v2.0-practical
 └── next_cpp26         ← C++26 主线 (P2996 反射) 【当前活跃】
      └── release/v2.0-cpp26
```

| 主线 | 分支 | C++ 标准 | 编译器 |
|------|------|----------|--------|
| **C++26** | `next_cpp26` | C++26 | Clang P2996 |
| **Practical** | `next_practical` | C++17/20 | GCC / Clang / MSVC |

### 命名规范
| 类型 | 格式 | 示例 |
|------|------|------|
| 开发分支 | `next_<edition>` | `next_cpp26`, `next_practical` |
| 发布分支 | `release/v<M>.<m>-<edition>` | `release/v2.0-cpp26` |
| 功能分支 | `feature/<name>` | `feature/new-compactor` |
| 修复分支 | `fix/<name>` | `fix/memory-leak` |
| 废弃分支 | `deprecated/<name>` | `deprecated/next` |

## Steps

### 切换主线
```bash
# 切换到 C++26 主线
git checkout next_cpp26

# 切换到 Practical 主线
git checkout next_practical
```

### 创建功能分支
```bash
# 基于 C++26 主线
git checkout -b feature/new-feature next_cpp26

# 基于 Practical 主线
git checkout -b feature/new-feature next_practical
```

### 查看分支
```bash
git branch -v         # 本地分支 + 最新提交
git branch -a         # 包含远程分支
```

### 合并功能分支
```bash
git checkout next_cpp26
git merge feature/new-feature
git push origin next_cpp26
git branch -d feature/new-feature
```

### 跨主线同步（仅限通用内容）
```bash
# Cherry-pick 文档/构建脚本修复到另一条主线
git checkout next_practical
git cherry-pick <commit-hash>
```

### 创建发布分支
```bash
git checkout next_cpp26
git checkout -b release/v2.1-cpp26
git push origin release/v2.1-cpp26
```

### 删除分支
```bash
git branch -d feature/old-feature          # 删除已合并的本地分支
git branch -D feature/abandoned-feature    # 强制删除
git push origin --delete feature/old-feature  # 删除远程分支
```

## Important Rules
- ⚠️ 两条主线 **不直接合并**（`next_cpp26` ✕ `next_practical`）
- ⚠️ 不要在 `main` 上直接开发
- ⚠️ 切换分支前确保工作目录干净（`git status`）
- P2996 反射代码只能存在于 `next_cpp26` 分支
- Boost.PFR 代码只能存在于 `next_practical` 分支
- 使用 `| cat` 管道避免 git 命令进入 pager 模式
- 完整分支策略参见 `BRANCHING.md`