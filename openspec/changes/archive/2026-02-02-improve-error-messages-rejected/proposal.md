# Change: Improve Error Message Readability

## Why

当前错误信息存在以下问题：
1. `is_xbuffer_safe<T>` 失败时不指出哪个成员违规
2. 反射错误信息晦涩，难以定位问题
3. Boost 分配异常缺少上下文信息
4. 缺少诊断工具帮助用户理解错误原因

## What Changes

- 实现详细的类型安全诊断系统
- 添加成员级别的错误定位
- 包装 Boost 异常提供更多上下文
- 创建诊断工具函数

## Impact

- Affected specs: `specs/error-handling/spec.md` (new)
- Affected code:
  - `xoffsetdatastructure2.hpp`: is_xbuffer_safe implementation
  - `xoffsetdatastructure2.hpp`: XBufferExt class
- Risk: Low - 主要是增强，不改变现有行为
