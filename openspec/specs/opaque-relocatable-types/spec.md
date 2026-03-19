## ADDED Requirements

### Requirement: Relocatable Opaque Registration Macros

TypeLayout SHALL 提供 `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE`、`TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE` 和 `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE` 三个宏，用于注册不满足 `trivially_copyable` 但在重定位模型（如 offset_ptr）下 byte-copy safe 的 opaque 类型。

这些宏 SHALL 与 `_AUTO` 系列功能一致（自动推导 sizeof/alignof、设置 `pointer_free = true`、生成 `O(tag|size|align)` 签名），但 SHALL NOT 包含 `static_assert(std::is_trivially_copyable_v<Type>)` 断言。

**约束**:
- 宏定义位置: `external/typelayout/include/boost/typelayout/opaque.hpp`
- 生成的 `TypeSignature` 特化 SHALL 设置 `is_opaque = true` 和 `pointer_free = true`
- 调用者承担 byte-copy safety 保证的责任

#### Scenario: 注册非 trivially_copyable 的 offset_ptr 容器
- **WHEN** 用户使用 `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE(MyString, "string")` 注册一个不满足 `trivially_copyable` 但使用 offset_ptr 的容器类型
- **THEN** 编译成功，不触发任何 `static_assert`
- **AND** `has_opaque_signature<MyString>` 返回 true
- **AND** `detail::layout_traits<MyString>::has_pointer` 返回 false

#### Scenario: 容器模板注册
- **WHEN** 用户使用 `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(MyVector, "vector")` 注册容器模板
- **THEN** `TypeSignature<MyVector<int>>` 的签名包含元素类型签名
- **AND** `has_opaque_signature<MyVector<int>>` 返回 true

#### Scenario: 原有 _AUTO 宏行为不变
- **WHEN** 用户使用 `TYPELAYOUT_OPAQUE_TYPE_AUTO(TrivialType, "trivial")` 注册一个非 trivially_copyable 类型
- **THEN** 触发 `static_assert` 编译失败

---

### Requirement: SigExporter Relocatable Export Method

TypeLayout 的 `SigExporter` 类 SHALL 提供 `add_relocatable<T>(name)` 方法，用于导出包含 relocatable opaque 成员的类型签名。

该方法 SHALL 使用 `static_assert(!detail::layout_traits<T>::has_pointer)` 替代 `static_assert(std::is_trivially_copyable_v<T>)` 作为准入检查。

**约束**:
- 方法定义位置: `external/typelayout/include/boost/typelayout/tools/sig_export.hpp`
- 原有 `add<T>(name)` 方法 SHALL NOT 被修改

#### Scenario: 导出包含 XString 成员的类型
- **WHEN** 调用 `ex.add_relocatable<Player>("Player")`，其中 Player 包含 XString 成员
- **THEN** 导出成功，签名正确包含 opaque 子签名
- **AND** 不触发 `trivially_copyable` 断言

#### Scenario: 导出包含原始指针的类型被拒绝
- **WHEN** 调用 `ex.add_relocatable<UnsafeType>("Unsafe")`，其中 UnsafeType 包含 `int*` 成员
- **THEN** 触发 `static_assert` 编译失败，提示 "type must be pointer-free"
