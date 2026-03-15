## Why

核心代码在最近的清理 commit (`8ff3532f`) 中做了大幅精简（39→23 测试文件），但 `build.sh`、`AGENTS.md` 中的测试计数和分类未同步更新，导致：
1. `build.sh` 中 `test_xbuffer_api` 被错误归类到反射测试区，`--no-reflection` 模式下漏跑
2. `AGENTS.md` 仍声称 27 个测试（实际 23 个）
3. `build.sh` 基础测试计数为 6（应为 7，含 xbuffer_api）

## What Changes

- 修复 `build.sh`：将 `test_xbuffer_api` 从反射测试区移至基础测试区，非反射模式 `TOTAL_TESTS` 改为 7
- 修复 `AGENTS.md`：测试总数从 27 更新为 23（7 basic + 16 reflection）

## Capabilities

### New Capabilities

（无新增能力）

### Modified Capabilities

（无规格级变更 — 这是纯元数据/配置修正）

## Impact

- `build.sh` — 测试执行顺序和计数
- `AGENTS.md` — 文档中的测试套件描述
- 不影响任何 `.cpp` 源文件或 `CMakeLists.txt`
