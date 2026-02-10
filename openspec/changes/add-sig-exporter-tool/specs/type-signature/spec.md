## MODIFIED Requirements
### Requirement: 跨平台签名导出工具

系统 SHALL 提供基于 TypeLayout `SigExporter` 的跨平台签名导出工具，支持为关键数据类型生成可移植的 `.sig.hpp` 签名头文件。

**实现方式**：
- 使用 `TYPELAYOUT_EXPORT_TYPES` 宏自动生成导出程序
- 导出的 `.sig.hpp` 文件可在任何 C++17 编译器上 include

#### Scenario: 导出签名到头文件
- **WHEN** 用户编译并运行 `tools/export_signatures` 工具
- **THEN** 在指定目录生成包含签名常量的 `.sig.hpp` 文件
- **AND** 文件包含 Player, Item, GameData 的 Layout 和 Definition 签名
- **AND** 该文件可在任何 C++17 编译器上 include 并比较

#### Scenario: 跨平台签名比较 (C++17 兼容)
- **WHEN** 用户在 Platform B 上编译包含 Platform A 导出签名的代码
- **THEN** 使用 TypeLayout 的 `compat::layout_match()` 进行比较
- **AND** 不需要 P2996 编译器
- **AND** 正确报告布局是否兼容