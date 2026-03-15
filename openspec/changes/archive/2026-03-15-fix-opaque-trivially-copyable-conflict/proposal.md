## Why

在 `unify-domain-s-with-typelayout` 变更中，我们将 XOffset 的 Domain S (安全类型集合) 委托给 TypeLayout 的 `is_local_serialization_free_v<T>`。Docker 全量构建揭示了一个 **根本性语义冲突**：

- TypeLayout 的 `is_local_serialization_free_v<T>` 要求 `std::is_trivially_copyable_v<T> && !has_pointer`
- XOffset 的 opaque 容器（`XString`、`XVector<T>`、`XSet<T>`、`XMap<K,V>`）**不是** `trivially_copyable`——它们有 non-trivial 析构函数和拷贝构造函数
- 但这些容器在 `offset_ptr` 重定位模型下 **是** byte-copy safe 的（C2 Lemma C2.1）

这导致 `is_local_serialization_free_v<XVector<int>>` 返回 false，而我们在 `DefaultPolicy::accept<T>()` 中直接使用了它，所以所有包含 XOffset 容器的用户类型都编译失败。此外 `TYPELAYOUT_OPAQUE_TYPE_AUTO` 宏本身也有 `static_assert(is_trivially_copyable_v<Type>)` 断言，在 `XOFFSET_REGISTER_TYPE(XString, ...)` 时直接触发。

**这不是一个简单的 bug，而是两个库的安全模型在 "什么是 byte-copy safe" 上的语义分歧。**

## What Changes

### TypeLayout 侧 (external/typelayout)

- 在 `opaque.hpp` 中新增 `TYPELAYOUT_OPAQUE_*_RELOCATABLE` 宏系列，不含 `trivially_copyable` 断言，专为 relocatable（offset_ptr-based）容器设计
- 在 `sig_export.hpp` 的 `SigExporter` 类中新增 `add_relocatable<T>()` 方法，放宽 `trivially_copyable` 约束，改为检查 `!layout_traits<T>::has_pointer`
- **不修改** 原有 `_AUTO` 宏和 `add<T>()` 方法——它们的 `trivially_copyable` 保护对 TypeLayout 的其他用户（不使用 offset_ptr 的场景）是正确的

### XOffset 侧 (xoffsetdatastructure.hpp)

- `XOFFSET_REGISTER_TYPE/CONTAINER/MAP` 宏改用 `_RELOCATABLE` 变体，而不是 `_AUTO`
- `DefaultPolicy::accept<T>()` 分支处理：opaque 类型检查 `!has_pointer`；非 opaque 类型走 `is_local_serialization_free_v<T>`
- `StrictPolicy::accept<T>()` 同理，C2 检查对 opaque 类型放宽，C1 签名比较不变
- 添加缺失的 `using` 声明：`has_opaque_signature`、`layout_traits`
- `export_signatures.cpp` 改用 `add_relocatable<T>()` 导出包含 opaque 成员的类型

### 测试修复

- 修复 `unify-domain-s-with-typelayout` 中 Task 7 遗留的测试编译错误

## Capabilities

### New Capabilities

- `opaque-relocatable-types`: TypeLayout 对 "不是 trivially_copyable 但在重定位模型下 byte-copy safe" 的类型（relocatable types）的支持能力。涵盖 `_RELOCATABLE` 宏系列和 `add_relocatable` API。

### Modified Capabilities

- `type-signature`: XOffset 的 opaque 容器注册从 `_AUTO` 迁移到 `_RELOCATABLE`，导出工具从 `add<T>` 迁移到 `add_relocatable<T>`。Safety 准入逻辑（Policy）对 opaque 类型使用独立检查路径。
- `core-model`: Domain S 的 opaque 子集定义需要更新——opaque 类型的准入标准从 `trivially_copyable && !has_pointer` 放宽为仅 `!has_pointer`（附加 "调用者保证 byte-copy safety" 前提）。

## Impact

- **编译兼容性**: 修复后所有 Docker 构建目标应通过（当前 11 个目标全部失败）
- **TypeLayout 上游**: 需要在 TypeLayout 的 `feat/opaque-auto-macros` 分支上追加 `_RELOCATABLE` 宏和 `add_relocatable` API，然后推送
- **API 变化**: 无面向用户的 API 变化——所有变化在内部宏和 Policy 实现中
- **安全模型影响**: 不降低安全性——`_RELOCATABLE` 仅对注册了 opaque 签名且 `pointer_free = true` 的类型生效，调用者（XOffset）承担 byte-copy safety 保证的责任
