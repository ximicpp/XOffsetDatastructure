# Change: Formalize the XOffset Safe Type Subset & TypeLayout Responsibility Model

## Why

XOffsetDatastructure 实质上定义了一个**类型子集** (Safe Type Subset)——只有属于这个子集的类型才能放入 XBuffer 进行零拷贝序列化。当前这个子集的定义分散在多个机制中：

1. **准入判定** (`is_xbuffer_safe<T>`) — 回答"类型 T 是否属于安全子集？"
2. **二进制合约** (TypeLayout signature) — 回答"两个属于安全子集的类型是否内存布局兼容？"
3. **迁移操作** (`XBufferCompactor`) — 回答"如何在缓冲区间搬运安全子集内的类型？"

这三个机制各自发展，存在以下问题：

- **子集定义不形式化**：安全规则是隐式的 if-constexpr 链，没有统一的类型分类模型
- **准入判定与迁移操作使用不同的类型分类逻辑**（架构审查 F8）
- **准入判定对容器的识别使用 sizeof 启发式**，而非精确类型匹配（F6）
- **迁移操作不验证准入**（F7）
- **TypeLayout 的角色定位不够清晰**：它是"签名引擎"还是"类型系统工具"？

需要一个统一的概念模型，将"安全类型子集"形式化，并明确 XOffset 和 TypeLayout 各自的职责。

## What Changes

**Phase 1 (概念分析)**：产出 `docs/TYPE_SUBSET_MODEL.md`
- 形式化 Safe Type Subset 的数学定义
- 建立统一的类型分类法 (Type Taxonomy)
- 明确 XOffset 与 TypeLayout 的三层职责模型
- 评估当前实现与理想模型的差距

**Phase 2 (设计改进)**：基于分析结论
- 设计统一的类型分类器 (Type Classifier)
- Safety / Migration / Signature 三者共享同一分类结果
- 评估哪些分类能力应下沉到 TypeLayout，哪些留在 XOffset

**Phase 3 (实施)**：如有必要，修改代码
- 合并 `improve-safety-detection` 中的实施项（F6/F7/F8）
- 统一 Safety 和 Compactor 的类型 dispatch

## Impact
- Affected specs: `type-signature`
- Affected proposals: may supersede `improve-safety-detection` (F6/F7/F8)
- Affected code: `xoffsetdatastructure2.hpp` — C5 + C6 组件
- Affected docs: New `docs/TYPE_SUBSET_MODEL.md`
