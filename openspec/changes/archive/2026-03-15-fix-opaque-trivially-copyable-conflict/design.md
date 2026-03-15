## Context

在 `unify-domain-s-with-typelayout` 变更中，XOffset 的安全准入逻辑被委托给 TypeLayout 的 `is_local_serialization_free_v<T>`。Docker 构建发现 11 个编译目标全部失败，根因是 TypeLayout 对 "serialization-free" 的定义（`trivially_copyable && !has_pointer`）与 XOffset 的 opaque 容器语义不兼容——后者不是 `trivially_copyable` 但在 offset_ptr 模型下仍然是 byte-copy safe 的。

当前代码状态：
- TypeLayout `opaque.hpp` 已有 `_AUTO` 宏（带 `trivially_copyable` 断言）
- XOffset 的注册宏已改为使用 `_AUTO`，触发断言失败
- `DefaultPolicy::accept<T>()` 直接委托 `is_local_serialization_free_v<T>`，对 opaque 容器返回 false
- 缺少 `has_opaque_signature` 和 `layout_traits` 的 using 声明

## Goals / Non-Goals

**Goals:**
- 让 TypeLayout 支持 "relocatable but not trivially_copyable" 的 opaque 类型
- 修复全部 11 个 Docker 编译目标
- 保持 TypeLayout 原有 `_AUTO` 系列宏和 `add<T>()` 的语义不变（对上游其他用户无影响）
- 明确 XOffset 的 opaque 类型安全保证由 XOffset 承担，而非 TypeLayout

**Non-Goals:**
- 不修改 TypeLayout 的 `is_local_serialization_free_v<T>` 定义——它的语义（`trivially_copyable && !has_pointer`）对纯 TypeLayout 场景是正确的
- 不重新设计 opaque 注册机制——仅添加变体
- 不处理 `unify-domain-s-with-typelayout` 中 Task 8（文档更新）和 Task 7 遗留的测试问题——这些可以后续处理

## Decisions

### D1: 新增 `_RELOCATABLE` 宏系列，而非修改 `_AUTO`

**选择**: 在 `opaque.hpp` 中新增三个 `TYPELAYOUT_OPAQUE_*_RELOCATABLE` 宏，与 `_AUTO` 平行。

**理由**: `_AUTO` 的 `trivially_copyable` 断言对 TypeLayout 的通用用户是正确的保护。修改它会降低整体安全性。新增 `_RELOCATABLE` 变体让调用者（XOffset）显式选择放宽约束，语义清晰。

**备选方案**:
- A) 移除 `_AUTO` 的 `trivially_copyable` 断言 — 拒绝：降低安全性
- B) 在 XOffset 中不使用宏，手写 `TypeSignature` 特化 — 拒绝：冗余且易出错
- C) 给 `_AUTO` 添加可选的 `bool check_trivially_copyable = true` 模板参数 — 拒绝：宏不支持默认参数语义

### D2: DefaultPolicy 对 opaque 类型使用独立检查路径

**选择**: `DefaultPolicy::accept<T>()` 中使用 `if constexpr (has_opaque_signature<T>)` 分支：
- opaque 类型: 仅检查 `!layout_traits<T>::has_pointer`
- 非 opaque 类型: 使用 `is_local_serialization_free_v<T>`

**理由**: opaque 类型已通过注册声明了 `pointer_free = true`。它们不需要 `trivially_copyable` 检查——byte-copy safety 由 offset_ptr 重定位模型保证（C2 Lemma C2.1）。这是 XOffset 的领域知识，不属于 TypeLayout 的职责。

### D3: StrictPolicy 拆分 C1/C2 检查

**选择**: 不再直接调用 `is_transfer_safe<T>()`（因为它内部也检查 `trivially_copyable`），而是：
1. C2 检查：同 DefaultPolicy（opaque 分支）
2. C1 检查：直接比较 `get_layout_signature<T>()` 与 GoldSignature

**理由**: `is_transfer_safe` 是 TypeLayout 的运行时 API（非 consteval），不适合在 consteval 的 `accept()` 中使用。拆分后每个步骤都是 consteval-safe 的。

### D4: SigExporter 新增 `add_relocatable<T>()` 方法

**选择**: 在 `SigExporter` 类中新增方法，检查 `!layout_traits<T>::has_pointer` 而非 `trivially_copyable`。

**理由**: `export_signatures.cpp` 导出 `Player`、`Item`、`GameData`——它们都包含 XString/XVector 成员，不是 `trivially_copyable`。需要一个放宽约束的导出路径。

### D5: export_signatures.cpp 使用手动注册替代宏

**选择**: 将 `TYPELAYOUT_EXPORT_TYPES(Player, Item, GameData)` 替换为手写的 `main()` 调用 `ex.add_relocatable<T>()`。

**理由**: `TYPELAYOUT_EXPORT_TYPES` 宏内部调用 `ex.add<T>()`，无法切换到 `add_relocatable`。手写 main 是最简单的解决方案。

## Risks / Trade-offs

**[Risk] `_RELOCATABLE` 宏被误用于非 relocatable 类型** → 宏名称中 "RELOCATABLE" 清楚传达语义；文档注释明确说明 "调用者承担 byte-copy safety 保证"。TypeLayout 不做强制验证——这是设计上的信任边界。

**[Risk] StrictPolicy 拆分后失去与 TypeLayout `is_transfer_safe` 的一致性** → 拆分后的逻辑语义等价，只是绕过了 `trivially_copyable` 检查。如果未来 TypeLayout 更新 `is_transfer_safe` 的逻辑，XOffset 需要同步更新。

**[Trade-off] TypeLayout 上游分支需要更新** → 需要在 `feat/opaque-auto-macros` 分支追加提交。两个库耦合度增加，但这是不可避免的——XOffset 是 TypeLayout 的核心用户。
