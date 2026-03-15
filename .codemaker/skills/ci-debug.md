# Skill: ci-debug

## Description
当 GitHub Actions CI 失败时，查看日志、分析失败原因并提出修复方案。

## When to Use
- CI 运行失败时
- 本地通过但 CI 失败时
- 需要排查 CI 环境问题时

## Steps

### Step 1: 获取失败的 CI 运行信息
```bash
# 列出最近失败的运行
gh run list --status failure --limit 5

# 查看特定运行的日志
gh run view <run-id> --log

# 下载日志文件
gh run view <run-id> --log > ci-failure.log
```

### Step 2: 定位失败步骤
CI 工作流的步骤顺序：
1. **Checkout** — 代码检出失败 → 子模块问题
2. **Pull Docker image** — 镜像拉取失败 → 网络/镜像问题
3. **Build & test** — 编译/测试失败 → 代码问题
4. **Signature verification** — 签名漂移 → 类型布局变更

### Step 3: 常见失败模式与修复

#### 编译错误
```
error: ... in 'xoffsetdatastructure.hpp'
```
**修复**: 在本地 Docker 中重现：
```bash
docker run --rm -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash ./build.sh
```

#### 测试失败
```
test_xxx ... FAILED
```
**修复**: 在 Docker 中运行单个测试进行调试。

#### 子模块缺失
```
fatal: No url found for submodule path 'external/typelayout'
```
**修复**: 确认 `.gitmodules` 正确，子模块已提交。

#### Docker 镜像拉取失败
```
Error: pull access denied for ghcr.io/ximicpp/typelayout-p2996
```
**修复**: 确认镜像权限设置为 public。

#### 签名合约漂移
```
WARNING: Signature contract drift detected!
```
**修复**: 在 Docker 中重新构建以生成最新签名文件，然后提交更新。

### Step 4: 本地复现
```bash
# 使用与 CI 完全相同的环境
docker run --rm -it -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash

# 在交互 Shell 中手动执行每一步
bash ./build.sh
```

### Step 5: 修复并重新触发 CI
```bash
# 修复问题后
git add -A && git commit -m "fix: resolve CI failure - <description>"
git push origin <branch>
```

## Important Notes
- CI 环境是 Linux x86_64，与本地 macOS/Windows 可能不同
- 超时限制：30 分钟
- CI 使用 `ghcr.io/ximicpp/typelayout-p2996:latest` 镜像
- 可手动触发 CI：GitHub Actions 页面 → "Run workflow"
