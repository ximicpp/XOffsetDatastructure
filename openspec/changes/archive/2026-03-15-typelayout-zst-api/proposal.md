## Why

TypeLayout 经过多次重构，其签名引擎和安全分类 API 已经能够完整判定"给定架构集合 × 类型集合"的 Zero-Serialization Transfer (ZST)。然而 XOffset 仍然自研了 `classify_for_xoffset<T>()`、Warning→Risk 升级、`long` 拒绝等重复逻辑，且运行期 `classify_safety(string_view)` 存在 `union[` 漏检 bug。需要在 TypeLayout 侧补齐 C1+C2 联合判定 API，使 XOffset 能完全委托 serialization-free 核心判断，消除重复实现。

## What Changes

- **TypeLayout (submodule)**:
  - **Bug fix**: `classify_safety(string_view)` 补上缺失的 `union[` 检测
  - `classify_safety(string_view)` 从 `inline` 升级为 `inline constexpr`，支持编译期使用
  - 新增 `all_serialization_free(PlatformInfo&, PlatformInfo&)` constexpr 函数，联合判定 C1(layout match) + C2(safety == Safe)
  - 新增 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE(first, ...)` 宏，提供编译期 static_assert
  - 新增 `CompatReporter::all_serialization_free()` 和 `is_type_serialization_free(name)` 运行期 helper

- **XOffsetDatastructure (主仓库)**:
  - 删除 `classify_for_xoffset<T>()`，`DefaultPolicy` / `StrictPolicy` 直接调用 TypeLayout 的 `is_layout_safe<T>()`
  - 删除 ghost symbol `using is_serialization_free_local`
  - 删除 `ArchSpec` 结构体及 15 个冗余 `static_assert`，保留 preprocessor 64-bit + little-endian 门控
  - 更新 `get_safety_error_message<T>()` 移除 `long` 专用分支
  - **BREAKING**: `classify_for_xoffset<T>()` 被移除（`detail` 命名空间，非公开 API）
  - **行为变化**: `long` 在 LLP64 上从编译期 C2 拒绝变为 CI C1 拒绝（概念更正确）

## Capabilities

### New Capabilities
- `zst-verdict`: Zero-Serialization Transfer 完整判定能力 — TypeLayout 提供给定架构集合和类型集合的 C1∧C2 联合 constexpr 判定 API

### Modified Capabilities
- `type-signature`: 安全分类 API 增强 — classify_safety(string_view) 修复 union 漏检并升级为 constexpr；XOffset 的 serialization-free 核心判断完全委托给 TypeLayout

## Impact

- **TypeLayout 文件**: `compat_check.hpp` (修改), `compat_auto.hpp` (修改)
- **XOffset 文件**: `xoffsetdatastructure.hpp` (删减约 100 行), `tests/test_policy_trait.cpp`, `tests/test_remediation_fixes.cpp`
- **公开 API**: `is_xbuffer_safe<T>::value`、`XOFFSET_REGISTER_*`、XBuffer/XVector/XString/XMap/XSet 使用方式均**不变**
- **依赖**: `external/typelayout` submodule 需更新到包含 Change A 的 commit
- **CI**: 可选增加 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE` 编译期跨平台验证步骤
