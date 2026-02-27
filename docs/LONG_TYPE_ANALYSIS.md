# `long` 类型问题：技术原理全景分析

> 本文档从底层原理出发，系统性归纳 `long` / `unsigned long` / `long long` 在
> 跨平台零拷贝序列化场景下的问题本质、检测边界、以及解决方案的技术推导过程。

---

## 目录

1. [第一层：数据模型差异的根因](#第一层数据模型差异的根因)
2. [第二层：C++ 类型系统中 typedef 的等价性](#第二层c-类型系统中-typedef-的等价性)
3. [第三层：编译器脱糖（Desugaring）行为](#第三层编译器脱糖desugaring行为)
4. [第四层：TypeLayout 签名引擎的处理链路](#第四层typelayout-签名引擎的处理链路)
5. [第五层：各检测层面的能力边界](#第五层各检测层面的能力边界)
6. [第六层：plong 签名标记方案的原理](#第六层plong-签名标记方案的原理)
7. [总结：不可能三角](#总结不可能三角)

---

## 第一层：数据模型差异的根因

### 1.1 为什么 `long` 的大小不统一？

C/C++ 标准 **从未规定** `long` 的精确字节宽度。标准只要求：

```
sizeof(char) ≤ sizeof(short) ≤ sizeof(int) ≤ sizeof(long) ≤ sizeof(long long)
```

具体大小由平台的 **数据模型（Data Model）** 决定：

```
                 char   short   int   long   long long   pointer
  ┌─────────────────────────────────────────────────────────────────┐
  │ ILP32 (32-bit)  1      2      4     4        8          4      │
  │ LLP64 (Win x64) 1      2      4     4        8          8      │  ← Windows
  │ LP64  (Unix x64) 1      2      4     8        8          8      │  ← Linux/macOS
  └─────────────────────────────────────────────────────────────────┘
```

**关键冲突**: LLP64 (Windows) 与 LP64 (Linux) 唯一的分歧点就是 `long`：

| 类型 | Windows x64 (LLP64) | Linux x64 (LP64) | 是否一致 |
|------|:---:|:---:|:---:|
| `char` | 1 | 1 | ✅ |
| `short` | 2 | 2 | ✅ |
| `int` | 4 | 4 | ✅ |
| **`long`** | **4** | **8** | **❌ 唯一的分歧** |
| `long long` | 8 | 8 | ✅ |
| `pointer` | 8 | 8 | ✅ |

### 1.2 为什么会有两种数据模型？

- **LP64 (Unix)**: 选择让 `long` 跟随指针大小变为 8 字节，因为 Unix 传统上用 `long` 存储地址相关的值。
- **LLP64 (Windows)**: 选择保持 `long` 为 4 字节，因为大量 Win32 API（`DWORD = unsigned long`）假设 `long` 是 32 位。改变它会破坏数十亿行遗留代码。

这是 **历史设计决策** 导致的永久性分歧，不可调和。

### 1.3 对零拷贝序列化的影响

当同一段代码在两个平台编译时：

```cpp
struct PlayerData {
    long score;      // 4 bytes on Windows, 8 bytes on Linux
    int32_t health;  // 4 bytes everywhere
};
```

**内存布局完全不同**：

```
Windows (LLP64):                Linux (LP64):
┌──────────┬──────────┐         ┌──────────────────────┬──────────┐
│ score(4B)│health(4B)│         │     score(8B)        │health(4B)│
│ offset 0 │ offset 4 │         │     offset 0         │ offset 8 │
└──────────┴──────────┘         └──────────────────────┴──────────┘
sizeof = 8                      sizeof = 16 (with padding)
```

零拷贝意味着 **直接把内存字节映射为结构体**，布局不一致 = **静默数据损坏**。

---

## 第二层：C++ 类型系统中 typedef 的等价性

### 2.1 `typedef` 不创建新类型

C++ 标准明确规定（[dcl.typedef]）：

> A typedef-name does not introduce a new type the way a class declaration
> or enum declaration does.

这意味着：

```cpp
typedef long int64_t;       // Linux <stdint.h> 的做法
// 等价于：int64_t 和 long 是 **同一个类型**
```

在 C++ 的类型系统层面：

```cpp
// Linux LP64 上：
std::is_same_v<long, int64_t>           == true   // 同一个类型
std::is_same_v<unsigned long, uint64_t> == true   // 同一个类型

// Windows LLP64 上：
std::is_same_v<long, int64_t>           == false  // 不同类型
std::is_same_v<long long, int64_t>      == true   // 同一个类型
```

### 2.2 `using` 别名同理

```cpp
using int64_t = long;  // C++ 风格，效果完全相同
```

### 2.3 这意味着什么？

任何基于 `std::is_same_v` 的检测手段都无法在 Linux 上区分 `long` 和 `int64_t`。
这不是工具的限制，而是 **语言定义层面** 的等价性。

```
           Linux LP64 的类型系统视角：
           ┌─────────────────────┐
           │        long         │ ← 这是唯一的真实类型
           │                     │
           │  int64_t = long     │ ← typedef，不是新类型
           │  int_least64_t      │ ← typedef，不是新类型
           │  int_fast64_t       │ ← typedef，不是新类型
           └─────────────────────┘
           编译器眼中只有一个类型：long
```

---

## 第三层：编译器脱糖（Desugaring）行为

### 3.1 什么是脱糖？

编译器在处理类型时，会将 typedef/alias 解析为其底层"规范类型（canonical type）"。
这个过程称为 **脱糖（desugaring）**。

```
源代码:     int64_t x;
            ↓ typedef 展开
中间表示:   long x;          (在 LP64 上)
            ↓ 规范化
最终类型:   long              (canonical type)
```

### 3.2 脱糖在反射中的体现

P2996 反射的 `type_of()` 返回的是 **规范化后的类型**：

```cpp
struct Probe {
    int64_t a;   // 用户写的是 int64_t
    long    b;   // 用户写的是 long
};

// P2996 反射结果（Linux LP64）：
constexpr auto mems = nonstatic_data_members_of(^^Probe);
display_string_of(type_of(mems[0]))  →  "long"   // ⚠️ int64_t 被脱糖为 long
display_string_of(type_of(mems[1]))  →  "long"   // 本来就是 long
type_of(mems[0]) == type_of(mems[1]) →  true      // 完全无法区分
```

### 3.3 脱糖 vs 反射 ID 的反差

有趣的是，P2996 对 **独立类型名** 的反射可以区分 typedef：

```cpp
^^int64_t != ^^long    // ✅ 不同的反射 ID（"spelling-aware"）
```

但这个能力 **不适用于 struct 成员的 `type_of()`**，因为 `type_of()` 返回的是
编译器内部的规范类型（canonical type），typedef 信息已丢失。

### 3.4 原理图示

```
                       ┌──────────────────────────────────┐
  用户源代码           │  struct S { int64_t x; long y; } │
                       └──────────┬───────────────────────┘
                                  │
                    ┌─────────────▼─────────────┐
  编译器前端        │  typedef 展开 (desugaring)  │
  (parsing)        │  int64_t → long  (on LP64)  │
                    └─────────────┬─────────────┘
                                  │
                    ┌─────────────▼─────────────┐
  编译器 AST        │  struct S { long x; long y; } │  ← 两个成员类型已经相同
                    └─────────────┬─────────────┘
                                  │
              ┌───────────────────┼───────────────────┐
              │                   │                   │
   ┌──────────▼────────┐  ┌──────▼──────┐  ┌────────▼─────────┐
   │ P2996 type_of()   │  │ is_same_v   │  │ TypeLayout 签名   │
   │ → "long" (both)   │  │ → true      │  │ → i64 (both)     │
   │ ❌ 无法区分        │  │ ❌ 无法区分  │  │ ❌ 无法区分       │
   └───────────────────┘  └─────────────┘  └──────────────────┘
```

---

## 第四层：TypeLayout 签名引擎的处理链路

### 4.1 签名生成流程

TypeLayout 使用 C++ 模板特化 + P2996 反射来生成类型签名：

```
TypeSignature<T, Mode>::calculate()
  │
  ├─ 基础类型 → type_map.hpp 中的特化
  │   ├─ int32_t  → "i32[s:4,a:4]"
  │   ├─ int64_t  → "i64[s:8,a:8]"
  │   ├─ long     → requires 守卫决定是否有独立特化
  │   └─ ...
  │
  └─ struct/class → 反射遍历成员
      └─ 对每个成员：
          using FieldType = [:type_of(member):];
          TypeSignature<FieldType, Mode>::calculate()
```

### 4.2 `type_map.hpp` 的 `requires` 守卫机制

TypeLayout 为 `long` 设置了条件特化：

```cpp
// type_map.hpp (简化)
template <SignatureMode Mode>
    requires (!std::is_same_v<long, int32_t> && !std::is_same_v<long, int64_t>)
struct TypeSignature<long, Mode> {
    // 这个特化只有在 long 既不等于 int32_t 也不等于 int64_t 时才存在
    // 实际上...目前没有任何主流平台满足这个条件！
};
```

**各平台的行为**：

| 平台 | `long == int32_t?` | `long == int64_t?` | requires 结果 | long 特化存在？ | long 走哪个路径？ |
|------|:---:|:---:|:---:|:---:|---|
| **Windows** (LLP64) | ❌ | ❌ | `true` | **✅ 存在** | 独立特化 → `i32[s:4,a:4]` |
| **Linux** (LP64) | ❌ | ✅ | `false` | **❌ 不存在** | 走 `int64_t` 路径 → `i64[s:8,a:8]` |

> **Windows 的特殊情况**：在 MSVC 中 `long` 是 4 字节，`int` 也是 4 字节，
> 但 `std::is_same_v<long, int>` 在 MSVC 上为 `false`（`long` 和 `int` 是不同的
> 类型即使大小相同）。同时 `std::is_same_v<long, int32_t>` 取决于 `int32_t` 被定义
> 为 `int` 还是 `long`。在 MSVC 中 `int32_t = int`，所以 `long ≠ int32_t`。

### 4.3 签名生成结果对比

```cpp
struct Example {
    int32_t a;
    long    b;    // 问题字段
    int64_t c;
};
```

**Windows 签名**：
```
record[s:16,a:8]{@0:i32[s:4,a:4],@4:i32[s:4,a:4],@8:i64[s:8,a:8]}
                                   ^^^^
                            long → i32（4 字节）
```

**Linux 签名**：
```
record[s:24,a:8]{@0:i32[s:4,a:4],@8:i64[s:8,a:8],@16:i64[s:8,a:8]}
                                   ^^^^
                            long → i64（8 字节，和 int64_t 相同）
```

签名不匹配 → 跨平台比较时 CompatReporter 会报 DIFFER → **问题可被发现**，
但 **无法定位到是哪个字段** 使用了 `long`，因为签名中只有 `i32`/`i64`，没有类型名。

### 4.4 classify_safety 的签名扫描

`classify_safety<T>()` 通过扫描签名字符串中的标记（marker）来判断安全等级：

```cpp
consteval SafetyLevel classify_safety() {
    constexpr auto sig = get_layout_signature<T>();
    if constexpr (sig.contains("bits<"))  return Risk;   // 位域
    if constexpr (sig.contains("wchar[")) return Risk;   // wchar_t
    if constexpr (sig.contains("f80["))   return Risk;   // long double
    if constexpr (sig.contains("ptr["))   return Warning; // 指针
    // ...
    return Safe;
}
```

**当前签名中没有 `long` 的独立标记**，`long` 被编码为 `i32` 或 `i64`，
与 `int32_t` / `int64_t` 无法区分。这就是为什么 `classify_safety` 无法检测 `long`。

---

## 第五层：各检测层面的能力边界

### 5.1 能力边界矩阵

```
检测方法                    │ naked long │ struct 成员 long │ Linux 上区分  │
                           │ (顶层类型)  │ (递归检测)       │ long vs int64_t │
───────────────────────────┼────────────┼─────────────────┼────────────────┤
std::is_same_v<T, long>   │ ✅ Win     │ ❌               │ ❌              │
                           │ ❌ Linux*  │                  │                │
───────────────────────────┼────────────┼─────────────────┼────────────────┤
P2996 ^^long != ^^int64_t │ ✅ Both    │ ❌ (脱糖)        │ ✅ (仅独立名)  │
───────────────────────────┼────────────┼─────────────────┼────────────────┤
P2996 type_of(member)      │ N/A        │ ❌ (脱糖)        │ ❌              │
───────────────────────────┼────────────┼─────────────────┼────────────────┤
TypeLayout 签名标记        │ ✅ Win     │ ✅ Win (传播)    │ ❌ (同一类型)  │
                           │ ❌ Linux** │ ❌ Linux**       │                │
───────────────────────────┼────────────┼─────────────────┼────────────────┤
签名跨平台对比             │ ✅ Both    │ ✅ Both          │ ✅ (大小不同)  │
───────────────────────────┴────────────┴─────────────────┴────────────────┘

*  Linux 上 long == int64_t，拒绝 long 等于拒绝 int64_t，库不可用
** Linux 上 long 特化不存在（requires 排除），走 int64_t 路径
```

### 5.2 根本限制总结

```
                    ┌─────────────────────────────────────┐
                    │         不可逾越的语言边界           │
                    │                                     │
                    │  在 LP64 上：long ≡ int64_t         │
                    │  这是 C++ 标准定义的类型等价性       │
                    │                                     │
                    │  任何编译期手段（模板、反射、         │
                    │  concept、static_assert）都           │
                    │  无法突破这一等价性                   │
                    │                                     │
                    │  唯一的例外：^^long != ^^int64_t     │
                    │  但这仅适用于独立类型名反射，         │
                    │  不适用于 struct 成员的 type_of()    │
                    └─────────────────────────────────────┘
```

### 5.3 各平台检测能力对比

**Windows (LLP64)**：`long` 是独立类型 → **可检测、可拒绝、可递归**
```
long ≠ int32_t ≠ int64_t
  → is_same_v 可区分
  → TypeSignature<long> 特化存在
  → 签名可传播标记到 struct 成员
  → classify_safety 可扫描标记
```

**Linux (LP64)**：`long == int64_t` → **不可检测、不可拒绝**
```
long ≡ int64_t  (同一类型)
  → is_same_v 无法区分
  → TypeSignature<long> 特化被 requires 排除
  → long 走 int64_t 路径，签名相同
  → classify_safety 看不到差异
```

---

## 第六层：plong 签名标记方案的原理

### 6.1 方案思路

在 TypeLayout 的 `type_map.hpp` 中，给 `long` 分配一个独立的签名标记 `plong`
（p = platform-dependent），使其在签名字符串中可被识别：

```cpp
// type_map.hpp 修改方案
template <SignatureMode Mode>
    requires (!std::is_same_v<long, int32_t> && !std::is_same_v<long, int64_t>)
struct TypeSignature<long, Mode> {
    static consteval auto calculate() noexcept {
        // 使用 "plong" 标记代替 "i32"
        if constexpr (sizeof(long) == 4) return FixedString{"plong[s:4,a:4]"};
        else return FixedString{"plong[s:8,a:8]"};
    }
};
```

### 6.2 方案在各平台的行为

| 平台 | requires 结果 | 特化存在？ | `long` 的签名 | `int32_t` 的签名 |
|------|:---:|:---:|---|---|
| **Windows** | `true` | ✅ | `plong[s:4,a:4]` | `i32[s:4,a:4]` |
| **Linux** | `false` | ❌ | `i64[s:8,a:8]` (走 int64_t) | `i32[s:4,a:4]` |

### 6.3 签名传播效果

```cpp
struct MyData {
    int32_t a;
    long    b;     // 问题字段
};
```

**Windows 签名（修改后）**：
```
record[s:8,a:4]{@0:i32[s:4,a:4],@4:plong[s:4,a:4]}
                                     ^^^^^
                                     可检测！
```

**Linux 签名**：
```
record[s:16,a:8]{@0:i32[s:4,a:4],@8:i64[s:8,a:8]}
                                     ^^^
                                     走 int64_t 路径，无 plong 标记
```

### 6.4 classify_safety 集成

```cpp
// classify_safety.hpp 添加扫描规则
if constexpr (sig.contains(FixedString{"plong["}))  return SafetyLevel::Risk;
if constexpr (sig.contains(FixedString{"pulong["})) return SafetyLevel::Risk;
```

效果：
- **Windows**: `struct MyData` 中的 `long` 成员会在签名中产生 `plong[`，
  `classify_safety` 扫描到后返回 `Risk` → **自动递归检测** ✅
- **Linux**: `long == int64_t`，特化不存在，无 `plong` 标记，`int64_t` 正常通过 ✅

### 6.5 方案优势

| 特性 | 当前方案 (XOffset Plan C) | plong 标记方案 (TypeLayout) |
|------|:---:|:---:|
| Windows naked long 检测 | ✅ | ✅ |
| Windows struct 成员 long 检测 | ❌ | ✅ (签名传播) |
| Linux 不误杀 int64_t | ✅ | ✅ |
| 检测位置 | XOffset API 入口 | TypeLayout 签名引擎 |
| 递归深度 | 0（仅顶层） | ∞（签名天然递归） |
| 跨平台比较诊断 | "签名不匹配" | "plong vs i64 不匹配"（精确定位） |

### 6.6 方案局限

| 局限 | 原因 |
|------|------|
| Linux 上无法检测 `long` | `long == int64_t`，语言等价性不可突破 |
| 需要修改 TypeLayout 子模块 | 改动在 `type_map.hpp` 和 `classify_safety.hpp` |
| `long long` 情况类似 | Linux 上 `long long == int64_t` 为 false，但需要同样处理 |

---

## 总结：不可能三角

```
                     全平台自动检测
                         ╱╲
                        ╱  ╲
                       ╱    ╲
                      ╱  ❌  ╲
                     ╱  不可能 ╲
                    ╱          ╲
     Linux 兼容性 ╱──────────────╲ 拒绝 long
     (int64_t=long)               (安全保证)
```

**你只能同时满足两个**：

1. **全平台检测 + 拒绝 long**：但 Linux 上 int64_t 也被拒绝 → **破坏 Linux 兼容性** ❌
2. **全平台检测 + Linux 兼容性**：但无法拒绝 long → **没有安全保证** ❌
3. **拒绝 long + Linux 兼容性**：但只能在 Windows 上检测 → **非全平台** ✅ (最佳折中)

### 最终策略

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  Windows (LLP64)：                                           │
│    → TypeLayout plong 标记 + classify_safety 扫描            │
│    → 自动递归检测 struct 成员中的 long                        │
│    → 编译期 Risk 拒绝                                        │
│                                                              │
│  Linux (LP64)：                                              │
│    → long == int64_t，技术上安全，无需拒绝                   │
│    → 但跨平台场景下仍然危险                                  │
│    → 依赖：编码规范 + 跨平台签名对比 + Code Review           │
│                                                              │
│  跨平台对比：                                                │
│    → CompatReporter 比较两端签名                             │
│    → Windows plong[s:4] vs Linux i64[s:8] → DIFFER          │
│    → 精确识别 long 导致的布局不兼容                           │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 附录 A：P2996 反射探针实验结果

> 实验代码：`tests/test_long_reflection_probe.cpp`

### 独立类型名反射（可区分）

```
^^long          → reflection ID: A
^^int64_t       → reflection ID: B
^^long != ^^int64_t  →  true ✅

display_string_of(^^long)    → "long"
display_string_of(^^int64_t) → "int64_t"
```

### struct 成员反射（不可区分）

```
struct Probe { int64_t a; long b; };

type_of(member_a)  →  canonical type: long
type_of(member_b)  →  canonical type: long
type_of(member_a) == type_of(member_b)  →  true ❌

display_string_of(type_of(member_a))  →  "long"
display_string_of(type_of(member_b))  →  "long"
```

### 结论

| 反射操作 | 区分能力 | 原因 |
|---------|:---:|------|
| `^^T` 独立类型名 | ✅ | spelling-aware，保留源码名 |
| `type_of(member)` 成员类型 | ❌ | 返回 canonical type，typedef 已脱糖 |

---

## 附录 B：各层面检测方案速查表

| # | 方案 | 层面 | Win naked | Win member | Linux naked | Linux member | 推荐 |
|---|------|------|:---:|:---:|:---:|:---:|:---:|
| 1 | `is_same_v<T, long>` | XOffset | ✅ | ❌ | ❌* | ❌ | 当前实现 |
| 2 | P2996 `^^long` 比较 | XOffset | ✅ | ❌ | ✅** | ❌ | 不实用 |
| 3 | `plong` 签名标记 | TypeLayout | ✅ | ✅ | ❌* | ❌* | **建议** |
| 4 | 跨平台签名对比 | CompatReporter | ✅ | ✅ | ✅ | ✅ | **最终防线** |
| 5 | 编码规范 + Review | 人工 | ✅ | ✅ | ✅ | ✅ | **始终需要** |

> \* Linux 上拒绝 long 会同时拒绝 int64_t  
> \*\* 仅适用于独立类型名，不适用于 struct 成员

---

*本文档是 XOffsetDatastructure 技术分析的一部分。*  
*基于 P2996 Bloomberg Clang 探针测试结果（`tests/test_long_reflection_probe.cpp`）。*  
*另见：`docs/LONG_PORTABILITY_GUIDE.md` — 用户面向的使用指南。*
