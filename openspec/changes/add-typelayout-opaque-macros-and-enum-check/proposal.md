# Change: 在 TypeLayout 中添加 Opaque 容器宏和枚举安全检查

## Why

`analyze-typelayout-feature-gaps` 分析确认了 2 个需要在 TypeLayout 库中添加的功能：
1. **Opaque 容器特化辅助宏**：XOffsetDatastructure 为 4 个容器手写了 35 行高度重复的 TypeSignature 特化
2. **枚举 Trivial Safety 检查**：XOffsetDatastructure 的 is_safe_type 未处理枚举，但游戏数据中枚举很常见

## What Changes

### TypeLayout 库修改（`external/typelayout/`）
1. 新增 `include/boost/typelayout/core/opaque.hpp`：定义 3 个辅助宏
   - `TYPELAYOUT_OPAQUE_TYPE(Type, name, size, align)` — 无元素类型
   - `TYPELAYOUT_OPAQUE_CONTAINER(Template, name, size, align)` — 1 个类型参数
   - `TYPELAYOUT_OPAQUE_MAP(Template, name, size, align)` — 2 个类型参数
2. 在 `core/signature_detail.hpp` 或新文件中添加 `is_fixed_enum<T>()`
3. 更新 `typelayout.hpp` include 新头文件

### XOffsetDatastructure 修改
1. 用宏替换 `xoffsetdatastructure2.hpp` 底部的 4 个手动特化（35 行 → 4 行）
2. 在 `detail::is_safe_type<T>()` 中添加枚举支持
3. 添加枚举类型测试

## Impact
- Affected specs: `type-signature`
- Affected code: `external/typelayout/include/`, `xoffsetdatastructure2.hpp`, tests
- **BREAKING**: 无。宏生成的特化代码与当前手写代码完全等价
