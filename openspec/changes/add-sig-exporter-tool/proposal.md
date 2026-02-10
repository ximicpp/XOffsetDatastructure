# Change: 集成 TypeLayout SigExporter 签名导出工具

## Why
`type-signature` spec 的 Requirement 5（跨平台签名导出工具）要求系统提供跨平台签名导出能力。TypeLayout 已经实现了完整的 `SigExporter` 工具（`tools/sig_export.hpp`），但 XOffsetDatastructure 完全未利用。需要为关键数据类型（Player, GameData, Item）创建签名导出工具，生成可在任何 C++17 编译器上比较的 `.sig.hpp` 文件。

## What Changes
- 创建 `tools/export_signatures.cpp`：使用 `TYPELAYOUT_EXPORT_TYPES` 宏导出 Player, GameData, Item 的签名
- 在 `CMakeLists.txt` 中添加 `export_signatures` 构建目标
- 创建 `tools/sigs/` 目录存放导出的签名头文件
- 在 `build.sh` 中可选执行签名导出步骤
- 更新文档说明用法

## Impact
- Affected specs: `type-signature`（实现 Requirement 5 的 Scenario 1）
- Affected code:
  - 新增 `tools/export_signatures.cpp`
  - 修改 `CMakeLists.txt`（添加 target）
  - 可选修改 `build.sh`