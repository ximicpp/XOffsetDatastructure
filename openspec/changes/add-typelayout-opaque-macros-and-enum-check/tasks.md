## 1. TypeLayout: Opaque 容器特化宏
- [ ] 1.1 创建 `external/typelayout/include/boost/typelayout/core/opaque.hpp`
- [ ] 1.2 实现 `TYPELAYOUT_OPAQUE_TYPE(Type, name, size, align)` 宏
- [ ] 1.3 实现 `TYPELAYOUT_OPAQUE_CONTAINER(Template, name, size, align)` 宏
- [ ] 1.4 实现 `TYPELAYOUT_OPAQUE_MAP(Template, name, size, align)` 宏
- [ ] 1.5 在 `typelayout.hpp` 中 include `opaque.hpp`

## 2. TypeLayout: 枚举安全检查
- [ ] 2.1 在 `core/signature_detail.hpp` 中添加 `is_fixed_enum<T>()` consteval 函数
- [ ] 2.2 确保对 scoped enum 和显式底层类型的 unscoped enum 返回 true

## 3. XOffsetDatastructure: 使用新宏
- [ ] 3.1 用 4 行宏调用替换 `xoffsetdatastructure2.hpp` 底部 35 行手写特化
- [ ] 3.2 验证替换后签名输出完全等价（static_assert 不变）

## 4. XOffsetDatastructure: 枚举支持
- [ ] 4.1 在 `detail::is_safe_type<T>()` 中添加枚举检测分支
- [ ] 4.2 创建 `tests/test_enum_support.cpp` 测试枚举类型
- [ ] 4.3 更新 `tests/CMakeLists.txt`

## 5. 验证
- [ ] 5.1 在 Docker 中构建并运行所有测试
- [ ] 5.2 确认现有 21 个测试全部通过（签名不变）
- [ ] 5.3 确认新的枚举测试通过
