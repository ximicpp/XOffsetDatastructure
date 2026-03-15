## Why

XOffset 的类型安全逻辑（Domain S 判定、Policy trait、诊断）当前使用 TypeLayout 旧 API（`is_layout_safe`、`classify_safety`、`get_definition_signature`、`SignatureMode` 等），这些 API 在 TypeLayout main 分支中已不存在。同时 XOffset 内部维护了大量冗余的安全逻辑（手动叶子类型列表、ArchSpec、classify_for_xoffset 等），应委托给 TypeLayout。

本提案基于 TypeLayout `typelayout-relocatable-opaque` 变更完成后的 main 分支，将 XOffset 的安全判定全面迁移到 TypeLayout 的新 API 上。

## What Changes

### 核心库 xoffsetdatastructure.hpp

**API 迁移:**
- `is_layout_safe<T>()` → `is_local_serialization_free_v<T>` + opaque 递归检查
- `classify_safety<T>()` → `classify_v<T>`
- `get_definition_signature` → `get_layout_signature`
- `classify_safety.hpp` → `classify.hpp` + `serialization_free.hpp`

**DefaultPolicy 重写:**
- opaque 类型: `!has_pointer + opaque_element_types::all_elements_safe()`
- 叶子类型: `is_local_serialization_free_v<T>`
- struct with opaque: 递归成员检查 (P2996 反射)

**StrictPolicy 简化:**
- C2: 复用 `DefaultPolicy::accept<T>()`
- C1: `get_layout_signature<T>() == GoldSignature`

**注册宏迁移:**
- `XOFFSET_REGISTER_TYPE` → `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE`
- `XOFFSET_REGISTER_CONTAINER` → `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE` + `opaque_element_types` 特化
- `XOFFSET_REGISTER_MAP` → `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE` + `opaque_element_types` 特化

**新增组件:**
- `opaque_element_types<T>` trait — 由注册宏自动特化，暴露元素类型安全性
- `accept_all_members_impl` / `accept_all_bases_impl` — consteval 递归安全检查

### 测试和示例
- 全局替换旧 API 引用
- 更新安全性断言（union/long/wchar_t 现在 accepted）
- 移除硬编码签名 static_assert（签名格式已变）

### 工具
- `export_signatures.cpp` 使用 `add_relocatable<T>()`
- `check_compat.cpp` 暂时禁用（依赖已删除 API）

## Capabilities

### Modified Capabilities
- `core-model`: Domain S 定义从手动枚举迁移到 TypeLayout 委托
- `type-signature`: 注册宏、Policy、导出工具全部迁移

## Impact

- **依赖**: 需要 `typelayout-relocatable-opaque` 完成后更新 submodule
- **Docker 构建**: 全部 32 个测试必须通过
- **API 变化**: 无面向用户的 API 变化（内部实现变更）
