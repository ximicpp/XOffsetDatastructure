# Skill: signature-verify

## Description
检查 TypeLayout 签名文件是否有二进制合约漂移，确保类型布局在不同构建之间保持一致。

## When to Use
- 修改数据结构定义后
- TypeLayout 子模块升级后
- CI 的签名验证步骤失败时
- 发布版本前

## Background
签名文件 (`tools/sigs/*.sig.hpp`) 包含每个数据结构的编译时类型签名。当类型布局（成员顺序、大小、对齐）发生变化时，签名会改变，这可能导致序列化数据的二进制不兼容。

## Signature File Location
```
tools/sigs/
  x86_64_linux_clang.sig.hpp   # x86-64 Linux (Clang P2996) 平台签名
```

## Steps

### Step 1: 检查本地签名文件状态
```bash
git diff tools/sigs/
```

### Step 2: 在 Docker 中重新生成签名
```bash
docker run --rm -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash ./build.sh
```
build.sh 会自动在反射模式下导出签名文件。

### Step 3: 对比签名变化
```bash
git diff tools/sigs/x86_64_linux_clang.sig.hpp
```

**如果只有时间戳变化** — 安全，正常的重新生成。
**如果签名内容变化** — ⚠️ 二进制合约漂移！需要审查。

### Step 4: 处理签名漂移

#### 预期变化（有意修改了数据结构）
```bash
git add tools/sigs/
git commit -m "chore: update signatures after type layout change"
```

#### 非预期变化（不应该有布局变更）
需要排查原因：
- 检查是否无意修改了结构成员
- 检查 TypeLayout 子模块版本是否变化
- 检查编译器版本是否变化

### Step 5: CI 中的签名验证
CI 的 `Signature contract verification` 步骤会对比 Docker 中重新生成的签名与仓库中已提交的签名：
- 如果一致 → 通过
- 如果不一致 → 警告（当前不阻断构建，但会在日志中标注）

## Important Notes
- 签名文件是自动生成的，不要手动编辑
- 签名格式为 `constexpr` C++17 代码，可在任何 C++17 编译器中使用（无需 P2996）
- 签名漂移可能影响已有序列化数据的兼容性
- 版本发布前必须确认签名文件已更新
