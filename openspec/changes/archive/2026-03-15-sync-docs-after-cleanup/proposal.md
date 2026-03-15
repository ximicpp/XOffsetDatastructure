## Why

核心代码经过 `refactor: cleanup core header` 提交后，测试从 27 个精简为 23 个（7 基础 + 16 反射），4 个测试文件被合并删除。同时核心代码移除了多个概念和 API。刚刚同步的文档再次过时。

## What Changes

### 测试数量和列表修正（27 → 23）

- **`docs/QUICK_REFERENCE.md`**: 测试矩阵从 27/20 改为 23/16，移除 4 个已删除的测试名
- **`docs/BUILD_AND_TEST_GUIDE.md`**: 同上
- **`docs/README.md`**: 27→23, 20→16
- **`AGENTS.md`**: 27→23, 20→16（Test suite summary 部分）
- **`openspec/specs/test-organization/spec.md`**: 27→23, 20→16
- **`openspec/specs/documentation/spec.md`**: 27→23

### API 引用修正

- **`docs/ARCHITECTURE_REVIEW.md`**: `SupportedContainer` 概念已被移除的注释
- **`docs/TYPE_SUBSET_MODEL.md`**: `SupportedContainer` 引用更新
- **`docs/TYPELAYOUT_INTEGRATION_ANALYSIS.md`**: 测试文件引用从 `test_typelayout_integration` 改为 `test_type_signatures`

## Capabilities

（无新功能 — 纯文档同步）

## Impact

- 文件: 约 8 个 .md 文件
- 用户影响: 文档准确性
- 代码影响: 无
