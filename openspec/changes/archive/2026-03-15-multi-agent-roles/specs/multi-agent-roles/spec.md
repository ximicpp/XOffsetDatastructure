## ADDED Requirements

### Requirement: AGENTS.md defines three agent roles with clear scope

AGENTS.md SHALL 包含 `## Agent Roles` 章节，定义以下三个角色：

| 角色 | Scope | 核心约束 |
|------|-------|----------|
| core | `xoffsetdatastructure.hpp`, `external/typelayout` | API 变更需创建 openspec change |
| tests | `tests/`, `build.sh` (test list) | 新测试必须注册到 CMakeLists.txt |
| docs | `docs/`, `README.md`, `tests/README.md`, `examples/` | 保持与代码一致 |

#### Scenario: Agent reads role and understands boundaries
- **GIVEN** 一个新 Agent 会话被告知 "你是 test agent"
- **WHEN** Agent 读取 AGENTS.md 的 `[tests]` 分区
- **THEN** Agent 知道自己的文件 Scope、行为规则、以及如何与其他角色协调

### Requirement: Cross-agent coordination via openspec tags

openspec tasks.md 中 SHALL 支持跨 Agent 标签：
- `[needs-tests]` — 表示该任务需要 test agent 跟进
- `[needs-docs]` — 表示该任务需要 docs agent 跟进
- `[core]` / `[tests]` / `[docs]` — 标明任务所属角色

#### Scenario: Test agent finds pending work
- **GIVEN** core agent 完成了一个 API 变更并在 tasks.md 中标注 `[needs-tests]`
- **WHEN** test agent 检查 `openspec/changes/*/tasks.md`
- **THEN** test agent 找到待处理的测试任务
