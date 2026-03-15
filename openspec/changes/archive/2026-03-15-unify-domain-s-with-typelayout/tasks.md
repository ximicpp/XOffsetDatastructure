## 1. TypeLayout 远端: OPAQUE_*_AUTO 宏

- [x] 1.1 基于远端 `origin/main` 创建 `feat/opaque-auto-macros` 分支
- [x] 1.2 在 `opaque.hpp` 中添加 `TYPELAYOUT_OPAQUE_TYPE_AUTO(Type, name)` 宏
- [x] 1.3 在 `opaque.hpp` 中添加 `TYPELAYOUT_OPAQUE_CONTAINER_AUTO(Template, name)` 宏
- [x] 1.4 在 `opaque.hpp` 中添加 `TYPELAYOUT_OPAQUE_MAP_AUTO(Template, name)` 宏
- [x] 1.5 验证 deferred to integration test (Task 9)
- [x] 1.6 Push `feat/opaque-auto-macros` to TypeLayout remote

## 2. XOffset: 更新 submodule

- [x] 2.1 submodule 指向 `feat/opaque-auto-macros` 分支 (3cd70ec)
- [x] 2.2 更新 includes: `serialization_free.hpp` + `classify.hpp`

## 3. XOffset: 删除自建类型分类逻辑

- [x] 3.1 删除旧三级 SafetyLevel 和 classify_safety/is_layout_safe using declarations
- [x] 3.2 同上（合并到 3.1）
- [x] 3.3 ArchSpec 已在上一轮变更中删除
- [x] 3.4 is_safe_leaf 已在上一轮变更中删除
- [x] 3.5 递归安全检查函数已在上一轮变更中删除
- [x] 3.6 is_safe_type 已在上一轮变更中删除
- [x] 3.7 删除 classify_safety.hpp include，替换为 serialization_free.hpp + classify.hpp

## 4. XOffset: 重写 Policy 层

- [x] 4.1 DefaultPolicy::accept → `is_local_serialization_free_v<T>`
- [x] 4.2 StrictPolicy::accept → `is_transfer_safe<T>(GoldSignature)`
- [x] 4.3 添加 TypeLayout includes
- [x] 4.4 is_xbuffer_compatible 无需修改（已委托给 Policy::accept）

## 5. XOffset: 更新 opaque 容器注册

- [x] 5.1 XOFFSET_REGISTER_TYPE 已使用 TYPELAYOUT_OPAQUE_TYPE_AUTO（兼容远端）
- [x] 5.2 XOFFSET_REGISTER_CONTAINER 已使用 TYPELAYOUT_OPAQUE_CONTAINER_AUTO
- [x] 5.3 XOFFSET_REGISTER_MAP 已使用 TYPELAYOUT_OPAQUE_MAP_AUTO
- [ ] 5.4 验证 `is_local_serialization_free_v<XVector<int32_t>>` 编译期返回 true (deferred to build)

## 6. XOffset: 更新诊断与错误消息

- [x] 6.1 get_safety_error_message 更新（移除 union 分支，改进 polymorphic 消息）
- [x] 6.2 validate_xbuffer_type 移除 "Union types" 排除提示
- [x] 6.3 NOT ALLOWED 列表更新

## 7. XOffset: 更新测试

- [ ] 7.1 更新 `test_policy_trait.cpp`：SafetyLevel 三级→五级，`classify_for_xoffset` → `classify_v`
- [ ] 7.2 更新 `test_remediation_fixes.cpp`：union/long/wchar_t 从"拒绝"改为"接受"
- [ ] 7.3 更新或删除 `test_classify_safety.cpp`：移除对不存在的 `classify_safety<T>()` 的依赖
- [ ] 7.4 更新 `test_type_safety_comprehensive.cpp`：适配新的准入标准（union/long 等放行）
- [ ] 7.5 添加新测试验证 `is_local_serialization_free_v` 和 `is_transfer_safe` 的集成

## 8. 文档更新

- [ ] 8.1 更新 `docs/CORE_FORMAL_MODEL.md` §3 Domain S 定义：从 S₀ 枚举改为 `is_local_serialization_free_v<T>` 委托
- [ ] 8.2 更新 §3.2 排除规则表：移除 union 行，long/wchar_t/long double 标注为"由 C1 签名比较保障"
- [ ] 8.3 更新 `xoffsetdatastructure.hpp` 头部注释（Policy 描述、Domain S 描述）
- [ ] 8.4 更新 README.md 的 Formal Correctness Model 部分

## 9. 集成验证

- [ ] 9.1 Docker 全量构建测试通过（`./build.sh`，所有测试 pass）
- [ ] 9.2 验证 `is_xbuffer_safe<T>::value` 对以下类型返回 true：`int32_t`、`union { int a; float b; }`、`struct { long x; }`、`struct { wchar_t w; }`、`XVector<int32_t>`
- [ ] 9.3 验证 `is_xbuffer_safe<T>::value` 对以下类型返回 false：`struct { int* p; }`、polymorphic class、`std::string`
- [ ] 9.4 Push TypeLayout 和 XOffset 变更