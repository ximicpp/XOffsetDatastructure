# XOffsetDatastructure 用户指南（0.2.0-alpha.1，V2 格式）

本文面向使用者，说明怎么定义类型、构建、修改、保存、加载，以及必须遵守的约束。
格式细节见 [V2_FORMAT.md](V2_FORMAT.md)，输入与兼容边界见
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
  `load()`（走校验路径）。

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

## 5. 必须遵守的约束

| 约束 | 不遵守的后果 |
| --- | --- |
| 手动分配（如 `XBuffer::allocator<T>()`、`arena()->allocate_aligned`）且不能从根到达的内存，必须在 `save_verified` 前释放 | 保存出的 wire 无法通过加载校验（铺满检查失败） |
| 同一个 buffer 的写操作由调用方同步 | 数据竞争，行为未定义 |
| buffer 对象要比它的 `XHandle`、借用的 span 活得久 | 悬垂访问 |
| 同一可执行文件内所有翻译单元使用相同的头文件版本、编译选项和工具链 | 布局或 schema 身份不一致 |
| 接收不可信输入前限制 wire 长度、`maximum_capacity` 和并发 buffer 数 | 单个合法 wire 也可能占用大量内存 |
| 只用于 64 位小端平台 | 其他平台编译期 `#error` |

## 6. 错误处理

- 所有库错误都是 `XException`（派生自 `std::runtime_error`）；空间不足是它的子类
  `XBadAlloc`。
- 库容器的修改在分配失败时保留原有逻辑内容，但容量和分配器记账不一定回滚。多步
  业务操作不是事务。

## 7. schema 变更

当前版本**不支持 schema 演进**：

- wire 头里的 schema 身份必须与读取方类型完全一致。增加、删除、重排或改变任何
  一个字段，旧 wire 都会被拒绝。
- 适合收发双方使用同一版本类型定义的场景（同一次构建部署的进程之间）。需要长期
  保存或跨版本读取的数据，应保留能读取原版本的程序，自行导出再导入。
- 不同编译器之间的 wire 兼容目前只在固定工具链内成立；`tools/sigs/` 的签名基线
  用于发现布局变化，不构成任意平台的兼容保证。

## 8. API 稳定性

alpha 之间不承诺源码 API 或 ABI 稳定。以下接口是设计上计划保持的核心面，变更会
在 CHANGELOG 中单独列出：

- `XBuffer::create<T>`、`root<T>`、`has_root<T>`、`save_verified<T>`、
  `load_verified<T>`、`maximum_capacity`、`with_growth`、`XGrowPolicy`
- `TypedXBuffer<T>` 的 `create`、`root`、`save_verified`、`load`
- 容器 `XString`、`XVector`、`XFixedVector`、`XSet`、`XMap` 的公开成员函数
- `XOFFSET_REGISTER_SCHEMA_NAME`、`XException`、`XBadAlloc`

其余接口属于实验性质，可能在后续 alpha 中改变：直接接收（`begin_verified_receive`）、
`XBufferPool` 与预留策略、`XHandle`、`XCompactor`、`load_unverified`、诊断与统计接口、
`XBufferCore` 的底层成员。

## 9. 性能数据的口径

唯一的正式测量来自一台 Apple M4、一个 demo 负载，对比 cista、FlatBuffers、
Cap'n Proto、cereal，见 [docs/perf/round3-20261021/REPORT.md](perf/round3-20261021/REPORT.md)。
引用时请同时说明机器、负载和竞品用法（例如 FlatBuffers 走 Object API）；
没有 Linux 或 Windows 的性能数据。
