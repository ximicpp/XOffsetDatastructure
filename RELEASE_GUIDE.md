# XOffsetDatastructure v2.0 发布指南

本文档说明如何使用 `release_v2.0.sh` 脚本发布两个独立的 v2.0 版本。

## 📦 发布版本概览

| 版本 | 分支 | 标签 | 类型 | 状态 |
|------|------|------|------|------|
| **v2.0-practical** | `release/v2.0-practical` | `v2.0-practical` | Boost.PFR 实现 | 生产环境可用 ✅ |
| **v2.0-cpp26** | `release/v2.0-cpp26` | `v2.0-cpp26` | C++26 反射实现 | 实验性 ⚠️ |

---

## 🚀 快速开始

### 1. 运行发布脚本

```bash
./release_v2.0.sh
```

脚本会执行以下操作：

1. ✅ **预检查**：验证源分支存在、检查冲突
2. 🔨 **创建孤立分支**：从 `next_practical` 和 `next_cpp26` 创建无历史记录的新分支
3. 📝 **提交发布版本**：每个版本只有一个干净的初始提交
4. 🏷️ **创建标签**：为每个版本打标签
5. 📤 **推送到远程**：（可选）推送分支和标签到 GitHub

---

## 📋 脚本功能详解

### 预检查 (Pre-flight Checks)

脚本会自动检查：

- ✅ 当前目录是否为 Git 仓库
- ✅ 源分支 `next_practical` 和 `next_cpp26` 是否存在
- ⚠️ 如果发布分支已存在，提示是否删除重建
- ⚠️ 如果标签已存在，提示是否删除重建

### 创建 v2.0-practical

```bash
# 源分支：next_practical
# 目标分支：release/v2.0-practical
# 标签：v2.0-practical
```

**特性：**
- Boost.PFR 实现的类型安全
- 跨平台支持（Linux, macOS, Windows, iOS, Android）
- 生产环境可用
- 完整的 CI/CD 测试

### 创建 v2.0-cpp26

```bash
# 源分支：next_cpp26
# 目标分支：release/v2.0-cpp26
# 标签：v2.0-cpp26
```

**特性：**
- C++26 原生反射（P2996）
- 无外部依赖（不需要 Boost）
- 实验性功能
- 需要特殊编译器构建

---

## 🔍 验证发布

### 查看创建的分支

```bash
# 查看本地分支
git branch | grep release

# 应该显示：
#   release/v2.0-practical
#   release/v2.0-cpp26
```

### 查看标签

```bash
# 查看本地标签
git tag | grep v2.0

# 应该显示：
#   v2.0-practical
#   v2.0-cpp26
```

### 检查分支内容

```bash
# 检查 practical 版本
git checkout release/v2.0-practical
git log --oneline
# 应该只显示 1 个提交

# 检查 cpp26 版本
git checkout release/v2.0-cpp26
git log --oneline
# 应该只显示 1 个提交
```

### 验证编译

```bash
# 测试 practical 版本
git checkout release/v2.0-practical
mkdir -p build && cd build
cmake ..
cmake --build .
./examples/helloworld
./examples/demo

# 测试 cpp26 版本（需要特殊编译器）
git checkout release/v2.0-cpp26
mkdir -p build && cd build
cmake ..
cmake --build .
```

---

## 📤 推送到 GitHub

脚本会询问是否推送到远程仓库。如果选择 `y`，会执行：

```bash
# 推送分支
git push origin release/v2.0-practical
git push origin release/v2.0-cpp26

# 推送标签
git push origin v2.0-practical
git push origin v2.0-cpp26
```

### 手动推送（如果脚本时选择了跳过）

```bash
# 一次性推送所有
git push origin release/v2.0-practical release/v2.0-cpp26
git push origin v2.0-practical v2.0-cpp26

# 或者分别推送
git push origin release/v2.0-practical
git push origin v2.0-practical
git push origin release/v2.0-cpp26
git push origin v2.0-cpp26
```

---

## 🎯 创建 GitHub Release

推送后，在 GitHub 上创建正式发布：

### 1. 进入 Releases 页面

访问：`https://github.com/<your-username>/XOffsetDatastructure/releases/new`

### 2. 创建 v2.0-practical Release

- **Tag**: `v2.0-practical`
- **Title**: `v2.0-practical: Production-Ready Edition`
- **Type**: ✅ **Latest release**
- **Description**:

```markdown
# XOffsetDatastructure v2.0 - Practical Edition

Production-ready implementation using **Boost.PFR** for compile-time type safety.

## ✨ Key Features

- ✅ Type-safe offset-based containers (XVector, XMap, XSet, XString)
- ✅ Compile-time type safety checks using Boost.PFR
- ✅ Zero-copy binary serialization
- ✅ Memory-efficient growth (1.1x factor)
- ✅ Cross-platform support (Linux, macOS, Windows, iOS, Android)
- ✅ Automatic code generation from YAML schemas

## 📦 Requirements

- C++20 compiler (Clang 15+, GCC 11+, MSVC 2022+)
- Boost.PFR (header-only)
- CMake 3.15+

## 🚀 Quick Start

```bash
# Clone repository
git clone -b release/v2.0-practical https://github.com/<your-username>/XOffsetDatastructure.git
cd XOffsetDatastructure

# Build
mkdir build && cd build
cmake ..
cmake --build .

# Run examples
./examples/helloworld
./examples/demo

# Run tests
ctest
```

## 📚 Documentation

- See `examples/helloworld.cpp` for basic usage
- See `examples/demo.cpp` for comprehensive features
- See `schemas/README.md` for YAML schema format

## 📝 License

MIT License
```

### 3. 创建 v2.0-cpp26 Release

- **Tag**: `v2.0-cpp26`
- **Title**: `v2.0-cpp26: C++26 Reflection Edition (Experimental)`
- **Type**: ⚠️ **Pre-release**
- **Description**:

```markdown
# XOffsetDatastructure v2.0 - C++26 Edition (Experimental)

Experimental implementation using **native C++26 reflection (P2996)** without external dependencies.

## ⚠️ EXPERIMENTAL

This version requires an experimental Clang build with P2996 support.  
**Not recommended for production use** until C++26 is standardized.

## ✨ Key Features

- ✅ Native C++26 reflection (P2996)
- ✅ Zero external dependencies (no Boost)
- ✅ Compile-time type introspection
- ✅ Advanced meta-programming capabilities
- ✅ Automatic type signature generation

## 📦 Requirements

- C++26 (experimental)
- Clang with P2996 reflection support (experimental build)
- CMake 3.15+

## 🔨 Building Experimental Compiler

See `scripts/build_clang_p2996_wsl.sh` for instructions on building Clang with P2996 support.

## 📚 Documentation

- See `tests/test_reflection_*.cpp` for reflection examples
- See `tests/test_type_introspection.cpp` for advanced usage
- See `tests/test_splice_operations.cpp` for meta-programming

## 📝 License

MIT License
```

---

## 🛠️ 脚本选项说明

### 交互式提示

脚本会在以下情况要求用户确认：

1. **开始发布流程**
   ```
   This script will create two independent release branches:
     • v2.0-practical: Production-ready with Boost.PFR
     • v2.0-cpp26: Experimental with C++26 reflection
   
   Continue? (y/N):
   ```

2. **删除已存在的分支/标签**
   ```
   Branch 'release/v2.0-practical' already exists
   Delete and recreate? (y/N):
   ```

3. **推送到远程仓库**
   ```
   This will push the following to origin:
     • Branch: release/v2.0-practical
     • Tag: v2.0-practical
     • Branch: release/v2.0-cpp26
     • Tag: v2.0-cpp26
   
   Continue? (y/N):
   ```

### 错误处理

脚本使用 `set -e`，任何命令失败都会立即退出。

常见错误：

- **"Not a Git repository"**: 请在 Git 仓库根目录运行
- **"Branch does not exist"**: 确保 `next_practical` 和 `next_cpp26` 分支存在
- **"Aborted by user"**: 用户取消操作

---

## 📊 发布后的分支结构

```
XOffsetDatastructure/
├── main                           # 原主分支
├── next_practical                 # 开发分支（保留）
├── next_cpp26                     # 开发分支（保留）
├── release/v2.0-practical         # 发布分支（新建，无历史）
│   └── [单个干净的提交]
└── release/v2.0-cpp26             # 发布分支（新建，无历史）
    └── [单个干净的提交]

Tags:
├── v2.0-practical -> release/v2.0-practical
└── v2.0-cpp26     -> release/v2.0-cpp26
```

---

## 🔄 后续维护

### 修复 Bug 或添加功能

如果需要更新发布版本：

```bash
# 1. 在原开发分支上修复
git checkout next_practical
# ... 进行修复 ...
git commit -m "fix: some bug"

# 2. 重新运行发布脚本创建新版本
./release_v2.0.sh  # 会提示删除旧版本

# 或者创建新的小版本
# 手动修改脚本中的版本号：v2.0.1-practical
```

### 用户如何获取发布版本

```bash
# 克隆 practical 版本
git clone -b release/v2.0-practical https://github.com/<user>/XOffsetDatastructure.git

# 或克隆 cpp26 版本
git clone -b release/v2.0-cpp26 https://github.com/<user>/XOffsetDatastructure.git

# 或通过标签
git clone --depth 1 --branch v2.0-practical https://github.com/<user>/XOffsetDatastructure.git
```

---

## ❓ FAQ

### Q: 为什么使用孤立分支（orphan branch）？

**A:** 孤立分支没有提交历史，只有一个干净的初始提交。这样：
- ✅ 发布版本体积更小
- ✅ 用户获取时更快
- ✅ 不暴露开发过程中的中间状态
- ✅ 提交记录清晰简洁

### Q: 两个版本有什么区别？

**A:**

| 特性 | v2.0-practical | v2.0-cpp26 |
|------|----------------|------------|
| 类型反射 | Boost.PFR | C++26 原生反射 |
| 编译器要求 | C++20 标准编译器 | Clang P2996 实验版本 |
| 外部依赖 | Boost.PFR (header-only) | 无 |
| 生产环境 | ✅ 推荐 | ⚠️ 实验性 |
| 平台支持 | 全平台 | 限制平台 |
| CI/CD | ✅ 完整测试 | ⚠️ 实验性测试 |

### Q: 可以同时维护两个版本吗？

**A:** 可以。两个发布分支是独立的，互不影响。你可以：
- 在 `next_practical` 上继续开发，发布 v2.1-practical
- 在 `next_cpp26` 上继续开发，发布 v2.1-cpp26

### Q: 如果发布后发现问题怎么办？

**A:** 
1. 在原开发分支修复
2. 重新运行脚本（选择删除旧版本）
3. 或者发布一个新的小版本（如 v2.0.1）

### Q: 脚本安全吗？

**A:** 是的。脚本：
- ✅ 不会删除或修改原开发分支
- ✅ 所有操作都有确认提示
- ✅ 使用 `set -e` 确保错误时停止
- ✅ 会在删除前询问用户

---

## 📞 联系与支持

如有问题，请：
- 📧 提交 GitHub Issue
- 💬 查看文档：`docs/`
- 🧪 运行测试：`ctest`

---

**祝发布顺利！** 🎉
