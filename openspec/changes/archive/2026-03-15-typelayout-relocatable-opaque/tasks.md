## 1. opaque.hpp: 添加 _RELOCATABLE 宏

- [x] 1.1 在 `TYPELAYOUT_REGISTER_OPAQUE` 之后，添加 `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE(Type, name)` — 无 trivially_copyable 断言，`pointer_free = true`
- [x] 1.2 添加 `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(Template, name)` — 嵌入元素签名，`pointer_free` 从 `calculate().contains_token("ptr[")` 推导。注意 `calculate()` 必须在 `pointer_free` 之前声明
- [x] 1.3 添加 `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(Template, name)` — 嵌入 key+value 签名，`pointer_free` 同上

## 2. sig_export.hpp: 添加 add_relocatable

- [x] 2.1 在 `SigExporter::add<T>()` 之后添加 `add_relocatable<T>(name)` 方法，`static_assert(!layout_traits<T>::has_pointer)` 替代 `trivially_copyable` 检查

## 3. layout_traits.hpp: cross-validation 修复

- [x] 3.1 cross-validation static_assert 条件中添加 `has_opaque_signature<T> || has_opaque ||`

## 4. 推送到 TypeLayout main

- [x] 4.1 在 TypeLayout main 分支上 commit 并 push (e81aa33..801b8d4)