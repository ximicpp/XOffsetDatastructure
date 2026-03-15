## Why

核心头文件 `xoffsetdatastructure.hpp` 经过多轮开发累积了未使用的代码（空命名空间块、
未使用的 include/concept/alias、仅为 stdout 引入的 `<iostream>`）。清理这些冗余
提升代码干净度，减少编译依赖。

## What Changes

1. 删除空的 `namespace XOffsetDatastructure {}` 块 (L64-66)
2. 删除未使用的 `#include <tuple>`
3. 删除未使用的 `SupportedContainer` concept
4. 删除未使用的 `XOffsetPtr<T>` alias（保留错误消息中的文字引用）
5. 移除 `XBufferStats::print()`（库头文件不应 `std::cout`），从而可移除 `#include <iostream>` 和 `#include <iomanip>`
6. 内联 `HasIterator`/`HasValueType`/`HasMappedType`/`HasKeyType` 到使用处

注意：`XBufferCoreSeqFit` 和 `x_seq_fit` 保留。

## Capabilities

_无新增/修改能力。纯清理。_

## Impact

- **代码**: 仅修改 `xoffsetdatastructure.hpp`
- **测试**: 可能需要更新使用 `XBufferStats::print` 或 `XOffsetPtr` 的测试
- **风险**: 低 — Docker 构建验证
