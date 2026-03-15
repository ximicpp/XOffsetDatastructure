# Skill: docker-build-test

## Description
在 P2996 Docker 容器中执行完整的构建和测试流程，验证所有测试通过。这是每次代码修改后的标准验证步骤。

## When to Use
- 每次修改 `xoffsetdatastructure.hpp` 或测试文件后
- 提交代码前的必要验证
- 排查构建问题时

## Prerequisites
- Docker 已安装并运行
- P2996 Docker 镜像可用（`ghcr.io/ximicpp/typelayout-p2996:latest` 或 `xoffset-clang-p2996:latest`）

## Steps

### Step 1: 检查可用的 Docker 镜像
```bash
docker images --format "{{.Repository}}:{{.Tag}}" 2>/dev/null | grep -E "typelayout-p2996|xoffset-clang"
```

### Step 2: 在 Docker 中构建和测试

**使用 GitHub CI 镜像（推荐）：**
```bash
docker run --rm -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash ./build.sh
```

**Apple Silicon (M1/M2/M3) 需要加 platform 标志：**
```bash
docker run --rm --platform linux/amd64 -v $(pwd):/workspace -w /workspace ghcr.io/ximicpp/typelayout-p2996:latest bash ./build.sh
```

**使用本地自定义镜像：**
```bash
docker run --rm -v $(pwd):/workspace -w /workspace xoffset-clang-p2996:latest bash ./build.sh
```

**使用便捷脚本：**
```bash
./scripts/local-docker-test.sh
```

### Step 3: 验证输出
- 确认所有测试 PASS（如 `32/32 tests passed`）
- 确认 Demo 和签名检查成功
- 如有失败，查看具体错误信息

## Important Notes
- ⚠️ 必须使用 `bash ./build.sh`，不要直接 `./build.sh`（权限问题）
- ⚠️ 首次拉取镜像可能需要较长时间
- 构建输出在容器内的 `/workspace/build/` 目录

## Expected Output
```
[BUILD] All tests passed!
[BUILD] 32/32 tests PASSED
[SIG] Signature export complete
```
