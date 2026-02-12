# Change: Fix Examples for Public User Readiness

## Why
analyze-examples-readiness 发现 6 个阻塞公开发布的问题和 4 个建议修复项。
helloworld.cpp 和 demo.cpp 作为用户第一入口，必须正确、一致、无内部实现泄漏。

## What Changes

### F1: helloworld L50 — assert+多语句合并导致 Release UB
- 改为 `if (loaded.has_root<Player>())` 安全检查

### F2/F3/F4: 消除 XBUFFER_ROOT_NAME 暴露（3 处）
- helloworld L100: 改用 XBufferExt 的 root<T>()
- demo L182: 改用 root<T>()
- demo L305: 改用 root<T>()
- **W4 关联**: compact_automatic 返回 XBuffer，需要转为 XBufferExt 才能用 root()

### F5: player.hpp 统一为新式 allocator-aware 风格
- 添加 `allocator_type`、`allocator_arg_t` 约束、move+alloc 构造函数
- 移除未使用的 full constructor (W3)

### F6: game_data.hpp GameData 统一为新式风格
- 添加 `allocator_type`、move+alloc 构造函数

### W1: 消除多语句单行合并
### W2: demo grow() 后展示指针重获取最佳实践

## Impact
- Affected: `examples/helloworld.cpp`, `examples/demo.cpp`,
  `examples/player.hpp`, `examples/game_data.hpp`
- 无 API 或 ABI 变更
- **BREAKING**: 无（仅改动示例代码和示例数据结构）
