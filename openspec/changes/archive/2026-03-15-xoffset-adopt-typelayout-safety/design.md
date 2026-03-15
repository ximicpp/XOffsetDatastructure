## Context

XOffset 当前引用的 TypeLayout API（`is_layout_safe`、`classify_safety`、`get_definition_signature`、`SignatureMode`）在 TypeLayout main 分支上均已删除或重命名。同时 XOffset 内部的安全逻辑（`classify_for_xoffset`、`ArchSpec`、手动叶子类型列表）与 TypeLayout 的功能重复。

前置依赖: TypeLayout main 已完成 `typelayout-relocatable-opaque` 变更。

## Goals / Non-Goals

**Goals:**
- 将 XOffset 安全判定 100% 委托给 TypeLayout 新 API
- 全部 Docker 32 个测试通过
- 删除冗余安全逻辑代码

**Non-Goals:**
- 不修改 TypeLayout（已在独立提案中处理）
- 不修改用户面向的 XOffset API（XBuffer、XVector 等）
- 不处理文档更新（后续独立任务）

## Decisions

### D1: DefaultPolicy 三层检查逻辑

**选择**: `DefaultPolicy::accept<T>()` 使用三层分支：
1. opaque 类型 → `!layout_traits<T>::has_pointer && opaque_element_types<T>::all_elements_safe()`
2. trivially_copyable + !has_pointer → `is_local_serialization_free_v<T>`
3. 非多态 struct/class → 递归检查所有 bases 和 members

**理由**:
- 层 1: opaque 容器不是 trivially_copyable 但 byte-copy safe
- 层 2: 标准安全叶子类型（int, float, enum 等）
- 层 3: 包含 opaque 成员的用户 struct（如 Player, GameData）也不是 trivially_copyable，需要递归验证每个成员

### D2: opaque_element_types trait

**选择**: 新增 `detail::opaque_element_types<T>` trait，由 `XOFFSET_REGISTER_CONTAINER/MAP` 宏自动生成特化。

**理由**: TypeLayout 签名不追踪 vtable pointer，`contains_token("ptr[")` 无法检测多态元素类型。需要 XOffset 层面对元素类型做 `accept()` 递归检查。trait 将容器的模板参数暴露给 `DefaultPolicy`。

### D3: StrictPolicy 复用 DefaultPolicy

**选择**: `StrictPolicy::accept<T>()` = `DefaultPolicy::accept<T>() && signature == Gold`

**理由**: C2 检查逻辑一致，避免重复。StrictPolicy 仅在 C2 基础上增加 C1 签名匹配。

### D4: 旧 API 全局替换

**选择**: 使用 sed 全局替换，而非逐文件手改。

**替换表**:
| 旧 | 新 |
|---|---|
| `get_definition_signature` | `get_layout_signature` |
| `definition_signatures_match` | `layout_signatures_match` |
| `is_fixed_enum` | `std::is_enum_v` |
| `SignatureMode::Definition` | 删除（只有一种签名模式） |

### D5: union/long/wchar_t 准入放宽

**选择**: 在 TypeLayout 新安全模型下，union、long、wchar_t 在单平台场景下是安全的（C2 通过）。移除 XOffset 对它们的额外拒绝。

**理由**: `is_local_serialization_free_v<long>` = true（trivially_copyable + no pointer）。跨平台不安全性由 C1 签名比较在 CI 中检测。
