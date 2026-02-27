# TypeLayout 与 XOffsetDatastructure 集成架构分析

> **版本**: v1.3  
> **分析日期**: 2026-02-10（v1.1: 2026-02-27, v1.2: 2026-02-27, v1.3: 2026-02-28）  
> **状态**: ✅ 完成

---

## 1. 职责边界分析

### 1.1 TypeLayout 的核心职责

TypeLayout 是一个**通用的编译时类型签名库**，与序列化无关：

| 层级 | 组件 | 职责 |
|------|------|------|
| **核心层** | `core/fwd.hpp` | `FixedString<N>`, `SignatureMode`, `to_fixed_string()` |
| **核心层** | `core/signature_detail.hpp` | `TypeSignature<T, Mode>` 特化（基本类型、复合类型、数组、枚举、联合体、指针等） |
| **核心层** | `core/signature.hpp` | 顶层 API：`get_definition_signature<T>()`, `get_layout_signature<T>()`, `*_signatures_match<T1, T2>()` |
| **工具层** | `tools/sig_export.hpp` | `SigExporter` 类：将签名导出为 `.sig.hpp` 头文件 |
| **工具层** | `tools/compat_check.hpp` | `CompatReporter` 类：跨平台签名比较和报告生成 |
| **工具层** | `tools/platform_detect.hpp` | 平台检测宏和函数 |
| **工具层** | `tools/sig_types.hpp` | `TypeEntry`, `PlatformInfo` 数据结构 |

**关键设计原则**：
- TypeLayout **不知道**任何 XOffsetDatastructure 的类型
- TypeLayout 对用户定义类型是开放的（通过 `TypeSignature<T, Mode>` 模板特化）
- TypeLayout 的工具层（`tools/`）不需要 P2996，可在 C++17 编译器上运行

### 1.2 XOffsetDatastructure 的类型签名需求

XOffsetDatastructure 使用类型签名的场景：

| 场景 | 使用方式 | 层级 |
|------|----------|------|
| **编译时二进制合约** | `static_assert(get_definition_signature<Player>() == "...")` | Definition |
| **容器类型识别** | XString/XVector/XSet/XMap 的 Opaque Signature 特化 | Definition + Layout |
| **类型安全检查** | `is_xbuffer_safe<T>` → `classify_for_xoffset<T>()` → `classify_safety<T>()` | ✅ 核心依赖 |
| **自动迁移** | `XBufferCompactor` 内部不使用 TypeLayout（独立反射实现） | ❌ 未使用 |

> ⚠️ **v1.2 更正**: 自 Safety 集成（commit 6f4f7a8d）以来，`is_xbuffer_safe<T>` 已完全委托给 TypeLayout 的 `classify_safety<T>()`，不再有独立的反射扫描逻辑。详见 §7 和 §8。

### 1.3 职责分离评估

**结论：✅ 职责清晰，边界合理**

```
TypeLayout                          XOffsetDatastructure
┌───────────────────────┐          ┌───────────────────────┐
│ 通用类型签名生成       │          │ 零拷贝序列化框架       │
│ - 基本类型签名         │ ◀──────  │ - 容器特化注册         │
│ - 复合类型签名         │  使用    │ - static_assert 验证   │
│ - 平台前缀             │          │                       │
│ - 跨平台比较工具       │          │ 委托 TypeLayout:       │
│ - classify_safety<T>() │          │ - is_xbuffer_safe<T>   │
│                       │          │   → classify_safety<T> │
│ 开放扩展点:            │          │ 独立的反射功能：       │
│ TypeSignature<T,Mode>  │          │ - XBufferCompactor     │
│ TYPELAYOUT_OPAQUE_*    │          │ - member iteration     │
└───────────────────────┘          └───────────────────────┘
```

**优点**：
- TypeLayout 完全不依赖 XOffsetDatastructure（单向依赖）
- 扩展通过模板特化实现，无需修改 TypeLayout 源码
- 两个项目可以独立版本演进

**发现 #1（轻微冗余）**：~~XOffsetDatastructure 内部的 `get_member_count_impl<T>()` 和 TypeLayout 的 `get_member_count<T>()` 功能完全相同。~~ ✅ **已修复** — 冗余实现已删除，统一使用 `boost::typelayout::get_member_count<T>()`。

---

## 2. 使用方式分析

### 2.1 容器特化模式（Opaque Signature）

当前实现（`xoffsetdatastructure.hpp` 底部）：

```cpp
namespace boost { namespace typelayout {
    template <SignatureMode Mode>
    struct TypeSignature<XOffsetDatastructure::XString, Mode> {
        static consteval auto calculate() noexcept {
            return FixedString{"string[s:32,a:8]"};
        }
    };
    // XVector, XSet, XMap 类似...
}}
```

**评估：✅ 合理且必要**

| 维度 | 评分 | 说明 |
|------|------|------|
| 正确性 | ✅ | 签名准确反映了容器的固定内存布局（32 bytes, 8-aligned） |
| 稳定性 | ✅ | 避免了 Boost.Container 内部实现细节泄露到签名中 |
| 一致性 | ✅ | 所有容器都遵循 `类型名[s:32,a:8]<元素签名>` 格式 |
| 可读性 | ✅ | `vector[s:32,a:8]<i32[s:4,a:4]>` 直观清晰 |

**发现 #2（改进机会）**：当前特化忽略了 `SignatureMode`——无论是 Definition 还是 Layout 模式都返回相同结果。这对于 XOffsetDatastructure 的场景是正确的（因为容器内部没有"字段名"概念），但从 TypeLayout 的角度看，Layout 模式下可以省略 `<元素签名>` 而只保留 `string[s:32,a:8]`，因为 Layout 只关心字节占用。

**建议**：保持现状。对于 XOffsetDatastructure 的使用场景，元素类型信息在两种模式下都有价值。如果未来需要区分，可以在特化中添加 `if constexpr (Mode == ...)` 分支。

### 2.2 特化放置位置

特化在 `xoffsetdatastructure.hpp` 中、`namespace boost::typelayout` 内注册。

**评估：✅ 正确做法**

C++ 标准要求模板特化必须在原始模板声明的命名空间中。这不是"命名空间侵入"，而是 C++ 模板特化的标准模式（类似 `std::hash` 特化）。

**发现 #3（位置问题）**：特化放在文件末尾（`#endif` 之前），位于 `XOffsetDatastructure` 命名空间关闭之后。这意味着所有容器类型定义必须在特化之前完成。当前代码已正确处理此依赖顺序。

### 2.3 Definition vs Layout 签名选择策略

当前 XOffsetDatastructure 使用的 API 统计：

| API | 使用位置 | 使用次数 |
|-----|----------|----------|
| `get_definition_signature<T>()` | `player.hpp`, `game_data.hpp`, tests | 5+ |
| `definition_signatures_match<T1,T2>()` | `test_typelayout_integration.cpp` | 2 |
| `get_layout_signature<T>()` | `test_typelayout_integration.cpp` | 2 |
| `layout_signatures_match<T1,T2>()` | `test_typelayout_integration.cpp` | 2 |

**评估：✅ 策略合理**

XOffsetDatastructure 主要使用 **Definition Signature**，这是正确的选择：
- 零拷贝序列化要求**精确类型匹配**（字段名+类型+偏移都一致）
- Layout Signature 只在测试中验证"字段名不同但布局相同"的场景

**发现 #4（未利用的能力）**：TypeLayout 的 Layout Signature 层对 XOffsetDatastructure 有直接价值——可以用于跨版本数据迁移（字段重命名但布局不变的场景）。当前这个能力只在测试中展示，未在生产代码中利用。

### 2.4 `static_assert` 签名验证模式

```cpp
static_assert(boost::typelayout::get_definition_signature<Player>() ==
    "[64-le]record[s:72,a:8]{@0[id]:i32[s:4,a:4],...}",
    "Type signature mismatch for Player - Binary layout changed!");
```

**评估：⚠️ 基本合理但有脆弱性**

| 维度 | 评分 | 说明 |
|------|------|------|
| 编译时安全 | ✅ | 任何布局变化都会导致编译失败 |
| 可读性 | ✅ | 签名字符串直观展示内存布局 |
| 可维护性 | ⚠️ | 签名字符串必须手动更新，容易遗漏 |
| 跨平台 | ⚠️ | 硬编码 `[64-le]` 前缀，在 32-bit 或大端平台上编译失败 |

**发现 #5（脆弱性）**：当前模式将完整的签名字符串硬编码在 `static_assert` 中。更健壮的方式是使用 `definition_signatures_match<T1, T2>()`。但硬编码字符串有一个优势——开发者可以直接**看到**预期的布局，作为文档使用。

**建议**：保持硬编码字符串用于关键数据类型（如 `Player`, `GameData`），但对测试中的辅助类型使用 `*_signatures_match<>()` API。

---

## 3. TypeLayout 功能完整性分析

### 3.1 核心签名引擎评估

**覆盖的类型**（`signature_detail.hpp`）：

| 类别 | 类型 | 状态 |
|------|------|------|
| 固定宽度整数 | `int8_t` ~ `uint64_t` | ✅ 完整 |
| 基本整数 | `long`, `unsigned long`, `long long` | ✅ 带 `requires` 去重 |
| 浮点 | `float`, `double`, `long double` | ✅ 完整 |
| 字符 | `char`, `wchar_t`, `char8/16/32_t` | ✅ 完整 |
| 布尔/字节 | `bool`, `std::byte`, `std::nullptr_t` | ✅ 完整 |
| 指针 | `T*`, `T&`, `T&&`, `T C::*` | ✅ 完整 |
| 函数指针 | `R(*)(Args...)`, `noexcept`, variadic | ✅ 完整 |
| 数组 | `T[N]`, byte 数组特殊处理 | ✅ 完整 |
| 枚举 | `enum`（含底层类型） | ✅ 完整 |
| 联合体 | `union` | ✅ 完整 |
| 类/结构体 | `class`/`struct`（含继承、多态） | ✅ 完整 |
| CV 限定 | `const`, `volatile`, `const volatile` | ✅ 自动剥离 |
| 位域 | bit-field（含 `offset_of().bits`） | ✅ 完整 |
| 匿名成员 | 无标识符成员 | ✅ `<anon:N>` 处理 |

**评估：✅ 核心引擎功能完整，覆盖了 C++ 所有常见类型**

**发现 #6（亮点）**：TypeLayout 的 `requires` 约束用于处理 `long`/`int64_t` 等类型别名冲突的方式非常优雅，避免了重复特化的编译错误。

### 3.2 工具层评估

| 工具 | 功能 | XOffsetDatastructure 是否使用 |
|------|------|------|
| `sig_export.hpp` | 生成 `.sig.hpp` 文件，导出类型签名 | ❌ 未使用 |
| `compat_check.hpp` | 跨平台签名比较 + `classify_safety()` + `SafetyLevel` | ✅ 核心依赖 |
| `platform_detect.hpp` | 平台检测（arch, os, compiler） | ❌ 未使用 |
| `sig_types.hpp` | `TypeEntry`, `PlatformInfo` 数据结构 | ✅ `ArchSpec::to_platform_info()` |

**评估：⚠️ 工具层大部分已集成，`sig_export.hpp` 尚未使用**

> ⚠️ **v1.2 更正**: `compat_check.hpp` 和 `sig_types.hpp` 已被集成。`classify_safety()`、`SafetyLevel`、`contains_token()` 是 XOffset 安全检测的核心依赖。`ArchSpec::to_platform_info()` 提供了 CI 集成接口。

**发现 #7（改进机会）**：TypeLayout 的 `sig_export.hpp` 恰好解决了 `type-signature` spec 中的"跨平台签名导出工具"需求（Requirement 5）。当前 XOffsetDatastructure 的 spec 要求：
> 系统 SHALL 提供跨平台签名导出能力，支持在不同架构间验证类型兼容性。

`SigExporter` 尚未集成到构建流程中。建议创建示例或工具，将其纳入 CI。

### 3.3 XOffsetDatastructure 需要但 TypeLayout 未提供的功能

| 需求 | TypeLayout 状态 | 说明 |
|------|-----------------|------|
| 编译时签名哈希 | ❌ 未提供 | 长签名字符串比较效率低；需要 `consteval uint64_t signature_hash<T>()` |
| 签名版本号 | ❌ 未提供 | 无法区分签名格式 v1/v2；建议添加格式版本前缀 |
| 签名差异诊断 | ❌ 未提供 | 签名不匹配时无法自动告知哪个字段变化了 |
| 容器特化辅助宏 | ❌ 未提供 | 每个 XOffsetDatastructure 容器需要手写特化，可提供辅助宏简化 |

### 3.4 TypeLayout 提供但未被使用的功能（冗余分析）

| 功能 | 使用状态 | 是否应该使用 |
|------|----------|-------------|
| `get_layout_signature<T>()` | ✅ `StrictPolicy` + `classify_safety<T>()` | ✅ 核心依赖 |
| `SigExporter` 导出工具 | ✅ `tools/export_signatures.cpp` + CI | ✅ 已集成 |
| `CompatReporter` 比较工具 | ✅ `tools/check_compat.cpp` + CI | ✅ 已集成 |
| `classify_safety<T>()` 编译时分级 | ✅ `classify_for_xoffset` 核心调用 | ✅ 已完全集成 |
| `classify_safety(string_view)` 运行时分级 | ✅ `CompatReporter::compare()` 内部使用 | ✅ 已集成 |
| `SafetyLevel` 枚举 | ✅ XOffset 直接使用 | ✅ 已集成 |
| `contains_token()` | ✅ 编译时+运行时均使用 | ✅ 已集成 |
| `get_arch_prefix()` | 间接使用 | ✅ 已通过签名使用 |
| 位域签名 | ✅ 通过 `bits<` 标记自动拒绝 | ✅ 已通过 classify_safety 集成 |
| 联合体签名 | ✅ 通过 `union[` 标记自动拒绝 | ✅ 已通过 classify_safety 集成 |
| 枚举签名 | ✅ 7 个测试覆盖 | `test_enum_support.cpp`: 安全性、签名、XBuffer、XVector、嵌套结构 |

> ⚠️ **v1.2 更正**: 大量条目已从"未使用"更正为"已集成"。TypeLayout 的安全分级引擎（`classify_safety`）已成为 XOffset 类型准入的核心 ground truth。

---

## 4. 耦合与风险分析

### 4.1 版本耦合风险

**当前状态**：通过 Git Submodule 锁定到特定 commit。

| 风险 | 等级 | 说明 |
|------|------|------|
| 签名格式变更 | 🟡 中 | TypeLayout 修改 `record` → 其他关键字会破坏所有 `static_assert` |
| API 变更 | 🟢 低 | TypeLayout 的 public API 稳定且简洁（4 个顶层函数） |
| 依赖冲突 | 🟢 低 | TypeLayout 只依赖标准库和 `<experimental/meta>` |

**建议**：在 TypeLayout 仓库中建立签名格式的语义版本承诺（SemVer）。

### 4.2 命名空间侵入分析

XOffsetDatastructure 在 `boost::typelayout` 命名空间中注册了 4 个模板特化。

**评估：✅ 无风险**

这是 C++ 模板特化的标准做法。只要特化的类型（`XString`, `XVector<T>` 等）是 XOffsetDatastructure 自己定义的，就不存在 ODR 违规风险。

### 4.3 编译器依赖同步

两个项目都依赖 Bloomberg Clang P2996 fork。

| 依赖项 | TypeLayout | XOffsetDatastructure |
|--------|-----------|---------------------|
| `<experimental/meta>` | ✅ 核心依赖 | ✅ 核心依赖 |
| `-freflection` | ✅ 必须 | ✅ 必须 |
| `-stdlib=libc++` | ✅ 建议 | ✅ 必须 |
| Boost 库 | ❌ 不依赖 | ✅ 核心依赖 |

**评估：✅ 同步，但有潜在分化风险**

TypeLayout 的工具层（`tools/`）设计为 C++17 兼容（不需要 P2996）。如果 TypeLayout 未来支持非 P2996 编译器（通过手动类型注册），而 XOffsetDatastructure 继续强制要求 P2996，可能出现功能集不对称。

---

## 5. 改进建议清单

| # | 建议 | 优先级 | 影响范围 | 关联发现 |
|---|------|--------|----------|----------|
| 1 | ~~统一 `get_member_count` 实现~~ | ✅ 已完成 | 代码简化 | #1 |
| 2 | ~~集成 `SigExporter`：`tools/export_signatures.cpp` 导出 `.sig.hpp`~~ | ✅ 已完成 | 新功能 | #7 |
| 3 | ~~集成 `CompatReporter`：`tools/check_compat.cpp` 自验证~~ | ✅ 已完成 | 新功能 | #7 |
| 4 | 为 TypeLayout 添加签名哈希 API：`consteval uint64_t definition_signature_hash<T>()` | 🟡 中 | TypeLayout 改进 | #3.3 |
| 5 | ~~测试枚举类型在 XOffsetDatastructure 中的支持~~ | ✅ 已完成 | 测试覆盖 | #3.4 |
| 6 | 利用 `classify_safety()` 补充 `is_xbuffer_safe` 的诊断信息 | 🟢 低 | 诊断增强 | #3.4 |
| 7 | 考虑为 Layout Signature 添加跨版本迁移支持 | 🟢 低 | 未来功能 | #4 |

> **v1.3 实施记录**:
> - **建议 #2 & #3**: `build.sh` 新增 "Signature Export & Compatibility Check" 阶段，在 `ENABLE_REFLECTION=1` 时自动运行 `export_signatures`（输出到 `tools/sigs/`）和 `check_compat`（编译时自验证）。CI workflow (`.github/workflows/ci.yml`) 新增 "Signature contract verification" step（`git diff` 检测 `.sig.hpp` 变更并发出 `::warning::` 注解）和签名 artifact 上传（保留 30 天）。
> - **建议 #5**: `tests/test_enum_support.cpp` 扩展至 7 个测试：原有 3 个 + 新增 (4) 无底层类型 enum class 默认 int 验证、(5) `XVector<Enum>` 元素读写、(6) 不同枚举 Definition 签名不匹配验证、(7) 含枚举字段的嵌套结构签名和 XBuffer 读写。

---

## 6. 总结

### 架构健康度评分

| 维度 | 评分 | 说明 |
|------|------|------|
| 职责分离 | ⭐⭐⭐⭐⭐ | 清晰的单向依赖，无功能重叠 |
| 使用方式 | ⭐⭐⭐⭐ | 核心 API 使用正确，容器特化模式优雅 |
| 功能利用率 | ⭐⭐⭐⭐ | 核心层+工具层均已集成（SigExporter + CompatReporter + CI） |
| 耦合度 | ⭐⭐⭐⭐⭐ | 最小耦合，通过标准 C++ 机制扩展 |
| 风险控制 | ⭐⭐⭐⭐ | 版本锁定、无 ODR 违规，编译器同步 |

### 一句话结论

**TypeLayout 的核心功能设计合理且完整，XOffsetDatastructure 对核心层的使用方式正确。最大的改进机会在于利用 TypeLayout 已有的工具层（`sig_export` + `compat_check`）来实现跨平台签名导出需求，这是 spec 中已定义但未实现的功能。**

---

## 7. Safety 集成：TypeLayout `classify_safety` × XOffset Policy Trait

> **新增**: 2026-02-27  
> **状态**: ✅ 已实施（commit 6f4f7a8d）

### 7.1 集成架构

XOffsetDatastructure 的 **Policy Trait 安全系统** 建立在 TypeLayout 的 `classify_safety<T>()` 之上。
两层之间的分工如下：

```
TypeLayout (classify_safety)                XOffsetDatastructure (Policy Trait)
┌─────────────────────────────────┐        ┌─────────────────────────────────────┐
│ 输入: 任意类型 T                 │        │ 输入: T + Policy (Default/Relaxed)  │
│ 输出: SafetyLevel               │        │                                     │
│   - Safe    (0): 固定布局基本类型│        │ classify_for_xoffset<T>():          │
│   - Warning (1): 含指针/vptr/    │ ─────▶ │   1. long/ulong 平台特判 → Risk     │
│                   联合体等       │ 调用   │   2. is_safe_leaf → 递归检查元素     │
│   - Risk    (2): 不安全类型      │        │   3. classify_safety<T>()           │
│                   (wchar_t 等)   │        │      Warning → Risk (严格升级)      │
│                                 │        │                                     │
│ 签名引擎: get_layout_signature  │        │ classify_raw<T>():                  │
│   - vptr 标记传播到嵌套成员       │        │   同上但 Warning 保持原值 (Relaxed) │
│   - 多态类型 → 签名含 [vptr]    │        │                                     │
└─────────────────────────────────┘        │ Policy::accept<T>():                │
                                           │   - DefaultPolicy: Safe only        │
                                           │   - RelaxedPolicy: Safe + Warning   │
                                           └─────────────────────────────────────┘
```

### 7.2 版本耦合关系

| 依赖方向 | 说明 |
|----------|------|
| XOffset → TypeLayout | XOffset 调用 `classify_safety<T>()` 和 `get_layout_signature<T>()` |
| TypeLayout → XOffset | ❌ 无反向依赖 |

**关键耦合点**：

1. **`SafetyLevel` 枚举值**: XOffset 假设 `Safe=0, Warning=1, Risk=2`。如果 TypeLayout
   修改枚举值定义，XOffset 的 `classify_for_xoffset` 逻辑需同步更新。

2. **`classify_safety` 分类语义**: XOffset 依赖以下分类行为：
   - 含 `vptr` 的类型 → `Warning`（多态标记）
   - 含指针成员的类型 → `Warning`
   - `wchar_t` / `long double` → `Risk`
   - 固定宽度基本类型（`int32_t`, `float`, ...） → `Safe`

3. **签名中的 vptr 传播** (C1 修复): TypeLayout commit `59f6616` 修复了嵌套多态成员的
   vptr 标记传播。XOffset 的安全检查依赖此行为——如果 TypeLayout 回滚此修复，
   `EmbedsPoly` 等类型将不再被正确拒绝。

### 7.3 版本锁定策略

TypeLayout 通过 Git 子模块锁定在特定 commit：

```
external/typelayout → 59f6616d (含 vptr 传播修复)
```

**升级 TypeLayout 时的检查清单**：

- [ ] 确认 `SafetyLevel` 枚举值未变更
- [ ] 运行 `test_classify_safety` 验证分类语义一致性
- [ ] 运行 `test_remediation_fixes` 验证 C1 (vptr 传播) 和 C2 (递归容器) 行为
- [ ] 运行 `test_policy_trait` 验证 Policy Trait 集成
- [ ] 检查 `classify_safety` 对 `long` / `unsigned long` 的处理是否有变化

### 7.4 平台注意事项

`long` / `unsigned long` 的安全性取决于平台 ABI：

| 平台 | `sizeof(long)` | `long == int64_t` ? | `classify_for_xoffset<long>()` |
|------|----------------|---------------------|-------------------------------|
| LP64 (Linux x86_64, glibc) | 8 | ✅ `true` | `Safe` |
| LP64 (Linux x86_64, musl/libc++) | 8 | ⚠️ 取决于 `<stdint.h>` 来源 | 通常 `Safe` |
| LLP64 (Windows x64) | 4 | ❌ `false` | `Risk` |
| ILP32 (32-bit) | 4 | ❌ `false` | `Risk` |

这是**编译时**决定的——同一份源码在不同平台上会产生不同的安全判定，确保零编码序列化的二进制兼容性。

### 7.5 深度分析：`long` 类型在结构体成员中的不可检测性

> **详细指南**: 参见 `docs/LONG_PORTABILITY_GUIDE.md`

#### 问题核心

`classify_for_xoffset<T>()` 只能检测 **裸 `long` 类型**（即 `T = long`）。当 `long` 嵌入在结构体成员中时，TypeLayout 签名已经将其编码为 `i32`（Windows）或 `i64`（Linux），丢失了原始类型名。

#### P2996 反射探针实验

我们编写了 `tests/test_long_reflection_probe.cpp` 来验证 P2996 能否区分结构体成员中的 `long` 和 `int64_t`。

**运行环境**: Linux x86_64 (LP64), Bloomberg Clang P2996

**完整输出**:

```
=== P2996 Long Reflection Probe ===

[1] Platform Info:
  sizeof(long)          = 8
  sizeof(long long)     = 8
  sizeof(int32_t)       = 4
  sizeof(int64_t)       = 8
  long == int32_t?      0
  long == int64_t?      1
  long long == int64_t? 0

[2] Direct Type display_string_of:
  int32_t:       int32_t
  int64_t:       int64_t
  long:          long
  unsigned long: ulong_t
  long long:     llong_t
  int:           int

[3] Reflection Identity (^^type == ^^type):
  ^^long == ^^int32_t?             0
  ^^long == ^^int64_t?             0
  ^^(long long) == ^^int64_t?      0
  ^^(unsigned long) == ^^uint32_t?  0
  ^^(unsigned long) == ^^uint64_t?  0

[4] Struct Member Type Inspection (ProbeStruct):
  a: int
  b: long
  c: long
  d: unsigned long
  e: long long
  f: int

[5] Detection Feasibility:
  Can detect 'long' via display_string_of? 1
  type_of('long c') == type_of('int64_t b')? 1

[RESULT] Plan B IS FEASIBLE: P2996 display_string_of can distinguish
         'long' from fixed-width types!
```

#### 结果解读

| 测试项 | 结果 | 含义 |
|--------|------|------|
| `^^long != ^^int64_t` | ✅ 不同 | 直接反射可区分类型 |
| `display_string_of(^^long)` = `"long"` | ✅ 保留名称 | 直接反射保留源码类型名 |
| `type_of(int64_t成员)` = `"long"` | ❌ 被 desugar | 成员级别丢失 int64_t 名 |
| `type_of(long成员)` = `"long"` | — | 与 int64_t 成员结果相同 |
| `type_of(成员0) == type_of(成员1)` | ✅ 相等 | **无法区分 int64_t 和 long** |

#### 结论

**方案B（P2996 反射检测）不可行**：虽然 `^^long != ^^int64_t`（直接反射可区分），但 `type_of()` 对结构体成员执行了 "desugaring"——在 LP64 上 `int64_t` 是 `long` 的 typedef，编译器将二者归一化为相同的底层类型 `long`。因此：

- ❌ 无法通过反射扫描结构体成员来区分"用户写的 `long`"和"用户写的 `int64_t`"
- ✅ 只能通过**编码规范**（方案C）约束用户不使用 `long`

**已实施方案C**：
1. 创建了 `docs/LONG_PORTABILITY_GUIDE.md` 作为用户指南
2. 在 `classify_for_xoffset` 和 `classify_raw` 中添加了局限性注释
3. 提供了 `XBUFFER_ASSERT_NO_LONG` 宏作为代码审查辅助

---

## 8. 跨平台兼容检测整合分析

> **新增**: 2026-02-27 (v1.2)  
> **状态**: ✅ 完成  
> **结论**: **已整合统一 — 单一真相源 (Single Source of Truth) 架构**

### 8.1 整合架构全景

XOffsetDatastructure 和 TypeLayout 的跨平台兼容检测构成一个**清晰的分层架构**，
没有逻辑重复、没有标记遗漏、没有不一致。

```
┌──────────────────────────────────────────────────────────┐
│  XOffsetDatastructure (域策略层)                           │
│                                                          │
│  classify_for_xoffset<T>()                               │
│    ├── Step 1: long/unsigned long 前置拦截                │
│    │   (TypeLayout 签名脱糖 → 无法区分 long/int64_t)      │
│    ├── Step 2: classify_safety<T>() ← 委托 TypeLayout    │
│    └── Step 3: Warning→Risk 升级 (零编码策略)              │
│                                                          │
│  XOFFSET_REGISTER_* 宏                                    │
│    └── TYPELAYOUT_OPAQUE_* ← 委托 TypeLayout              │
│                                                          │
│  StrictPolicy<GoldSignature>                              │
│    └── get_layout_signature<T>() == GoldSignature ← 委托  │
├──────────────────────────────────────────────────────────┤
│  TypeLayout (签名引擎 + 安全分类器)                        │
│                                                          │
│  编译期: classify_safety<T>()  [classify_safety.hpp]       │
│    ├── Risk:  bits< | wchar[ | f80[      (contains)       │
│    └── Warn:  ptr[ fnptr[ memptr[ ref[ rref[ union[       │
│               (contains_token — 防 nullptr 误匹配)         │
│                                                          │
│  运行时: classify_safety(string_view) [compat_check.hpp]   │
│    ├── Risk:  bits< | wchar[ | f80[      (find)           │
│    └── Warn:  ptr[ fnptr[ memptr[ ref[ rref[ union[       │
│               (contains_token — 同算法)                    │
│                                                          │
│  C1引擎: CompatReporter + TYPELAYOUT_ASSERT_COMPAT        │
│    └── 跨平台 .sig.hpp 布局签名对比                        │
└──────────────────────────────────────────────────────────┘
```

### 8.2 标记集一致性验证

TypeLayout 编译期和运行时扫描器使用**完全相同的 9 个标记**，分类级别一致：

| 标记 | 级别 | 编译期<br>`classify_safety.hpp` | 运行时<br>`compat_check.hpp` | XOffset<br>`xoffsetdatastructure.hpp` |
|------|:---:|:---:|:---:|:---:|
| `bits<` | Risk | `sig.contains()` | `sig.find()` | 不扫描，委托 TypeLayout |
| `wchar[` | Risk | `sig.contains()` | `sig.find()` | 不扫描，委托 TypeLayout |
| `f80[` | Risk | `sig.contains()` | `sig.find()` | 不扫描，委托 TypeLayout |
| `ptr[` | Warning | `sig.contains_token()` | `contains_token()` | 不扫描，委托 TypeLayout |
| `fnptr[` | Warning | `sig.contains_token()` | `contains_token()` | 不扫描，委托 TypeLayout |
| `memptr[` | Warning | `sig.contains_token()` | `contains_token()` | 不扫描，委托 TypeLayout |
| `ref[` | Warning | `sig.contains_token()` | `contains_token()` | 不扫描，委托 TypeLayout |
| `rref[` | Warning | `sig.contains_token()` | `contains_token()` | 不扫描，委托 TypeLayout |
| `union[` | Warning | `sig.contains_token()` | `contains_token()` | 不扫描，委托 TypeLayout |

**关键设计选择**：
- **Risk 标记** (`bits<`, `wchar[`, `f80[`) 使用普通子串匹配 — 这些标记不存在子串碰撞风险
- **Warning 标记** 使用 `contains_token()` — 防止 `nullptr[` 被 `ptr[` 误匹配

### 8.3 匹配算法一致性验证

编译期 `FixedString::contains_token()` 与运行时 `contains_token(string_view, string_view)` 的算法语义**完全等价**：

| 属性 | 编译期 (`FixedString`) | 运行时 (`string_view`) |
|------|:---:|:---:|
| 算法 | 逐位置匹配 + 前字符检查 | 逐位置匹配 + 前字符检查 |
| 位置 0 匹配 | `return true` | `return true` |
| 前缀判断 | `is_alpha(value[i-1])` | `(prev >= 'a' && prev <= 'z') \|\| (prev >= 'A' && prev <= 'Z')` |
| 误匹配跳过 | `++i` 继续搜索 | `pos = found + 1` 继续搜索 |
| 修复背景 | 防止 `nullptr[s:8,a:8]` 中的 `ptr[` 被误匹配 | 同上 |

### 8.4 XOffset 域特定策略

XOffset 没有复制任何 TypeLayout 的签名扫描代码，仅添加了两个**不可能在 TypeLayout 层实现**的域特定策略：

| 策略 | 实现位置 | 说明 | 为何不能在 TypeLayout 实现 |
|------|----------|------|--------------------------|
| **Warning→Risk 升级** | `classify_for_xoffset<T>()` | TypeLayout 的 Warning（指针/union/vptr）在 XOffset 中升级为 Risk | TypeLayout 是通用库，Warning 在某些场景（同平台 IPC）是可接受的 |
| **long/unsigned long 前置拦截** | `classify_for_xoffset<T>()` | 在 TypeLayout 分类前拦截裸 `long` | TypeLayout 签名已将 `long` 脱糖为 `i32`/`i64`，无法区分 |

### 8.5 容器穿透机制

XOffset 容器（`XVector`, `XString`, `XSet`, `XMap`）通过 `XOFFSET_REGISTER_*` 宏注册，
内部调用 `TYPELAYOUT_OPAQUE_*` 宏。这使得 TypeLayout 的签名引擎自动嵌入元素类型签名，
`classify_safety<XVector<T>>()` 会自动递归检查 `T` 的安全性 — **无需 XOffset 手写递归逻辑**。

```
用户调用:  is_xbuffer_safe<XVector<MyStruct>>::value
   │
   ├─ classify_for_xoffset<XVector<MyStruct>>()
   │     └─ classify_safety<XVector<MyStruct>>()   // TypeLayout
   │           └─ 签名: "vector[s:32,a:8]<record[...]{...}>"
   │                 └─ 扫描签名中的 9 个标记 → Safe/Warning/Risk
   │
   └─ Warning→Risk 升级 (如果有 Warning)
```

### 8.6 C1/C2 分离

Serialization-free 保证由两个独立的检查组成：

| 层 | 检查 | 实施位置 | 阶段 |
|----|------|----------|------|
| **C2: 本地安全** | `classify_for_xoffset<T>() == Safe` | XOffset 编译期 | `static_assert` |
| **C1: 跨平台布局匹配** | `TYPELAYOUT_ASSERT_COMPAT(a, b)` | TypeLayout CI 层 | 跨平台 `.sig.hpp` 对比 |

两者之间无逻辑耦合：C2 在每个平台独立判定，C1 在 CI 中比较多平台的签名快照。

### 8.7 诊断函数 vs 判定函数

`get_safety_error_message<T>()` 内部使用了 `is_polymorphic_v`、`is_union_v`、`is_pointer_v` 等 type_traits，但这些**仅用于生成友好的编译错误信息**，不参与实际的安全判定流程。

实际判定的**唯一入口**是：
```
is_xbuffer_safe<T>::value
  → detail::is_safe_type<T>()
    → detail::is_xbuffer_compatible<T, DefaultPolicy>()
      → DefaultPolicy::accept<T>()
        → classify_for_xoffset<T>()
          → classify_safety<T>()     ← TypeLayout ground truth
```

### 8.8 已知的设计限制

以下限制均已文档化，不是 bug：

| # | 限制 | 原因 | 缓解措施 |
|---|------|------|----------|
| L1 | `long` 埋入结构体成员不可检测 | P2996 `type_of()` 对成员执行 desugaring | 编码规范 + `XBUFFER_ASSERT_NO_LONG` + C1 CI |
| L2 | 非多态虚继承的 vbase 指针不可检测 | P2996 将 vbase 指针表现为 padding | C1 跨平台签名对比可捕获布局差异 |
| ~~L3~~ | ~~枚举类型未测试~~ | ✅ **已解决** (v1.3) | `test_enum_support.cpp` 覆盖 7 个测试场景 |

### 8.9 验证矩阵

| 维度 | 结果 | 详情 |
|------|:---:|------|
| **标记集一致** | ✅ | 9 个标记在编译期/运行时完全相同 |
| **分类级别一致** | ✅ | Risk/Warning 归属完全对齐 |
| **匹配算法一致** | ✅ | `contains_token` 边界语义两套实现完全等价 |
| **无重复扫描** | ✅ | XOffset 不包含任何自己的签名扫描代码 |
| **域策略合理** | ✅ | 两个策略都是 TypeLayout 层无法实现的 |
| **容器穿透统一** | ✅ | 通过 `TYPELAYOUT_OPAQUE_*` 一体化，无手工递归 |
| **C1/C2 分离清晰** | ✅ | C2 本地安全（编译期）+ C1 跨平台布局（CI 层） |

### 8.10 结论

> **XOffsetDatastructure 和 TypeLayout 的跨平台兼容检测已完全整合统一。**
>
> TypeLayout 是**签名引擎和安全分类器**（Single Source of Truth），
> XOffset 是**域策略层**（Warning→Risk 升级 + long 拦截）。
> 两者职责分明，无逻辑冗余，无标记遗漏。
>
> 32/32 测试在 P2996 Docker 环境中全部通过，验证了整合的正确性。

---

## 附录 A: `COMPILETIME_SAFETY_API_ANALYSIS.md` 归档说明

> **⚠️ 归档**: `docs/COMPILETIME_SAFETY_API_ANALYSIS.md` (v1.0, 2026-02-10) 描述的是
> **Safety 集成之前**的旧架构，其中 `is_xbuffer_safe<T>` 有独立的反射扫描逻辑。
> 自 commit `6f4f7a8d` 起，该文档的核心分析（§2 能力矩阵对比、§3 候选设计方案、§4 推荐方案C）
> **已不再适用**。
>
> 现有架构的权威参考为本文档的 §7 和 §8。
>
> `COMPILETIME_SAFETY_API_ANALYSIS.md` 保留作为历史记录，不再更新。
