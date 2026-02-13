# Change: Fix compact_automatic Return Type & Related Items

## Why
`analyze-core-design-review` 中 F1/F7/F9 三个问题因结构性风险延期，需要单独处理。

核心问题：`XBufferCompactor::compact_automatic<T>()` 返回 `XBuffer`（基类），
用户无法使用 `root<T>()`、`save_to_string()` 等 `XBufferExt` 便利方法，
被迫手动创建 wrapper（如 helloworld.cpp 和 demo.cpp 中所示）。

技术难点：`XBufferCompactor` 定义在 `XBufferExt` 之前，直接改返回类型需要
重构类声明顺序或引入前向声明。

## What Changes
- F1: `compact_automatic` 返回 `XBufferExt` 而非 `XBuffer`
- F7: 在 demo.cpp 中添加 XHandle 使用演示
- F9: 更新 README compact_automatic 相关说明
- 清理 helloworld.cpp / demo.cpp 中的 `XBufferExt` wrapper hack

## Impact
- Affected specs: `examples`
- Affected code: `xoffsetdatastructure.hpp`, `examples/helloworld.cpp`, `examples/demo.cpp`, `README.md`
