## Context

XOffset 的类型安全体系建立在 Domain S（安全类型集）上，它同时服务 C1（值保持）和 C2（引用保持）。当前 Domain S 通过手工枚举 S₀ 叶类型并排除 union/long/wchar_t/long double 来实现，其中约 150 行自建逻辑与 TypeLayout 的类型分析功能重叠。

TypeLayout 远端 `origin/main`（commit `e81aa33`）已提供：
- `is_local_serialization_free_v<T>` — `trivially_copyable(T) && !has_pointer(T)`（C2 判定）
- `is_transfer_safe<T>(remote_sig)` — C2 + 签名匹配（C1+C2 一体判定）
- `classify_v<T>` — 五级 SafetyLevel（诊断用）
- `layout_traits<T>` — 编译期类型特征（has_pointer, has_padding, is_platform_variant 等）

缺少的唯一依赖：`TYPELAYOUT_OPAQUE_*_AUTO` 宏，用于注册 XVector/XString 等容器。

## Goals / Non-Goals

**Goals:**
- XOffset 的 Domain S 与 TypeLayout 的 `is_local_serialization_free_v<T>` 完全对齐
- C1 和 C2 彻底分离：C2 = `is_local_serialization_free_v<T>`，C1 = 签名匹配
- XOffset 不再包含任何自建类型分类/安全判定逻辑，TypeLayout 是唯一类型安全引擎
- TypeLayout 远端新增 `OPAQUE_*_AUTO` 宏作为正式 API（非 XOffset 特定 hack）

**Non-Goals:**
- 不修改 TypeLayout 的 `is_local_serialization_free` 语义
- 不修改 TypeLayout 的五级 SafetyLevel 分类标准
- 不在本次变更中实现 CI 签名比较的自动化流程（现有 Phase 2 机制保持不变）
- 不改变 XBuffer 的运行时行为（内存分配、segment manager、compact 等）

## Decisions

### Decision 1: Domain S = `is_local_serialization_free_v<T>` ∪ opaque types

**选择**: 将 Domain S 从手工枚举改为完全委托 TypeLayout。

**理由**:
- 原 Domain S 中的 C1 约束（排除 `long`/`wchar_t`/`long double`）与 TypeLayout 签名比较功能完全重叠
- `union` 的排除理由（"活跃成员歧义"）属于应用层语义（同 enum 值域），不是 byte-copy 正确性问题
- `trivially_copyable + !has_pointer` 精确覆盖了 C2 所需的全部约束

**替代方案**: 在 TypeLayout 之上叠加 XOffset 特有约束（如 `!is_union_v`）→ 拒绝，因为 union 排除没有充分的理论依据。

### Decision 2: StrictPolicy 使用 `is_transfer_safe<T>(gold)`

**选择**: `StrictPolicy<Gold>::accept<T>()` 直接调用 TypeLayout 的 `is_transfer_safe<T>(Gold)`。

**理由**: `is_transfer_safe` = `is_local_serialization_free_v<T> && sig(T) == gold`，精确匹配 StrictPolicy 的语义（C1+C2 编译期一体验证）。

### Decision 3: OPAQUE_*_AUTO 宏在 TypeLayout 远端实现

**选择**: 在 TypeLayout 的 `opaque.hpp` 中添加三个 AUTO 宏，作为 TypeLayout 正式 API。

**理由**:
- 这些宏是通用的 opaque 注册便利工具，不仅服务 XOffset
- 放在 TypeLayout 中可以随 TypeSignature 接口演进一起维护
- 宏需要设置 `pointer_free` trait，这是 TypeLayout 内部概念

**接口适配**:
- 远端 TypeSignature 无 `SignatureMode` 参数（vs 我们本地分支有 Mode_）
- AUTO 宏默认 `pointer_free = true`（因为 opaque 容器使用 offset_ptr）

### Decision 4: 诊断使用 `classify_v<T>` 五级分类

**选择**: `get_safety_error_message<T>()` 使用 TypeLayout 五级分类给出精确错误提示。

**分级映射**:
```
TrivialSafe      → ✅ 完全安全
PaddingRisk      → ✅ memcpy 安全（padding 泄漏是信息安全问题，非正确性问题）
PlatformVariant  → ⚠️ 本平台安全，跨平台需签名验证
PointerRisk      → ❌ 含指针/引用，不安全
Opaque           → ✅/❌ 取决于 opaque 注册（registered = safe）
```

### Decision 5: 基于远端 `origin/main` 开分支添加 AUTO 宏

**选择**: 在 TypeLayout 远端 `origin/main` 上开 `feat/opaque-auto-macros` 分支，添加 AUTO 宏后 merge 到 main。XOffset submodule 指向 merge 后的 main。

**理由**: 远端 `origin/main` 和我们之前的本地分支差异过大（77 文件、17000 行），无法 merge。必须基于远端重新实现。

## Risks / Trade-offs

**[Risk] 行为放宽导致用户困惑**
→ Mitigation: 更新 `validate_xbuffer_type` 错误消息和 README。union/long/wchar_t 从 "NOT ALLOWED" 变为合法。明确说明单平台 vs 跨平台的安全保障差异。

**[Risk] 远端 TypeLayout API 接口与预期不符**
→ Mitigation: 已完整审查远端 `origin/main` 的 API。`is_local_serialization_free_v<T>` 和 `is_transfer_safe<T>` 的实现已确认符合需求。

**[Risk] opaque 类型的 `is_local_serialization_free_v` 行为依赖 `pointer_free` 声明**
→ Mitigation: AUTO 宏默认 `pointer_free = true`，这对 XOffset 容器（使用 offset_ptr）是正确的。文档中明确说明。

**[Trade-off] 失去"编译期 early rejection"**
之前 `long` 在 `is_xbuffer_safe` 就被拒绝，现在只在跨平台签名比较时才发现差异。用户可能在单平台开发时用了 `long`，部署到跨平台时才发现问题。
→ Mitigation: 提供 `classify_v<T> == PlatformVariant` 的 lint-level 警告（非拒绝）；StrictPolicy 在编译期即可捕获。