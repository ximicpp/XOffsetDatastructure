# XOffsetDatastructure 用户指南（0.3.0-beta.1，V2 格式）

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
- 容器扩容、插入、删除会移动元素，规则和 `std::vector` 相同；`XSet`、`XMap` 是有序
  平铺数组，规则和 `std::flat_set`、`std::flat_map` 相同，**和 `std::set`、`std::map`
  不同**。逐个操作的规则见第 3.2 节；arena 基址和根对象地址在增长时保持不变。
- **元素是含 `XString`、`XVector`、`XMap` 等成员的记录类型时，先把这些记录声明为
  普通记录（第 3.1 节）。** 不声明也完全正确，只是中间插入删除、扩容要逐个元素搬移：
  云端 GCC 16 微基准里（只看比例，不是正式测量），元素带字符串和向量成员的
  `XVector` 中间删除，不声明约是 `std::vector` 的 1.6 倍，声明后约 0.73 倍。
- 能改什么、不能改什么（schema、写者、所有权）见第 3.3 节。
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

### 3.2 引用和迭代器什么时候失效

库不替你跟踪指向元素的引用，和 std 容器一样要按操作判断。地址分三层：

| 层 | 指什么 | 例子 |
| --- | --- | --- |
| 元素 | 容器里的元素对象，包括元素里嵌套的容器头 | `p.scores[0]`、`marches[3]`、`marches[3].troops` 这个头本身 |
| 子数据 | 元素拥有的块 | `p.bio.view()` 指向的字符、`marches[3].troops` 的元素数组 |
| 区域 | 整个 payload：根对象和它能到达的一切 | `buf.root<Profile>()` |

**容器操作只影响这个容器自己。** 分配只从空闲块或区域尾部取新块，释放只把块记进
空闲表，不会挪动别的容器的元素或子数据。

| 操作 | 本容器元素的引用、指针、迭代器 | 对照 |
| --- | --- | --- |
| `push_back`、`emplace_back` | 没扩容时都有效；扩容时全部失效 | 同 `std::vector` |
| `insert(pos, …)` | `pos` 及之后失效；扩容时全部失效 | 同 `std::vector` |
| `erase(pos)`、`pop_back()` | 被删的及之后失效 | 同 `std::vector` |
| `stable_erase_if(pred)` | 第一个被删的位置及之后失效 | 同 `std::erase_if` |
| `reserve(n)`、`resize(n)` 超过容量 | 全部失效 | 同 `std::vector` |
| `resize(n)` 不超过容量 | 缩小时第 n 个及之后失效，增大时都有效 | 同 `std::vector` |
| `clear()`、`assign(…)` | 全部失效 | 同 `std::vector`；`clear()` 保留容量，并把旧元素的字节清零 |
| `XString` 的赋值、`clear()`、`reserve()` | 之前取到的 `view()`、`data()`、`c_str()` 失效 | 同 `std::string` |
| `XSet` 的 `insert`、`emplace`；`XMap` 的 `try_emplace`、`emplace`，以及 `insert_or_assign`、`operator[]` 插入新键 | 插入位置及之后失效；扩容时全部失效 | 同 `std::flat_set`、`std::flat_map`；**不同于 `std::set`、`std::map`**（它们插入不影响别的元素） |
| `XMap` 的 `insert_or_assign` 给已有键赋值 | 其他元素不受影响 | 同 std |
| `XSet`、`XMap` 的 `erase(key)` | 被删的及之后失效 | 同 `std::flat_map`；不同于 `std::map` |
| 读、改已有元素的值（不改变容器长度） | 都有效 | 同 std |
| 同一个 buffer 里的移动赋值 `a = std::move(b)` | `b` 的元素不挪动，改归 `a`，`b` 变空；`a` 原来的元素失效。跨 buffer 时是复制，`b` 不变 | 同 `std::vector` 的移动赋值 |

- 扩容有时能原地完成（容器的块正好在区域尾部时），这时地址不变。这不在承诺里，
  请按"扩容即全部失效"写代码。
- 元素搬家时，它拥有的子数据通常不动：容器头在新位置重建，仍指向原来的块。这也
  只是实现细节，和 `std::vector<std::string>` 的短字符串优化一样，不要依赖。
- `XSet`、`XMap` 查找是 O(log n)，插入、删除是 O(n)，要和 `std::flat_set`、
  `std::flat_map` 比，而不是和基于节点的 `std::set`、`std::map` 比。

**buffer 操作影响整块区域。** epoch 是 buffer 对象上的计数，`XHandle` 看到它变了就
重新解析根。

| 操作 | 根对象和区域的地址 | 指进 payload 的引用、span、迭代器 | epoch |
| --- | --- | --- | --- |
| 容器操作触发的 Automatic 增长、显式 `grow()` | 不变（在虚拟内存预留里扩展） | 只按上表失效 | 不变 |
| `save_verified`、`save_verified_into`、`save` | 不变（只收缩逻辑长度） | 有效 | 不变 |
| `reload_verified`、`reset<T>()` | 不变（复用映射） | 全部失效 | 加一 |
| `src.clone_into(dst)` | dst 不变（复用映射） | dst 原有的全部失效；`src` 不受影响 | dst 加一（自拷贝被拒绝时不变） |
| `clone()` | 结果是新 buffer | 源 buffer 不受影响 | 新 buffer 从 0 开始 |
| `load_verified`、`load_unverified`、`XCompactor::compact` | 结果是新 buffer | 源 buffer 不受影响（压实只读源，换更大的容量重试时也不动它） | 新 buffer 从 0 开始 |
| `freeze_verified(std::move(buf))` | payload 不动，改归快照 | 成功后 `buf` 变空，payload 只能通过快照读；抛异常时 `buf` 不变 | `buf` 加一 |
| `begin_verified_receive` / `finish_verified` | 结果 buffer 的 payload 就是收包时写入的那块内存（`payload_address()`） | 收包用的 span 在 `finish_verified` 之后不能再写 | 新 buffer 从 0 开始 |
| `XBuffer` 的移动构造、移动赋值、`swap` | 不变，存储随对象转移 | 仍然有效，但现在属于另一个对象 | 两边都加一 |
| 析构 | 释放 | 全部失效 | 不涉及 |

- `XHandle` 绑在 buffer 对象上，不跟着 payload 走：移动之后原对象为空，绑在它上面的
  `XHandle` 解析为空指针；`swap` 之后解析到这个对象现在的内容。要跟着数据走，在新
  对象上重新建 `XHandle`。
- payload 里没有绝对地址，任何两次操作之间都可以把区域的字节整块复制到别处继续
  用（保存、加载就是这样做的）；但复制得到的是新 buffer，旧地址上的引用不会指过去。

### 3.3 能改什么、不能改什么

能改的：任意字段写入；变长修改，也就是字符串、数组、集合、映射的增删改；空闲块
复用；收到（`load_verified`）之后接着改、再发出。

不能做的有三条，是这种数据结构本身的限制：

1. **不能改 schema。** 增加、删除、重排或改变任何字段，旧 wire 都会被拒绝（第 7 节）。
   类型定义要在收发两端同时更新。
2. **一个 buffer 同一时刻只能有一个写者。** 库不加锁，写操作要和这个 buffer 上的其他
   读写互斥，由调用方同步（第 5 节）。多个进程通过共享内存同时读写一个 buffer 不在
   支持范围内。
3. **所有权是一棵树。** 每个块只属于一个容器头：两个容器不能共享同一块子数据，对象
   之间不能互相引用，也不能成环。原生指针和引用在编译期就被拒绝（第 2 节）；要表达
   图，存元素下标或 ID（例如 `std::uint32_t` 下标，或 `XMap` 的键）。拷贝赋值是深拷贝，
   同一个 buffer 里的移动赋值是转移所有权。**不要把一个容器移动赋值给它自己子树里的
   容器，或者反过来**：Release 构建不检查这种情况，结果是未定义行为。

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

### 4.1 大量小消息：用 `XBufferPool` 复用映射

每个 `XBuffer` 都有自己的虚拟内存预留：建立时预留 `maximum_capacity` 并开启宿主
前缀页和载荷页，销毁时整块释放。这套系统调用和缺页在 Apple M4 上约 4.4 µs，
与预留大小关系不大。消息只有几 KB、而且每条都新建、销毁一个 buffer 时（战斗服往返、
只读战报、客户端收包），这部分开销会远大于校验本身。

这类接收方和发送方应当共用一个池，让销毁后的映射留给下一条消息复用：

```cpp
XBufferPool pool({16, 16 * cap_bytes, 64u << 20});   // 最多缓存 16 块空闲映射
const auto cap = XBuffer::maximum_capacity(cap_bytes).with_pool(pool);

XBuffer in = XBuffer::load_verified<Msg>(wire, cap);  // 仍是完整校验
// ... 使用 in；离开作用域时映射回到池里，而不是还给操作系统
```

- 池只复用映射，不跳过任何校验，也不改变 wire。复用的映射在重新暴露前会清零。
- 池按 `maximum_capacity` 的精确值匹配，同一类消息请用同一个上限。
- 池里的空闲映射仍占着虚拟地址和已触碰的页，`Limits` 的三个上限（块数、预留字节、
  可访问字节）决定最多留多少；`pool.clear()` 立即全部释放。
- `XBufferPool` 目前属于实验接口（见第 8 节）。

SLG 原型（M 规模，Apple M4，原型数据，非正式验收）的效果：

| 场景 | 每条消息新建映射 | 用池 |
| --- | ---: | ---: |
| 战斗服往返，XOffset ÷ cereal / FlatBuffers / protobuf（几何均值） | 2.91 / 4.16 / 1.28 | 0.58 / 0.85 / 0.25 |
| 只读战报，每次读取 | 4.9 µs | 0.44 µs |

只读战报用池后仍慢于 cereal（约 2.6 倍），剩余开销在建立 owner 和"先拷贝再校验"。

### 4.2 同进程整块复制：`clone()` / `clone_into()`

同一进程里需要一份独立副本时（状态分叉、回滚、快照后继续改），不必经过 wire：

```cpp
XBuffer snapshot = buf.clone();     // 新 owner：大小、maximum_capacity、增长策略、池都同 buf
buf.clone_into(scratch);            // 复用 scratch 的映射和地址，像 reload_verified
```

- 只拷贝一次已用区，空闲表、类型化根访问和 schema 绑定一起带过去，不生成 wire、
  不校验（源是本进程正在用的 buffer）。副本与源完全独立，之后同样的修改得到同样的字节，
  `save_verified` 输出也相同。
- 未校验的源（`load_unverified`）复制后仍未校验：`root<T>()` 照样拒绝。
- `clone_into(dst)` 之后，dst 原有内容的根引用、元素指针、`string_view`、span 全部失效，
  `dst.epoch()` 加一，`XHandle` 按新内容重新解析；dst 的地址、`maximum_capacity` 和增长
  策略不变，大小变成源的大小（不超过 dst 的上限）。
- 失败：`clone_into(dst)` 的源就是 dst 时抛 `XException`，什么都不改；源的已用区超过 dst
  的 `maximum_capacity` 时抛 `XBadAlloc`；除自拷贝外的任何失败都让 dst 成为没有根的空
  arena，与 `reload_verified` 失败相同，之后可以再 `clone_into` 或 `reset<T>()`。
  `clone()` 失败不会改动源。
- 详细语义见 [BUFFER_REUSE.md](BUFFER_REUSE.md)。

## 5. 必须遵守的约束

| 约束 | 不遵守的后果 |
| --- | --- |
| 手动分配（如 `XBuffer::allocator<T>()`、`arena()->allocate_aligned`）且不能从根到达的内存，必须在 `save_verified` 前释放 | 保存出的 wire 无法通过加载校验（铺满检查失败） |
| 同一个 buffer 的写操作由调用方同步 | 数据竞争，行为未定义 |
| 不把容器移动赋值给它自己子树里的容器（或反过来，第 3.3 节） | Release 构建下行为未定义 |
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

alpha 之间不承诺源码 API 或 ABI 稳定。下面 K0–K7 是核心承诺，后续版本不得回退；
相关接口如有改名或签名变化，会在 CHANGELOG 中单独列出并给出替代写法。

| # | 承诺 | 接口 / 机制 |
| --- | --- | --- |
| K0 | 可重定位：payload 里没有任何绝对地址，任何修改之后仍可整块复制到任意地址接着读写；同一进程、跨进程、跨机器搬的是同一份字节。K2、K3、K5 都建立在它上面 | 编译期准入（typelayout：布局签名里带指针记号的类型一律拒绝，含裸指针、成员指针和虚表）；容器头只存相对偏移，根用基址偏移 |
| K1 | 用普通 C++ 聚合体定义 schema，反射自动得出布局和指纹，不用 IDL | `XOFFSET_REGISTER_SCHEMA_NAME`、`XString`、`XVector`、`XFixedVector`、`XSet`、`XMap` |
| K2 | 对象直接建在 arena 里，原地读、原地改（含变长修改），空闲块复用 | `XBuffer::create<T>`、`root<T>`、`has_root<T>`、容器的公开成员函数、`maximum_capacity`、`with_growth`、`XGrowPolicy` |
| K3 | 独立输出：发出的是调用方独占的副本，发完可以继续改 | `save_verified<T>` |
| K4 | 接收时完整结构校验：截断、越界、重叠、未铺满、非法值、schema 不符一律拒绝 | `load_verified<T>`、`XException` |
| K5 | 接收后接着改、再发出，空闲空间随空洞表带过去，不退化 | 同上 |
| K6 | schema 指纹精确匹配，不同就拒绝 | wire 头 `schema_hash` |
| K7 | 跨编译器、跨平台的对象表示一致 | `tools/sigs/` 签名基线与跨目标比对 |

K0 的边界：搬的单位是整个区域，不是其中单个对象（容器头禁止拷贝和移动）。正在使用的
buffer 地址固定（在 VM 预留内增长），只在显式时间点搬：输出、接收、reload、`XCompactor`
压缩；`XHandle` 用 epoch 在搬动后重新定位。同 ABI、已信任的数据可以用 `load_unverified`
省掉校验，不可信输入走 K4。

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
没有 Linux 或 Windows 的性能数据。第 3 节那组云端比例（GCC 16，Linux 虚拟机）只用来
说明声明普通记录的量级，不是正式测量。
