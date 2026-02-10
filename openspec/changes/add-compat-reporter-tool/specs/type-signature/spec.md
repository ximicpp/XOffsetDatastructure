## ADDED Requirements
### Requirement: 跨平台兼容性验证工具

系统 SHALL 提供基于 TypeLayout `CompatReporter` 的跨平台兼容性验证工具，可比较多个平台的签名并生成兼容性矩阵报告。

**实现方式**：
- 使用 `TYPELAYOUT_CHECK_COMPAT` 宏自动生成比较程序
- 编译时 `static_assert` 验证 + 运行时报告输出
- 仅需 C++17 编译器（不需要 P2996）

#### Scenario: 运行时兼容性报告
- **WHEN** 用户运行 `tools/check_compat` 工具
- **THEN** 输出跨平台兼容性矩阵
- **AND** 报告包含 Layout 匹配状态、Definition 匹配状态和 Safety 分级
- **AND** 标注哪些类型可以零拷贝传输、哪些需要序列化

#### Scenario: Safety 分级验证
- **WHEN** 验证 XOffsetDatastructure 的核心类型（Player, Item, GameData）
- **THEN** Safety 分级应为 Safe（不含指针、位域）
- **AND** 在同架构下 Layout 和 Definition 均为 MATCH