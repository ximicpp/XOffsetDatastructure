## Why

Type Safety 区域占 `xoffsetdatastructure.hpp` 的 312 行（16%），是最大的单一区域。但核心判定逻辑仅 ~68 行，其余 ~244 行（78%）是诊断函数、巨型 static_assert 字符串和冗余注释。诊断代码的行数远超核心逻辑，需要精简以提高可维护性。

## What Changes

- **压缩 `validate_xbuffer_type`**：将 38 行的巨型 static_assert 字符串缩减为 5-8 行简洁消息
- **压缩 `get_safety_error_message`**：将 29 行 if-else 链合并为更紧凑的分支结构（~15 行）
- **合并 `diagnose_base_at` + `diagnose_member_at`**：用一个模板函数替代两个几乎相同的函数
- **精简注释**：保留架构级注释（Safety Responsibility Model 等），删除重复解释和冗余描述
- 核心判定逻辑（`DefaultPolicy::accept`、`StrictPolicy`、`accept_all_members_impl`、`accept_all_bases_impl`）不做修改

## Capabilities

### New Capabilities
（无新增能力）

### Modified Capabilities
（无规格变更 — 本次仅为实现层面的代码精简，所有公共 API 和行为不变）

## Impact

- **文件**: `xoffsetdatastructure.hpp`（Type Safety 区域，第 759-1070 行）
- **预期缩减**: ~80 行（312 → ~230 行）
- **API 影响**: 无。`is_xbuffer_safe<T>`、`validate_xbuffer_type<T>`、`diagnose_unsafe_members<T>` 接口完全不变
- **行为影响**: 编译错误消息会更短但信息量不变
- **风险**: 低。纯粹的诊断/文档精简，核心判定逻辑不改动
- **验证**: Docker 构建 23 个测试全部通过