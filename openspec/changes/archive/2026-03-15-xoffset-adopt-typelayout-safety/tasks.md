**前置依赖**: `typelayout-relocatable-opaque` 提案完成并推送到 TypeLayout main

## 1. 更新 TypeLayout submodule

- [ ] 1.1 `git submodule update` 指向 TypeLayout main 最新 commit（含 _RELOCATABLE 宏）

## 2. xoffsetdatastructure.hpp: include 和 using 声明迁移

- [ ] 2.1 替换 `#include <boost/typelayout/tools/classify_safety.hpp>` 为 `classify.hpp` + `serialization_free.hpp`
- [ ] 2.2 替换 `using` 声明：`is_layout_safe` → `is_local_serialization_free_v`，`classify_safety` → `classify_v`，添加 `has_opaque_signature`、`layout_traits`
- [ ] 2.3 更新头部注释中的 API 说明

## 3. xoffsetdatastructure.hpp: DefaultPolicy + StrictPolicy 重写

- [ ] 3.1 添加 `accept_all_members_impl` 和 `accept_all_bases_impl` consteval 递归辅助函数
- [ ] 3.2 添加 `opaque_element_types<T>` trait（默认实现 `all_elements_safe() = true`）
- [ ] 3.3 重写 `DefaultPolicy::accept<T>()` — 三层分支（opaque / leaf / struct 递归）
- [ ] 3.4 重写 `StrictPolicy::accept<T>()` — 复用 DefaultPolicy + 签名匹配

## 4. xoffsetdatastructure.hpp: 注册宏迁移

- [ ] 4.1 `XOFFSET_REGISTER_TYPE` 内部使用 `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE`
- [ ] 4.2 `XOFFSET_REGISTER_CONTAINER` 内部使用 `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE` + 生成 `opaque_element_types` 特化
- [ ] 4.3 `XOFFSET_REGISTER_MAP` 内部使用 `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE` + 生成 `opaque_element_types` 特化

## 5. xoffsetdatastructure.hpp: 诊断和消息更新

- [ ] 5.1 移除 "Union type not allowed" 诊断
- [ ] 5.2 更新多态类型诊断消息为 "contains vtable pointer"
- [ ] 5.3 更新 `validate_xbuffer_type` 的帮助消息（移除 union 限制）

## 6. 全局 API 替换

- [ ] 6.1 `get_definition_signature` → `get_layout_signature`（所有 .cpp/.hpp 文件）
- [ ] 6.2 `definition_signatures_match` → `layout_signatures_match`
- [ ] 6.3 `is_fixed_enum` → `std::is_enum_v`
- [ ] 6.4 `SignatureMode::Definition` → 删除相关引用

## 7. examples 修复

- [ ] 7.1 移除 `player.hpp` 和 `game_data.hpp` 中硬编码的签名 static_assert
- [ ] 7.2 修复 `demo.cpp` 和 `helloworld.cpp` 中的 API 引用

## 8. tests 修复

- [ ] 8.1 更新 `test_policy_trait.cpp` 适配新安全模型
- [ ] 8.2 更新 `test_remediation_fixes.cpp` — union/long/wchar_t accepted
- [ ] 8.3 更新 `test_type_safety_comprehensive.cpp` — long/unsigned long accepted
- [ ] 8.4 更新 `test_typelayout_integration.cpp` — 移除 SignatureMode, 修复 Point==Coord 逻辑
- [ ] 8.5 更新 `test_enum_support.cpp` — is_fixed_enum → is_enum_v
- [ ] 8.6 移除 `test_classify_safety` 编译目标（tests/CMakeLists.txt）

## 9. tools 修复

- [ ] 9.1 重写 `export_signatures.cpp` — 使用 `add_relocatable<T>()`
- [ ] 9.2 禁用 `check_compat.cpp`（tools/CMakeLists.txt）

## 10. Docker 全量构建验证

- [ ] 10.1 运行 Docker 构建，所有编译目标通过
- [ ] 10.2 全部 32 个测试 PASSED
