## ADDED Requirements

### Requirement: Test files have clear ownership boundaries

每个测试源文件 SHALL 有唯一且明确的功能职责域。不同文件之间的 `static_assert` 和
运行时断言 MUST NOT 存在完全重叠（即同一断言在两个文件中以相同类型和相同条件出现）。

#### Scenario: No duplicate test coverage
- **WHEN** 检查所有测试源文件中的 `static_assert` 声明
- **THEN** 不存在两个不同文件对同一类型做完全相同的安全性断言

#### Scenario: Every source file is registered
- **WHEN** 查看 `tests/` 目录中的所有 `.cpp` 文件
- **THEN** 每个 `.cpp` 文件都在 `tests/CMakeLists.txt` 中注册为编译目标（`test_polymorphic_rejection.cpp` 除外，它通过 `try_compile` 处理）

### Requirement: Test suite compiles and passes after reorganization

重构后的测试套件 SHALL 保持 100% 通过率。所有原有的类型安全断言、运行时验证、
和反射测试 MUST 在重组后的文件中保留等效覆盖。

#### Scenario: Full build passes
- **WHEN** 在 Docker 环境中运行 `bash ./build.sh`
- **THEN** 所有测试编译成功且全部通过（0 failures）

#### Scenario: Test count matches expectation
- **WHEN** 构建完成后检查测试数量
- **THEN** 总测试数为 27（7 个基础测试 + 20 个反射测试）

### Requirement: Merged files stay within size limit

合并后的每个测试文件 SHALL 不超过 300 行。超过此限制的文件 MUST 进一步拆分。

#### Scenario: File size check
- **WHEN** 对 `test_reflection_core.cpp`、`test_reflection_advanced.cpp`、`test_type_signatures.cpp` 做行数统计
- **THEN** 每个文件的行数 ≤ 300
