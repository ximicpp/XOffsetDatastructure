## ADDED Requirements

### Requirement: TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE

TypeLayout SHALL 提供 `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE(Type, name)` 宏，用于注册不满足 `trivially_copyable` 但 byte-copy safe 的具体类型。

- SHALL NOT 包含 `static_assert(std::is_trivially_copyable_v<Type>)`
- SHALL 设置 `is_opaque = true`, `pointer_free = true`
- SHALL 生成签名格式 `O(name|sizeof|alignof)`

#### Scenario: 注册非 trivially_copyable 的 relocatable 类型
- **WHEN** `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE(MyString, "string")` 展开
- **THEN** 编译成功
- **AND** `has_opaque_signature<MyString>` 为 true
- **AND** `layout_traits<MyString>::has_pointer` 为 false

---

### Requirement: TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE

TypeLayout SHALL 提供 `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(Template, name)` 宏，用于注册单参数容器模板。

- SHALL 嵌入元素类型签名：`O(name|sizeof|alignof)<element_sig>`
- SHALL 从签名动态推导 `pointer_free`：`!calculate().contains_token("ptr[")`
- `calculate()` SHALL 在 `pointer_free` 之前声明

#### Scenario: 安全元素类型
- **WHEN** 注册 `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(MyVector, "vector")`
- **THEN** `TypeSignature<MyVector<int>>::pointer_free` 为 true

#### Scenario: 不安全元素类型
- **WHEN** `MyVector<int*>` 实例化
- **THEN** `TypeSignature<MyVector<int*>>::pointer_free` 为 false

---

### Requirement: TYPELAYOUT_OPAQUE_MAP_RELOCATABLE

TypeLayout SHALL 提供 `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(Template, name)` 宏，用于注册双参数容器模板。

- SHALL 嵌入 key 和 value 类型签名：`O(name|sizeof|alignof)<key_sig,value_sig>`
- SHALL 从签名动态推导 `pointer_free`

#### Scenario: Map 注册
- **WHEN** 注册 `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(MyMap, "map")`
- **THEN** `TypeSignature<MyMap<int, float>>::pointer_free` 为 true
- **AND** `TypeSignature<MyMap<int, int*>>::pointer_free` 为 false

---

### Requirement: SigExporter::add_relocatable

`SigExporter` SHALL 提供 `add_relocatable<T>(name)` 方法。

- SHALL 使用 `static_assert(!layout_traits<T>::has_pointer)` 替代 `trivially_copyable` 检查
- 其余行为与 `add<T>()` 完全一致

#### Scenario: 导出包含 opaque 成员的类型
- **WHEN** `ex.add_relocatable<Player>("Player")`
- **THEN** 导出成功，签名正确

#### Scenario: 导出含指针的类型被拒绝
- **WHEN** `ex.add_relocatable<UnsafeType>("Unsafe")` 且 UnsafeType 含 `int*`
- **THEN** `static_assert` 编译失败

---

## MODIFIED Requirements

### Requirement: layout_traits cross-validation

`layout_traits<T>` 的 padding cross-validation static_assert SHALL 跳过 opaque 类型（`has_opaque_signature<T>`）和包含 opaque 成员的类型（`has_opaque`）。

#### Scenario: opaque 容器不触发 cross-validation 失败
- **WHEN** `layout_traits<MyVector<Item>>` 被实例化
- **THEN** cross-validation static_assert 不触发（被跳过）
