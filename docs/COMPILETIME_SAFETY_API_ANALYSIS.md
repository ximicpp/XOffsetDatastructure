# 编译时 Safety 分级 API 设计分析

> **版本**: v1.0  
> **分析日期**: 2026-02-10  
> **状态**: 📦 **已归档** — 本文档描述的是 Safety 集成之前的旧架构  
> **替代文档**: [`TYPELAYOUT_INTEGRATION_ANALYSIS.md`](./TYPELAYOUT_INTEGRATION_ANALYSIS.md) §7 + §8
>
> ⚠️ 自 commit `6f4f7a8d`（Safety 集成）以来，`is_xbuffer_safe<T>` 已完全委托给
> TypeLayout 的 `classify_safety<T>()`，本文档中的能力矩阵对比（§2）、候选设计方案（§3）
> 和推荐方案（§4）**已不再适用**。保留作为历史参考。

---

## 1. is_xbuffer_safe<T> 逐条规则分类

### 1.1 完整检查规则清单

| # | 检查规则 | 代码位置 | 判定 | 分类 |
|---|---------|---------|------|------|
| R1 | type-erased 容器黑名单 (std::function, std::any, shared_ptr, unique_ptr, weak_ptr) | L612-638 | Unsafe | 🔴 **领域特定** |
| R2 | 基本类型白名单 (int8~64, uint8~64, float, double, bool, char) | L555-568 | Safe | 🟡 **可通用化** |
| R3 | 枚举类型 (is_fixed_enum) | L746-748 | Safe | ✅ **已委托 TypeLayout** |
| R4 | XString 识别 | L750-752 | Safe | 🔴 **领域特定** |
| R5 | XVector/XSet/XMap 识别 (sizeof==32, alignof==8 + 递归检查元素) | L643-675 | Safe | 🔴 **领域特定** |
| R6 | 多态类型 (is_polymorphic) | L712-713 | Unsafe | ✅ **通用** |
| R7 | 继承检测 (has_bases) | L716-717 | Unsafe | 🟡 **通用但策略不同** |
| R8 | 联合体检测 (is_union) | L720-721 | Unsafe | 🟡 **通用但策略不同** |
| R9 | 指针成员检测 (is_pointer) | L691-693 | Unsafe | ✅ **通用** |
| R10 | 引用成员检测 (is_reference) | L689-690 | Unsafe | ✅ **通用** |
| R11 | 递归成员安全检查 | L678-730 | 递归 | 混合 |

### 1.2 分类统计

| 类别 | 规则 | 数量 | 说明 |
|------|------|------|------|
| ✅ 通用类型属性 | R6, R9, R10 | 3 | 任何跨进程/序列化场景都需要的检查 |
| ✅ 已委托 TypeLayout | R3 | 1 | `is_fixed_enum<T>()` |
| 🟡 可通用化 | R2, R7, R8 | 3 | 基本类型白名单可提取；继承/联合体策略各项目不同 |
| 🔴 领域特定 | R1, R4, R5 | 3 | XString/XVector 识别和黑名单检测完全是 XOffset 的领域知识 |

---

## 2. TypeLayout classify_safety() 对比

### 2.1 TypeLayout 当前实现

```cpp
// 运行时，基于签名字符串扫描
inline SafetyLevel classify_safety(std::string_view sig) {
    if (sig.find("bits<") != npos) return Risk;       // 位域
    if (sig.find("wchar[") != npos) return Risk;      // 平台依赖类型
    if (sig.find("ptr[") != npos ...) return Warning;  // 指针
    if (sig.find(",vptr") != npos) return Warning;     // 虚表
    return Safe;
}
```

### 2.2 能力矩阵对比

| 检查项 | is_xbuffer_safe (编译时) | classify_safety (运行时) | 重叠？ |
|--------|------------------------|-------------------------|--------|
| 指针检测 | ✅ `is_pointer_v` | ✅ `ptr[` 字符串匹配 | 重叠 |
| 引用检测 | ✅ `is_reference_v` | ✅ `ref[` 字符串匹配 | 重叠 |
| 多态检测 | ✅ `is_polymorphic_v` | ✅ `,vptr` 字符串匹配 | 重叠 |
| 位域检测 | ❌ 不检查 | ✅ `bits<` 字符串匹配 | TypeLayout 独有 |
| wchar_t 检测 | ❌ 不检查 | ✅ `wchar[` 字符串匹配 | TypeLayout 独有 |
| 联合体检测 | ✅ `is_union_v` | ❌ 不检查 | XOffset 独有 |
| 继承检测 | ✅ `bases_of().size()` | ❌ 不检查 | XOffset 独有 |
| 容器白名单 | ✅ XString/XVector/... | ❌ 无此概念 | XOffset 独有 |
| 容器黑名单 | ✅ std::function/any/... | ❌ 无此概念 | XOffset 独有 |
| 枚举检查 | ✅ `is_fixed_enum` | ❌ 不检查 | 已委托 |
| 编译时可用 | ✅ consteval | ❌ 运行时 | — |

**核心发现**：两者仅在**指针/引用/多态**三项上重叠，但这 3 项恰好都是 `std::` 标准 type_traits 已经提供的。TypeLayout 的 `classify_safety()` 并没有提供 is_xbuffer_safe 不知道的编译时能力。

---

## 3. 候选设计方案

### 3.1 方案 A: 细粒度 consteval traits

```cpp
namespace boost::typelayout {
    template<T> consteval bool has_pointer_members();     // 递归扫描
    template<T> consteval bool has_bitfield_members();    // 递归扫描
    template<T> consteval bool has_reference_members();   // 递归扫描
    template<T> consteval bool is_trivially_portable();   // 组合判断
}
```

**优点**: 最大灵活性，XOffset 可以自由组合  
**缺点**: TypeLayout 需要实现深度递归成员扫描，而 is_xbuffer_safe 已经有自己的递归扫描逻辑——两者会重复  
**关键问题**: `has_pointer_members<T>()` 需要递归遍历所有成员，但**不知道哪些类型是"安全容器"**（如 XVector 内部有 offset_ptr 但整体安全），会产生误报

### 3.2 方案 B: 粗粒度 consteval 分级

```cpp
namespace boost::typelayout {
    enum class CompileTimeSafety { TriviallyPortable, HasPointers, HasVTable, HasBitFields, HasUnion };
    template<T> consteval CompileTimeSafety classify_type_safety();
}
```

**优点**: API 简洁  
**缺点**: 粒度太粗。is_xbuffer_safe 需要对指针、容器、黑名单分别处理，粗粒度分级无法覆盖这些分支  
**关键问题**: 同方案 A——无法处理"安全容器"的特殊情况

### 3.3 方案 C: 不添加新 API，保持现状

**理由**:
1. **重叠极小**：两者真正重叠的 3 项（指针/引用/多态）都是标准 type_traits 已提供的
2. **职责本质不同**：
   - TypeLayout `classify_safety()` 回答的是："这个类型的签名中是否包含不可移植的元素？"（签名层面）
   - `is_xbuffer_safe<T>` 回答的是："这个类型可以放进共享内存 XBuffer 吗？"（业务层面）
3. **递归扫描冲突**：两者都需要递归成员遍历，但对"叶子节点"的判定标准不同——TypeLayout 不知道 XString/XVector 是安全的
4. **is_xbuffer_safe 不可简化**：即使 TypeLayout 提供了 `has_pointer_members<T>()`，is_xbuffer_safe 仍然需要自己的递归遍历（因为要识别容器白名单并在容器内部递归检查元素类型）

---

## 4. 推荐方案

### ✅ 推荐方案 C: 保持现状

**核心论点**: TypeLayout 和 is_xbuffer_safe 的递归扫描**不可合并**，因为它们的终止条件不同。

```
is_xbuffer_safe 递归逻辑:
  遇到 int32_t     → 终止, safe ✓
  遇到 XVector<T>  → 终止, 递归检查 T ✓     ← TypeLayout 不知道这一步
  遇到 XString     → 终止, safe ✓           ← TypeLayout 不知道这一步
  遇到 struct S    → 展开成员, 递归检查每个成员
  遇到 int*        → 终止, unsafe ✗

classify_safety (如果变成编译时):
  遇到 int32_t     → 终止, safe ✓
  遇到 XVector<T>  → 展开内部成员?? 看到 offset_ptr → Warning ✗  ← 误报！
  遇到 XString     → 展开内部成员?? 看到 allocator → Warning ✗  ← 误报！
  遇到 struct S    → 展开成员, 递归检查每个成员
  遇到 int*        → 终止, unsafe ✗
```

**如果 TypeLayout 提供编译时 safety API，XVector 会被误判为 Warning**（因为内部包含 offset_ptr）。除非 TypeLayout 也维护一个"安全容器白名单"——但这就把 XOffset 的领域知识泄露进了 TypeLayout。

### 4.1 唯一值得 TypeLayout 添加的

从分析中发现一个**不需要递归**的小工具确实有价值：

```cpp
// 仅检查直接类型属性（非递归），不涉及成员扫描
template<typename T>
[[nodiscard]] consteval bool has_stable_layout() noexcept {
    return !std::is_polymorphic_v<T> &&    // 无 vptr
           !std::is_union_v<T> &&          // 无联合体
           std::is_standard_layout_v<T>;   // 标准布局
}
```

但这本质上就是 `std::is_standard_layout_v<T> && !std::is_polymorphic_v<T>`——标准库已经提供了。**不值得 TypeLayout 重复封装**。

---

## 5. 结论

| 问题 | 结论 |
|------|------|
| TypeLayout 应该添加编译时 Safety API 吗？ | **不应该** |
| 原因 | 两者的递归扫描不可合并（终止条件不同），真正重叠的 3 项已有标准 type_traits |
| is_xbuffer_safe 可以简化吗？ | 当前实现已经是最优结构 |
| 唯一已成功委托的 | `is_fixed_enum<T>()` — 这是非递归的叶子检查，适合放在 TypeLayout |

**一句话**: 类型安全检查的**叶子判断**（如 `is_fixed_enum`）可以委托给 TypeLayout，但**递归遍历框架**必须留在 XOffsetDatastructure，因为只有它知道哪些容器类型是安全的。
