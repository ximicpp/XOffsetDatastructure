# TypeLayout 功能缺口分析：从 XOffsetDatastructure 需求出发

> **版本**: v1.0  
> **分析日期**: 2026-02-10  
> **状态**: ✅ 完成  
> **TypeLayout 版本**: Git submodule @ main  
> **XOffsetDatastructure 版本**: next_cpp26

---

## 1. 当前集成概况

### 1.1 XOffsetDatastructure 对 TypeLayout 的使用清单

| # | 使用方式 | 文件 | 代码行 | TypeLayout API |
|---|---------|------|--------|----------------|
| 1 | 编译时二进制合约 | `player.hpp:31` | `static_assert(get_definition_signature<Player>() == "..."` | `get_definition_signature<T>()` |
| 2 | 编译时二进制合约 | `game_data.hpp:70` | `static_assert(get_definition_signature<Item>() == "..."` | `get_definition_signature<T>()` |
| 3 | 编译时二进制合约 | `game_data.hpp:80` | `static_assert(get_definition_signature<GameData>() == "..."` | `get_definition_signature<T>()` |
| 4 | 成员计数（Compactor） | `xoffsetdatastructure2.hpp:547` | `boost::typelayout::get_member_count<T>()` | `get_member_count<T>()` |
| 5 | 容器 Opaque 特化 | `xoffsetdatastructure2.hpp:901-935` | `TypeSignature<XString, Mode>` 等 4 个特化 | `TypeSignature<T, Mode>` 扩展点 |
| 6 | 签名导出工具 | `tools/export_signatures.cpp` | `TYPELAYOUT_EXPORT_TYPES(Player, Item, GameData)` | `SigExporter` |
| 7 | 兼容性检查工具 | `tools/check_compat.cpp` | `TYPELAYOUT_CHECK_COMPAT(x86_64_linux_clang)` | `CompatReporter` |

### 1.2 XOffsetDatastructure 中独立实现（未使用 TypeLayout）的反射功能

| # | 功能 | 代码位置 | 行数 | 与 TypeLayout 的关系 |
|---|------|---------|------|---------------------|
| A | `is_xbuffer_safe<T>` 类型安全检查 | `xoffsetdatastructure2.hpp:553-812` | ~260 行 | TypeLayout 有 `classify_safety()` 但仅运行时且基于字符串 |
| B | `XBufferCompactor` 成员迭代 | `xoffsetdatastructure2.hpp:519-550` | ~30 行 | TypeLayout 内部有 `definition_fields<T>()` 但未作为 API |
| C | `detail::is_basic_type<T>()` | `xoffsetdatastructure2.hpp:555-569` | ~15 行 | TypeLayout 按类型特化，无 `is_basic_type` 概念 |
| D | 容器类型检测 concepts | `xoffsetdatastructure2.hpp:270-311` | ~40 行 | TypeLayout 无容器检测能力（纯签名生成） |
| E | Type-erased 容器黑名单 | `xoffsetdatastructure2.hpp:588-638` | ~50 行 | TypeLayout 无此能力 |

---

## 2. 功能缺口详细分析

### 2.1 🔴 [P0] 编译时签名哈希 API

**需求来源**

`game_data.hpp` 中的 `static_assert` 用完整字符串比较 GameData 签名，该字符串长达 **290 字符**：

```cpp
// game_data.hpp:80-92 — 当前写法（13 行，290+ 字符的签名字符串）
static_assert(boost::typelayout::get_definition_signature<GameData>() ==
    "[64-le]record[s:144,a:8]{"
    "@0[player_id]:i32[s:4,a:4],"
    "@4[level]:i32[s:4,a:4],"
    "@8[health]:f32[s:4,a:4],"
    "@16[player_name]:string[s:32,a:8],"
    "@48[items]:vector[s:32,a:8]<record[s:48,a:8]{"
        "@0[item_id]:i32[s:4,a:4],"
        "@4[item_type]:i32[s:4,a:4],"
        "@8[quantity]:i32[s:4,a:4],"
        "@16[name]:string[s:32,a:8]}>,"
    "@80[achievements]:set[s:32,a:8]<i32[s:4,a:4]>,"
    "@112[quest_progress]:map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>}",
    "Type signature mismatch for GameData");
```

**问题**：
1. 签名字符串必须手动维护，任何字段变更都要手动重新生成
2. 嵌套类型（如 `XVector<Item>`）签名递归展开，导致指数级增长
3. 多个 `static_assert` 中重复相同签名（Item 的签名既出现在 Item 的 assert 中，也内嵌在 GameData 的 assert 中）

**建议 TypeLayout 添加的 API**

```cpp
namespace boost::typelayout {
    // FNV-1a 或 xxHash 的 consteval 实现
    template <typename T>
    [[nodiscard]] consteval uint64_t definition_signature_hash() noexcept;

    template <typename T>
    [[nodiscard]] consteval uint64_t layout_signature_hash() noexcept;
}
```

**使用示例（XOffsetDatastructure 场景）**

```cpp
// 替换前：13 行 290 字符
static_assert(boost::typelayout::get_definition_signature<GameData>() == "...", "...");

// 替换后：1 行，哈希值可以通过工具一次性生成
static_assert(boost::typelayout::definition_signature_hash<GameData>() == 0x8A3F7B2C1D4E5F60ULL,
              "GameData binary layout changed! Run `export_signatures` to see diff.");
```

**实现难度**: 低。`FixedString<N>` 已经有 `value` 数组可以遍历，实现 consteval FNV-1a 只需 ~15 行。

**注意事项**: 哈希值碰撞风险极低（64-bit FNV-1a 对 <1KB 签名字符串的碰撞概率 < 1e-18），但建议同时保留字符串比较 API 用于调试。

---

### 2.2 🔴 [P0] 签名差异诊断 API

**需求来源**

当 `static_assert` 签名验证失败时，开发者只能看到：
```
error: static assertion failed: Type signature mismatch for Player
```

但无法得知**哪个字段**变化了。在拥有 10+ 字段的真实数据结构中，需要手动对比两行 200+ 字符的签名来定位差异。

**建议 TypeLayout 添加的 API**

```cpp
namespace boost::typelayout {
    // 运行时差异报告（用于测试/诊断）
    struct SignatureDiff {
        struct FieldChange {
            std::string field_name;
            std::string old_value;
            std::string new_value;
            enum Kind { Added, Removed, Modified, Reordered } kind;
        };
        bool match;
        std::vector<FieldChange> changes;
    };

    // 运行时 API：比较两个签名字符串
    SignatureDiff diff_signatures(std::string_view sig_a, std::string_view sig_b);

    // 编译时 API（受限）：只能报告"第一个不匹配的字节位置"
    template <typename T1, typename T2>
    consteval std::size_t first_signature_mismatch_pos() noexcept;
}
```

**使用示例**

```cpp
// 运行时测试中
auto diff = boost::typelayout::diff_signatures(
    old_sig.data(), 
    boost::typelayout::get_definition_signature<Player>().value
);
if (!diff.match) {
    for (const auto& change : diff.changes) {
        std::cout << "  Field '" << change.field_name << "': "
                  << change.old_value << " → " << change.new_value << "\n";
    }
}
```

**实现难度**: 中。需要实现签名解析器（tokenizer），但签名格式是 TypeLayout 自己定义的，所以完全可控。

**分阶段策略**:
- Phase 1: 简单的逐字符比较，报告第一个不匹配的位置和周围上下文
- Phase 2: 完整的字段级 diff（需要签名解析器）

---

### 2.3 🔴 [P0] Opaque 容器特化辅助宏

**需求来源**

XOffsetDatastructure 当前为 4 种容器类型手写了 4 个 `TypeSignature` 特化：

```cpp
// xoffsetdatastructure2.hpp:901-935 — 当前写法
// XString 特化 (7 行)
template <SignatureMode Mode>
struct TypeSignature<XOffsetDatastructure2::XString, Mode> {
    static consteval auto calculate() noexcept {
        return FixedString{"string[s:32,a:8]"};
    }
};

// XVector 特化 (8 行)
template <typename T, SignatureMode Mode>
struct TypeSignature<XOffsetDatastructure2::XVector<T>, Mode> {
    static consteval auto calculate() noexcept {
        return FixedString{"vector[s:32,a:8]<"} +
               TypeSignature<T, Mode>::calculate() +
               FixedString{">"};
    }
};

// XSet 特化 (8 行) — 与 XVector 几乎相同
// XMap 特化 (9 行) — K,V 版本
```

**问题**：
- 4 个特化共 ~35 行，模式高度重复
- 新增容器类型（如未来的 `XDeque`）需要复制粘贴
- 所有特化都忽略了 `SignatureMode`（Layout 和 Definition 返回相同结果），这是有意的但不明显

**建议 TypeLayout 添加的 API**

```cpp
// 方案 A: 宏（最简洁，推荐）
// 用于不含元素类型的容器（如 XString）
#define TYPELAYOUT_OPAQUE_TYPE(Type, name, size, align)             \
    template <SignatureMode Mode>                                     \
    struct TypeSignature<Type, Mode> {                               \
        static consteval auto calculate() noexcept {                 \
            return FixedString{name "[s:" #size ",a:" #align "]"};   \
        }                                                            \
    };

// 用于含 1 个元素类型的容器（如 XVector<T>）
#define TYPELAYOUT_OPAQUE_CONTAINER(Template, name, size, align)     \
    template <typename T, SignatureMode Mode>                         \
    struct TypeSignature<Template<T>, Mode> {                        \
        static consteval auto calculate() noexcept {                 \
            return FixedString{name "[s:" #size ",a:" #align "]<"} + \
                   TypeSignature<T, Mode>::calculate() +             \
                   FixedString{">"};                                 \
        }                                                            \
    };

// 用于含 2 个类型参数的容器（如 XMap<K,V>）
#define TYPELAYOUT_OPAQUE_MAP(Template, name, size, align)           \
    template <typename K, typename V, SignatureMode Mode>             \
    struct TypeSignature<Template<K, V>, Mode> {                     \
        static consteval auto calculate() noexcept {                 \
            return FixedString{name "[s:" #size ",a:" #align "]<"} + \
                   TypeSignature<K, Mode>::calculate() +             \
                   FixedString{","} +                                \
                   TypeSignature<V, Mode>::calculate() +             \
                   FixedString{">"};                                 \
        }                                                            \
    };
```

**使用示例（XOffsetDatastructure 替换后）**

```cpp
// 替换前：35 行
// 替换后：4 行
namespace boost { namespace typelayout {
    TYPELAYOUT_OPAQUE_TYPE(XOffsetDatastructure2::XString, "string", 32, 8)
    TYPELAYOUT_OPAQUE_CONTAINER(XOffsetDatastructure2::XVector, "vector", 32, 8)
    TYPELAYOUT_OPAQUE_CONTAINER(XOffsetDatastructure2::XSet, "set", 32, 8)
    TYPELAYOUT_OPAQUE_MAP(XOffsetDatastructure2::XMap, "map", 32, 8)
}}
```

**实现难度**: 极低。纯宏定义，~20 行新增代码。

---

### 2.4 🟡 [P1] 签名版本标识

**需求来源**

TypeLayout 的签名格式（如 `record[s:N,a:M]{@offset[name]:type...}`）是隐式定义的。如果 TypeLayout 升级时修改了签名格式（例如将 `record` 改为 `struct`，或改变偏移表示方式），所有 `static_assert` 都会失败，且无法区分是"类型真的变了"还是"签名格式变了"。

XOffsetDatastructure 通过 Git submodule 锁定 TypeLayout 版本可以临时规避此问题，但长期来看需要签名格式的版本承诺。

**建议 TypeLayout 添加的 API**

```cpp
namespace boost::typelayout {
    // 编译时版本常量
    inline constexpr int signature_format_version = 1;
    
    // 签名中包含版本标识（可选）
    // 格式: [v1][64-le]record{...}
    template <typename T>
    [[nodiscard]] consteval auto get_versioned_definition_signature() noexcept {
        return FixedString{"[v"} + to_fixed_string(signature_format_version) + 
               FixedString{"]"} + get_definition_signature<T>();
    }
}
```

**使用示例**

```cpp
// 检测格式版本
static_assert(boost::typelayout::signature_format_version == 1,
              "TypeLayout signature format changed! Update all static_assert strings.");
```

**实现难度**: 极低。添加一个常量 + 可选的包装函数。

---

### 2.5 🟡 [P1] 编译时 Safety 分级 API

**需求来源**

XOffsetDatastructure 的 `is_xbuffer_safe<T>` 用 ~260 行代码实现了以下检查：
- 检测基本类型（int8_t ~ double, bool, char）→ Safe
- 检测 XString/XVector/XSet/XMap → Safe（如果元素也安全）
- 检测指针/引用 → Unsafe
- 检测虚函数/继承 → Unsafe
- 检测联合体 → Unsafe
- 检测 type-erased 容器（std::function, std::any, shared_ptr 等）→ Unsafe
- 递归检查所有成员

TypeLayout 的 `classify_safety()` 已经能通过**签名字符串扫描**检测指针、位域、vptr，但它是运行时 API，且粒度较粗（Safe/Warning/Risk 三级）。

**缺口**：
1. TypeLayout 的 `classify_safety()` 是运行时的，不能用于 `static_assert`
2. TypeLayout 不知道哪些容器类型是"安全的"（这是 XOffsetDatastructure 的领域知识）
3. TypeLayout 没有"类型黑名单"概念（如 std::function 是不安全的）

**建议 TypeLayout 添加的 API**

```cpp
namespace boost::typelayout {
    // 编译时 safety 分类（基于反射，不是字符串扫描）
    enum class CompileTimeSafety {
        TriviallyPortable,  // POD，无指针，无位域
        HasPointers,        // 包含指针类型
        HasBitFields,       // 包含位域
        HasVTable,          // 多态类型
        HasUnion,           // 包含联合体
    };
    
    template <typename T>
    [[nodiscard]] consteval CompileTimeSafety classify_type_safety() noexcept;
    
    // 成员级诊断
    template <typename T>
    [[nodiscard]] consteval bool has_pointer_members() noexcept;
    
    template <typename T>
    [[nodiscard]] consteval bool has_bitfield_members() noexcept;
    
    template <typename T>  
    [[nodiscard]] consteval bool is_trivially_portable() noexcept;
}
```

**使用示例（XOffsetDatastructure 简化后）**

```cpp
// 可以将部分检查委托给 TypeLayout
template<typename T>
consteval bool is_safe_type() {
    // TypeLayout 负责结构层面的安全检查
    if constexpr (!boost::typelayout::is_trivially_portable<T>()) {
        if constexpr (boost::typelayout::has_pointer_members<T>()) return false;
        if constexpr (std::is_polymorphic_v<T>) return false;
    }
    // XOffsetDatastructure 负责容器特定检查
    if constexpr (is_xstring<T>()) return true;
    if constexpr (is_safe_xvector<T>()) return true;
    // ...
}
```

**实现难度**: 中。需要在 `signature_detail.hpp` 中遍历成员时收集安全性信息。

**注意**: TypeLayout 不应该知道 XOffsetDatastructure 的容器类型。正确的设计是 TypeLayout 提供**结构层面的安全分级**，XOffsetDatastructure 在此基础上叠加**容器特定规则**。

---

### 2.6 🟡 [P1] 编译时成员迭代工具

**需求来源**

XOffsetDatastructure 的 `XBufferCompactor::migrate_members` 和 `detail::are_all_members_safe` 都需要遍历类型的所有非静态成员。当前写法：

```cpp
// xoffsetdatastructure2.hpp:519-550
template<typename T, std::size_t Index>
static consteval auto get_member_at() {
    using namespace std::meta;
    auto members = nonstatic_data_members_of(^^T, access_context::unchecked());
    return members[Index];
}

template<typename T, std::size_t Index>
static void migrate_member_at(const T& old_obj, T& new_obj, ...) {
    constexpr auto member = get_member_at<T, Index>();
    using MemberType = [:type_of(member):];
    const auto& old_member = old_obj.[:member:];
    auto& new_member = new_obj.[:member:];
    migrate_member<MemberType>(old_member, new_member, ...);
}

template<typename T, std::size_t... Is>
static void migrate_members_impl(const T& old_obj, T& new_obj, ...,
                                  std::index_sequence<Is...>) {
    (migrate_member_at<T, Is>(old_obj, new_obj, ...), ...);
}
```

TypeLayout 内部的 `definition_fields<T>()` 和 `get_layout_content<T>()` 有完全相同的成员遍历模式，但只输出签名字符串，不暴露"对每个成员执行回调"的能力。

**建议 TypeLayout 添加的 API**

```cpp
namespace boost::typelayout {
    // 成员信息结构（编译时）
    template <typename T, std::size_t Index>
    struct MemberInfo {
        using type = /* [:type_of(member):] */;
        static constexpr auto name = get_member_name<member, Index>();
        static constexpr std::size_t offset = offset_of(member).bytes;
        static constexpr bool is_bit_field = is_bit_field(member);
    };
    
    // 编译时 for_each (通过回调模板)
    template <typename T, template<typename, std::size_t> class Visitor>
    constexpr void for_each_member();
    
    // 或者更实用的：获取第 N 个成员的类型和偏移
    template <typename T, std::size_t N>
    using member_type_at = /* ... */;
    
    template <typename T, std::size_t N>
    consteval std::size_t member_offset_at() noexcept;
}
```

**使用示例**

```cpp
// XBufferCompactor 可以简化为
template<typename T, std::size_t Index>
static void migrate_member_at(const T& old_obj, T& new_obj, ...) {
    using MemberType = boost::typelayout::member_type_at<T, Index>;
    constexpr std::size_t offset = boost::typelayout::member_offset_at<T, Index>();
    // ... 或者直接使用 P2996 splice（因为 TypeLayout 内部也是这样实现的）
}
```

**实现难度**: 中。已有内部实现，需要提升为 public API 并设计合适的接口。

**设计决策**: 这里有一个张力——TypeLayout 是"签名库"而非"通用反射库"。暴露成员迭代可能超出其职责范围。建议**只暴露最小接口**（`member_type_at` + `member_offset_at`），不提供回调式 for_each。

---

### 2.7 🟢 [P2] 签名格式化/美化输出

**需求来源**

GameData 的 Definition 签名有 290+ 字符，在测试日志和调试输出中难以阅读：

```
[64-le]record[s:144,a:8]{@0[player_id]:i32[s:4,a:4],@4[level]:i32[s:4,a:4],@8[health]:f32[s:4,a:4],@16[player_name]:string[s:32,a:8],@48[items]:vector[s:32,a:8]<record[s:48,a:8]{@0[item_id]:i32[s:4,a:4],@4[item_type]:i32[s:4,a:4],@8[quantity]:i32[s:4,a:4],@16[name]:string[s:32,a:8]}>,@80[achievements]:set[s:32,a:8]<i32[s:4,a:4]>,@112[quest_progress]:map[s:32,a:8]<string[s:32,a:8],i32[s:4,a:4]>}
```

**建议 TypeLayout 添加的 API**

```cpp
namespace boost::typelayout {
    // 运行时 pretty-print（不需要编译时）
    std::string pretty_print_signature(std::string_view sig, int indent = 2);
}
```

**美化后输出示例**

```
[64-le]record[s:144,a:8]{
  @0  [player_id]      : i32[s:4,a:4]
  @4  [level]           : i32[s:4,a:4]
  @8  [health]          : f32[s:4,a:4]
  @16 [player_name]     : string[s:32,a:8]
  @48 [items]           : vector[s:32,a:8]<
    record[s:48,a:8]{
      @0  [item_id]     : i32[s:4,a:4]
      @4  [item_type]   : i32[s:4,a:4]
      @8  [quantity]    : i32[s:4,a:4]
      @16 [name]        : string[s:32,a:8]
    }>
  @80 [achievements]    : set[s:32,a:8]<i32[s:4,a:4]>
  @112[quest_progress]  : map[s:32,a:8]<string[s:32,a:8], i32[s:4,a:4]>
}
```

**实现难度**: 低。字符串解析 + 缩进渲染，~80 行。

---

### 2.8 🟢 [P2] 枚举类型 Trivial Safety 检查

**需求来源**

XOffsetDatastructure 的 `is_safe_type<T>()` 未处理枚举类型。在游戏数据中枚举很常见（如 `enum class WeaponType : uint8_t`），且固定宽度枚举在所有平台上都是二进制安全的。

TypeLayout 已经支持枚举签名（`enum<QualifiedName>[s:N,a:M]<underlying_type>`），但没有提供"枚举是否 trivially portable"的判断工具。

**建议 TypeLayout 添加的 API**

```cpp
namespace boost::typelayout {
    // 检查枚举是否有固定底层类型（C++11 起的 scoped enum 总是有的）
    template <typename T>
    [[nodiscard]] consteval bool is_fixed_enum() noexcept {
        static_assert(std::is_enum_v<T>);
        return std::is_scoped_enum_v<T> || /* 检查底层类型是否固定 */;
    }
}
```

**实现难度**: 极低。`std::is_enum_v<T>` + `std::underlying_type_t<T>` 即可判断。

---

### 2.9 🟢 [P2] 紧凑数组签名模式

**需求来源**

TypeLayout 的数组签名很详细：
```
array[s:40,a:4]<i32[s:4,a:4],10>
```

对于 XOffsetDatastructure 的使用场景（`static_assert` 签名对比），这种详细格式导致签名字符串膨胀。

**建议**: 低优先级。当前详细格式是正确的行为。如果确实需要紧凑模式，可以考虑：

```cpp
// 可选的紧凑模式
template <typename T>
consteval auto get_compact_signature() noexcept;
// 输出: i32[10] 而不是 array[s:40,a:4]<i32[s:4,a:4],10>
```

**实现难度**: 低，但需要引入第三种 `SignatureMode`，可能增加 API 复杂度。

**建议**: 暂不实现。用签名哈希（2.1）可以更好地解决签名膨胀问题。

---

## 3. 优先级排序与依赖关系

### 3.1 实施路线图

```
Phase 1 (立即可做，无依赖)
├── [P0] 2.3 Opaque 容器特化辅助宏    ← 最简单，立即收益
├── [P1] 2.4 签名版本标识              ← 极简单
└── [P2] 2.8 枚举 Trivial Safety 检查  ← 极简单

Phase 2 (核心功能增强)
├── [P0] 2.1 编译时签名哈希 API        ← 依赖 FixedString<N> 已有
├── [P0] 2.2 签名差异诊断 API (Phase1) ← 简单的逐字符比较
└── [P2] 2.7 签名格式化/美化输出       ← 运行时工具

Phase 3 (高级功能)
├── [P0] 2.2 签名差异诊断 API (Phase2) ← 需要签名解析器
├── [P1] 2.5 编译时 Safety 分级 API    ← 需要新的反射遍历
└── [P1] 2.6 编译时成员迭代工具        ← 需要设计决策
```

### 3.2 依赖关系图

```
2.3 Opaque 宏 ─────────────────────── 无依赖
2.4 签名版本 ──────────────────────── 无依赖
2.8 枚举检查 ──────────────────────── 无依赖

2.1 签名哈希 ─────── 依赖 FixedString<N> (已有)
2.7 Pretty-print ──── 无依赖

2.2 签名 Diff ────── Phase 2 依赖 2.7 的签名解析器
2.5 Safety 分级 ──── 依赖 P2996 反射
2.6 成员迭代 ─────── 依赖 P2996 反射；2.5 可以复用其实现
```

### 3.3 工作量估算

| # | 功能 | 优先级 | 新增代码量 | 实现难度 | 预估工时 |
|---|------|--------|-----------|---------|---------|
| 2.3 | Opaque 容器宏 | P0 | ~25 行 | 极低 | 0.5h |
| 2.1 | 签名哈希 API | P0 | ~30 行 | 低 | 1h |
| 2.2 | 签名 Diff (Phase 1) | P0 | ~40 行 | 低 | 1h |
| 2.4 | 签名版本标识 | P1 | ~10 行 | 极低 | 0.25h |
| 2.5 | 编译时 Safety | P1 | ~80 行 | 中 | 3h |
| 2.6 | 成员迭代工具 | P1 | ~50 行 | 中 | 2h |
| 2.7 | Pretty-print | P2 | ~80 行 | 低 | 1.5h |
| 2.8 | 枚举检查 | P2 | ~10 行 | 极低 | 0.25h |
| 2.9 | 紧凑数组签名 | P2 | ~30 行 | 低 | 1h |
| | **合计** | | **~355 行** | | **~10.5h** |

---

## 4. 对 XOffsetDatastructure 的预期收益

| 当前痛点 | TypeLayout 新功能 | 预期改善 |
|---------|------------------|---------|
| 290 字符的 static_assert 签名字符串 | 签名哈希 API | 缩减到 1 行 16 字符哈希 |
| 签名不匹配时无诊断信息 | 签名 Diff API | 精确定位变更字段 |
| 4 个容器特化 35 行重复代码 | Opaque 容器宏 | 缩减到 4 行宏调用 |
| is_xbuffer_safe 260 行独立实现 | 编译时 Safety API | 可委托 ~50% 检查逻辑给 TypeLayout |
| XBufferCompactor 手动反射遍历 | 成员迭代工具 | 减少 ~15 行模板代码 |
| 签名格式升级兼容性未知 | 签名版本标识 | 明确格式变更检测 |
| 测试输出中签名难以阅读 | Pretty-print | 结构化缩进输出 |

---

## 5. 设计原则建议

在实现以上功能时，建议 TypeLayout 遵循以下原则：

1. **保持纯编译时 API 优先**：TypeLayout 的核心价值是 `consteval`。新 API 应尽量保持 consteval。
2. **不引入领域知识**：TypeLayout 不应知道 XString, XVector 等容器。Opaque 宏只是语法糖，不引入语义依赖。
3. **向后兼容**：签名格式不应在 minor 版本中变化。签名版本标识 (2.4) 可以作为格式升级的安全网。
4. **最小 API 表面**：成员迭代工具 (2.6) 应只暴露 `member_type_at` 和 `member_offset_at`，不提供高层抽象（高层抽象应由使用者自行构建）。
5. **工具层保持 C++17**：所有 `tools/` 目录下的新增功能（Pretty-print, Diff）应保持 C++17 兼容。
