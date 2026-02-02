# Implementation Tasks

## 1. 增长因子预设
- [ ] 1.1 定义 XOFFSET_GROWTH_COMPACT (1.1x)
- [ ] 1.2 定义 XOFFSET_GROWTH_BALANCED (1.5x)
- [ ] 1.3 定义 XOFFSET_GROWTH_FAST (2.0x)
- [ ] 1.4 更新 XVector 使用宏选择
- [ ] 1.5 添加使用文档

## 2. 分配算法选项
- [ ] 2.1 添加 XOFFSET_USE_BEST_FIT 宏
- [ ] 2.2 实现条件编译切换
- [ ] 2.3 添加性能对比文档
- [ ] 2.4 更新 README 说明

## 3. 哈希表容器评估
- [ ] 3.1 评估 Boost.Unordered flat_hash_map
- [ ] 3.2 设计 XHashMap 接口
- [ ] 3.3 实现原型 (如可行)
- [ ] 3.4 添加性能对比测试

## 4. 性能基准测试框架
- [ ] 4.1 创建 benchmarks/ 目录
- [ ] 4.2 实现容器操作基准测试
- [ ] 4.3 实现序列化/反序列化基准测试
- [ ] 4.4 生成性能报告模板

## 5. 测试与验证
- [ ] 5.1 验证不同增长因子的内存使用
- [ ] 5.2 验证分配算法切换
- [ ] 5.3 回归测试确保功能不变
