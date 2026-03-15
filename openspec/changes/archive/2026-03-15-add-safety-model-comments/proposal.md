## Why

经过详细的安全模型审查，`DefaultPolicy::accept<T>()` 的三层分支逻辑被验证为正确，
但第三分支（递归成员检查）绕过了 `trivially_copyable` 要求的原因不够明显。
新开发者可能会疑惑：为什么一个非 trivially_copyable 的 struct 能通过类型检查？

答案是：非平凡析构来自 opaque 容器成员（XVector/XString 等），而这些容器的
byte-copy 安全性由 `RELOCATABLE` 宏的调用者保证。但这个推理链需要在代码中文档化。

同时，审查发现一个边界行为（opaque 容器的 C 数组如 `XVector<int>[3]` 会被拒绝）
应在注释中记录为已知限制。

## What Changes

- 在 `DefaultPolicy::accept<T>()` 各分支添加架构注释
- 记录安全责任分层：opaque 外壳 = 用户保证，opaque 内容 = TypeLayout 保证
- 记录已知限制：opaque 容器的 C 数组

## Capabilities

### New Capabilities
_无_

### Modified Capabilities
_无（纯注释变更）_

## Impact

- **代码**: 仅修改 `xoffsetdatastructure.hpp` 中的注释，零逻辑变更
- **测试**: 无需修改
- **风险**: 零
