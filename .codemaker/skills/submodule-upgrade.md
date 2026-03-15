# Skill: submodule-upgrade

## Description
将 TypeLayout 子模块升级到最新版本（或指定版本），并提交子模块指针变更。

## When to Use
- TypeLayout 有新版本发布时
- 需要使用 TypeLayout 的新功能或修复时
- 用户明确要求更新子模块时

## Prerequisites
- 网络可用
- 工作目录干净

## Steps

### Step 1: 进入子模块目录并拉取最新
```bash
cd external/typelayout
git fetch origin
git checkout main
git pull origin main
```

### Step 2: 回到项目根目录
```bash
cd ../..
```

### Step 3: 检查子模块变更
```bash
git submodule status
git diff external/typelayout
```

### Step 4: 暂存并提交
```bash
git add external/typelayout
git commit -m "chore: update TypeLayout submodule to <new-commit-hash>"
```

### Step 5: 验证构建
升级后必须运行完整构建测试：
```bash
# Docker 方式
docker run --rm -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash ./build.sh
```

### Step 6: 推送
```bash
git push origin <branch>
```

### 升级到指定版本（非最新）
```bash
cd external/typelayout
git fetch origin
git checkout <specific-tag-or-commit>
cd ../..
git add external/typelayout
git commit -m "chore: update TypeLayout submodule to <version>"
```

## Important Notes
- ⚠️ 升级子模块后必须运行完整构建测试
- TypeLayout API 变更可能导致编译错误，需检查兼容性
- 子模块的 commit hash 记录在父仓库中，需要提交父仓库的变更
- 签名文件 `tools/sigs/*.sig.hpp` 可能需要重新生成
