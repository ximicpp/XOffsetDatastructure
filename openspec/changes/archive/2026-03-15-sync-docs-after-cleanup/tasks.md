## 1. docs/QUICK_REFERENCE.md — 测试矩阵更新 (27→23)

- [x] 1.1 "总测试: 27个" → "总测试: 23个"
- [x] 1.2 "反射测试 (20个)" → "反射测试 (16个)"
- [x] 1.3 移除 4 个已删除测试名：test_type_introspection, test_reflection_compaction, test_typelayout_integration, test_remediation_fixes
- [x] 1.4 "Tests Run: 27 / Tests Passed: 27" → "Tests Run: 23 / Tests Passed: 23"
- [x] 1.5 "27个测试" → "23个测试"（时间表和检查清单）

## 2. docs/BUILD_AND_TEST_GUIDE.md — 测试矩阵更新 (27→23)

- [x] 2.1 "总测试数: 27" → "总测试数: 23"
- [x] 2.2 "反射测试 (20个)" → "反射测试 (16个)"
- [x] 2.3 移除 4 个已删除测试名
- [x] 2.4 所有 "27" → "23"（Tests Run、run_test、检查清单等）

## 3. docs/README.md — 数量更新

- [x] 3.1 "27个测试 = 7个基础测试 + 20个反射测试" → "23个测试 = 7个基础测试 + 16个反射测试"
- [x] 3.2 "Tests Run: 27 / Tests Passed: 27" → "Tests Run: 23 / Tests Passed: 23"

## 4. docs/ZERO_BOILERPLATE.md — 数量更新

- [x] 4.1 "27 tests total" → "23 tests total"

## 5. AGENTS.md — Test suite summary 更新

- [x] 5.1 "27 tests" → "23 tests", "23/23" 构建验证
- [x] 5.2 "Basic tests: 7 ... Reflection tests: 20" → "Basic tests: 7 ... Reflection tests: 16"

## 6. openspec/specs — 规格更新

- [x] 6.1 openspec/specs/test-organization/spec.md: "27（7 + 20）" → "23（7 + 16）"
- [x] 6.2 openspec/specs/documentation/spec.md: "27" → "23", "20" → "16"

## 7. docs 中已删除测试名引用修正

- [x] 7.1 docs/TYPELAYOUT_INTEGRATION_ANALYSIS.md: "test_typelayout_integration.cpp" → "test_type_signatures.cpp"
- [x] 7.2 docs/TYPELAYOUT_INTEGRATION_ANALYSIS.md: "test_remediation_fixes" → "test_policy_trait"
- [x] 7.3 docs/IMPLEMENTATION_REVIEW.md: [L3] test_reflection_compaction.cpp 标记为 RESOLVED

## 8. 验证

- [x] 8.1 grep 验证 docs/ 中不再出现已删除的 4 个测试文件名（仅 RESOLVED 条目）
- [x] 8.2 grep 验证 docs/ 和 AGENTS.md 中不再出现 "27个测试" 或 "20个反射"