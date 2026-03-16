## 1. 升级 TypeLayout 子模块

- [x] 1.1 [core] `cd external/typelayout && git fetch origin && git checkout origin/main`，然后 `cd ../.. && git add external/typelayout`
- [x] 1.2 [core] 验证 `external/typelayout/include/boost/typelayout/layout_traits.hpp` 存在
- [x] 1.3 [core] 验证 `external/typelayout/include/boost/typelayout/tools/serialization_free.hpp` 存在
- [x] 1.4 [core] 验证 `external/typelayout/include/boost/typelayout/tools/classify.hpp` 存在
- [x] 1.5 [core] 验证 `TYPELAYOUT_OPAQUE_TYPE_RELOCATABLE` 宏在 `opaque.hpp` 中已定义

## 2. 清理过时注释

- [x] 2.1 [core] 将第 64 行 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE` 替换为 `serialization_free_assert<T>`
- [x] 2.2 [core] 合并第 45-65 行的两个重复注释块为一个简洁的块，删除第 61-65 行的重复内容

## 3. 构建验证

- [x] 3.1 [core] Docker 构建验证 23/23 测试通过
- [x] 3.2 [core] 确认无编译警告涉及 TypeLayout 头文件

## 4. 跨 Agent 协调

- [ ] 4.1 [needs-docs] 如果子模块版本信息出现在文档中，更新相关文档
