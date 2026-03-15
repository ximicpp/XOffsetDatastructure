## 1. TypeLayout: classify_safety Bug 修复与 constexpr 升级

- [x] 1.1 在 `compat_check.hpp` 的 `classify_safety(string_view)` 中添加 `union[` 检测（在 `vptr` 检测之后）
- [x] 1.2 将 `classify_safety(string_view)` 从 `inline` 改为 `inline constexpr`
- [x] 1.3 验证 consteval 版和 runtime 版 classify_safety 的标记检测列表完全一致

## 2. TypeLayout: all_serialization_free constexpr API

- [x] 2.1 在 `compat_auto.hpp` 中新增 `all_serialization_free(const PlatformInfo&, const PlatformInfo&)` constexpr 函数，实现 C1+C2 联合判定
- [x] 2.2 新增 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE(first, ...)` 宏，使用现有 `TYPELAYOUT_DETAIL_FOR_EACH_CTX` 展开 pairwise static_assert

## 3. TypeLayout: CompatReporter 运行期 ZST helpers

- [x] 3.1 在 `CompatReporter` 中新增 `all_serialization_free()` 方法（遍历 compare() 结果）
- [x] 3.2 在 `CompatReporter` 中新增 `is_type_serialization_free(const std::string& name)` 方法

## 4. TypeLayout: 测试

- [ ] 4.1 编写测试验证 `classify_safety(string_view)` 对含 `union[` 签名返回 Warning (deferred to TypeLayout repo)
- [ ] 4.2 编写测试验证 `all_serialization_free` 对各种输入组合 (deferred to TypeLayout repo)
- [ ] 4.3 编写测试验证 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE` 宏展开 (deferred to TypeLayout repo)
- [ ] 4.4 编写测试验证 `CompatReporter` helpers (deferred to TypeLayout repo)

## 5. XOffset: 删除自研 serialization-free 逻辑

- [x] 5.1 删除 `xoffsetdatastructure.hpp` 中的 `classify_for_xoffset<T>()` 函数
- [x] 5.2 删除 `using boost::typelayout::compat::is_serialization_free_local` ghost symbol
- [x] 5.3 更新 `DefaultPolicy::accept<T>()` 直接调用 `is_layout_safe<std::remove_cv_t<T>>()`
- [x] 5.4 更新 `StrictPolicy<Gold>::accept<T>()` 使用 `get_layout_signature<T>() == Gold && is_layout_safe<std::remove_cv_t<T>>()`

## 6. XOffset: 删除冗余 ArchSpec

- [x] 6.1 删除 `ArchSpec` 结构体、`Arch64LE` 常量、`TargetArchitecture` 常量
- [x] 6.2 删除 15 个 `static_assert`（sizeof/alignof 冗余检查）
- [x] 6.3 保留 preprocessor 层面 `XOFFSET_64BIT_CHECK` 和 `XOFFSET_LITTLE_ENDIAN` 门控不变

## 7. XOffset: 更新诊断与注释

- [x] 7.1 更新 `get_safety_error_message<T>()` 删除 `long` 专用分支
- [x] 7.2 更新头部注释，移除对 `is_serialization_free_local` 的引用，引用 TypeLayout ZST API
- [x] 7.3 在 Type Safety Architecture 区域添加注释说明 C2 委托给 TypeLayout 的设计意图

## 8. XOffset: 测试适配

- [x] 8.1 更新 `test_policy_trait.cpp` 中的 `test_classify_levels()` — 删除 `classify_for_xoffset` 断言，改为 `classify_safety` + `is_layout_safe` 验证
- [x] 8.2 更新 `test_policy_trait.cpp` 中的 `SmallTypePolicy` — `classify_for_xoffset<T>() == Safe` 改为 `is_layout_safe<T>()`
- [x] 8.3 更新 `test_remediation_fixes.cpp` 中的 Warning→Risk 升级测试 — 改为直接验证 `is_layout_safe<PolyBase>() == false`
- [x] 8.4 更新 `test_remediation_fixes.cpp` 中的 long rejection 测试 — 重写为跨平台签名差异验证或删除

## 9. 集成验证

- [x] 9.1 更新 `external/typelayout` submodule ref — pushed to `feat/xoffset-zst-delegation` branch (5e75364), XOffset committed as 844e7eb7
- [x] 9.2 Docker build 全量测试通过 (`./build.sh`) — 32/32 tests passed ✅
- [x] 9.3 验证 `is_xbuffer_safe<T>::value` 对标准测试类型集合的结果与重构前一致 (verified via test suite)
