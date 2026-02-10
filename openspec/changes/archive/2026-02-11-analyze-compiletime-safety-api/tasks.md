## 1. is_xbuffer_safe<T> 逐条规则分类
- [x] 1.1 列出 is_xbuffer_safe<T> 的所有检查规则（逐行标注）
- [x] 1.2 将每条规则分类为"通用类型属性"或"序列化领域规则"
- [x] 1.3 分析 TypeLayout classify_safety() 已覆盖的检查项
- [x] 1.4 识别两者的检查重叠和差异

## 2. 编译时 vs 运行时分析
- [x] 2.1 分析 classify_safety() 运行时实现的优缺点
- [x] 2.2 分析 is_xbuffer_safe<T> 编译时实现的优缺点
- [x] 2.3 评估将运行时 API 提升为编译时 API 的可行性和限制

## 3. 候选设计方案
- [x] 3.1 方案 A: 细粒度 consteval trait（has_pointer_members, has_bitfields, is_polymorphic 等）
- [x] 3.2 方案 B: 粗粒度分级（consteval CompileTimeSafety classify<T>()）
- [x] 3.3 方案 C: 不添加新 API，保持现状（两者独立实现）
- [x] 3.4 对比分析各方案的优缺点

## 4. 产出
- [x] 4.1 撰写分析文档，包含推荐方案和理由
- [x] 4.2 推荐方案 C（不添加新 API）：递归扫描不可合并，真正重叠的 3 项已有标准 type_traits