## ADDED Requirements

### Requirement: Member-Level Error Localization
类型安全检查失败时，系统 SHALL 提供违规成员的名称和路径。

#### Scenario: Polymorphic member detection
- **WHEN** 结构体包含多态类型成员
- **THEN** 错误信息应包含: "Member 'memberName' has virtual functions - remove 'virtual' keyword or use CRTP"

#### Scenario: Raw pointer member detection
- **WHEN** 结构体包含原始指针成员
- **THEN** 错误信息应包含: "Member 'memberName' is a raw pointer - use XOffsetPtr<T> or XVector<T> instead"

### Requirement: Diagnostic Tool Function
系统 SHALL 提供诊断工具函数，帮助用户理解类型安全问题。

#### Scenario: Type diagnosis
- **WHEN** 用户调用 `diagnose_type<T>()` 
- **THEN** 返回人类可读的诊断报告，列出所有潜在问题

### Requirement: Enhanced Exception Messages
XBuffer 操作异常 SHALL 包含上下文信息。

#### Scenario: Allocation failure context
- **WHEN** 内存分配失败
- **THEN** 异常信息应包含：请求大小、可用空间、缓冲区状态
