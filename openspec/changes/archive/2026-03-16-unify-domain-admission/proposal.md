## Why

XOffset 内部的域准入逻辑（`DefaultPolicy`、`StrictPolicy`、`opaque_element_types`、
`accept_all_members_impl` 等）与 TypeLayout 最新版提供的 `is_byte_copy_safe_v<T>`
递归准入谓词完全重复。TypeLayout 现在提供统一的编译时谓词，决策树与 XOffset 的
`DefaultPolicy::accept<T>()` 完全等价。

## What Changes

- **删除** `DefaultPolicy`、`StrictPolicy`、`accept_all_members_impl`、`accept_all_bases_impl`、
  `opaque_element_types`、`diagnose_unsafe_members`、`is_xbuffer_compatible` 全部代码
- **替换** `is_xbuffer_safe<T>::value` 底层实现为 `boost::typelayout::is_byte_copy_safe_v<T>`
- **删除** 注册宏中的 `opaque_element_types` 特化（TypeLayout 宏自动生成 `opaque_copy_safe`）
- **重写** `test_policy_trait.cpp` 使用 `is_byte_copy_safe_v`
- **升级** TypeLayout 子模块到 `origin/main` 最新 (`af59c7f`)

## Capabilities

### Modified Capabilities
- `type-safety-delegation`: 域准入逻辑完全委托给 TypeLayout

## Impact

- **文件**: `xoffsetdatastructure.hpp` (净减约 120 行), `test_policy_trait.cpp`, `test_type_safety.cpp`, `tests/README.md`
- **API**: `is_xbuffer_safe<T>` 保留，底层改用 `is_byte_copy_safe_v`。`DefaultPolicy`、`StrictPolicy`、`diagnose_unsafe_members`、`is_xbuffer_compatible` 已删除。
- **构建**: 23/23 测试通过
