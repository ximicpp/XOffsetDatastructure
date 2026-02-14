# Change: Rename Public API — Improve Naming Conventions

## Summary
分析并重构 XOffsetDatastructure 库中所有公开类型的命名，消除语义模糊、层级倒置和角色混淆问题。

## Motivation

当前命名体系存在以下问题：

### P1: XBuffer vs XBufferExt — 层级倒置（严重）
- `XBuffer` 是底层内存管理别名，用户几乎不应直接使用
- `XBufferExt` 是用户应该使用的主接口（make/root/save/load）
- **问题**：`Ext` 后缀暗示"扩展/可选"，但实际上它才是主 API
- **结果**：新用户会错误地优先使用 `XBuffer`

### P2: XBufferCompactor — 工具类命名不统一
- 纯 static 方法的工具类，使用了 `-or` 动作者名词
- 与 `XBufferVisualizer` 风格一致但都不够好
- 用户调用 `XBufferCompactor::compact_automatic<T>(buf)` — 冗长

### P3: XBufferVisualizer — 同上
- 只有两个 static 方法（`get_memory_stats`, `print_stats`）
- 功能简单却有一个重量级的类名

### P4: XManagedMemory — 内部类名暴露
- 这是 `boost::interprocess` 命名空间下的底层模板类
- 用户不应直接使用，但名称与 `X` 前缀风格一致，容易混淆

### P5: XHandle — 无问题但需评估
- 命名语义清晰，`Handle` 是常见的 C++ 模式
- 保持不变

### P6: XOffsetPtr — 无问题
- 直接映射 `boost::interprocess::offset_ptr`
- 命名清晰，保持不变

## Proposal

### 重命名方案

| 当前名称 | 新名称 | 理由 |
|----------|--------|------|
| `XBufferExt` | **`XBuffer`** | 这是用户主接口，应该拥有最简名称 |
| `XBuffer` (原) | **`XBufferCore`** | 底层内存管理，Core 后缀表明"内部核心" |
| `XBufferBestFit` | **`XBufferCoreBestFit`** | 对应 Core 重命名 |
| `XBufferCompactor` | **`XCompactor`** | 简化，去掉冗余 Buffer 前缀 |
| `XBufferVisualizer` | **`XBufferStats`** | 功能是统计信息，不是"可视化器" |
| `XManagedMemory` | 保持不变 | boost::interprocess 命名空间下的内部实现 |
| `XHandle` | 保持不变 | 命名清晰 |
| `XOffsetPtr` | 保持不变 | 命名清晰 |
| `XVector` | 保持不变 | 命名清晰 |
| `XMap` | 保持不变 | 命名清晰 |
| `XSet` | 保持不变 | 命名清晰 |
| `XString` | 保持不变 | 命名清晰 |
| `XAllocator` | 保持不变 | 命名清晰 |

### 兼容性策略

使用 `[[deprecated]]` 类型别名保持向后兼容：

```cpp
// Deprecated aliases for backward compatibility
using XBufferExt [[deprecated("Use XBuffer instead")]] = XBuffer;
// 原来的 XBuffer 用户（如果有）会报错，但这类用户极少
```

### 变更范围

1. **`xoffsetdatastructure.hpp`** — 核心类型重命名
2. **`tests/*.cpp`** — 全部 25 个测试文件更新
3. **`examples/*.cpp`** — 示例文件更新
4. **`tests/CMakeLists.txt`** — 测试名称更新（如 `XBufferExtAPI` → `XBufferAPI`）
5. **`docs/`** — 文档引用更新

### 不变更范围

- `XManagedMemory` — 在 `boost::interprocess` 命名空间内，不属于公共 API
- `XOFFSET_REGISTER_*` 宏 — 内部引用 `XCompactor` 会更新，但宏接口不变
- `external/` — 外部依赖不修改

## Risk Assessment

- **Breaking change**: `XBufferExt` → `XBuffer` 和 `XBuffer` → `XBufferCore` 是 API 破坏性变更
- **Mitigation**: 提供 deprecated 别名；当前仅内部使用，无外部用户
- **测试**: 全部 25 个测试必须通过
