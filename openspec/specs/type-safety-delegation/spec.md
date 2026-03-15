## MODIFIED Requirements

### Requirement: Domain S 准入逻辑

`DefaultPolicy::accept<T>()` SHALL 使用三层分支判定类型安全性：

1. **Opaque 类型** (`has_opaque_signature<T>`): `!layout_traits<T>::has_pointer && opaque_element_types<T>::all_elements_safe()`
2. **标准叶子类型**: `is_local_serialization_free_v<T>`（`trivially_copyable && !has_pointer`）
3. **包含 opaque 成员的 struct/class** (`is_class && !is_union && !is_polymorphic`): 递归检查所有基类和成员

`StrictPolicy<Gold>::accept<T>()` SHALL 复用 `DefaultPolicy::accept<T>()` 作为 C2 检查，并额外比较 `get_layout_signature<T>()` 与 Gold 签名。

#### Scenario: XVector<int32_t> 通过 DefaultPolicy
- **WHEN** `DefaultPolicy::accept<XVector<int32_t>>()`
- **THEN** 返回 true（opaque + pointer_free + 元素 int32_t 安全）

#### Scenario: XVector<PolyBase> 被 DefaultPolicy 拒绝
- **WHEN** `DefaultPolicy::accept<XVector<PolyBase>>()` 且 PolyBase 有 virtual 函数
- **THEN** 返回 false（`opaque_element_types` 对 PolyBase 调用 `accept` 返回 false）

#### Scenario: Player struct 通过 DefaultPolicy
- **WHEN** `DefaultPolicy::accept<Player>()` 且 Player 包含 XString + XVector<int32_t>
- **THEN** 返回 true（非 opaque, 非 trivially_copyable → 递归检查每个成员 → 全部安全）

#### Scenario: union 类型在单平台场景下 accepted
- **WHEN** `DefaultPolicy::accept<TrivialUnion>()`
- **THEN** 返回 true（trivially_copyable + no pointer）

---

### Requirement: Opaque 容器注册宏

`XOFFSET_REGISTER_CONTAINER(Template, name, strategy)` 和 `XOFFSET_REGISTER_MAP(Template, name, strategy)` SHALL 额外生成 `detail::opaque_element_types<Template<...>>` 特化，其 `all_elements_safe()` 方法递归调用 `DefaultPolicy::accept<ElementType>()`。

#### Scenario: XOFFSET_REGISTER_CONTAINER 生成 element type trait
- **WHEN** `XOFFSET_REGISTER_CONTAINER(XVector, "vector", AllocatorAware)` 展开
- **THEN** `detail::opaque_element_types<XVector<T_>>` 特化被生成
- **AND** `all_elements_safe()` 等价于 `DefaultPolicy::accept<T_>()`

---

### Requirement: TypeLayout API 迁移

XOffset 所有源文件 SHALL 使用 TypeLayout main 分支的当前 API：

| 旧引用 | 新引用 |
|--------|--------|
| `get_definition_signature<T>()` | `get_layout_signature<T>()` |
| `definition_signatures_match<T,U>()` | `layout_signatures_match<T,U>()` |
| `boost::typelayout::is_fixed_enum<T>()` | `std::is_enum_v<T>` |
| `boost::typelayout::compat::classify_safety<T>()` | `boost::typelayout::classify_v<T>` |
| `boost::typelayout::compat::is_layout_safe<T>()` | `boost::typelayout::is_local_serialization_free_v<T>` |
| `SignatureMode::Definition` | 删除 |
| `classify_safety.hpp` | `classify.hpp` + `serialization_free.hpp` |

#### Scenario: 全部源文件编译通过
- **WHEN** Docker 全量构建
- **THEN** 0 个编译错误
- **AND** 32 个测试全部 PASSED
