## MODIFIED Requirements

### Requirement: 跨平台签名导出工具

系统 SHALL 提供基于 TypeLayout `SigExporter` 的跨平台签名导出工具，支持为关键数据类型生成可移植的 `.sig.hpp` 签名头文件。

**实现方式**：
- 对包含 relocatable opaque 成员的类型（如 Player, Item, GameData），使用 `SigExporter::add_relocatable<T>()` 方法
- 对纯 trivially_copyable 类型，继续使用 `SigExporter::add<T>()` 方法
- `tools/export_signatures.cpp` SHALL 使用手写 `main()` 替代 `TYPELAYOUT_EXPORT_TYPES` 宏，以调用 `add_relocatable`

#### Scenario: 导出签名到头文件
- **WHEN** 用户编译并运行 `tools/export_signatures` 工具
- **THEN** 在指定目录生成包含签名常量的 `.sig.hpp` 文件
- **AND** 文件包含 Player, Item, GameData 的 Layout 签名
- **AND** 该文件可在任何 C++17 编译器上 include 并比较

#### Scenario: 包含 XString/XVector 成员的类型成功导出
- **WHEN** 导出 Player 类型（包含 XString name 和 XVector<int32_t> items 成员）
- **THEN** 签名正确生成，opaque 成员以 `O(string|...) / O(vector|...)<...>` 格式嵌入
- **AND** 不触发 `trivially_copyable` 断言

---

## ADDED Requirements

### Requirement: XOffset Opaque 容器使用 Relocatable 注册

XOffset 的内部容器注册宏（`XOFFSET_REGISTER_TYPE`、`XOFFSET_REGISTER_CONTAINER`、`XOFFSET_REGISTER_MAP`）SHALL 使用 TypeLayout 的 `_RELOCATABLE` 宏系列，而非 `_AUTO` 系列。

**约束**：
- XString → `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE`
- XVector → `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE`
- XSet → `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE`
- XMap → `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE`

#### Scenario: XString 注册成功
- **WHEN** `XOFFSET_REGISTER_TYPE(XString, "string", AllocatorAware)` 展开
- **THEN** 编译成功，不触发 `trivially_copyable` 断言
- **AND** `has_opaque_signature<XString>` 返回 true

#### Scenario: XVector<int32_t> 通过 DefaultPolicy 准入
- **WHEN** 评估 `DefaultPolicy::accept<XVector<int32_t>>()`
- **THEN** 返回 true（opaque + pointer_free）

---

### Requirement: Policy 对 Opaque 类型的独立检查路径

`DefaultPolicy::accept<T>()` 和 `StrictPolicy::accept<T>()` SHALL 对 opaque 类型使用独立的安全检查路径：
- 非 opaque 类型：`is_local_serialization_free_v<T>`（`trivially_copyable && !has_pointer`）
- Opaque 类型：仅检查 `!layout_traits<T>::has_pointer`（不要求 `trivially_copyable`）

**约束**：
- 使用 `has_opaque_signature<T>` concept 进行 opaque 判断
- `has_opaque_signature` 和 `layout_traits` 的 `using` 声明 SHALL 存在于 `detail` 命名空间中

#### Scenario: DefaultPolicy 接受 opaque 容器
- **WHEN** `DefaultPolicy::accept<XVector<int32_t>>()` 在 consteval 上下文中求值
- **THEN** 走 opaque 分支，返回 true
- **AND** 不检查 `trivially_copyable`

#### Scenario: DefaultPolicy 拒绝含指针的 opaque 类型
- **WHEN** 一个 opaque 类型注册时 `pointer_free = false`
- **THEN** `DefaultPolicy::accept` 返回 false

#### Scenario: StrictPolicy 对 opaque 类型验证签名
- **WHEN** `StrictPolicy<GOLD>::accept<XVector<int32_t>>()` 在 consteval 上下文中求值
- **THEN** C2 检查通过（opaque + pointer_free）
- **AND** C1 检查比较 `get_layout_signature<XVector<int32_t>>()` 与 GoldSignature
