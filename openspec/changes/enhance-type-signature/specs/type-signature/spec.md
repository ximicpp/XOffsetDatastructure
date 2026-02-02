## ADDED Requirements

### Requirement: Type Signature Hash
类型签名系统 SHALL 提供编译时哈希函数，用于快速比较签名。

#### Scenario: Hash generation
- **WHEN** 调用 `type_signature_hash<T>()`
- **THEN** 返回 64 位编译时常量哈希值

#### Scenario: Hash stability
- **WHEN** 相同类型在不同编译单元生成哈希
- **THEN** 哈希值应相同

### Requirement: Signature String Access
类型签名系统 SHALL 提供获取完整签名字符串的接口。

#### Scenario: String retrieval
- **WHEN** 调用 `type_signature_string<T>()`
- **THEN** 返回可在运行时使用的签名字符串

### Requirement: Signature Validation
类型签名系统 SHALL 提供签名验证工具。

#### Scenario: Compile-time validation
- **WHEN** 使用 `static_assert` 验证两个类型签名相同
- **THEN** 编译时报告匹配或不匹配

#### Scenario: Runtime validation with details
- **WHEN** 运行时验证签名不匹配
- **THEN** 提供详细的差异报告，指出哪些字段不同

### Requirement: Signature Persistence
XBuffer SHALL 支持在缓冲区头部存储类型签名信息。

#### Scenario: Signature storage
- **WHEN** 创建 XBuffer 并存储对象
- **THEN** 签名哈希应存储在缓冲区元数据中

#### Scenario: Signature verification on load
- **WHEN** 加载 XBuffer 并访问对象
- **THEN** 应验证签名与当前类型匹配
