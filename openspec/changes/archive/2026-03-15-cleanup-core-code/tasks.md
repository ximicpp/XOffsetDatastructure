## 1. 清理未使用代码

- [x] 1.1 删除空的 `namespace XOffsetDatastructure {}` 块 (L64-66)
- [x] 1.2 删除 `#include <tuple>` (L32)
- [x] 1.3 删除 `SupportedContainer` concept (L412)
- [x] 1.4 删除 `XOffsetPtr<T>` alias (L415)，更新 `get_safety_error_message` 中引用它的错误消息

## 2. 移除 XBufferStats::print 及其 include 依赖

- [x] 2.1 移除 `XBufferStats::print()` 方法
- [x] 2.2 移除 `#include <iostream>` (L28)
- [x] 2.3 移除 `#include <iomanip>` (L30)
- [x] 2.4 检查测试中是否有 `XBufferStats::print` 调用 — 无，无需修改

## 3. 内联中间 concepts

- [x] 3.1 将 `HasIterator`/`HasValueType`/`HasMappedType`/`HasKeyType` 内联到 `SetLikeContainer`/`MapLikeContainer`/`SequentialContainer` 定义中，删除独立定义

## 4. 修复级联影响

- [x] 4.1 修复 `examples/helloworld.cpp` — 添加 `#include <iomanip>`（之前依赖核心头文件间接引入）
- [x] 4.2 修复 CMakeLists.txt — 移除 4 个源文件缺失的测试引用
- [x] 4.3 修复 build.sh — 同步测试列表（27 → 23）

## 5. 验证

- [x] 5.1 Docker 构建通过（23/23 测试） [needs-tests]