## Why

XOffset 的 Domain S（安全类型集）当前混合了两层关注：C2（本地安全：无绝对地址）和 C1（跨平台安全：类型宽度稳定）。C1 部分（排除 `long`、`wchar_t`、`long double`、`int`）与 TypeLayout 签名比较功能重叠——TypeLayout 签名已精确编码每个类型的 sizeof/alignof，跨平台差异由签名不匹配自动暴露。此外，union 的排除理由（"活跃成员歧义"）与 enum 值域合法性问题同级，属于应用层语义而非 byte-copy 正确性问题。

远端 TypeLayout (`origin/main`) 已提供 `is_local_serialization_free_v<T>`（C2 判定）和 `is_transfer_safe<T>(remote_sig)`（C1+C2 判定），可完全覆盖 XOffset 的类型安全需求。XOffset 应统一 Domain S 的定义，使其与 TypeLayout 的 serialization-free 语义对齐，消除自建类型分类逻辑。

## What Changes

- **BREAKING**: Domain S 定义变更——`union`、`int`、`long`、`wchar_t`、`long double` 不再被 `is_xbuffer_safe<T>` 拒绝。这些类型在单平台场景下可以安全进入 XBuffer。跨平台安全性由 C1 签名比较保障。
- **BREAKING**: 移除 `classify_for_xoffset<T>()`、旧三级 `SafetyLevel`（Safe/Warning/Risk）、`is_layout_safe<T>()`、`ArchSpec` 等全部自建类型分类逻辑
- `DefaultPolicy::accept<T>()` 委托给 TypeLayout 的 `is_local_serialization_free_v<T>`
- `StrictPolicy<Gold>::accept<T>()` 委托给 TypeLayout 的 `is_transfer_safe<T>(Gold)`
- 诊断信息使用 TypeLayout 五级 `classify_v<T>`（TrivialSafe/PaddingRisk/PlatformVariant/PointerRisk/Opaque）
- TypeLayout 远端新增 `TYPELAYOUT_OPAQUE_*_AUTO` 宏，支持 XVector/XString/XSet/XMap 的 opaque 注册
- 更新 `CORE_FORMAL_MODEL.md`：Domain S 重新定义为 `{ T | is_local_serialization_free_v<T> } ∪ { registered opaque types }`
- 更新 submodule 指向远端 TypeLayout 最新版本

## Capabilities

### New Capabilities

_None — this change unifies existing capabilities, not introducing new ones._

### Modified Capabilities

- `core-model`: Domain S 的形式化定义变更——从手工枚举 S₀ + 排除规则改为委托 TypeLayout 的 `is_local_serialization_free_v<T>`；移除 union 排除；C1 相关约束从 Domain S 移到签名验证层
- `type-signature`: 深化 TypeLayout 集成——从"使用 TypeLayout 生成签名"升级为"使用 TypeLayout 作为唯一类型安全引擎"；新增 opaque AUTO 宏依赖；新增 `is_local_serialization_free_v` 和 `is_transfer_safe` 的使用

## Impact

- **TypeLayout 仓库**: 需要在 `opaque.hpp` 中新增 `TYPELAYOUT_OPAQUE_TYPE_AUTO`、`TYPELAYOUT_OPAQUE_CONTAINER_AUTO`、`TYPELAYOUT_OPAQUE_MAP_AUTO` 宏
- **xoffsetdatastructure.hpp**: 移除 ~150 行自建逻辑，替换为 TypeLayout API 调用
- **CORE_FORMAL_MODEL.md**: §3 Domain S 重写
- **tests/**: `test_policy_trait.cpp`、`test_remediation_fixes.cpp`、`test_classify_safety.cpp`、`test_type_safety_comprehensive.cpp` 需要适配新语义
- **validate_xbuffer_type 错误消息**: 移除 "Union types" 等不再适用的提示
- **用户代码**: 之前被拒绝的 union/long/wchar_t 类型现在可以进入 XBuffer（行为放宽，不会破坏已有合法代码）