## Context

`build.sh` 和 `AGENTS.md` 中的测试元数据在最近的测试合并清理后未同步。这是纯配置修正，不涉及架构决策。

## Goals / Non-Goals

**Goals:**
- `build.sh` 中 `test_xbuffer_api` 归入基础测试区（它不依赖反射）
- 所有测试计数源（`build.sh`、`AGENTS.md`、`CMakeLists.txt`、`tests/README.md`）保持一致 = 23
- `--no-reflection` 模式下能运行 7 个基础测试

**Non-Goals:**
- 不修改任何 `.cpp` 测试文件
- 不修改 `CMakeLists.txt`（已经正确）
- 不添加新测试

## Decisions

1. **将 `test_xbuffer_api` 移至基础测试区** — 它在 `CMakeLists.txt` 中就是基础测试，`build.sh` 应保持一致。
2. **基础测试编号 1-7，反射测试 8-23** — 重新编排 `run_test` 编号。
3. **AGENTS.md 只改数字** — `"23 tests"` 替换 `"27 tests"`，以及 `"16"` 替换 `"20"`。

## Risks / Trade-offs

- 风险极低，纯文本/脚本修正
- 测试执行顺序微调，不影响 CI 结果
