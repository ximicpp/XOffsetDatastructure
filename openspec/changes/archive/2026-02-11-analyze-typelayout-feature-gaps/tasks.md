## 1. 深度需求分析
- [x] 1.1 分析 XOffsetDatastructure 中所有直接使用 TypeLayout API 的代码路径
- [x] 1.2 分析 XOffsetDatastructure 中与 TypeLayout 功能重叠的内部实现（is_xbuffer_safe, XBufferCompactor）
- [x] 1.3 分析已有的 static_assert 签名验证模式的可维护性问题
- [x] 1.4 分析容器特化注册的模板重复问题

## 2. 功能缺口评估
- [x] 2.1 评估编译时签名哈希 API 的可行性和设计方案
- [x] 2.2 评估签名差异诊断 API 的可行性（编译时 vs 运行时 diff）
- [x] 2.3 评估 Opaque 容器特化辅助宏的设计
- [x] 2.4 评估 consteval classify_safety<T>() 的可行性
- [x] 2.5 评估编译时成员迭代工具的 API 设计
- [x] 2.6 评估签名版本标识机制

## 3. 产出分析文档
- [x] 3.1 撰写 `docs/TYPELAYOUT_FEATURE_GAPS.md`，包含完整的需求-缺口映射
- [x] 3.2 为每个缺口提供建议的 API 签名和使用示例
- [x] 3.3 标注实现优先级和预估工作量
- [x] 3.4 标注各功能之间的依赖关系