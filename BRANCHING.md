# XOffsetDatastructure 分支管理策略

## 概述

本项目维护 **两条并行主线**，分别面向不同的 C++ 标准和编译器要求：

| 主线 | C++ 标准 | 反射机制 | 编译器要求 | 目标用户 |
|------|----------|----------|-----------|----------|
| **Practical** | C++17 / C++20 | Boost.PFR（非侵入式） | GCC / Clang / MSVC | 生产环境、广泛兼容 |
| **C++26** | C++26 | P2996 原生反射 | Clang P2996 专用 | 前沿探索、未来标准 |

---

## 分支拓扑

```
main (稳定入口)
 │
 ├── next_practical          ← Practical 主线开发分支 (C++17/20)
 │    └── release/v2.0-practical  ← 稳定发布版本
 │
 └── next_cpp26              ← C++26 主线开发分支 (P2996 反射)
      └── release/v2.0-cpp26      ← 稳定发布版本

deprecated/
 ├── experiment              ← 已废弃：早期实验
 └── next                    ← 已废弃：旧的统一 next 分支
```

---

## 分支详细说明

### 核心分支

| 分支名 | 角色 | 说明 |
|--------|------|------|
| `main` | 稳定入口 | 项目的公开入口点。README 和基础构建脚本。不直接开发。 |
| `next_cpp26` | C++26 开发 | **当前最活跃**。基于 P2996 反射的零样板实现。需要 Clang P2996 编译器。 |
| `next_practical` | Practical 开发 | 基于 Boost.PFR 的实用实现。支持 GCC/Clang/MSVC，面向生产环境。 |

### 发布分支

| 分支名 | 版本 | 说明 |
|--------|------|------|
| `release/v2.0-cpp26` | v2.0 | C++26 反射版本的稳定快照 |
| `release/v2.0-practical` | v2.0 | Practical 版本的稳定快照，支持 MSVC 签名生成 |

### 废弃分支

| 分支名 | 说明 |
|--------|------|
| `deprecated/next` | 旧的统一开发分支，已拆分为 `next_cpp26` 和 `next_practical` |
| `deprecated/experiment` | 早期实验代码，仅保留历史参考 |

---

## 工作流

### 日常开发

```
1. 确定工作主线（Practical 或 C++26）
2. 切换到对应的 next_* 分支
3. 开发 → 测试 → 提交 → 推送
```

```bash
# C++26 主线开发（当前活跃）
git checkout next_cpp26
# ... 开发和测试 ...
git push origin next_cpp26

# Practical 主线开发
git checkout next_practical
# ... 开发和测试 ...
git push origin next_practical
```

### 发布流程

```bash
# 从开发分支创建/更新发布分支
git checkout next_cpp26
git checkout -b release/v2.1-cpp26    # 新版本
# 或更新现有发布分支
git checkout release/v2.0-cpp26
git merge next_cpp26
git push origin release/v2.0-cpp26
```

### 功能分支（可选）

对于大型功能，可以从 `next_*` 分支创建功能分支：

```bash
git checkout -b feature/new-compactor next_cpp26
# ... 开发 ...
git checkout next_cpp26
git merge feature/new-compactor
git branch -d feature/new-compactor
```

---

## 分支间关系

### 独立并行
两条主线 **独立开发**，不直接合并：

```
next_practical ──────────────────────→ (C++17/20 生态)
                    ✕ 不合并
next_cpp26     ──────────────────────→ (C++26 生态)
```

### 可共享的内容
以下内容可以通过 cherry-pick 在两条主线间同步：
- 文档更新（`docs/`）
- 构建脚本改进（`build.sh`, CI）
- 测试数据结构设计（不含反射代码）
- Bug 修复（如果逻辑通用）

```bash
# 从 next_cpp26 cherry-pick 一个文档修复到 next_practical
git checkout next_practical
git cherry-pick <commit-hash>
```

### 不可共享的内容
- P2996 反射代码（`#ifdef __cpp_reflection` 内的代码）
- TypeLayout 子模块（仅 C++26 主线使用）
- Boost.PFR 相关代码（仅 Practical 主线使用）

---

## 命名规范

| 类型 | 格式 | 示例 |
|------|------|------|
| 开发分支 | `next_<edition>` | `next_cpp26`, `next_practical` |
| 发布分支 | `release/v<major>.<minor>-<edition>` | `release/v2.0-cpp26` |
| 功能分支 | `feature/<name>` | `feature/new-compactor` |
| 修复分支 | `fix/<name>` | `fix/memory-leak` |
| 废弃分支 | `deprecated/<name>` | `deprecated/next` |

---

## CI 覆盖

| 分支 | CI 触发 | 构建环境 |
|------|---------|----------|
| `next_cpp26` | push + PR | Docker P2996 (`ghcr.io/ximicpp/typelayout-p2996`) |
| `next_practical` | push + PR | 标准 GCC/Clang/MSVC |
| `main` | push + PR | 两者（取决于 CI 配置） |
| `release/*` | 手动 / tag | 对应主线的 CI 环境 |

---

## 当前状态（2026-03）

| 分支 | 最新提交 | 活跃度 |
|------|----------|--------|
| `next_cpp26` | `1c8a30f6` — skills + rule | 🟢 活跃 |
| `next_practical` | `c2a4797e` — optimize comments | 🟡 维护 |
| `main` | `78b82205` — build scripts fix | 🟡 稳定 |
| `release/v2.0-cpp26` | `ccf94e43` — v2.0 release | 🔵 已发布 |
| `release/v2.0-practical` | `0337d166` — MSVC sig gen | 🔵 已发布 |
| `deprecated/*` | — | 🔴 废弃 |
