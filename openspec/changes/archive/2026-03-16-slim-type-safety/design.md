## Context

`xoffsetdatastructure.hpp` 的 Type Safety 区域（第 759-1070 行，312 行）中，核心判定逻辑仅 68 行，其余 244 行为诊断函数和注释。本次纯粹精简诊断/文档代码，不改变任何判定行为。

## Goals / Non-Goals

**Goals:**
- 将 Type Safety 区域从 312 行缩减到 ~230 行
- 保持所有公共 API 签名和行为完全不变

**Non-Goals:**
- 不修改 `DefaultPolicy::accept` 的判定逻辑
- 不修改 `accept_all_members_impl` / `accept_all_bases_impl` 的递归逻辑
- 不重构 Reflect Construct / Transfer / Migrate 的同构递归（属于更大范围的重构）

## Decisions

1. **`validate_xbuffer_type` 的 static_assert 消息精简**
   - 当前：38 行 ASCII art + 完整类型列表
   - 改为：5-8 行核心信息 + 指引用户调用 `diagnose_unsafe_members<T>()`
   - 理由：编译器错误窗口有限，巨型字符串反而淹没有用信息

2. **`get_safety_error_message` 分支合并**
   - 当前：9 个 if-else 分支（29 行）
   - 改为：合并 `std::string` 和 `std container` 分支，压缩到 ~15 行
   - 理由：这些分支可以用更简洁的 constexpr 表达

3. **`diagnose_base_at` + `diagnose_member_at` 合并**
   - 当前：两个函数体几乎相同，只是反射 API 不同（`bases_of` vs `nonstatic_data_members_of`）
   - 改为：保留两个独立函数（P2996 反射的 splice 语法要求不同的 meta 调用，无法泛化为一个模板）
   - 但合并 `diagnose_bases_impl` + `diagnose_members_impl` 的 fold expression 为内联调用
   - 理由：减少层级，不影响编译器诊断质量

4. **注释精简策略**
   - 保留：Safety Responsibility Model 说明、Branch 1/2/3/4 单行注释
   - 删除：重复解释 TypeLayout 职责的段落、已在 proposal 中记录的设计理由

## Risks / Trade-offs

- [编译错误可读性] 精简后的 static_assert 消息更短 → 用户需要额外调用 `diagnose_unsafe_members` 获取详细信息。但 `validate_xbuffer_type` 已经自动调用了 `diagnose_unsafe_members`，所以实际上用户体验不变。
- [注释删除] 删除部分注释可能降低新开发者的理解速度 → 关键架构注释保留，细节解释迁移到 `docs/technical_overview.md`（标记 `[needs-docs]`）。