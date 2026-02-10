# Change: 移除废弃的 XTypeSignature 兼容层

## Why
TypeLayout 集成已完成并通过所有 21 项测试验证。`XTypeSignature` 命名空间作为过渡兼容层已完成其使命，继续保留会增加维护负担和新用户的认知混乱。现在是完全清理的最佳时机。

## What Changes
- **BREAKING**: 删除 `XTypeSignature` 命名空间（`xoffsetdatastructure2.hpp` 第 57-115 行）
- 将 `BASIC_ALIGNMENT` 和 `ANY_SIZE` 常量迁移到 `XOffsetDatastructure2` 命名空间
- 将平台 `static_assert` 断言移到文件顶层作用域
- 更新所有测试文件，移除对 `XTypeSignature` 的引用
- 更新所有文档，去除 "旧 vs 新" 比较，确立 TypeLayout 为唯一标准
- 更新 `openspec/project.md` 和 `AGENTS.md` 中的命名空间引用

## Impact
- Affected specs: `type-signature`（MODIFIED: 移除兼容层 Requirement；REMOVED: XTypeSignature 兼容层）
- Affected code:
  - `xoffsetdatastructure2.hpp`：删除兼容层代码
  - `tests/test_typelayout_integration.cpp`：移除 Test 7（兼容层测试）
  - `tests/test_reflection_type_signature.cpp`：替换 `using namespace XTypeSignature`
  - `examples/player.hpp`、`examples/game_data.hpp`：更新注释和 alignas 引用
  - `docs/MIGRATION_TYPELAYOUT.md`：更新迁移指南，移除兼容层章节
  - `docs/core-features-analysis.md`：更新类型签名系统描述
  - `docs/technical_overview.md`（如存在）：同步更新
  - `AGENTS.md`：更新命名空间约定
  - `openspec/project.md`：更新命名空间列表
