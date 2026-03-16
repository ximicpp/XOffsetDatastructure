## Why

TypeLayout 子模块停留在旧 commit (`99b78ec`，`v0.2.0-structural` 分支)，
落后 `origin/main` 约 20 个 commit。`xoffsetdatastructure.hpp` 中的代码和注释
是面向 main 最新 API 编写的（`layout_traits<T>`、`classify_v<T>`、
`is_local_serialization_free_v<T>`、`TYPELAYOUT_OPAQUE_*_RELOCATABLE` 等），
但子模块版本尚未提供这些符号，导致：

1. 三个 `#include` 指向当前子模块中不存在的头文件（`serialization_free.hpp`、`classify.hpp`）
2. 六个 `using` 声明引用当前子模块中不存在的符号
3. 注册宏 `TYPELAYOUT_OPAQUE_*_RELOCATABLE` 在当前子模块中未定义

同时，部分注释引用了一个从未存在过的宏名 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE`，
以及两个紧邻的注释块内容重复，可以合并精简。

## What Changes

- **升级 TypeLayout 子模块** 至 `origin/main` 最新 commit（`6319750`），
  使 `layout_traits.hpp`、`serialization_free.hpp`、`classify.hpp`、
  `safety_level.hpp`、`RELOCATABLE` 宏等全部可用
- **修正过时注释**：将 `TYPELAYOUT_ASSERT_SERIALIZATION_FREE` 引用替换为
  实际 API 名称 `serialization_free_assert<T>`
- **合并重复注释块**：第 45-65 行区域有两个紧邻的注释块说的近乎同一件事
  （平台验证 + TypeLayout 委托），合并为一个简洁的块
- **删除冗余注释**：移除与代码不一致的过时描述

## Capabilities

### New Capabilities

（无新能力引入）

### Modified Capabilities

- `type-signature`: 子模块版本约束从旧 commit 升级到 main 最新；spec 中
  "分支: 跟踪 `main` 分支" 的要求现在被实际满足

## Impact

- **文件**: `xoffsetdatastructure.hpp`（注释修改）、`external/typelayout`（子模块指针）
- **依赖**: TypeLayout 子模块从 `99b78ec` → `origin/main` HEAD
- **构建**: 升级后所有 `#include` 和 `using` 声明应能正确解析；需 Docker 构建验证 23/23 测试通过
- **API**: 无公共 API 变更；仅注释和子模块版本
- **风险**: TypeLayout main 上的 API 可能有细微签名变化，需要构建验证确认兼容性
