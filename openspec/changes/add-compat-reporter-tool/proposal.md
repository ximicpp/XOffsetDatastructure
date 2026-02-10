# Change: 集成 TypeLayout CompatReporter 跨平台兼容性验证

## Why
TypeLayout 提供了完整的 `CompatReporter` 工具（`tools/compat_check.hpp`），可以比较多个平台的签名并生成兼容性矩阵报告。XOffsetDatastructure 作为跨进程共享内存的序列化库，天然需要跨平台兼容性验证能力，但目前完全没有利用这个工具。

## What Changes
- 创建 `tools/check_compat.cpp`：使用 `TYPELAYOUT_CHECK_COMPAT` 宏比较已导出的平台签名
- 创建编译时 `static_assert` 验证（Linux 内 x86_64 自比较）
- 创建运行时兼容性报告输出
- 在 `CMakeLists.txt` 中添加 `check_compat` 构建目标（C++17，不需要 P2996）

## Impact
- Affected specs: `type-signature`（实现 Requirement 5 的 Scenario 2）
- Affected code:
  - 新增 `tools/check_compat.cpp`
  - 修改 `CMakeLists.txt`（添加 C++17 target）
- 依赖: `add-sig-exporter-tool` 提案先完成（需要先导出签名）