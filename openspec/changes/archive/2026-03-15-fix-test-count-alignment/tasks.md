## 1. 修复 build.sh 测试分类和计数

- [x] 1.1 将 `test_xbuffer_api` 的 `run_test` 调用从反射测试区移至基础测试区（第 7 个）
- [x] 1.2 将非反射模式 `TOTAL_TESTS` 从 6 改为 7
- [x] 1.3 重新编排反射测试区的 `run_test` 编号（8-23）
- [x] 1.4 更新基础测试区注释为 `# Basic tests (7 tests)`
- [x] 1.5 更新反射测试区注释中的测试数量为 16

## 2. 修复 AGENTS.md 测试计数

- [x] 2.1 更新 Role: tests 中的 "Test suite summary" 部分：basic 7, reflection 16, total 23（已确认：AGENTS.md 已是最新，无需修改）

## 3. 验证

- [x] 3.1 确认 build.sh、CMakeLists.txt、tests/README.md、AGENTS.md 四个数据源的测试计数一致（7 basic + 16 reflection = 23）
