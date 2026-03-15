## Context

TypeLayout main (`e81aa33`) 的 opaque 注册只有 `TYPELAYOUT_REGISTER_OPAQUE(Type, Tag, HasPointer)`：
- 要求 `trivially_copyable`
- 仅支持具体类型（不支持模板）
- `pointer_free` 为手动参数

XOffset 需要注册 offset_ptr 容器模板，它们不是 trivially_copyable 但 byte-copy safe。

## Goals / Non-Goals

**Goals:**
- 新增 `_RELOCATABLE` 宏系列，支持 relocatable 类型注册
- 容器/Map 变体嵌入元素类型签名，`pointer_free` 从签名自动推导
- `SigExporter` 新增 `add_relocatable` 方法
- `layout_traits` cross-validation 兼容 opaque 类型

**Non-Goals:**
- 不修改现有 `TYPELAYOUT_REGISTER_OPAQUE` 宏
- 不修改 `SigExporter::add<T>()` 
- 不修改 `is_local_serialization_free_v` 定义

## Decisions

### D1: 新增宏而非修改现有宏

**选择**: 新增三个 `_RELOCATABLE` 宏，与 `REGISTER_OPAQUE` 并行存在。

**理由**: `REGISTER_OPAQUE` 的 `trivially_copyable` 断言对 TypeLayout 的通用场景是正确的。新增变体让调用者显式声明 "此类型虽非 trivially_copyable 但 byte-copy safe"。

### D2: 容器/Map pointer_free 从签名动态推导

**选择**: `CONTAINER_RELOCATABLE` 和 `MAP_RELOCATABLE` 的 `pointer_free` 定义为：
```cpp
static constexpr bool pointer_free =
    !calculate().contains_token(FixedString{"ptr["});
```

**理由**: `XVector<int>` 应为 `pointer_free = true`，但 `XVector<int*>` 应为 `pointer_free = false`。硬编码无法满足。从生成的签名中扫描 `ptr[` token 是最准确且零维护的方式。`contains_token` 在 `fixed_string.hpp` 中已存在。

**注意**: `calculate()` 必须在 `pointer_free` 之前声明（C++ 成员初始化顺序要求）。

### D3: cross-validation 条件扩展

**选择**: `layout_traits` 的 padding cross-validation static_assert 新增两个跳过条件：
```cpp
has_opaque_signature<T> || has_opaque || ...existing conditions...
```

**理由**: opaque 类型的 `compute_has_padding` 短路返回 false（不暴露内部结构），但签名中嵌入的元素类型可能包含 padding 标记，导致 bitmap 与 sig parser 不一致。`has_opaque` 覆盖包含 opaque 成员的外层 struct。

### D4: add_relocatable 使用 !has_pointer 检查

**选择**: `SigExporter::add_relocatable<T>(name)` 的 static_assert 为 `!layout_traits<T>::has_pointer`。

**理由**: 对包含 relocatable opaque 成员的类型（如 Player），`trivially_copyable` 为 false 但指针安全性由 opaque 注册保证。`!has_pointer` 是唯一需要的运行时安全约束。
