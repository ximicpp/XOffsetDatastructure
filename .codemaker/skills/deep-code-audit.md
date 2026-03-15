# Skill: deep-code-audit

## Description
对 `xoffsetdatastructure.hpp` 进行系统性的逐块深度审计，检查代码逻辑正确性、死代码、冗余、可精简项。

## When to Use
- 重大重构前
- 定期代码质量审查时
- 用户要求 "分析" / "审查" / "review" 代码时

## Audit Methodology

### 7 层架构分层
1. **Layer 1: 平台与编译环境** — `ArchSpec`, 编译器检测, 静态断言
2. **Layer 2: 内存分配器** — Boost.Interprocess 分配器封装
3. **Layer 3: 核心容器** — `XString`, `XVector`, `XMap` 等 IPC 容器
4. **Layer 4: 安全引擎** — `validate_xbuffer_type<T>`, TypeLayout C1/C2 分类
5. **Layer 5: 反射构造** — P2996 `reflect_init_all`, `aggregate_construct`
6. **Layer 6: Buffer 管理** — `XBuffer`, `TypedXBuffer`, `XHandle`, 内存增长策略
7. **Layer 7: 压缩器** — `XCompactor`, `migrate_element`

### 审计检查项（每个 Block）
- [ ] **正确性**：逻辑是否正确？边界条件是否处理？
- [ ] **死代码**：是否有未使用的函数/类/分支？
- [ ] **冗余**：是否有重复逻辑可以 DRY？
- [ ] **安全性**：是否有潜在的 UB / 内存泄漏？
- [ ] **性能**：是否有不必要的拷贝/分配？
- [ ] **可读性**：注释是否过时？命名是否清晰？

### 严重性分级
- **P0 (Critical)** — 逻辑错误、数据丢失风险、UB → 立即修复
- **P1 (Important)** — 死代码、冗余、可精简 → 近期处理
- **P2 (Nice-to-have)** — 风格优化、文档改进 → 择机处理

## Steps

### Step 1: 阅读完整文件
读取 `xoffsetdatastructure.hpp` 全文，按照 7 层架构划分代码块。

### Step 2: 逐块分析
对每个 Block 执行检查项清单。

### Step 3: 汇总发现
按 P0 → P1 → P2 优先级整理发现列表。

### Step 4: 逐项确认
向用户展示每个发现项，等待用户确认是否修复。

### Step 5: 执行修复
按照用户确认的顺序，逐项修复并验证构建。

## Output Template
```
| ID | 层级 | Block | 问题描述 | 严重性 | 建议修复 |
|----|------|-------|----------|--------|----------|
| F1 | L1   | ArchSpec | ... | P0 | ... |
| F2 | L4   | Safety | ... | P1 | ... |
```

## Important Notes
- 审计后必须通过 `docker-build-test` 验证所有修改
- 每个修复应单独提交，便于回滚
- P0 修复优先于 P1，P1 优先于 P2
