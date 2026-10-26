# 移除 C++26 依赖：设计方案与实施计划

2026-10-26 · 设计线程「设计移除 C++26 依赖的方案」 · 状态：**已定案**。susu 10-26 16:36Z 回复“全按推荐”，D1–D6 都按推荐项定（第 9 节）；实施拆成 3 个线程，指令见第 10 节。

- 基线：next_cpp26 `7620bc3`（库头文件与 `902ff18` 逐字节相同），TypeLayout `fe29592`。
- 研究分支：`claude/remove-cpp26-design-20261026`，只放本文和探针，不改库代码，不进 next_cpp26，不动在途 PR 和 Mac 队列。
- 探针：`/mnt/project-files/remove-cpp26-20261026/probes/`（分支里 `docs/design/remove-cpp26/probes/` 同一份），`run_probes.sh <仓库>` 一键复现。

## 0. 结论

1. **next_cpp26 对 C++26 的依赖只有一项：P2996 反射。** 除反射外，库代码只用到 C++20（concepts、consteval、`std::span`、`<compare>`、模板 lambda），外加一个有特性宏保护的 C++23 库函数（`std::string::resize_and_overwrite`）。库里没有 `template for`，没有 contracts，没有 C++26 库组件。TypeLayout 子模块的准入和签名也全靠反射。
2. 反射在库里只被问五类问题（第 2 节）。最多的一类是成员个数、类型、偏移和访问，约 50 个函数在用，一行字段表就能完全替代。剩下几个语义检查（用户写的析构、没有固定底层类型的旧式 enum）在 C++20 下改为“声明即承诺”，再由 C++26 构建自动核对。
3. **推荐方案**：C++20 起步（C++23 可选）。库内抽一层“schema 字段接口”，挂两个后端：C++20 编译器用一行 `XOFFSET_RECORD(T, 字段...)`；C++26 编译器继续用反射，可以不写这一行，写了就逐条核对。wire 格式不变，C++26 构建的机器码不变。
4. **本线程的探针已经验证了关键假设**（第 5 节）：
   - 字段表算出的 wire 签名、schema 哈希、补齐判定与库里反射算的逐字节相同（P2996 与 GCC 16 各 18 个类型）。
   - GCC 13.3、Clang 18.1、GCC 16.2（`-std=c++20`）算出的哈希与 P2996 反射值相同。
   - 漏写、错序、重复字段的 5 种写法在编译期全部被拦下，其中一种是只查 sizeof 发现不了的。
   - 热路径遍历的机器码与反射版逐条相同。
   - 反射可以自动写出字段表。
5. 最直接的收益是客户端：`docs/CLIENT_TARGETS.md`（0.3.0-beta.1）记录 Linux arm64 和 Windows x64（MinGW-w64 GCC 16）已验证，**Android 和 iOS 目前不可行，缺的正是带反射的工具链**（NDK Clang 没有 P2996、NDK 不再带 GCC；iOS 没有 P2996 版 libc++ 和 compiler-rt）。C++20 构建后，NDK Clang 和 Xcode 自带的 Apple clang 就能直接编，这两个阻塞随之解除。
6. 对定位的影响：K1 仍然是“普通聚合体、不用 IDL”，C++20 下多一行由编译器证明完整的字段表；K7 变强，每个编译器都能自己算签名、在构建期核对；K8 从“客户端用 clang-p2996 交叉编译”改成“各平台原生编译器”，susu 10-26 已批准（D5），取代 10-21 12:50Z 的决定。CppCon 2026 的反射叙事保留：反射给出零样板，也是证明 C++20 描述无误的核对者。
7. 实施分 5 个阶段、3 个实现线程（high），全部先在独立分支做。第一批进 next_cpp26 的是“C++26 机器码不变”的重构，按 D4 在 INT3 合入后的空档分批合（第 7 节）。

## 1. 现状：C++26 依赖在哪里

| 位置 | 用反射做什么 | 规模 |
| --- | --- | --- |
| `xoffsetdatastructure.hpp` | 准入、校验遍历、重定位、稳定擦除、反射构造和转移、wire 签名、补齐检测、XCompactor 迁移、`XOFFSET_ORDINARY_RECORDS` 的检查 | 28 个函数，71 行 |
| `receive_raw.hpp` | 直接接收：隐式生存期判定、enum 取值范围、成员不重叠、激活和再激活遍历 | 12 个函数，46 行 |
| `xoffset_core_lifetime.hpp` | SHIFT 重定位、子树拷贝（T4）、子树度量、修正、压实 | 7 个函数，26 行 |
| `xoffset_verified_paths.hpp` | 压实输出、直接写 wire、组合写出 | 7 个函数，21 行 |
| `xoffset_snapshot_index.hpp` | 快照索引遍历 | 2 个函数，12 行 |
| `external/typelayout` | `is_byte_copy_safe_v`（K0 准入）、布局签名、`FixedString`。另外 `config.hpp` 在 `#if` 里用 `__has_feature`，GCC 13 预处理就失败（GCC 14 起才有） | `reflect.hpp`、`signature_impl.hpp`、`admission.hpp` |
| 构建 | `cmake/XOffsetBuildChecks.cmake`：`cxx_std_26`，`-std=c++26 -freflection`（P2996 再加 `-fexpansion-statements -stdlib=libc++`） | |

合计 5 个头文件、约 56 个函数、176 行用到 `std::meta` 或 `^^`，57 行拼接 `[: :]`。库里的遍历全部用 `index_sequence` 展开，没有 `template for`，所以替换是机械的。OS 存储层（`xoffset_virtual_memory.hpp`）本来就是 C++17、不用反射。

反射之外的部分：`resize_and_overwrite`（C++23）在 4 处输出路径上有 `#if __cpp_lib_string_resize_and_overwrite` 回退；`std::is_implicit_lifetime` 和 `__builtin_is_implicit_lifetime` 已经做了特性检测。编译器专有写法很少（`__attribute__((always_inline/noinline))`、`[[gnu::noinline]]`，已有 `_MSC_VER` 分支）。

## 2. 反射被拿来问什么，C++20 怎么替

| 类 | 反射查询 | 用在哪 | C++20 替代 |
| --- | --- | --- | --- |
| A 形状 | 成员个数，第 i 个成员的类型、字节偏移、访问（`obj.[:m:]`）；基类个数和类型 | 约 50 个遍历函数、wire 签名、补齐检测、构造、析构、迁移 | 字段表：每个字段是“成员指针 + offsetof”，两者都是编译期常量；基类在宏里显式列出 |
| B 位域 | `is_bit_field`、`bit_size_of` | 各处都是“遇到位域就不支持” | 位域取不了成员指针，也不能 offsetof，写进字段表直接编译失败，与现在的行为一致（v1 wire 本来就拒绝位域） |
| C 枚举值 | `enumerators_of`，只用于没有固定底层类型的旧式 enum 求取值范围 | 直接接收时校验 enum 值 | C++20 下要求固定底层类型（`enum class` 默认就是固定的，旧式 enum 写 `: std::uint8_t`），否则编译期报错并给出改法 |
| D 特殊成员 | 析构是否用户提供、有没有用户声明的特殊成员 | 直接接收和稳定擦除的“隐式生存期”判定 | 有 `std::is_implicit_lifetime` 或 `__builtin_is_implicit_lifetime` 就用；没有时以 `XOFFSET_RECORD` 的声明为承诺，和现在 `XOFFSET_ORDINARY_RECORDS` 对 `[[no_unique_address]]` 的处理是同一种做法；C++26 构建逐条核对 |
| E 准入 | TypeLayout `is_byte_copy_safe_v`：递归成员，扫描签名里的指针记号 | K0 准入 | XOffset 自己按字段接口递归：成员只能是非指针标量、enum、数组、XOffset 容器或已描述的记录；`std::is_polymorphic` 拒绝虚表；聚合体不可能有虚基类 |

## 3. 方案选项

### 3.1 C++20 下怎么描述 schema

| 选项 | 做法 | 结论 |
| --- | --- | --- |
| **S1 一行字段表（推荐）** | `XOFFSET_RECORD(T, a, b, c)`，非侵入，写在全局作用域，展开成成员指针加 offsetof | 偏移精确；编译期能证明字段表完整（3.1.1）；GCC、Clang 已实测，MSVC 待测；探针已验证 |
| S2 聚合体“魔法”（Boost.PFR 式，零样板） | 用花括号初始化探测成员个数，用结构化绑定拿类型，偏移按对齐规则模拟 | **否决**。C++20 在编译期拿不到成员偏移，按类型模拟会被成员上的 `alignas` 骗过：`struct S { char a; alignas(2) char b; int c; };` 实际 b 在 2，模拟出 1，而 sizeof（8）和 alignof（4）完全相同，任何整体检查都发现不了，结果是读写错位置（GCC 13 实测） |
| S3 外部生成器（libclang、AST → 字段表） | 构建前跑工具生成 S1 的宏行 | 不作为必需步骤，否则每个用户都多一个构建工具；只作可选的迁移助手。更好的生成器是反射本身（探针 4） |
| S4 IDL | 另写 schema 语言，再生成结构体 | 否决，违背 K1 |

#### 3.1.1 字段表的完整性证明（不靠 sizeof）

- 每个字段都从 `&T::m` 得到：类型精确，名字写错直接编译失败。
- **偏移严格递增且不重叠**：保证字段不重复、按声明顺序列出。
- **聚合体初始化计数**：T 是聚合体时，要求 `T{x1, ..., xE}` 合法、`T{x1, ..., xE+1}` 不合法。E 是基类个数加上每个字段占的初始化子句数（C 数组按元素数算，其余算 1）；`xi` 是一个能转换成任意非数组类型的探针类型，只在未求值的 `requires` 里出现。只有字段表恰好覆盖所有成员时，两个条件才同时成立。
- 只检查 sizeof 或“铺满”不够：`struct { u8 a; u8 hidden; u16 c; i64 b; }` 漏写 hidden 后，a 结束于 1、c 在 2，看起来就是正常补齐。探针 2b 里这个例子（NEG=5）被初始化计数拦下。
- 非聚合体（有构造函数、私有成员）不在 C++20 字段表的范围内。这和现在直接接收、稳定擦除、SHIFT 路径本来就要求聚合体一致；确实需要非聚合体的用户可以用 C++26 后端。
- C++26 构建时再加一道：字段表与反射逐条比对（成员、顺序、偏移、类型、特殊成员）。

### 3.2 是否保留 C++26 反射路径

| 选项 | 说明 | 取舍 |
| --- | --- | --- |
| **R1 双后端（推荐）** | 库内只依赖“字段接口”；C++26 编译器默认走反射，不写宏也行，写了就核对；C++20 编译器走字段表 | 保留 K1 的零样板和 CppCon 叙事；反射构建成为 C++20 描述的核对者；代价是验证矩阵多几列 |
| R2 只留字段表 | 所有编译器都写宏，反射只在测试里核对字段表 | 代码路径最少，但 C++26 用户也失去零样板，反射退为测试工具 |
| R3 主线仍是 C++26，另出 C++20 客户端子集 | 从服务端导出布局，生成只读和校验代码 | 没有真正移除依赖，两套实现，否决 |

### 3.3 目标标准

| 选项 | 编译器下限（初定，阶段 3 实测后定） | 取舍 |
| --- | --- | --- |
| **C++20 起步，C++23 可选（推荐）** | GCC 12+、Clang 16+、Apple clang 15（Xcode 15）+、MSVC 19.38（VS 2022 17.8）+、Android NDK r26+ | 库只需要 C++20。C++23 下用 `resize_and_overwrite`；C++20 下输出路径多一次与输出等长的清零（`resize()`），性能宣称以 C++23 构建为准 |
| C++23 起步 | GCC 13+、Clang 17+；MSVC 要 `/std:c++latest` | 只省掉一处回退，却抬高了 MSVC 和旧 Xcode 的门槛，不划算 |
| C++17 起步 | 能覆盖更老的编译器 | 要重写 concepts、consteval、span、模板 lambda，工作量翻倍，否决 |

已实测（探针级别）：GCC 13.3、Clang 18.1、GCC 16.2 `-std=c++20`、clang-p2996 `-std=c++20`。整库在这些编译器上能否编过由阶段 3 验证；MSVC、Apple clang 本线程没有环境。

### 3.4 TypeLayout

| 选项 | 说明 | 取舍 |
| --- | --- | --- |
| **T1 核心库不再依赖（推荐）** | 准入改由 XOffset 按字段接口计算；`FixedString`、`to_fixed_string` 搬进 XOffset；TypeLayout 留给 C++26 下的签名导出工具（`tools/sigs`、`check_compat`），再加一个“两套准入结论一致”的交叉测试 | 核心库在 C++20 下不需要任何子模块；TypeLayout 仓库不用动 |
| T2 TypeLayout 也加字段表后端 | TypeLayout 自己也能在 C++20 下用 | 这是 TypeLayout 项目的方向，工作量在另一个仓库，可以以后单独立项；走这条要先修 `config.hpp` 那一行 |
| T3 维持现状 | | 不可行：TypeLayout 准入本身要反射 |

## 4. 推荐设计

### 4.1 用户写法

```cpp
struct alignas(8) Hero {
    int32_t id{}; int32_t level{}; int32_t star{};
    XString name; XVector<Skill> skills; XVector<Buff> buffs;
};
// C++20 必须写；C++26 可以不写，写了就由反射逐条核对
XOFFSET_RECORD(slg::xm::Hero, id, level, star, name, skills, buffs)

// 只在 arena 内用的继承类型（v1 wire 不支持基类，与现状相同）
XOFFSET_RECORD_DERIVED(Player, (Entity), level, items)
```

- `XOFFSET_RECORD` 同时包含现在 `XOFFSET_ORDINARY_RECORDS` 的声明（无基类、无位域、成员不重叠）。SLG 现在那一行 ORDINARY 声明换成 13 行 RECORD；`XOFFSET_ORDINARY_RECORDS` 在 C++26 下保留给零样板用户。
- 规则沿用现有宏：写在全局作用域；类型名带逗号时用别名；放在类型定义之后、第一次用于容器之前。C++20 下用到一个没描述的记录，编译报错并给出要补的那一行。
- enum：C++20 下必须有固定底层类型。
- 迁移助手 `tools/describe_dump`：在 GCC 16 或 P2996 上编译，传入根类型，反射打印出所有可达记录的 RECORD 行。探针 4 已经能生成 SLG 的 13 行，与手写的逐字相同。tests、bench、examples 里约 545 个 struct 定义（含非记录的辅助结构）用它生成，不手写。

### 4.2 库内结构

```
xoffset_schema.hpp           统一接口，库里所有遍历只用这一层
  member_count_v<T>, member_t<T, I>（type / offset / get(obj)）, base_count_v<T>, base_t<T, I>
  is_described_v<T>, enum_range<E>, implicit_lifetime_aggregate<T>, only_implicit_special_members<T>
xoffset_schema_reflect.hpp   反射后端（有 __cpp_impl_reflection 时默认）
xoffset_schema_describe.hpp  字段表后端 + XOFFSET_RECORD 宏 + 完整性检查
```

- 后端选择：`XOFFSET_SCHEMA_BACKEND` 取 `auto`（有反射就用反射）或 `describe`（强制字段表，在 C++26 编译器上也能用，用来做同编译器对照）。
- C++26 下某个类型写了 RECORD 时，库仍然用反射，只是加静态断言要求两者一致，这样同一个程序里不会出现两种描述。
- 现有约 56 个函数只把 `std::meta::...` 换成接口调用，逻辑一行不动。探针 3 显示接口层被完全内联，机器码与反射版相同。
- 库自带的描述：`XKeyValue<K, V>`；`std::array<E, N>`（按反射看到的形状 `{@0:array[N]<E>}` 生成，签名一致已验证）；库内部的记录。
- offsetof：GCC、Clang 用 `__builtin_offsetof`，模板实参里的逗号不再是问题；非标准布局的派生类型局部关闭 `-Winvalid-offsetof`（无虚基类时三家都支持，MSVC 的写法在阶段 4 确认）。

### 4.3 C++20 下做不到、改为承诺的检查

| 检查 | C++26 | C++20 |
| --- | --- | --- |
| 位域 | 拒绝 | 写不进字段表（编译失败），等价 |
| 没有固定底层类型的 enum 的取值范围 | 从枚举项算 | 要求固定底层类型，否则编译失败 |
| 用户提供的析构、用户声明的赋值运算符（隐式生存期） | 反射判定 | 有 std 或编译器内建特性就用；否则以 RECORD 为承诺；C++26 构建核对 |
| `[[no_unique_address]]` 成员重叠 | 物理重叠能查出，“可能重叠”是承诺 | 偏移递增检查拦下物理重叠，其余同样是承诺 |
| 虚基类 | 拒绝 | 聚合体不可能有虚基类，RECORD_DERIVED 只接受聚合体 |
| 漏写、错写字段 | 不适用 | 编译期完整性证明（3.1.1） |

这些都不影响 wire 和校验结论，只决定直接接收、稳定擦除、SHIFT 这些快路径能不能启用。承诺写错的后果和现在 ORDINARY 声明写错一样（未定义行为），所以文档要求：发布前至少用一个 C++26 编译器（GCC 16 或 P2996）把 schema 编一次。本仓库自己的验证矩阵一直会这样做。

### 4.4 wire 格式：不变

- schema 哈希是 FNV-1a(schema 名 + wire 签名)，签名只由偏移、大小、对齐和类型组成，不含成员名（`detail::wire_schema_hash`）。字段表给出同样的偏移和类型，签名就相同。
- 证据：探针 1（P2996、GCC 16 各 18 个类型，签名、哈希、补齐判定全部相同）；探针 2（四个编译器的 C++20 模式与 P2996 反射的哈希相同）。
- 门禁：`test_v2_golden` 和 `test_cross_compiler_wire` 的语料在 C++20 构建下逐字节相同，并且能互相 `load_verified` 对方的 wire。不升 wire 版本。

### 4.5 性能

- 零成本的理由：反射和字段表都只在编译期起作用，运行期代码由同样的常量偏移和同样的成员访问组成。探针 3：P2996 -O2 共 12 个函数 553 条指令、-O3 12 个函数 561 条，GCC 16 -O2 9 个函数 145 条、-O3 12 个函数 414 条，全部逐条相同，只有符号名不同。
- 构建模式和各自的门禁：
  - M26：今天的 C++26 + 反射。
  - M26D：同一编译器、C++26、强制字段表。要求与 M26 的反汇编逐条相同（GCC 16 和 P2996 两种编译器，SLG、正式 harness、reloc 三个基准程序）。
  - M23D：C++23 + 字段表，推荐的无反射构建。云端 Ir 门禁对 M26 按 S 类阈值（合计 ≤1.003，占比 ≥5% 的阶段 ≤1.02）。
  - M20D：C++20 + 字段表。唯一允许的差异是输出路径的 `resize()` 清零，单独计量并写进文档。
- G8 不受影响：G8 在 Mac 上用 P2996（M26）测，这套方案不改 M26 的机器码。原生 Apple clang 下的性能是新数据，不计入 G8，阶段 4 才测。
- 风险：符号名变化可能改变函数排列，带来 Mac 上 1% 级的计时漂移，由下一次 Mac 阶段复核兜底。

### 4.6 编译器矩阵和验证环境

| 环境 | 做什么 | 在哪里 |
| --- | --- | --- |
| GCC 13.3、Clang 18.1（容器自带） | C++20 全量 ctest、ASan/UBSan、差分 fuzz | 云端 |
| GCC 16.2 docker | M26、M26D、M23D、M20D，Ir 门禁 | 云端 |
| clang-p2996 d77eff1 | 同上，libc++ | 云端 |
| Apple clang（Xcode） | C++20 构建 + ctest，不计时，不需要静音门 | Mac，进 Mac 队列 |
| Android NDK clang | 交叉编译，跑法待定（模拟器或只编译） | 待定 |
| MSVC | C++20 构建 + ctest | **没有环境**（GitHub CI 已停用），见第 9 节 D6。Windows 本身已有 MinGW-w64 GCC 16 这条路（CLIENT_TARGETS.md，CI 停用前验证过），C++20 下 MinGW GCC 同样可用，所以 MSVC 不是 Windows 客户端的前提 |

Apple 平台有一件与反射无关、但原生构建会碰到的事要先核实：`receive_raw` 和 `validation_scratch` 的接口用了 `std::pmr`，Apple 系统 libc++ 对 `std::pmr` 有部署版本要求（我记得是 macOS 14、iOS 17 起，未核实）。阶段 4 在 Mac 上确认，必要时把 `std::pmr` 改成可选。

## 5. 本线程的探针（只验证可行性，不是库代码）

| # | 验证什么 | 结果 |
| --- | --- | --- |
| 1 | 字段表算出的 wire 签名、schema 哈希、补齐判定，与库里反射算出的比较（真实库，SLG 13 个类型 + 跨编译器语料 5 个类型） | P2996：18/18 相同；GCC 16（`-freflection`）：18/18 相同 |
| 2 | 不含 XOffset 头的 C++20 程序，用字段表算 7 个记录的哈希，与 P2996 反射值比较 | GCC 13.3、Clang 18.1、GCC 16.2（`-std=c++20`）、P2996（`-std=c++20` 和 `-std=c++26`）全部 7/7 相同；`-Wall -Wextra -Werror` 无警告 |
| 2b | 漏写、错序、用重复掩盖漏写、漏写数组成员、sizeof 查不出的漏写 | 5/5 在 GCC 13 和 Clang 18 上编译失败，报错指出类型和原因 |
| 3 | SHIFT 重定位热路径（`shift_rebase_inline_value`、`empty_vacated_slot`）在 4 个 SLG 记录上的机器码 | P2996 -O2/-O3、GCC 16 -O2/-O3 全部逐条相同 |
| 4 | 用反射生成字段表 | 从 3 个根类型生成 SLG 的 13 行，与手写逐字相同 |

探针没有覆盖、留给实施阶段的：整库在 C++20 下编译（阶段 3）；MSVC 和 Apple clang（没有环境）；大 schema 的常量求值步数（尤其 MSVC 的默认上限；约束是不能要求用户加 `-fconstexpr-steps`）；编译时间。

## 6. 对定位和对外叙事的影响

| 条目 | 现在 | 方案落地后 |
| --- | --- | --- |
| 定位一句话 | 不提编译器 | 不变 |
| K0 准入 | TypeLayout admission | XOffset 自己的准入，规则相同；C++26 下与 TypeLayout 交叉核对 |
| K1 | 普通聚合体，反射自动得出布局和指纹，不用 IDL | 仍是普通聚合体、不用 IDL；C++26 下零样板，C++20 下加一行由编译器证明完整的字段表 |
| K6 | 精确哈希 | 不变，哈希值也不变 |
| K7 | 签名只能用 P2996 算；为用户 schema 在构建期核对还是目标 | 每个编译器都能自己算签名，`XOFFSET_PIN_SCHEMA` 在 MSVC、Apple clang 上也能让构建失败，K7 的“目标”变得可达 |
| K8 | 客户端用 clang-p2996 交叉编译（susu 10-21 12:50Z 定）；Linux arm64、Windows x64（MinGW GCC 16）已验证，Android、iOS 因缺反射工具链不可行（CLIENT_TARGETS.md） | 客户端用各平台原生编译器按 C++20 构建，Android（NDK Clang）、iOS（Apple clang）变为可行。**susu 10-26 16:36Z 批准（D5），取代 10-21 12:50Z 的决定** |
| 第 2 节“限制” | 库只能用 P2996 编译器构建 | 删掉这一条 |

CppCon 2026（C++26 反射讲题）建议的说法：“反射给出零样板，也给出真相：它推导布局，写出并核对 C++20 的字段表；C++26 服务端和 C++20 客户端之间搬的是同一份字节。”这比“只能用实验编译器”更容易被采用，也给讲题一个落地的结尾。要避免的说法：“C++20 也零样板”（不是）；“字段表不会写错”（会写错，但编译不过）。

## 7. 实施计划

前提（D4 已定）：全部先在独立分支做；INT3 合入 next_cpp26 之前，不往 next_cpp26 合任何东西。阶段 1 和阶段 3 的一部分可以并行。

| 阶段 | 内容 | 验收 | 线程（high） |
| --- | --- | --- | --- |
| 1 字段接口重构 | 新增 `xoffset_schema.hpp` 和反射后端；56 个函数改走接口；准入改为 XOffset 自己算（C++26 下与 TypeLayout 交叉断言） | local_validate 全过；M26 改动前后反汇编逐条相同（GCC 16 + P2996，SLG、正式、reloc） | A「schema 接口重构」 |
| 2 字段表后端 | `XOFFSET_RECORD`、`XOFFSET_RECORD_DERIVED`、完整性证明、库自带描述、`XOFFSET_SCHEMA_BACKEND=describe`、RECORD 与反射比对、迁移生成器 `tools/describe_dump`；负例编译测试 | M26D 与 M26 反汇编逐条相同；M26D 下全量 ctest | A（接着做） |
| 3 C++20 构建 | CMake `XOFFSET_CXX_STANDARD`（20/23/26）；C++20 下不包含 `<meta>` 和 TypeLayout；FixedString 搬入；enum 规则；隐式生存期回退；`resize_and_overwrite` 回退；为 tests、bench、examples 生成 RECORD 行；local_validate 增加 GCC 13、Clang 18、GCC 16 c++20/23、P2996 c++20 四列 | 四列全量 ctest、ASan/UBSan、差分 fuzz 全过；跨编译器 wire 语料逐字节相同、互相能载入；M23D 对 M26 的 Ir 不出警告 | B「C++20 构建与测试矩阵」（CMake、TypeLayout 拆分、测试行生成可以与 A 并行） |
| 4 原生客户端工具链 | Apple clang（macOS、iOS 模拟器）、Android NDK、MSVC；`std::pmr` 可用性 | 各平台 ctest；跨平台 wire 语料一致；签名与 `tools/sigs` 基线一致 | C「原生工具链验证」 |
| 5 文档和定位 | README、USER_GUIDE、CLAUDE.md、PRODUCT_POSITIONING 的 K0/K1/K7/K8 | 产品定位线程审 | 产品定位线程 |

合并进 next_cpp26 的方式（D4，已定）：阶段 1、2 是“C++26 机器码不变”的重构，按文件拆成 5–6 个 PR，等 INT3 合入后的空档依次合。判定按 `MERGE_RULES_V2_20261026.md`（sha256 `62d178d4…`，10-26 16:38Z 起）新增的 0 类判据：在云端 GCC 16 和 P2996 d77eff1 两种编译器下，正式 harness、bench/slg、bench/reloc 每个函数的反汇编逐条相同（比较时忽略地址和符号名），PR 里附两种编译器的比对脚本输出。.text 里函数的顺序和大小也相同，直接算 0 类；只是摆放变了，仍算 0 类，但下一次阶段复核要把它单独列为二分点（INT1 的 map.insert 指令完全相同，只是对齐变了，Mac 上就慢了 6–8%）。Mac 在下一次阶段复核时顺带核对二进制。阶段 3 的 CMake 和测试改动按原有 0 类处理。和在途性能 PR（#87、#93 等）的冲突主要在 `xoffsetdatastructure.hpp` 的遍历函数里，规模小，由实现线程在自己的分支上处理，不动别人的分支。

粗估（线程时间，不含排队）：阶段 1 约 1 天，阶段 2 约 1 天，阶段 3 约 1–2 天，阶段 4 取决于环境。

## 8. 风险和未决

| 风险 | 影响 | 处理 |
| --- | --- | --- |
| MSVC：常量求值步数上限；`__VA_OPT__` 需要 `/Zc:preprocessor`；`[[no_unique_address]]` 被忽略 | MSVC 编不过，或布局不同 | 布局不同时哈希不同、对端拒收，不会静默出错；编译问题在阶段 4 处理；CMake 接口目标自动加 `/Zc:preprocessor` |
| 大 schema 的 consteval 字符串拼接在 GCC 12、Clang 16 上超出默认步数 | 编不过 | 阶段 3 用 SLG 最大的 BattleMsg、PlayerState（签名约 1.6 KB）实测；必要时签名改为分段哈希，不加编译参数 |
| C++20 下承诺型检查被用户写错 | 快路径上未定义行为 | 文档要求至少用一个 C++26 编译器编一次 schema；提供 describe_dump |
| 双后端的维护成本 | 验证矩阵变大 | local_validate 新增的列并行跑；反射后端预计约 100 行 |
| 函数摆放引起的 Mac 计时漂移 | 1% 级的假回退 | Mac 阶段复核兜底，按“二进制相同”判 |
| Apple 系统 libc++ 的 `std::pmr` 部署版本 | 低版本 iOS 编不过 | 阶段 4 核实，必要时改为可选 |

## 9. 决定（susu 10-26 16:36Z“全按推荐”）

| # | 问题 | 决定 |
| --- | --- | --- |
| D1 | 最低支持哪个标准 | C++20 起步，C++23 可选；性能宣称以 C++23 构建为准 |
| D2 | 保留 C++26 反射路径吗 | 保留，双后端：C++26 可以不写宏，写了就核对 |
| D3 | 核心库还依赖 TypeLayout 吗 | 不再依赖；TypeLayout 留给 C++26 签名工具和交叉核对 |
| D4 | 实现什么时候进 next_cpp26 | 先全部在独立分支做；INT3 合入后的空档，按“两种编译器反汇编相同算 0 类”分批合。性能优化已写进 MERGE_RULES_V2（sha256 `62d178d4…`），并加了一条：摆放变化算 0 类，但列为下一次阶段复核的二分点 |
| D5 | K8 客户端改用原生编译器吗 | 改；取代 10-21 12:50Z 的决定。K0/K1/K7/K8 和第 2 节“限制”由产品定位线程在阶段 5 更新 |
| D6 | MSVC 在哪里验证 | 先不承诺 MSVC（标“未验证”），Windows 客户端继续走 MinGW GCC；GCC、Clang、Apple clang、NDK 先做；有 Windows 机器时再加 |

## 10. 实现线程

三个实现线程（high）的标题、分支、依赖顺序、验收和完成标准写在 `/mnt/project-files/remove-cpp26-20261026/IMPL_THREADS_20261026.md`。顺序概括：A「schema 接口重构」和 B「C++20 构建与测试矩阵」现在并行开工；B 的整库 C++20 构建等 A 的字段表后端出来；C「原生工具链验证」先摸环境，等 B 的 C++20 列在云端全绿再跑；INT3 合入后按 A → B → C 的顺序合；阶段 5 由产品定位线程接。

## 附录 A：反射使用点（按文件，函数名）

- `xoffsetdatastructure.hpp`（28）：reflected_base_count, reflected_member_count, reflected_member_wire_admitted, reflected_member_needs_validation, validate_base_at, validate_member_at, member_can_relocate, base_can_relocate, can_relocate_without_allocation, move_assignment_owns_address, relocate_construct, ordinary_record_has_no_bases, ordinary_record_has_no_bit_fields, ordinary_record_members_disjoint, stable_erase_implicit_lifetime, stable_erase_member_supported, stable_erase_supported, stable_erase_rebase_subobjects, wire_record_field_signature, destroy_reflected_prefix, reflect_init_nth, reflect_init_base_nth, reflect_transfer_init_nth, reflect_transfer_base_nth, record_has_implicit_padding, collect_record_padding, migrate_base_at, migrate_member_at
- `receive_raw.hpp`（12）：raw_implicit_lifetime, raw_enum_bounds, raw_member_ranges_disjoint, raw_member_supported, raw_only_implicit_special_members, raw_member_future_supported, raw_needs_validation, raw_needs_activation, validate_wire_value_raw, reactivate_inline_value_raw，以及两处匿名遍历
- `xoffset_core_lifetime.hpp`（7）：shift_rebase_domain, shift_rebase_inline_value, empty_vacated_slot, subtree_copy_domain, subtree_measure_value, subtree_fix_value, subtree_compact_value
- `xoffset_verified_paths.hpp`（7）：compact_members_have_heads, compact_members_supported, compact_member_at, direct_member_at, compose_members_descend, meets, compose_member_at
- `xoffset_snapshot_index.hpp`（2）：inline_heads, find_parent
- TypeLayout：detail/reflect.hpp（成员/基类计数、虚基类检测）、detail/signature_impl.hpp（布局签名）、admission.hpp（`is_byte_copy_safe`）

## 附录 B：依据

- 代码：next_cpp26 `7620bc3`，TypeLayout `fe29592`。
- 仓库 `docs/CLIENT_TARGETS.md`（K8 各客户端目标的现状）。
- `/mnt/project-files/PRODUCT_POSITIONING.md`（K0–K8，第 2 节“限制”）；`/mnt/project-files/PERF_GOAL.md`（G8 在 Mac 上用 P2996 测）；`/mnt/project-files/perf-next-20261022/MERGE_RULES_V2_20261026.md`（0/S/L 类）。
- 探针运行记录：2026-10-26，本线程，云端容器（GCC 13.3、Clang 18.1、GCC 16.2 docker、clang-p2996 d77eff1）。
