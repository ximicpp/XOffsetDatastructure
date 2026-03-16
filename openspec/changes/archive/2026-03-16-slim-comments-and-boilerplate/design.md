## Context

1815 行头文件经过 Type Safety 精简（312→169 行），其他区域注释密度仍高。本次覆盖剩余所有区域的注释和样板精简。

## Goals / Non-Goals

**Goals:**
- 将头文件从 1815 行缩减到 ~1715 行
- 保持所有公共 API 签名和行为完全不变

**Non-Goals:**
- 不修改任何逻辑代码
- 不重构同构递归（Construct/Transfer/Migrate 统一属于更大范围的变更）
- 不拆分头文件

## Decisions

1. **注释精简原则**：保留 "是什么 + 为什么" 的单行注释，删除 "怎么用" 的多行解释（用户应查阅文档/示例）
2. **Section banner 统一风格**：所有 `// ====` banner 缩减为 2 行（标题 + 一句话描述）
3. **内联辅助函数**：`reflect_member_count_of<T>()` 和 `reflect_base_count_of<T>()` 只用于 `reflect_init_all_impl` 和 `reflect_transfer_init_all_impl`，直接在使用处内联

## Risks / Trade-offs

- [注释删除] 新开发者初次阅读可能需要更多参考文档 → 关键 "为什么" 注释保留
- [辅助函数内联] 如果未来更多地方需要 member/base count，需要重新提取 → 当前只有 2 处使用，内联更简洁