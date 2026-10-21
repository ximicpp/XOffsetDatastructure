# XOffsetDatastructure 用户指南（0.2.0-alpha.1，V2 格式）

本文面向使用者，说明怎么定义类型、构建、修改、保存、加载，以及必须遵守的约束。
部署要求见 [DEPLOYMENT.md](DEPLOYMENT.md)，格式细节见 [V2_FORMAT.md](V2_FORMAT.md)，输入与兼容边界见
[ALPHA_COMPATIBILITY.md](ALPHA_COMPATIBILITY.md)。

## 1. 基本模型

- 一个 `XBuffer` 拥有一块地址稳定的虚拟内存 arena。根对象和它引用的所有字符串、
  数组、集合都放在这块 arena 里，彼此之间用相对偏移相连。
- arena 的字节就是序列化结果，读写字段不需要编码或解码。
- `save_verified` 把 arena 复制成一份调用方独占的 wire（`std::string`），发送方之后
  可以继续修改原 buffer。
- `load_verified` 先完整校验 wire，再把字节复制进新的 arena。校验通过之后，读取
  就是普通的成员访问。

## 2. 定义类型

```cpp
#include "xoffsetdatastructure.hpp"
using namespace XOffsetDatastructure;

struct Profile {
    std::int32_t           id{0};
    XString                name;     // ≤10 字节的字符串内联在 16 字节头里
    XString                bio;      // 更长的字符串占一个 arena 块
    XVector<std::int32_t>  scores;
    XSet<std::int32_t>     tags;
    XMap<XString, std::int32_t> stats;
};
XOFFSET_REGISTER_SCHEMA_NAME(Profile, "example.Profile")
```

- 普通聚合体即可，布局和 schema 身份由 P2996 反射自动得出，不需要 IDL 或宏描述字段。
- 成员只能是标量、枚举、定长数组、库容器（`XString`、`XVector`、`XFixedVector`、
  `XSet`、`XMap`）和递归满足这些条件的结构体。原生指针、引用、`union`、多态类型、
  `std::string`/`std::vector` 等都会在编译期被拒绝。
- 每种根类型用 `XOFFSET_REGISTER_SCHEMA_NAME` 注册一个唯一且稳定的名称。

## 3. 构建与修改

```cpp
auto cap = XBuffer::maximum_capacity(32u << 20).with_growth(XGrowPolicy::Automatic);
XBuffer buf = XBuffer::create<Profile>(1u << 20, cap);
Profile& p = buf.root<Profile>();
p.id = 7;
p.name = "Alice";
p.bio = "a much longer biography text";
p.scores.push_back(90);
p.tags.insert(3);
p.stats.insert_or_assign("hp", 100);

std::vector<std::int32_t> v{1, 2, 3, 4};
p.scores.assign(v.begin(), v.end());   // 连续且平凡可复制：一次精确分配
```

- 默认增长策略是 `XGrowPolicy::Explicit`，空间不够时抛 `XBadAlloc`；需要自动增长时
  显式选择 `XGrowPolicy::Automatic`。`maximum_capacity` 始终是硬上限。
- 容器头必须位于所属 arena 内。给插入接口传 `std::string_view`、`std::string` 等
  原生值，不要在栈上构造绑定 arena 的 `XString` 临时对象。
- 容器扩容、插入、删除会移动元素，不能长期保存元素指针；arena 基址和根对象地址
  在增长时保持不变。
- 也可以用带类型的包装 `TypedXBuffer<T>`：`create`、`root()`、`save_verified()`、
  `load()`（走校验路径）。它属于便利接口（第 8 节 E2）。

### 3.1 删除与插入的性能

`XVector` 从中间 `erase`/`insert` 一个元素，要把后面的元素整体挪一格，成本与后面
的元素个数成正比。在循环里逐条从中间删除是 O(n·k)，元素含容器头时尤其明显
（SLG 原型里 2000 条行军每 tick 逐条删除，每周期约 160 万次元素搬移）。

- 批量删除用 `stable_erase_if`：一次遍历，保持原顺序，每个保留的元素最多挪一次。
- 元素是含 `XString`/`XVector`/`XMap` 等成员的记录类型时，下面两个声明会打开更快的
  路径。它们是对**源码**的承诺：记录只有普通的非静态成员，没有基类、`union`、
  位域、`[[no_unique_address]]`，成员互不重叠。库不靠反射猜测这些性质，声明错了
  是未定义行为。

```cpp
struct Troop { std::int32_t type{}; std::int32_t count{}; };
struct March {
    std::int64_t id{};
    std::int32_t eta{};
    XVector<Troop> troops;
};

namespace XOffsetDatastructure {
// 中间 erase/insert 一次 memmove 挪动，只重建内联容器头（成员类型也要声明）。
template<> struct wire_record_layout<Troop> { static constexpr bool ordinary_members = true; };
template<> struct wire_record_layout<March> { static constexpr bool ordinary_members = true; };
// 允许对 XVector<March> 调用 stable_erase_if。
template<> struct StableEraseOrdinaryMembers<March> : std::true_type {};
}

marches.stable_erase_if([](const March& m) { return m.eta <= 0; });
```

- 不写这些声明时，行为和结果完全相同，只是走逐元素搬移的较慢路径，并且
  `stable_erase_if` 不可用（编译期报错）。两条路径输出的 wire 字节相同。

## 4. 保存与加载

```cpp
std::string wire = buf.save_verified<Profile>();

XBuffer in = XBuffer::load_verified<Profile>(wire, cap);
const Profile& r = in.root<Profile>();
std::string_view who = r.name.view();
```

- wire 由 96 字节头、payload 和空洞表组成；空闲区不小于 4 KiB 时稀疏输出。
- 加载时的校验包括：头部与版本、schema 身份、空洞表、从根出发的整图遍历，以及
  「活块 + 空洞恰好铺满已用区」。任何一项不满足都会抛 `XException`，不会发布根对象。
- 加载后的 buffer 可以继续修改再保存；保存前的空闲空间会随空洞表带过去并继续复用。
- `load_unverified` 跳过校验，只适合调用方已经信任的数据。
- 直接把数据收进最终 arena 的接收接口是 `begin_verified_receive<T>` /
  `finish_verified`（`xoffset_verified_receive.hpp`），校验强度与 `load_verified` 相同。

## 5. 必须遵守的约束

| 约束 | 不遵守的后果 |
| --- | --- |
| 手动分配（如 `XBuffer::allocator<T>()`、`arena()->allocate_aligned`）且不能从根到达的内存，必须在 `save_verified` 前释放 | 保存出的 wire 无法通过加载校验（铺满检查失败） |
| 同一个 buffer 的写操作由调用方同步 | 数据竞争，行为未定义 |
| buffer 对象要比它的 `XHandle`、借用的 span 活得久 | 悬垂访问 |
| 同一可执行文件内所有翻译单元使用相同的头文件版本、编译选项和工具链 | 布局或 schema 身份不一致 |
| 接收不可信输入前限制 wire 长度、`maximum_capacity` 和并发 buffer 数 | 单个合法 wire 也可能占用大量内存 |
| 只用于 64 位小端平台（不支持 32 位安卓渠道包） | 其他平台编译期 `#error` |

## 6. 错误处理

- 所有库错误都是 `XException`（派生自 `std::runtime_error`）；空间不足是它的子类
  `XBadAlloc`。
- 库容器的修改在分配失败时保留原有逻辑内容，但容量和分配器记账不一定回滚。多步
  业务操作不是事务。

## 7. schema 变更

当前版本**不支持 schema 演进**：

- wire 头里的 schema 身份必须与读取方类型完全一致。增加、删除、重排或改变任何
  一个字段，旧 wire 都会被拒绝。
- 适合收发双方使用同一版本类型定义的场景（同一次构建部署的进程之间）。客户端场景下，
  任何改了 schema 的服务端更新都必须和客户端同时发版。
- schema 演进是 TODO，并且是客户端场景规模化之前必须完成的前置项。
- 需要长期保存或跨版本读取的数据，应保留能读取原版本的程序，自行导出再导入。
- 不同编译器之间的 wire 兼容目前只在固定工具链内成立；`tools/sigs/` 的签名基线
  用于发现布局变化，不构成任意平台的兼容保证。

## 8. 核心承诺与 API 稳定性

alpha 之间不承诺源码 API 或 ABI 稳定。下面 K1–K7 是核心承诺，后续版本不得回退；
相关接口如有改名或签名变化，会在 CHANGELOG 中单独列出并给出替代写法。

| # | 承诺 | 接口 / 机制 |
| --- | --- | --- |
| K1 | 用普通 C++ 聚合体定义 schema，反射自动得出布局和指纹，不用 IDL | `XOFFSET_REGISTER_SCHEMA_NAME`、`XString`、`XVector`、`XFixedVector`、`XSet`、`XMap` |
| K2 | 对象直接建在 arena 里，原地读、原地改（含变长修改），空闲块复用 | `XBuffer::create<T>`、`root<T>`、`has_root<T>`、容器的公开成员函数、`maximum_capacity`、`with_growth`、`XGrowPolicy` |
| K3 | 独立输出：发出的是调用方独占的副本，发完可以继续改 | `save_verified<T>` |
| K4 | 接收时完整结构校验：截断、越界、重叠、未铺满、非法值、schema 不符一律拒绝 | `load_verified<T>`、`XException` |
| K5 | 接收后接着改、再发出，空闲空间随空洞表带过去，不退化 | 同上 |
| K6 | schema 指纹精确匹配，不同就拒绝 | wire 头 `schema_hash` |
| K7 | 跨编译器、跨平台的对象表示一致 | `tools/sigs/` 签名基线与跨目标比对 |

K7 的现状与目标：

- **现状**：x86-64 Linux、x86-64 Windows、arm64 iOS、arm64 Android 四个目标上，库自带的
  3 个测试类型的签名在 CTest 里核对（`check_compat`、`check_signature_regeneration`）。
  这不会让用户自己的构建失败。
- **目标**：构建时为用户 schema 导出签名，任一目标不一致就构建失败；GCC 16 并入同一门禁。

K4 的前提：接收方必须设置 `maximum_capacity`。否则发送方可以发接近 payload 上限
（约 2 GiB）的数据，让接收方复制并做全图校验，造成资源耗尽。默认上限建议见
[DEPLOYMENT.md](DEPLOYMENT.md) 第 5 节。

校验的承诺边界：对不可信输入保证内存安全和 schema 一致，**不保证内容完整性**。
只改动某个标量字段值的数据会被接受，因此不能把校验当作防篡改手段。

**目标（尚未承诺）K8**：客户端与服务端用同一套库，客户端用 clang-p2996 交叉编译
Windows、Android、iOS，能接收、读写、校验、再发。三个客户端目标都交叉编译跑通、
跨目标 wire 一致之后，再加入核心承诺。

以下接口提供但属于可选或实验性质，可能在后续 alpha 中改变：

| # | 接口 | 说明 |
| --- | --- | --- |
| E1 | `freeze_verified`、`begin_verified_receive` / `finish_verified` | 零拷贝分段发送、直接收进最终 arena |
| E2 | `save_verified_into`、`TypedXBuffer<T>` | 便利接口 |
| E3 | `XBufferPool`、预留与增长策略细节 | 小 buffer 高频场景 |
| E4 | `load_unverified` | 只用于调用方已信任的数据 |
| E5 | `XCompactor` | 同类型压缩整理 |

`XHandle`、诊断与统计接口、`XBufferCore` 的底层成员不属于承诺范围。

## 9. 性能数据的口径

唯一的正式测量来自一台 Apple M4、一个 demo 负载，对比 cista、FlatBuffers、
Cap'n Proto、cereal，见 [docs/perf/round3-20261021/REPORT.md](perf/round3-20261021/REPORT.md)。
引用时请同时说明机器、负载和竞品用法（例如 FlatBuffers 走 Object API）；
没有 Linux 或 Windows 的性能数据。
