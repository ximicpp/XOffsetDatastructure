# Change: Optimize Performance

## Why

当前实现存在以下性能优化空间：
1. `flat_map` 插入复杂度 O(n)，频繁插入场景不理想
2. `XVector` 增长因子 1.1x 过于保守，导致频繁重新分配
3. `x_seq_fit` 分配算法可能造成内存碎片
4. 缺少性能基准测试和优化指南

## What Changes

- 提供多种增长因子预设 (compact/balanced/fast)
- 添加 `x_best_fit` 分配算法选项
- 评估并可选添加 `XHashMap` (基于哈希表)
- 建立性能基准测试框架

## Impact

- Affected specs: `specs/performance/spec.md` (new)
- Affected code:
  - `xoffsetdatastructure2.hpp`: growth_factor_custom
  - `xoffsetdatastructure2.hpp`: XBuffer typedef
  - New: hash map container (可选)
- Risk: Medium - 需要兼容现有代码
