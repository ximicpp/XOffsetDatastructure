# Skill: check-ci-status

## Description
查看 GitHub Actions CI 的运行状态，确认推送后的构建和测试是否通过。

## When to Use
- `git push` 后等待 CI 结果时
- 需要确认 CI 是否通过时
- PR 审查前检查 CI 状态时

## Prerequisites
- GitHub CLI (`gh`) 已安装并认证，或使用项目脚本

## Steps

### 方法 1: 使用 GitHub CLI
```bash
# 查看最近的 CI 运行
gh run list --limit 5

# 查看特定分支的 CI
gh run list --branch next_cpp26 --limit 5

# 查看最新运行的详情
gh run view --log

# 监听正在运行的 CI
gh run watch
```

### 方法 2: 使用项目脚本
```bash
# macOS / Linux
./scripts/check-ci-status.sh

# PowerShell (Windows)
./scripts/check-ci-status.ps1
./scripts/watch-ci.ps1
```

### 方法 3: 通过 Git 查看
```bash
# 查看最新提交的 CI 状态（需要 gh CLI）
gh api repos/ximicpp/XOffsetDatastructure/commits/$(git rev-parse HEAD)/status
```

### 方法 4: 直接访问 URL
```
https://github.com/ximicpp/XOffsetDatastructure/actions
```

## CI Workflow 说明
项目的 CI 定义在 `.github/workflows/ci.yml`：
1. **Checkout** — 检出代码（含子模块）
2. **Pull P2996 Docker image** — 拉取预构建的 P2996 镜像
3. **Build & test** — 在 Docker 中运行 `bash ./build.sh`
4. **Signature contract verification** — 检查签名文件是否有漂移

## CI 触发条件
- `push` 到 `main`, `master`, `next_cpp26` 分支
- `pull_request` 到这些分支
- 手动触发 (`workflow_dispatch`)

## Status Icons
- ✅ `success` — 全部通过
- ❌ `failure` — 有测试失败
- 🔄 `in_progress` — 正在运行
- ⏸️ `queued` — 等待运行

## Important Notes
- CI 通常需要 5-15 分钟完成
- 如果 CI 失败，使用 `ci-debug` skill 排查
- 推送前建议先通过本地 `docker-build-test` 验证
