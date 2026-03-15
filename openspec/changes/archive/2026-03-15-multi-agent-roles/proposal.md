## Why

XOffset 的开发工作天然分为三个并行域：核心代码、测试用例、文档。目前所有工作在
单一 Agent 会话中串行完成，导致上下文累积过长、无法并行、且缺乏职责边界。

通过在 AGENTS.md 中定义角色分区（core / tests / docs），配合 openspec changes
中的跨 Agent 标签（`[needs-tests]`、`[needs-docs]`），可以让不同会话的 Agent
各司其职、通过 git 和 openspec 目录结构自然协调。

## What Changes

1. **AGENTS.md** — 新增 `## Agent Roles` 章节，定义三个角色的 Scope、Rules 和协作协议
2. **openspec tasks 约定** — 文档化跨 Agent 标签 `[needs-tests]` `[needs-docs]` 的用法
3. **docker-build-test.md** — 更新测试数量（32 → 27）

## Capabilities

### New Capabilities

- **Multi-Agent Role System**: AGENTS.md 中的角色分区让不同会话的 Agent 知道自己的职责范围和文件权限边界

### Modified Capabilities

_无_

## Impact

- **代码**: 零 — 不修改任何源码
- **流程**: 新会话启动时可指定角色（"你是 test agent"），Agent 读 AGENTS.md 即知行为约束
- **协调**: openspec/changes 作为 Agent 间的通信协议，tasks.md 中的标签作为任务路由
- **风险**: 零 — 纯文档变更，不影响构建或测试
