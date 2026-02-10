# Change: 深入分析编译时 Safety 分级 API 的设计方案

## Why

XOffsetDatastructure 的 `is_xbuffer_safe<T>` 是项目中最复杂的反射逻辑（~260 行），负责递归检查类型是否可以安全放入共享内存 XBuffer。TypeLayout 的 `classify_safety()` 提供了类似的运行时安全分级能力，但两者的职责边界、检查粒度和实现层次都不同。

在决定是否由 TypeLayout 提供编译时 Safety API 之前，需要深入分析以下核心问题：
1. **职责划分**：哪些安全检查属于"通用类型属性"（TypeLayout 职责），哪些属于"序列化领域规则"（XOffsetDatastructure 职责）？
2. **实现路径**：编译时 Safety API 应基于 P2996 反射还是签名字符串分析？
3. **接口设计**：TypeLayout 应暴露什么粒度的 API（细粒度 trait 还是粗粒度分级）？
4. **可复用性**：编译时 Safety API 对 TypeLayout 的其他用户是否也有价值？

## What Changes

本提案是**纯分析性**工作。产出为：
- 详细的职责边界分析
- 对 `is_xbuffer_safe<T>` 每条检查规则的分类（通用 vs 领域特定）
- TypeLayout 编译时 Safety API 的候选设计方案（至少 2 个备选方案）
- 推荐方案及理由

## Impact
- Affected specs: `type-signature`
- Affected code: 无（纯分析）
- 后续影响: 分析结果将决定是否在 TypeLayout 中添加编译时 Safety API
