## Why

XOffset 的 opaque 容器（XString、XVector<T>、XSet<T>、XMap<K,V>）不是 `trivially_copyable`（有 non-trivial 析构函数等），但在 offset_ptr 重定位模型下是 byte-copy safe 的。TypeLayout main 分支当前的 opaque 注册宏 `TYPELAYOUT_REGISTER_OPAQUE` 有三个限制阻止 XOffset 使用：

1. **`static_assert(trivially_copyable)`** — XString 等注册时直接编译失败
2. **仅支持具体类型** — 无法注册 `XVector<T>` 这样的模板，无法嵌入元素类型签名
3. **`pointer_free` 为手动参数** — 对容器模板，pointer_free 应从元素类型签名自动推导

同样，`SigExporter::add<T>()` 和 `layout_traits` 的 cross-validation 也有 `trivially_copyable` 相关的硬编码约束。

## What Changes

在 TypeLayout main 分支上新增以下能力（不修改任何现有 API）：

### opaque.hpp
- 新增 `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE(Type, name)` — 具体类型，无 trivially_copyable 断言，`pointer_free = true`
- 新增 `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(Template, name)` — 单参数容器模板，嵌入元素签名，`pointer_free` 从签名动态推导
- 新增 `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(Template, name)` — 双参数容器模板，同上

### sig_export.hpp
- 新增 `SigExporter::add_relocatable<T>(name)` — 用 `!layout_traits<T>::has_pointer` 替代 `trivially_copyable` 检查

### layout_traits.hpp
- cross-validation static_assert 跳过 opaque 类型和包含 opaque 成员的类型

## Capabilities

### New Capabilities
- `relocatable-opaque`: 对非 trivially_copyable 但 byte-copy safe 的类型的 opaque 注册支持

### Modified Capabilities
- `sig-export`: SigExporter 新增 `add_relocatable` 方法
- `layout-traits`: cross-validation 兼容 opaque 类型

## Impact

- **零 breaking change** — 所有现有 API 行为不变
- **纯增量** — 仅新增宏、方法和一个 static_assert 条件
- **目标分支** — TypeLayout `main`，直接 push
