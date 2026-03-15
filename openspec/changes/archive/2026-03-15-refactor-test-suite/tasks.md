## 1. 删除孤立和完全覆盖的文件

- [x] 1.1 删除 `tests/run_all_tests.cpp`（引用不存在的函数，从未被 CMakeLists.txt 注册）
- [x] 1.2 删除 `tests/test_classify_safety.cpp`（引用已删除的 `classify_safety.hpp` API）
- [x] 1.3 删除 `tests/test_long_reflection_probe.cpp`（一次性探测脚本，不在 CMakeLists.txt 中）
- [x] 1.4 删除 `tests/test_type_safety.cpp`（100% 被 `test_type_safety_comprehensive.cpp` 覆盖）
- [x] 1.5 删除 `tests/test_vptr_layout.cpp`（诊断脚本，功能已被其他测试覆盖）
- [x] 1.6 删除 `tests/test_type_erased_detection.cpp`（Domain S 自然拒绝，无需专用测试）
- [x] 1.7 删除 `tests/test_polymorphic_rejection.cpp`（负编译测试已被 static_assert 覆盖）

## 2. 合并反射基础测试 → `test_reflection_core.cpp`

- [x] 2.1 读取 `test_reflection_operators.cpp`、`test_member_iteration.cpp`、`test_splice_operations.cpp` 的全部测试函数和测试结构体
- [x] 2.2 创建 `tests/test_reflection_core.cpp`，合并三个文件的测试函数，去重共享结构体，确保 ≤ 300 行
- [x] 2.3 删除 `tests/test_reflection_operators.cpp`
- [x] 2.4 删除 `tests/test_member_iteration.cpp`
- [x] 2.5 删除 `tests/test_splice_operations.cpp`

## 3. 合并反射高级测试 → `test_reflection_advanced.cpp`

- [x] 3.1 读取 `test_reflection_serialization.cpp` 和 `test_reflection_comparison.cpp` 的全部测试函数
- [x] 3.2 创建 `tests/test_reflection_advanced.cpp`，合并两个文件，去重结构体，确保 ≤ 300 行
- [x] 3.3 删除 `tests/test_reflection_serialization.cpp`
- [x] 3.4 删除 `tests/test_reflection_comparison.cpp`

## 4. 合并签名测试 → `test_type_signatures.cpp`

- [x] 4.1 读取 `test_reflection_type_signature.cpp` 和 `test_class_type_signatures.cpp` 的全部测试函数
- [x] 4.2 创建 `tests/test_type_signatures.cpp`，合并两个文件，去重结构体，确保 ≤ 300 行
- [x] 4.3 删除 `tests/test_reflection_type_signature.cpp`
- [x] 4.4 删除 `tests/test_class_type_signatures.cpp`

## 5. 重命名

- [x] 5.1 将 `tests/test_type_safety_comprehensive.cpp` 重命名为 `tests/test_type_safety.cpp`（旧同名文件已在 Task 1.4 中删除）

## 6. 更新 CMakeLists.txt

- [x] 6.1 从 `REFLECTION_TESTS` 列表中移除已删除/合并前的旧名称：`test_reflection_operators`、`test_member_iteration`、`test_splice_operations`、`test_reflection_serialization`、`test_reflection_comparison`、`test_reflection_type_signature`、`test_class_type_signatures`、`test_type_safety`、`test_type_safety_comprehensive`、`test_vptr_layout`、`test_type_erased_detection`
- [x] 6.2 添加新名称到 `REFLECTION_TESTS`：`test_reflection_core`、`test_reflection_advanced`、`test_type_signatures`、`test_type_safety`
- [x] 6.3 更新 `TOTAL_TEST_COUNT` 注释（Basic 7 + Reflection 20 = 27）
- [x] 6.4 验证 `try_compile` 块无需修改（`if(EXISTS ...)` 守护自动跳过已删除文件）

## 7. 更新文档

- [x] 7.1 更新 `tests/README.md` 中的测试文件列表，反映新的 27 个测试结构

## 8. 验证

- [x] 8.1 在 Docker 中运行 `bash ./build.sh`，确认所有 27 个测试编译通过且零失败
- [x] 8.2 检查合并后的 3 个新文件行数均 ≤ 300 行
- [x] 8.3 确认 `tests/` 目录中不存在未被 CMakeLists.txt 注册的 `.cpp` 文件（`test_polymorphic_rejection.cpp` 除外）