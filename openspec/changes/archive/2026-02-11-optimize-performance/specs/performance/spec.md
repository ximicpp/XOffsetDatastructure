## ADDED Requirements

### Requirement: Configurable Growth Factor
XVector SHALL 支持通过编译时宏选择增长因子预设。

#### Scenario: Compact growth mode
- **WHEN** 定义 `XOFFSET_GROWTH_MODE=COMPACT`
- **THEN** XVector 使用 1.1x 增长因子，优先节省内存

#### Scenario: Balanced growth mode
- **WHEN** 定义 `XOFFSET_GROWTH_MODE=BALANCED` 或未定义
- **THEN** XVector 使用 1.5x 增长因子，平衡内存和性能

#### Scenario: Fast growth mode
- **WHEN** 定义 `XOFFSET_GROWTH_MODE=FAST`
- **THEN** XVector 使用 2.0x 增长因子，优先性能

### Requirement: Configurable Allocation Algorithm
XBuffer SHALL 支持选择不同的内存分配算法。

#### Scenario: Sequential fit (default)
- **WHEN** 未定义 `XOFFSET_USE_BEST_FIT`
- **THEN** 使用 `x_seq_fit` 简单顺序分配，适合小缓冲区

#### Scenario: Best fit allocation
- **WHEN** 定义 `XOFFSET_USE_BEST_FIT`
- **THEN** 使用 `x_best_fit` 红黑树分配，减少大缓冲区碎片

### Requirement: Performance Benchmarks
项目 SHALL 包含标准性能基准测试，用于评估不同配置的性能影响。

#### Scenario: Container operation benchmarks
- **WHEN** 运行基准测试
- **THEN** 报告 XVector/XMap/XSet 的插入、查找、删除性能
