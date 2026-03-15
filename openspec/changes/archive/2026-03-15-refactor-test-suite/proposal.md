## Why

测试套件在多次迭代（旧分类系统 → TypeLayout 集成 → Domain S 重构）后累积了 39 个测试源文件，
其中 3 个已从构建移除但源文件残留、至少 4 组测试存在严重功能重叠、还有多个文件引用已删除的 API 或包含过时的注释。
测试文件数量膨胀导致 CI 编译时间增长、维护负担加重、新开发者难以理解测试覆盖范围。
现在是清理的最佳时机：Domain S 重构已完成并验证通过（32/32），测试语义已稳定。

## What Changes

### 删除（7 个文件）

- **删除** `test_classify_safety.cpp` — 引用已删除的 `classify_safety.hpp`，功能已被 `test_policy_trait.cpp` 覆盖
- **删除** `test_long_reflection_probe.cpp` — 一次性探测脚本（`^^long == ^^int64_t` 等），不是回归测试
- **删除** `run_all_tests.cpp` — 引用不存在的函数符号，从未被 CMakeLists.txt 注册
- **删除** `test_type_safety.cpp` — 100% 被 `test_type_safety_comprehensive.cpp` 覆盖（多态/继承/组合）
- **删除** `test_vptr_layout.cpp` — 诊断探测脚本，功能被 `test_type_safety_comprehensive.cpp` Test 6.5 和 `test_remediation_fixes.cpp` C1 覆盖
- **删除** `test_type_erased_detection.cpp` — 测试 `std::function`/`std::any`/`std::shared_ptr` 检测，这些类型在 Domain S 中因 `!is_trivially_copyable` 或 `has_pointer` 自然被拒绝，无需专用测试
- **删除** `test_polymorphic_rejection.cpp` — 负编译测试的概念已被 `test_type_safety_comprehensive.cpp` 的 `static_assert` 覆盖

### 合并（3 次合并，减少 5 个文件）

- **合并** `test_reflection_operators.cpp` + `test_member_iteration.cpp` + `test_splice_operations.cpp`
  → `test_reflection_core.cpp`
  - 这三个文件都测试 P2996 反射的基础操作（`^^T`、`members_of`、`[: :]` splice），
    共享相同的测试结构体，合并后约 200 行

- **合并** `test_reflection_serialization.cpp` + `test_reflection_comparison.cpp`
  → `test_reflection_advanced.cpp`
  - 两者都使用反射做结构分析/序列化/比较，共享 `SerializableData`/`ComparableData`

- **合并** `test_reflection_type_signature.cpp` + `test_class_type_signatures.cpp`
  → `test_type_signatures.cpp`
  - 两者都测试 TypeLayout 类型签名生成，前者偏反射+签名，后者偏 class 变体签名

### 重命名/精简（2 个文件）

- **精简** `test_type_safety_comprehensive.cpp` → `test_type_safety.cpp`（取代旧同名文件）
  - 已经是最完整的类型安全测试，去掉 "comprehensive" 后缀
- **精简** `test_typelayout_integration.cpp` — 删除冗余的 Test 1/Test 2 重叠注释

### 不变（保留 22 个核心测试）

- 数据结构功能：`test_basic_types`, `test_vector`, `test_map_set`, `test_nested`, `test_modify`
- 内存管理：`test_compaction`, `test_memory_efficiency`, `test_adaptive_reservation`
- API 功能：`test_xbuffer_api`, `test_xhandle`, `test_xstring_direct_assign`, `test_error_paths`
- 类型安全：`test_type_safety`(renamed), `test_policy_trait`, `test_remediation_fixes`, `test_typelayout_integration`
- 反射：`test_reflection_core`(merged), `test_reflection_advanced`(merged), `test_type_signatures`(merged), `test_reflection_compaction`
- 零样板：`test_zero_boilerplate`, `test_zero_boilerplate_vector`
- 高级：`test_complex_nesting`, `test_inheritance`, `test_enum_support`, `test_field_limit_fix`

### 结果

| 指标 | 重构前 | 重构后 | 变化 |
|------|--------|--------|------|
| 源文件数 | 39 | 27 | -12 (-31%) |
| CMake 注册测试 | 32 | 27 | -5 (-16%) |
| 孤立源文件 | 7 | 0 | -7 |
| 编译目标数 | 33 | 28 | -5 |

## Capabilities

### New Capabilities

_无新能力引入。这是纯粹的测试重组，不改变任何库代码或公共 API。_

### Modified Capabilities

_无需求级别变更。所有测试覆盖点在合并后保持不变。_

## Impact

- **代码**：仅影响 `tests/` 目录下的 `.cpp` 文件和 `tests/CMakeLists.txt`
- **CI**：测试数量从 32 减少到约 27，编译时间预计减少 ~15%
- **文档**：`tests/README.md` 需要更新测试列表
- **库代码**：零影响 — 不修改 `xoffsetdatastructure.hpp` 或 TypeLayout
- **风险**：低。所有删除/合并的测试断言都在保留的测试中有等效覆盖
