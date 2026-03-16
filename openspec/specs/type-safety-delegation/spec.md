## Requirements

### Requirement: Domain 准入逻辑

域准入 SHALL 完全委托给 TypeLayout 的 `is_byte_copy_safe_v<T>` 递归谓词。XOffset 不再维护任何自有准入逻辑（`DefaultPolicy`、`StrictPolicy`、`opaque_element_types` 等已删除）。

`is_byte_copy_safe_v<T>` 使用四层分支判定：

1. **Opaque 类型** (`has_opaque_signature<T>`): `!layout_traits<T>::has_pointer && opaque_elements_safe<T>::value`
2. **标准叶子类型**: `trivially_copyable && !has_pointer`（即 `is_local_serialization_free_v<T>`）
3. **包含 opaque 成员的 struct/class** (`is_class && !is_union && !is_polymorphic`): 递归检查所有基类和成员
4. **其他**: false

XOffset 的 `is_xbuffer_safe<T>::value` SHALL 直接等价于 `boost::typelayout::is_byte_copy_safe_v<T>`。

#### Scenario: XVector<int32_t> 通过准入
- **WHEN** `is_byte_copy_safe_v<XVector<int32_t>>`
- **THEN** 返回 true（opaque + pointer_free + 元素 int32_t 安全）

#### Scenario: XVector<PolyBase> 被拒绝
- **WHEN** `is_byte_copy_safe_v<XVector<PolyBase>>` 且 PolyBase 有 virtual 函数
- **THEN** 返回 false（`opaque_elements_safe` 递归到 PolyBase → PolyBase 是 polymorphic → false）

#### Scenario: Player struct 通过准入
- **WHEN** `is_byte_copy_safe_v<Player>` 且 Player 包含 XString + XVector<int32_t>
- **THEN** 返回 true（非 opaque, 非 trivially_copyable → Branch 3 递归 → 每个成员通过 Branch 1 → 全部安全）

#### Scenario: union 类型在单平台场景下 accepted
- **WHEN** `is_byte_copy_safe_v<TrivialUnion>`
- **THEN** 返回 true（trivially_copyable + no pointer → Branch 2 通过）

#### Scenario: is_xbuffer_safe 向后兼容
- **WHEN** `is_xbuffer_safe<T>::value`
- **THEN** 结果与 `boost::typelayout::is_byte_copy_safe_v<T>` 完全一致

---

### Requirement: Opaque 容器注册宏

`XOFFSET_REGISTER_CONTAINER(Template, name, strategy)` 和 `XOFFSET_REGISTER_MAP(Template, name, strategy)` SHALL 调用 TypeLayout 的 `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE` / `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE` 宏，后者自动生成 `opaque_elements_safe<Template<...>>` 特化。XOffset 不再生成任何自有的元素安全检查特化。

#### Scenario: XOFFSET_REGISTER_CONTAINER 委托 TypeLayout
- **WHEN** `XOFFSET_REGISTER_CONTAINER(XVector, "vector", AllocatorAware)` 展开
- **THEN** `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(XVector, "vector")` 被调用
- **AND** TypeLayout 自动生成 `opaque_elements_safe<XVector<T_>> : bool_constant<is_byte_copy_safe_v<T_>>`

---

### Requirement: 跨平台传输验证

`is_transfer_safe<T>(remote_sig)` SHALL 使用 `is_byte_copy_safe_v<T>` 作为本地安全前提（而非 `is_local_serialization_free_v<T>`），使所有 byte-copy safe 的类型（包括 relocatable opaque 类型）都可进行跨平台签名验证。

#### Scenario: 含 opaque 成员的 struct 跨平台验证
- **WHEN** `is_transfer_safe<Player>(remote_sig)` 且 Player 包含 XString + XVector<int32_t>
- **THEN** 若本地签名与 remote_sig 匹配，返回 true
- **AND** 若签名不匹配，返回 false

#### Scenario: 不安全类型被拒绝
- **WHEN** `is_transfer_safe<HasPointer>(remote_sig)` 且 HasPointer 包含原生指针
- **THEN** 返回 false（`is_byte_copy_safe_v<HasPointer>` 为 false，无论签名是否匹配）

---

### Requirement: TypeLayout API 迁移

XOffset 所有源文件 SHALL 使用 TypeLayout main 分支的当前 API：

| 旧引用 | 新引用 |
|--------|--------|
| `DefaultPolicy::accept<T>()` | `boost::typelayout::is_byte_copy_safe_v<T>` |
| `StrictPolicy<Gold>::accept<T>()` | `is_byte_copy_safe_v<T> && sig == gold` (inline) |
| `opaque_element_types<T>::all_elements_safe()` | `opaque_elements_safe<T>::value` (TypeLayout auto-generated) |
| `diagnose_unsafe_members<T>()` | 已删除（无替代） |
| `is_xbuffer_compatible<T, Policy>()` | `is_byte_copy_safe_v<T>` |

#### Scenario: 全部源文件编译通过
- **WHEN** Docker 全量构建
- **THEN** 0 个编译错误
- **AND** 23 个测试全部 PASSED