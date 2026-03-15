## ADDED Requirements

### Requirement: Safety responsibility model is documented in source

DefaultPolicy::accept 的注释 SHALL 明确说明安全责任分层：
1. Opaque 外壳 byte-copy 安全 = 用户/库作者通过 RELOCATABLE 宏保证
2. Opaque 内部元素安全 = TypeLayout 签名 + XOffset 递归验证保证
3. 已知限制（opaque C 数组）= 注释记录
