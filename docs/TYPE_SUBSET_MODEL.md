# XOffset Safe Type Subset Model

> **版本**: v1.1  
> **日期**: 2026-02-11  
> **状态**: 进行中

---

## §1 概念框架

### 1.1 核心模型：两个显式集合的交集

XOffsetDatastructure 的零拷贝安全性由**两个正交的显式集合**共同定义：

```
零拷贝安全 = 平台 ∈ A (架构集合)  ∧  类型 T ∈ S (安全类型集合)
二进制兼容 = 零拷贝安全  ∧  sig(T₁) == sig(T₂)  (签名一致)
```

| 集合 | 定义 | 验证机制 | 关联工具 |
|------|------|----------|----------|
| **A** (Target Architecture) | 数据在什么环境下存在 | `static_assert` 编译门控 | TypeLayout `[64-le]` 前缀 |
| **S** (Safe Type Subset) | 什么类型的数据可以存储 | `is_xbuffer_safe<T>` 编译时检查 | TypeLayout Definition Signature |

### 1.2 四个正交问题

当前系统回答四个正交问题，每个由不同的机制实现：

```
Q0. 架构：Platform ∈ A ?          → TargetArchitecture      (XOffset, 编译门控)
Q1. 准入：Type T ∈ S ?            → is_xbuffer_safe<T>      (XOffset, 类型检查)
Q2. 兼容：sig(T₁) == sig(T₂) ?    → TypeLayout signatures   (TypeLayout, 签名比较)
Q3. 迁移：如何搬运 T 的实例 ?       → XBufferCompactor        (XOffset, 反射迁移)
```

Q0 保证架构一致性，Q1 保证类型可重定位性，Q2 保证跨编译单元/跨版本兼容，Q3 提供迁移执行能力。**Q1 和 Q3 应基于同一个类型分类。**

---

## §1A 架构集合 (Target Architecture)

### 1A.1 问题

当前架构约束通过 15+ 个散落的 `static_assert` 和 `#error` 隐式定义（lines 4-80），意图不明确：

```cpp
// 当前：隐式约束，散落在文件头部
#if !XOFFSET_ARCH_64BIT
    #error "..."       // line 32
#endif
static_assert(sizeof(void*) == 8);   // line 77
static_assert(sizeof(int32_t) == 4); // line 69
// ... 还有 14 个类似的断言
```

这些断言回答的是"当前平台是否满足条件"，但**没有声明"目标架构是什么"**。

### 1A.2 显式架构定义（多预设支持）

架构定义为 `constexpr` 结构体，预置多种常见架构，通过 `using` 选择当前目标：

```cpp
namespace XOffsetDatastructure {

    /// 架构规格描述符
    struct ArchSpec {
        std::size_t pointer_size;
        bool        little_endian;
        std::size_t sizeof_int8;
        std::size_t sizeof_int16;
        std::size_t sizeof_int32;
        std::size_t sizeof_int64;
        std::size_t sizeof_float;
        std::size_t sizeof_double;
        std::size_t sizeof_bool;
        std::size_t sizeof_char;
        std::size_t pointer_align;
    };

    // ═══════════════════════════════════════════════════
    //  Architecture Presets (预置架构集合)
    // ═══════════════════════════════════════════════════

    /// 64-bit Little-Endian (x86-64 Linux/macOS/Windows)
    inline constexpr ArchSpec Arch64LE = {
        .pointer_size  = 8,
        .little_endian = true,
        .sizeof_int8   = 1, .sizeof_int16 = 2,
        .sizeof_int32  = 4, .sizeof_int64 = 8,
        .sizeof_float  = 4, .sizeof_double = 8,
        .sizeof_bool   = 1, .sizeof_char  = 1,
        .pointer_align = 8,
    };

    /// 64-bit Big-Endian (SPARC64, s390x, PowerPC64 BE)
    inline constexpr ArchSpec Arch64BE = {
        .pointer_size  = 8,
        .little_endian = false,
        .sizeof_int8   = 1, .sizeof_int16 = 2,
        .sizeof_int32  = 4, .sizeof_int64 = 8,
        .sizeof_float  = 4, .sizeof_double = 8,
        .sizeof_bool   = 1, .sizeof_char  = 1,
        .pointer_align = 8,
    };

    /// 32-bit Little-Endian (x86, ARM32 LE)
    inline constexpr ArchSpec Arch32LE = {
        .pointer_size  = 4,
        .little_endian = true,
        .sizeof_int8   = 1, .sizeof_int16 = 2,
        .sizeof_int32  = 4, .sizeof_int64 = 8,
        .sizeof_float  = 4, .sizeof_double = 8,
        .sizeof_bool   = 1, .sizeof_char  = 1,
        .pointer_align = 4,
    };

    /// 32-bit Big-Endian (MIPS BE, PowerPC32)
    inline constexpr ArchSpec Arch32BE = {
        .pointer_size  = 4,
        .little_endian = false,
        .sizeof_int8   = 1, .sizeof_int16 = 2,
        .sizeof_int32  = 4, .sizeof_int64 = 8,
        .sizeof_float  = 4, .sizeof_double = 8,
        .sizeof_bool   = 1, .sizeof_char  = 1,
        .pointer_align = 4,
    };

    // ═══════════════════════════════════════════════════
    //  Active Target (当前目标架构)
    // ═══════════════════════════════════════════════════
    
    /// 选择当前目标架构。修改此行即可切换预设。
    inline constexpr ArchSpec TargetArchitecture = Arch64LE;
}
```

**设计要点**：
- `ArchSpec` 是纯数据描述符（POD），不含逻辑
- 预设为 `constexpr` 常量，零运行时开销
- `TargetArchitecture` 是一个 `using` 级别的选择，修改一行切换目标
- 用户可自定义 `ArchSpec` 描述非标准平台

### 1A.3 平台验证（引用架构定义）

```cpp
// 平台验证：当前编译环境 ∈ 目标架构集合
static_assert(sizeof(void*) == TargetArchitecture::pointer_size,
    "XOffset requires 64-bit architecture");
static_assert(IS_LITTLE_ENDIAN == TargetArchitecture::little_endian,
    "XOffset requires little-endian");
static_assert(sizeof(int32_t) == TargetArchitecture::sizeof_int32);
static_assert(sizeof(double)  == TargetArchitecture::sizeof_double);
// ... 所有断言引用 TargetArchitecture 常量，而非硬编码魔数
```

### 1A.4 与 TypeLayout 的对称性

```
XOffset TargetArchitecture          TypeLayout PlatformInfo
┌───────────────────────┐          ┌───────────────────────┐
│ pointer_size = 8      │  ══════  │ pointer_size          │
│ little_endian = true  │  ══════  │ arch_prefix = [64-le] │
│ sizeof_int32 = 4      │          │ sizeof_long           │
│ sizeof_double = 8     │          │ sizeof_wchar_t        │
│ pointer_align = 8     │          │ sizeof_long_double    │
│                       │          │ max_align             │
│ (编译时常量,声明式)    │          │ (运行时/导出时数据)    │
└───────────────────────┘          └───────────────────────┘
   XOffset: "我要什么"               TypeLayout: "平台是什么"
```

TypeLayout 的 `PlatformInfo` 记录**实际平台属性**，XOffset 的 `TargetArchitecture` 声明**期望平台属性**。两者可以对接验证：

```cpp
// 跨平台场景：验证导入的数据是否来自兼容架构
static_assert(imported_platform.pointer_size == TargetArchitecture::pointer_size);
```

### 1A.5 安全类型白名单可引用架构定义

白名单中每个类型的正确性可以用架构常量验证：

```cpp
template<> struct is_safe_leaf<int32_t> : std::true_type {};
static_assert(sizeof(int32_t) == TargetArchitecture::sizeof_int32);
// ↑ int32_t 之所以在白名单中，是因为它在目标架构上尺寸确定
```

---

## §1B 安全类型集合 (Safe Type Subset)

### 1B.1 形式化定义

**Safe Type Subset** \( S \) 的递归定义（给定 Target Architecture \( A \)）：

```
S = Primitives ∪ SafeEnum ∪ SafeContainers ∪ SafeComposites

其中:
  Primitives     = { int8_t, int16_t, ..., uint64_t, float, double, bool, char }
  SafeEnum       = { E : std::is_enum_v<E> ∧ is_fixed_enum<E>() }
  SafeContainers = { XString } ∪ { XVector<T> : T ∈ S } ∪ { XSet<T> : T ∈ S } 
                   ∪ { XMap<K,V> : K ∈ S ∧ V ∈ S }
  SafeComposites = { struct C :
                       ¬is_polymorphic(C)    (无虚函数)
                     ∧ ¬has_bases(C)          (无继承)
                     ∧ ¬is_union(C)           (非联合体)
                     ∧ ∀ member m of C: type_of(m) ∈ S  (所有成员递归安全)
                   }
```

**排除集** \( U \setminus S \) 包含：
```
  Rejected = { T* }                         (原始指针)
           ∪ { T& }                         (引用)
           ∪ { std::function<...> }         (类型擦除)
           ∪ { std::any }                   (类型擦除)
           ∪ { std::shared_ptr<T> }         (智能指针)
           ∪ { std::unique_ptr<T> }         (智能指针)
           ∪ { std::weak_ptr<T> }           (智能指针)
           ∪ { 任何含虚函数的类型 }
           ∪ { 任何使用继承的类型 }
           ∪ { union }
           ∪ { 非 fixed underlying type 的 enum }
           ∪ { 包含上述任何类型作为成员的 struct }
```

---

## §2 统一类型分类法 (Type Taxonomy)

### 2.1 分类树

所有到达 `is_safe_type<T>()` 或 `migrate_member<T>()` 的类型必须被分类到以下 **7 个互斥叶节点**：

```
Type T
├── [REJECT] Type-erased container?  → 黑名单匹配
├── [LEAF-1] Primitive?              → 12 种基本类型
├── [LEAF-2] Safe Enum?              → is_enum_v && is_fixed_enum
├── [LEAF-3] XString?                → 精确类型匹配
├── [LEAF-4] XContainer?             → XVector<T> / XSet<T> / XMap<K,V> 模板匹配
│   └── 递归检查元素类型 ∈ S
├── [LEAF-5] XOffsetPtr<T>?          → 默认拒绝（引用语义，非值语义）
│   └── 用户可通过 is_safe_leaf 特化 opt-in
├── [REJECT] Structural violation?   → polymorphic / inheritance / union
├── [LEAF-6] Safe Composite?         → 所有成员递归 ∈ S
└── [REJECT] Everything else         → 指针、引用、未知类型
```

### 2.2 每个叶节点的三重语义

| 叶节点 | Safety (Q1) | Migration (Q3) | Signature (Q2) |
|--------|-------------|----------------|-----------------|
| **LEAF-1** Primitive | ✅ accept | 直接赋值 `=` | TypeLayout 原生签名 (`i32[s:4,a:4]`) |
| **LEAF-2** Safe Enum | ✅ accept | 直接赋值 `=` (trivially copyable) | TypeLayout 原生签名 (`enum[s:N,a:M]<underlying>`) |
| **LEAF-3** XString | ✅ accept | `XString(old.c_str(), new_alloc)` | Opaque 签名 (`string[s:32,a:8]`) |
| **LEAF-4** XContainer | ✅ accept + 递归 | 遍历元素，递归迁移 | Opaque 签名 (`vector[s:32,a:8]<elem>`) |
| **LEAF-5** XOffsetPtr | ❌ 默认拒绝 (opt-in) | 用户负责 (via `migrate_as`) | 用户负责 |
| **LEAF-6** Safe Composite | ✅ accept + 递归 | 反射遍历成员，递归迁移 | TypeLayout 原生签名 (`record[s:N,a:M]{...}`) |
| **REJECT** | ❌ reject | N/A (不应到达) | N/A |

### 2.3 关键不变式 (Invariants)

```
INV-1: ∀ T, is_safe_type<T>()  ⟹  migrate_member<T>() 可正确执行
INV-2: ∀ T, ¬is_safe_type<T>() ⟹  compact_automatic<T>() 编译失败
INV-3: ∀ T ∈ S, get_definition_signature<T>() 产生确定性签名
INV-4: 分类函数 classify<T>() 的结果被 Safety/Migration/Signature 共享
```

---

## §3 职责三层模型

### 3.1 层次定义

```
┌─────────────────────────────────────────────────────┐
│  Layer 3: Domain Rules (XOffsetDatastructure)        │
│  ─────────────────────────────────────────────────── │
│  "哪些类型可以放进共享内存？"                          │
│                                                       │
│  拥有: 类型子集定义 S, 容器白名单/黑名单,              │
│       准入判定 (is_xbuffer_safe), 迁移引擎             │
│  输入: 用户定义的 struct/class                         │
│  输出: accept/reject + 迁移策略                       │
├─────────────────────────────────────────────────────┤
│  Layer 2: Structural Analysis (TypeLayout)            │
│  ─────────────────────────────────────────────────── │
│  "这个类型的二进制布局是什么？"                         │
│                                                       │
│  拥有: 签名生成, 签名比较, 平台检测,                    │
│       叶子属性查询 (is_fixed_enum, get_member_count)    │
│  输入: 任意 C++ 类型                                   │
│  输出: 签名字符串, 布局信息                             │
├─────────────────────────────────────────────────────┤
│  Layer 1: Type Properties (C++ Standard Library)      │
│  ─────────────────────────────────────────────────── │
│  "这个类型有哪些编译时可查询的属性？"                    │
│                                                       │
│  拥有: is_polymorphic, is_enum, is_union,              │
│       is_pointer, is_reference, is_trivially_copyable  │
│  输入: 任意 C++ 类型                                   │
│  输出: bool                                           │
└─────────────────────────────────────────────────────┘
```

### 3.2 层间依赖规则

```
Layer 3 (XOffset) → Layer 2 (TypeLayout): 弱依赖
    使用: get_member_count<T>(), is_fixed_enum<T>(), Opaque 签名注册
    不使用: 签名内容做安全判断

Layer 3 (XOffset) → Layer 1 (C++ Std): 强依赖
    使用: is_polymorphic_v, is_enum_v, is_union_v, is_pointer_v, is_reference_v

Layer 2 (TypeLayout) → Layer 1 (C++ Std): 强依赖
    使用: sizeof, alignof, is_class_v, is_enum_v + P2996 reflection

Layer 2 (TypeLayout) → Layer 3 (XOffset): 零依赖
    TypeLayout 不知道 XOffset 的存在
```

### 3.3 职责划分原则

| 原则 | 说明 |
|------|------|
| **叶子上移** | 非递归的类型属性查询（如 `is_fixed_enum`）可以放在 TypeLayout (Layer 2) |
| **递归留下** | 需要"领域知识"的递归扫描（如"XVector 内部是安全的"）必须留在 XOffset (Layer 3) |
| **签名不做安全判断** | TypeLayout 的签名描述布局，不判断是否"安全" |
| **安全不看签名** | XOffset 的安全检查基于类型属性，不依赖签名字符串内容 |

---

## §4 当前实现 vs 理想模型的差距

### 4.1 差距清单

| # | 差距 | 理想模型 | 当前实现 | 影响 |
|---|------|----------|----------|------|
| **G1** | 无统一分类器 | classify<T>() 返回叶节点枚举 | Safety 和 Compactor 各自 if-constexpr 链 | INV-4 未满足 |
| **G2** | 容器识别不精确 | 模板匹配 `XVector<T>` | `sizeof==32 && alignof==8` | INV-1 可能被误判破坏 |
| **G3** | 迁移无准入验证 | compact_automatic 入口检查 | 无检查 | INV-2 未满足 |
| **G4** | Enum 迁移隐式正确 | 显式分类为 LEAF-2 | 隐式走 M1 POD (trivially_copyable) | 语义不清晰但结果正确 |
| **G5** | Compactor 分类不完整 | 7 个叶节点 | 4 个类别 (POD/XString/Container/Nested) | 缺少 Enum 和 Reject 类别 |

### 4.2 差距严重度

| 差距 | 严重度 | 说明 |
|------|--------|------|
| **G3** | 🔴 High | 不安全类型可进入迁移逻辑，可能导致运行时 UB |
| **G2** | 🟡 Med | 误判概率低但架构不干净 |
| **G1** | 🟡 Med | 分类逻辑重复，维护成本高 |
| **G5** | 🟡 Med | Compactor 分类不完整，新类别时易遗漏 |
| **G4** | 🟢 Low | 结果正确，仅语义不清晰 |

---

## §5 设计改进方案

### 5.1 核心设计：显式白名单 (Explicit Safe Leaf Whitelist)

**核心思想**：将"安全类型子集"从隐式的 if-constexpr 控制流变成**显式的声明式白名单**。

#### 白名单声明

```cpp
namespace detail {
    // 叶子类型白名单：声明式、可扩展、精确
    template<typename T> struct is_safe_leaf : std::false_type {};

    // — Primitives (LEAF-1) —
    template<> struct is_safe_leaf<int8_t>   : std::true_type {};
    template<> struct is_safe_leaf<int16_t>  : std::true_type {};
    template<> struct is_safe_leaf<int32_t>  : std::true_type {};
    template<> struct is_safe_leaf<int64_t>  : std::true_type {};
    template<> struct is_safe_leaf<uint8_t>  : std::true_type {};
    template<> struct is_safe_leaf<uint16_t> : std::true_type {};
    template<> struct is_safe_leaf<uint32_t> : std::true_type {};
    template<> struct is_safe_leaf<uint64_t> : std::true_type {};
    template<> struct is_safe_leaf<float>    : std::true_type {};
    template<> struct is_safe_leaf<double>   : std::true_type {};
    template<> struct is_safe_leaf<bool>     : std::true_type {};
    template<> struct is_safe_leaf<char>     : std::true_type {};

    // — XString (LEAF-3) —
    template<> struct is_safe_leaf<XString>  : std::true_type {};

    // — XContainers (LEAF-4): 模板匹配，精确识别 —
    template<typename T>        struct is_safe_leaf<XVector<T>>  : std::true_type {};
    template<typename T>        struct is_safe_leaf<XSet<T>>     : std::true_type {};
    template<typename K, typename V> struct is_safe_leaf<XMap<K,V>> : std::true_type {};
}
```

#### 安全检查（使用白名单）

```cpp
namespace detail {
    template<typename T>
    consteval bool is_safe_type() {
        using CleanT = std::remove_cv_t<T>;

        // 1. 叶子类型：查白名单
        if constexpr (is_safe_leaf<CleanT>::value) {
            // 容器需要递归检查元素类型
            if constexpr (requires { typename CleanT::key_type; typename CleanT::mapped_type; })
                return is_safe_type<typename CleanT::key_type>()
                    && is_safe_type<typename CleanT::mapped_type>();
            else if constexpr (requires { typename CleanT::value_type; })
                return is_safe_type<typename CleanT::value_type>();
            return true;
        }

        // 2. 枚举：委托 TypeLayout
        if constexpr (std::is_enum_v<CleanT>)
            return boost::typelayout::is_fixed_enum<CleanT>();

        // 3. 复合类型：递归检查所有成员
        if constexpr (std::is_class_v<CleanT>)
            return !std::is_polymorphic_v<CleanT>
                && !has_bases<CleanT>()
                && !std::is_union_v<CleanT>
                && are_all_members_safe<CleanT>();

        // 4. 其他一切：拒绝
        return false;
    }
}
```

#### 迁移（使用同一白名单）

```cpp
template<typename MemberType>
static void migrate_member(const MemberType& old_m, MemberType& new_m, ...) {
    using CleanT = std::remove_cv_t<MemberType>;

    if constexpr (is_safe_leaf<CleanT>::value) {
        if constexpr (std::is_same_v<CleanT, XString>) {
            new_m = XString(old_m.c_str(), new_alloc);    // LEAF-3
        } else if constexpr (SupportedContainer<CleanT>) {
            migrate_container(old_m, new_m, ...);          // LEAF-4
        } else {
            new_m = old_m;                                 // LEAF-1 (trivially copyable)
        }
    } else {
        migrate_members(old_m, new_m, ...);                // LEAF-5 (composite)
    }
    // Rejected: unreachable (blocked by safety gate at entry)
}
```

### 5.2 白名单设计的关键优势

| 对比维度 | 隐式规则链 (当前) | 显式白名单 (新设计) |
|----------|-------------------|-------------------|
| **可读性** | 需通读 60 行 if-constexpr | 白名单一目了然 |
| **可扩展性** | 必须修改函数体 | 加一行特化即可 |
| **容器识别** | sizeof 启发式 (⚠️ 误判) | 模板匹配 (精确) |
| **黑名单** | 需要维护 5 种 type-erased 特判 | **不再需要** — 不在白名单即 rejected |
| **Safety↔Compactor 对齐** | 两套分类逻辑 | 共享 `is_safe_leaf` |
| **用户可注册** | 不可能 | 用户特化 `is_safe_leaf<MyType>` |
| **代码量** | ~60 行 | ~30 行声明 + ~15 行递归 |

### 5.3 黑名单消除

当前的 `is_type_erased_container` 黑名单（`std::function`, `std::any`, `shared_ptr` 等）在白名单设计中**不再需要**。这些类型不在白名单中，走到 `is_class_v` 分支时会尝试递归检查成员，但其内部成员（虚函数、原始指针等）会被正确拒绝。

可以完全删除：`is_std_function`, `is_std_shared_ptr`, `is_std_unique_ptr`, `is_std_weak_ptr`, `is_type_erased_container` — 共约 30 行。

### 5.4 TypeLayout 职责边界（不变）

TypeLayout 的职责**不需要改变**：

| 当前 TypeLayout 提供 | 是否继续使用 | 说明 |
|---------------------|-------------|------|
| `get_member_count<T>()` | ✅ | Compactor 成员遍历 |
| `is_fixed_enum<T>()` | ✅ | classify() 内部使用 |
| `get_definition_signature<T>()` | ✅ | 二进制合约验证 |
| Opaque 签名宏 | ✅ | XContainer 签名注册 |

TypeLayout 不需要知道"安全类型子集"的存在。它只负责：
1. **描述**任意类型的布局（签名）
2. 提供可复用的**叶子属性查询**（`is_fixed_enum`、`get_member_count`）

子集的定义权完全在 XOffset 层。

---

## §6 结论

### 核心发现

1. **XOffset 的安全模型本质上是两个显式集合的交集**：
   - **架构集合 A** (`TargetArchitecture`)：定义数据在什么环境下存在
   - **类型集合 S** (`is_safe_leaf` 白名单)：定义什么类型的数据可以存储
   - 零拷贝安全 = Platform ∈ A ∧ Type ∈ S

2. **当前两个集合都是隐式定义的**：架构约束散落在 15+ 个 static_assert 中，类型约束隐藏在 60 行 if-constexpr 链中。应改为显式声明

3. **显式化带来三重好处**：
   - **可读性**：一个 `TargetArchitecture` struct + 一个 `is_safe_leaf<T>` 白名单即为完整定义
   - **可扩展性**：用户可特化 `is_safe_leaf` 注册自定义安全类型
   - **消除冗余**：黑名单（~30 行）可删除；Safety 和 Compactor 共享同一白名单

4. **TypeLayout 的职责已经最优**：它是通用的结构分析工具，不应知道 XOffset 的领域规则。当前弱耦合（2 个 consteval 调用 + 4 个宏注册）是正确的设计

5. **TypeLayout 签名的架构前缀 `[64-le]` 与 `TargetArchitecture` 形成对称**：前者记录"平台是什么"，后者声明"我要什么"，两者可对接验证

### 推荐实施优先级

| 优先级 | 改动 | 行数估算 |
|--------|------|---------|
| 🔴 P0 | G3: Compactor 入口添加 `validate_xbuffer_type<T>()` | +2 |
| 🟡 P1 | 引入 `TargetArchitecture` 结构体，重构 static_assert 引用 | +20, -15 |
| 🟡 P2 | G2: 引入 `is_safe_leaf<T>` 白名单替代隐式规则链 | +30, -60 (净减) |
| 🟡 P3 | G1/G5: Safety 和 Compactor 共享白名单 | +10, -20 (净减) |
| 🟢 P4 | G4: Enum 显式分类（随白名单重构自然解决） | 0 (包含在 P2) |
| 🟢 P5 | 删除 type-erased 黑名单（白名单使其不再需要） | -30 |
