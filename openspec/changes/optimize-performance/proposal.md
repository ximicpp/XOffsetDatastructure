# Change: Optimize Performance

## Why

当前实现存在以下性能优化空间：
1. `XVector` 增长因子 1.1x 过于保守，导致频繁重新分配
2. `x_seq_fit` 分配算法可能造成内存碎片
3. 缺少性能基准测试和优化指南

**注意**: 本库使用 `boost::container::flat_map` 实现 `XMap`，这是连续内存布局，符合跨进程共享需求。
`flat_map` 的 O(n) 插入复杂度是设计权衡（换取更好的缓存局部性和共享内存兼容性），不在本次优化范围内。

## What Changes

- 提供多种增长因子预设 (compact/balanced/fast)
- 添加 `x_best_fit` 分配算法选项

## Impact

- Affected specs: `specs/performance/spec.md` (new)
- Affected code:
  - `xoffsetdatastructure2.hpp`: growth_factor_custom
  - `xoffsetdatastructure2.hpp`: XBuffer typedef
- Risk: Low - 仅配置变更，兼容现有代码
