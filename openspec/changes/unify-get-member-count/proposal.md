# Change: 统一 get_member_count 实现，消除冗余代码

## Why
XOffsetDatastructure 内部有两个功能完全相同的成员计数函数：
1. `XBufferCompactor` 中的 `get_member_count_impl<T>()`（私有方法）
2. `detail` 命名空间中的 `get_safe_member_count<T>()`

这两者都等价于 TypeLayout 已提供的 `boost::typelayout::get_member_count<T>()`。三处实现使用完全相同的 P2996 调用：`nonstatic_data_members_of(^^T, access_context::unchecked()).size()`。

应统一使用 TypeLayout 版本，减少重复代码。

## What Changes
- 删除 `XBufferCompactor::get_member_count_impl<T>()`，替换为 `boost::typelayout::get_member_count<T>()`
- 删除 `detail::get_safe_member_count<T>()`，替换为 `boost::typelayout::get_member_count<T>()`
- 验证所有调用点行为不变

## Impact
- Affected specs: 无（纯内部重构）
- Affected code:
  - `xoffsetdatastructure2.hpp`：删除 2 个函数定义，更新 ~4 个调用点
- 风险：极低（纯 consteval 函数，编译时验证）