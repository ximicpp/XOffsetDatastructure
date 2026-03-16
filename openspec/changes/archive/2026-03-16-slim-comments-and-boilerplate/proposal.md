## Why

`xoffsetdatastructure.hpp` 经过 Type Safety 精简后为 1815 行。剩余区域仍有大量冗余注释（重复性解释、过度冗长的 section banner、不必要的参数说明）和可精简的样板代码。分析各区域后，存在约 ~120 行可安全精简的空间。核心逻辑和 API 不变。

## What Changes

精简范围覆盖除 Type Safety 外的所有区域：

**注释精简（~80 行）：**
- **XManagedMemory (220行)**：构造函数注释合并，grow() 内联注释压缩，section banner 精简。预计 -25 行
- **detail:: Container Impl (141行)**：删除过度冗长的 concept 解释块（`needs_reflect_construct` 13行注释→2行，`x_reflect_scoped_alloc` 11行注释→2行）。预计 -20 行
- **Public Containers (190行)**：XVector overload 上方的 13 行解释块精简为 2 行。预计 -10 行
- **Reflect Construct (110行)**：17 行开头注释块→3 行。预计 -12 行
- **Reflect Transfer (165行)**：14 行开头注释块→2 行，内联注释精简。预计 -10 行
- **XBuffer (154行)**：MaxCapacity/save/save_raw 的冗余 doc-comment 精简。预计 -10 行
- **XCompactor (254行)**：resolve_strategy 的 18 行 F7 注释块→4 行，compact 注释精简。预计 -15 行
- **Registration Macros (110行)**：23 行开头注释→6 行。预计 -15 行
- **XHandle (48行)**：10 行开头注释→2 行。预计 -8 行

**代码精简（~15 行）：**
- **XManagedMemory**：合并 `vector<char>&` 和 `vector<char>&&` 两个构造函数的共享逻辑为 private helper
- **Reflect Construct**: 内联 `reflect_member_count_of` / `reflect_base_count_of` 辅助函数（只用于 2 处）

## Capabilities

### New Capabilities
（无新增能力）

### Modified Capabilities
（无规格变更 — 纯实现层精简，所有公共 API 和行为不变）

## Impact

- **文件**: `xoffsetdatastructure.hpp`（全区域，除 Type Safety 外）
- **预期缩减**: ~100 行（1815 → ~1715 行）
- **API 影响**: 无。所有公共类、函数、宏签名不变
- **风险**: 低。纯注释和内部辅助函数精简
- **验证**: Docker 构建 23 个测试全部通过