## ADDED Requirements

### Requirement: Platform Fingerprint in Type Signature
类型签名系统 SHALL 包含平台指纹信息，以确保跨进程共享数据的兼容性。

#### Scenario: Platform fingerprint generation
- **WHEN** 生成类型签名时
- **THEN** 签名应包含架构 (x64/arm64)、字节序 (little/big)、ABI (itanium/msvc) 信息

### Requirement: Type Erasure Container Detection
`is_xbuffer_safe<T>` SHALL 检测并拒绝包含类型擦除容器的类型。

#### Scenario: std::function rejection
- **WHEN** 结构体包含 `std::function` 成员
- **THEN** `is_xbuffer_safe<T>` 应返回 false

#### Scenario: std::any rejection
- **WHEN** 结构体包含 `std::any` 成员
- **THEN** `is_xbuffer_safe<T>` 应返回 false

### Requirement: Schema Version Control
类型签名系统 SHALL 支持 schema 版本控制，允许检测和处理版本不兼容。

#### Scenario: Version mismatch detection
- **WHEN** 加载的数据 schema 版本与当前不匹配
- **THEN** 系统应提供版本信息用于兼容性判断
