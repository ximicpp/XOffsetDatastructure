## Context

测试套件经过多轮特性开发后膨胀到 39 个源文件。Domain S 重构已完成（32/32 通过），
测试语义稳定，现在可以安全地整理。当前问题：

1. **孤立文件**：7 个 `.cpp` 不在 CMakeLists.txt 中，占用仓库空间但不参与 CI
2. **功能重叠**：4 组测试（各 2-3 个文件）覆盖相同的功能点
3. **过时引用**：部分文件仍引用已删除的 API（`classify_safety`、`is_layout_safe`）

## Goals / Non-Goals

**Goals:**

- 将 39 个源文件精简到 27 个，每个测试文件有明确的职责边界
- 消除所有孤立源文件（不在 CMakeLists.txt 中的 `.cpp`）
- 保持 100% 的测试覆盖率不下降（所有 `static_assert` 和运行时断言在合并后保留）
- 更新 `tests/CMakeLists.txt` 和 `tests/README.md`

**Non-Goals:**

- 不修改 `xoffsetdatastructure.hpp` 或 TypeLayout 库代码
- 不引入新的测试框架（如 Google Test / Catch2）
- 不改变测试的执行方式（仍用 CTest + 独立二进制）
- 不新增任何测试用例

## Decisions

### D1: 删除策略 — 物理删除而非注释

孤立文件和完全覆盖的文件直接用 `git rm` 物理删除。

**理由**：这些文件在 git 历史中仍可追溯。保留为注释会增加混乱。
**替代方案**：移到 `tests/archive/` 子目录 → 不采用，仍占用空间且会被误读。

### D2: 合并策略 — 按功能域分组

将反射测试按功能分为三层：
1. `test_reflection_core.cpp` — P2996 基础操作（`^^T`、`members_of`、splice）
2. `test_reflection_advanced.cpp` — 反射应用（序列化、比较、diff）
3. `test_type_signatures.cpp` — TypeLayout 签名生成（class/struct 变体、反射签名）

**理由**：按"抽象层级"分组比按"开发时间线"分组更利于维护。  
**替代方案**：全部合并为一个 `test_reflection.cpp` → 文件过大（>500 行），不利于定位失败。

### D3: 重命名 `test_type_safety_comprehensive` → `test_type_safety`

直接替换旧的 `test_type_safety.cpp`（先删除旧文件）。

**理由**：`_comprehensive` 后缀冗余，这是唯一的类型安全综合测试。
**约束**：CMakeLists.txt 中 CTest 名称 `test_type_safety` 不变。

### D4: 合并时的 `static_assert` 冲突解决

当多个源文件定义相同名称的测试结构体（如 `SafeStruct`、`HasPointer`）时：
- 选择最完整的定义，删除重复的
- 如有语义差异，添加后缀区分（如 `HasPointer_direct` vs `HasPointer_nested`）

### D5: CMakeLists.txt 更新方式

在 `REFLECTION_TESTS` 列表中直接替换：删除旧名称、添加新名称。
不需要修改 `configure_macos_target` 或 `foreach` 循环逻辑。

### D6: 负编译测试（`try_compile`）保留

CMakeLists.txt 中的 `try_compile(POLYMORPHIC_COMPILES ...)` 块保留。
虽然删除了 `test_polymorphic_rejection.cpp`，但 `try_compile` 已有 `if(EXISTS ...)` 守护，
删除源文件后自动跳过，无需修改 CMake 逻辑。

## Risks / Trade-offs

- **[Risk] 合并引入编译错误** → 合并后立即运行 Docker 构建验证（32 → 27 测试全通过）
- **[Risk] 丢失测试覆盖点** → 每次合并前对比 `static_assert` 清单，确保无遗漏
- **[Trade-off] 文件减少 vs 单文件变大** → 控制合并后每个文件 ≤ 300 行；超过则拆分
- **[Trade-off] 简单直接删除 vs 渐进式迁移** → 选择一次性完成，因为所有测试都是独立二进制，无级联依赖
