# Change: Improve Cross-Process Data Sharing Safety

## Why

当前跨进程数据共享安全性存在以下风险：
1. 不同编译选项可能导致结构体布局不一致
2. 类型签名不包含平台/编译器信息
3. 缺少对 `std::function`、`std::any` 等类型擦除容器的检测
4. 无 schema 版本控制机制

## What Changes

- 添加平台指纹到类型签名（arch, endian, ABI）
- 扩展 `is_xbuffer_safe<T>` 检测类型擦除容器
- 添加结构体填充一致性验证
- 设计 schema 版本兼容性检查机制

## Impact

- Affected specs: `specs/cross-process-safety/spec.md` (new)
- Affected code:
  - `xoffsetdatastructure2.hpp`: XTypeSignature namespace
  - `xoffsetdatastructure2.hpp`: is_xbuffer_safe implementation
- Risk: Medium - 可能影响现有类型签名格式
