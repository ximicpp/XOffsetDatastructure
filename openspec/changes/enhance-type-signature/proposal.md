# Change: Enhance Type Signature System

## Why

当前类型签名系统存在以下功能缺失：
1. 签名字符串可能很长（嵌套结构），比较效率低
2. 缺少签名哈希用于快速比较
3. 签名不包含版本信息
4. 缺少签名持久化和验证工具

## What Changes

- 实现编译时签名哈希 (FNV-1a 或类似算法)
- 添加 `type_signature_hash<T>()` 函数
- 设计签名版本控制方案
- 创建签名比较和验证工具

## Impact

- Affected specs: `specs/type-signature/spec.md` (new)
- Affected code:
  - `xoffsetdatastructure2.hpp`: XTypeSignature namespace
  - New: 哈希函数实现
  - New: 验证工具
- Risk: Low - 纯增量功能
