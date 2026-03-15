## 1. TypeLayout: 添加 _RELOCATABLE 宏系列

- [x] 1.1 在 `opaque.hpp` 中添加 `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE(Type, name)` 宏（无 trivially_copyable 断言）
- [x] 1.2 在 `opaque.hpp` 中添加 `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE(Template, name)` 宏
- [x] 1.3 在 `opaque.hpp` 中添加 `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE(Template, name)` 宏

## 2. TypeLayout: SigExporter 添加 add_relocatable

- [x] 2.1 在 `sig_export.hpp` 的 `SigExporter` 类中添加 `add_relocatable<T>(name)` 方法，使用 `!layout_traits<T>::has_pointer` 检查

## 3. XOffset: 注册宏迁移到 _RELOCATABLE

- [x] 3.1 `XOFFSET_REGISTER_TYPE` 宏内部从 `TYPELAYOUT_OPAQUE_TYPE_AUTO` 改为 `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE`
- [x] 3.2 `XOFFSET_REGISTER_CONTAINER` 宏内部从 `TYPELAYOUT_OPAQUE_CONTAINER_AUTO` 改为 `TYPELAYOUT_OPAQUE_CONTAINER_RELOCATABLE`
- [x] 3.3 `XOFFSET_REGISTER_MAP` 宏内部从 `TYPELAYOUT_OPAQUE_MAP_AUTO` 改为 `TYPELAYOUT_OPAQUE_MAP_RELOCATABLE`

## 4. XOffset: Policy 层 opaque 分支

- [x] 4.1 在 `detail` 命名空间中添加 `using boost::typelayout::has_opaque_signature` 和 `using boost::typelayout::layout_traits`
- [x] 4.2 `DefaultPolicy::accept<T>()` 添加 `if constexpr (has_opaque_signature<T>)` 分支：opaque 类型仅检查 `!layout_traits<T>::has_pointer`
- [x] 4.3 `StrictPolicy::accept<T>()` 拆分 C1/C2：C2 同 DefaultPolicy（opaque 分支）+ C1 直接比较 `get_layout_signature<T>()` 与 GoldSignature

## 5. XOffset: export_signatures.cpp 修改

- [x] 5.1 将 `TYPELAYOUT_EXPORT_TYPES(Player, Item, GameData)` 替换为手写 `main()` 调用 `ex.add_relocatable<T>()`

## 6. Docker 全量构建验证

- [x] 6.1 运行 Docker 构建，所有编译目标通过（32 tests, 0 failures）
- [x] 6.2 所有测试执行通过
- [x] 6.3 验证 `is_xbuffer_safe<XVector<int32_t>>::value == true`（implicit via test_vector pass）
- [x] 6.4 验证 `is_xbuffer_safe<GameData>::value == true`（implicit via demo + test_map_set pass）

## 7. TypeLayout 上游推送

- [ ] 7.1 在 TypeLayout `feat/opaque-auto-macros` 分支上提交 _RELOCATABLE 宏和 add_relocatable 变更
- [ ] 7.2 更新 XOffset submodule 引用
