## Context

在 AGENTS.md 中增加角色分区，使多个 Agent 会话可以并行工作于同一仓库。
协调介质是已有的 openspec/ 目录和 git。不引入新工具。

## Goals / Non-Goals

**Goals:**
- 定义 core / tests / docs 三个 Agent 角色的文件 Scope 和行为规则
- 定义跨 Agent 协调协议（openspec 标签 + git commit）
- 更新过时的 skill 文件（测试数量 32 → 27）

**Non-Goals:**
- 不实现自动化 Agent 调度（依赖人工启动会话）
- 不修改 openspec CLI 工具
- 不修改任何源码

## Decisions

### D1: 角色定义在 AGENTS.md 而非独立文件

将三个角色的定义放在 AGENTS.md 的新章节中，而非创建 `AGENTS-core.md` 等独立文件。

**理由**: AGENTS.md 是所有 Agent 的统一入口点。角色分区放在同一个文件中，
Agent 可以看到全局视图（理解其他角色的边界），同时聚焦自己的分区。

### D2: 软边界而非硬隔离

Scope 定义是"建议"而非"强制"。Agent 可以在必要时越界（如 core agent 修改内联注释
影响了文档），但需在 commit message 中说明原因。

**理由**: 硬隔离在实践中会产生大量跨角色协调开销。软边界 + 约定更灵活。

### D3: 协调通过 openspec tasks 标签，不通过新文件

使用 `[needs-tests]` `[needs-docs]` 标签在 tasks.md 中标注跨角色需求，
而非创建专门的"协调文件"或"消息队列"。

**理由**: openspec changes 已经是结构化的变更记录，在其中增加标签是自然的扩展。

## Risks / Trade-offs

- **[Risk] Agent 不读 AGENTS.md** → 启动会话时明确指示"读 AGENTS.md [tests] 分区"
- **[Trade-off] 软边界可能导致越界** → 可接受，commit review 时发现
