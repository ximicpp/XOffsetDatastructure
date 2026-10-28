# XOffsetDatastructure 用户指南（3.1.0，wire 格式 2.1）

本文面向使用者，说明怎么定义类型、构建、修改、保存、加载，以及必须遵守的约束。
部署要求见 [DEPLOYMENT.md](DEPLOYMENT.md)，格式细节见 [V2_FORMAT.md](V2_FORMAT.md)，输入与兼容边界见
[ALPHA_COMPATIBILITY.md](ALPHA_COMPATIBILITY.md)。

## 1. 基本模型

- 一个 `XBuffer` 拥有一块连续的 arena。根对象和它引用的所有字符串、数组、集合都放在
  这块 arena 里，彼此之间用相对偏移相连，所以 arena 放在哪里都一样（存储见 3.4 节）。
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
    XString                name;     // ≤10 字节的字符串内联在 12 字节头里
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

### 2.1 字段表：`XOFFSET_RECORD`

没有反射的构建（C++20/C++23，或 `XOFFSET_SCHEMA_BACKEND_DESCRIBE=1`）里，库从一行字段表
知道记录的形状。C++26 构建可以不写；写了，宏会和反射逐条核对（成员、顺序、偏移、类型、基类），
不一致就编译失败，所以同一份源码在两种构建下描述的是同一个 schema，wire 字节相同。

```cpp
struct Troop { std::int32_t type{}; std::int32_t count{}; };
struct March { std::int64_t id{}; std::int32_t eta{}; XVector<Troop> troops; };
struct Entity { std::int64_t id{}; };
struct Player : Entity { std::int32_t level{}; XVector<std::int32_t> items; };

// 全局作用域，放在类型定义之后、第一次使用之前；被包含的记录写在前面。
XOFFSET_RECORD(Troop, type, count)
XOFFSET_RECORD(March, id, eta, troops)
XOFFSET_RECORD(Entity, id)
XOFFSET_RECORD_DERIVED(Player, (Entity), level, items)   // 基类按声明顺序写在括号里
```

- 按声明顺序列出**全部**非静态数据成员。写错名字编译不过；漏写、重复、顺序错由编译期
  完整性证明拦下：偏移严格递增且不重叠，加上聚合体初始化计数（`T{x1..xE}` 合法而
  `T{x1..xE+1}` 不合法）。只看 `sizeof` 不够：漏掉一个看起来像补齐的 1 字节成员，大小不变。
- 因此记录必须是聚合体（没有用户声明的构造函数、私有或受保护成员、虚函数）。类型名带逗号时用别名。
- `XOFFSET_RECORD` 同时就是 `XOFFSET_ORDINARY_RECORDS` 的声明（第 3.1 节），两者不要对同一个
  类型都写。没有反射时，「普通记录」的声明（不论用哪个宏，还是手写 `wire_record_layout`
  特化）还承诺记录没有用户声明的析构、拷贝/移动构造或赋值。没有反射时库只能部分检查：
  所有基类和成员的析构（或赋值）都平凡、记录本身却不平凡，就是用户提供的，判为不普通；有成员
  本身不平凡（例如 `XString`）时查不出来，仍是承诺。C++26 构建会完整检查。
  不能作这个承诺的记录用 `XOFFSET_RECORD_DERIVED(T, (), 成员...)`：只给字段表，不声明普通记录。
- 没有反射时，直接接收（raw receive）用到的枚举必须有固定底层类型（`enum class`，或
  `enum E : std::uint8_t {...}`），否则编译报错并给出改法。
- `XString`、`XVector`、`XSet`、`XMap`、`XKeyValue` 和 `std::array` 由库自带字段表。
- 联合体和非聚合体类（`std::string`、带虚函数或构造函数的类）写不了字段表。没有反射时库把它们
  当作不透明类型：不准入（`is_byte_copy_safe_v` 为 false），也不报“缺字段表”。
- 迁移助手 `tools/describe_dump`：在 GCC 16 或 P2996 上编译，传入根类型，用反射按依赖顺序印出
  所有可达记录的这几行（`bench/slg` 的 13 行就是它的输出，`describe_dump_slg` 测试保持一致）。
  写不了的类型（非聚合体、重叠的 `[[no_unique_address]]` 成员等）印成一行注释说明原因。
- 发布前至少用一个 C++26 编译器把 schema 编一次，让反射核对字段表和承诺。

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
- 自动增长通常把逻辑长度翻倍；`reload_verified` 之后，在以前用过的范围内
  每次只增长 4 KiB（重新露出的字节要清零，按需清零更省），越过以前的最高位置后恢复
  翻倍。逻辑长度只影响 `get_size()`，不影响 wire。
- 容器头必须位于所属 arena 内。给插入接口传 `std::string_view`、`std::string` 等
  原生值，不要在栈上构造绑定 arena 的 `XString` 临时对象。
- 容器扩容、插入、删除会移动元素，规则和 `std::vector` 相同；`XSet`、`XMap` 是有序
  平铺数组，规则和 `std::flat_set`、`std::flat_map` 相同，**和 `std::set`、`std::map`
  不同**。逐个操作的规则见第 3.2 节。Automatic 增长和容器操作不移动 arena；显式 `grow()`
  在可搬槽位上可能移动它（第 3.4 节）。
- **元素是含 `XString`、`XVector`、`XMap` 等成员的记录类型时，先把这些记录声明为
  普通记录（第 3.1 节），例如在全局作用域写 `XOFFSET_ORDINARY_RECORDS(Troop, March)`。**
  不声明也完全正确，只是中间插入删除、扩容要逐个元素搬移：
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
- 元素是含 `XString`/`XVector`/`XMap` 等成员的记录类型时，用 `XOFFSET_ORDINARY_RECORDS`
  声明这些记录类型（连同它们作为成员用到的记录类型），会打开更快的路径：中间
  `erase`/`insert`、追加扩容和 `reserve` 都用一次 memmove/memcpy 挪动整段元素，只重建
  内联容器头；`stable_erase_if` 也随之可用。声明是对**源码**的承诺：记录只有普通的非静态
  成员，没有基类、`union`、位域、`[[no_unique_address]]`，成员互不重叠。

```cpp
struct Troop { std::int32_t type{}; std::int32_t count{}; };
struct March {
    std::int64_t id{};
    std::int32_t eta{};
    XVector<Troop> troops;
};

// 全局作用域，放在类型定义之后、第一次在容器里使用之前。
XOFFSET_ORDINARY_RECORDS(Troop, March)

marches.stable_erase_if([](const March& m) { return m.eta <= 0; });
```

- 宏对每个类型做编译期检查：必须是类（不是 `union`），没有基类和位域，成员的
  `[偏移, 偏移 + sizeof)` 两两不相交。共享存储的 `[[no_unique_address]]` 成员会被
  查出来；但「可以重叠、只是这次没重叠」的成员查不出来，所以声明仍然是源码承诺，
  声明错了是未定义行为。库不会在没有声明时自动认定。
- 宏展开成 `wire_record_layout<T>::ordinary_members = true` 和
  `StableEraseOrdinaryMembers<T>` 两个特化；手写这两个特化效果相同，但没有检查。
- 不写声明时，行为和结果完全相同，只是走逐元素搬移的较慢路径，并且
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
| 容器操作触发的 Automatic 增长；显式 `grow()` 没有超出当前存储 | 不变（在预留或槽位里扩展） | 只按上表失效 | 不变 |
| 可搬槽位上显式 `grow()` 超出槽位（第 3.4 节） | 改变：搬到新存储 | 全部失效 | 加一 |
| `save_verified`、`save_verified_into`、`save` | 不变（只收缩逻辑长度） | 有效 | 不变 |
| `reload_verified`、`begin_verified_reload`、`reset<T>()` | 不变（复用映射）；可搬槽位放不下新内容时换存储（第 3.4 节） | 全部失效（`begin_verified_reload` 在开始时就失效，不等 `finish()`） | 加一 |
| `src.clone_into(dst)` | dst 不变（复用映射）；dst 是可搬槽位且放不下时换存储（第 3.4 节） | dst 原有的全部失效；`src` 不受影响 | dst 加一（自拷贝被拒绝时不变） |
| `clone()` | 结果是新 buffer，存储种类与源相同（可搬槽位或地址稳定） | 源 buffer 不受影响 | 新 buffer 从 0 开始 |
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

### 3.4 存储：VM、钉住槽位、可搬槽位

创建 buffer（`create`、构造函数、`load*`）时按参数选存储，`storage_kind()` 可以查：

| 存储 | 什么时候选 | 地址 |
| --- | --- | --- |
| VM（`XStorageKind::Vm`） | 带 `XBufferPool`；`with_storage(XStorage::Pinned)`；Automatic 且上限大于 `XBuffer::SLOT_MAX`；初始大小大于 `SLOT_MAX` | 整个生命周期不变 |
| 钉住槽位（`PinnedSlot`） | 上限不超过 `SLOT_MAX`（任何增长策略） | 整个生命周期不变 |
| 可搬槽位（`MovableSlot`） | 其余情况：Explicit 增长（默认）、上限更大、初始大小放得进槽位 | 只在下面几个显式点改变 |

- `SLOT_MAX` 默认是 256 KiB − 64 B，编译期可以用 `XOFFSET_SLOT_MAX_BYTES`（2 的幂，0 表示
  不用槽位）调整。槽位来自库内的 slab，按 2 的幂分级，小消息不再每个 buffer 预留一段
  虚拟地址；建立和销毁一个槽位不走系统调用。
- 可搬槽位的搬家点：`grow()` 超出当前槽位时搬到更大的槽位（超过 `SLOT_MAX` 时搬到 VM，
  之后钉住）；`reload_verified`、`begin_verified_reload` 收到放不下的内容时先换存储（不复制
  旧内容）；`set_growth_policy(XGrowPolicy::Automatic)` 会先搬到 VM，因为自动增长发生在
  容器操作内部，那里不能搬。
- 搬家后，之前拿到的根引用、元素指针、`allocator<T>()`、`arena()` 和借用的 span 全部失效，
  `epoch()` 改变。用 `XHandle`（自动重新定位）或重新调用 `root<T>()`。`address_stable()` 为
  真时不会搬；需要地址永远不变就写 `with_storage(XStorage::Pinned)`。
- Automatic 增长、容器操作、`save*`、`shrink_to_fit`、`reset<T>()` 都不搬。
- 不要在同一个 buffer 的宿主回调（`new_handler`、`stable_erase_if` 的谓词）里调 `grow()`。
- 释放的槽位留在进程里供下一个 buffer 复用（重新暴露前清零），和池一样计入常驻内存；
  `XBuffer::trim_slot_storage()` 丢弃空闲槽位的页，`XBuffer::slot_storage_stats()` 查看用量。
- 槽位空闲表、宿主空闲表和 owner 查找各有一份每线程缓存。GCC 编 MinGW/Cygwin 时 TLS 是
  模拟的（emutls），线程退出时缓存可能先于析构被释放，所以这些目标上缓存关闭，全部走全局
  结构（`XOFFSET_THREAD_CACHES` 默认 0，其他目标默认 1；Clang 编 MinGW 用原生 TLS，保留缓存）。
- 默认构造的空 `XBuffer` 可以直接 `reload_verified` 或 `begin_verified_reload`，按
  `load_verified` 不带上限时的规则（64 MiB、Explicit）建存储。

## 4. 保存与加载

```cpp
std::string wire = buf.save_verified<Profile>();

XBuffer in = XBuffer::load_verified<Profile>(wire, cap);
const Profile& r = in.root<Profile>();
std::string_view who = r.name.view();
```

- wire 由 40 字节头、payload 和空洞表组成（稀疏 wire 在头后多一张段表，见
  [V2_FORMAT.md](V2_FORMAT.md) 第 3 节）。默认只跳过「不短于 4 KiB、由不小于 256 B 的
  空闲块连成」的段（例如一次删掉大量记录留下的空洞），其余 payload 一次拷贝。
  `XSaveOptions{.emission = XWireEmission::dense}` 强制整段拷贝。
- 跨机器发送时可以打开零段省略：
  `save_verified<Profile>(XSaveOptions{.emission = XWireEmission::zero_runs})`
  （`save_verified_into` 同样接受这个参数）。它跳过不短于 64 字节的全零段（容器余量、
  空洞、为 0 的字段），接收方按原偏移填零，装载后的 payload 与稠密 wire 逐字节相同，
  余量照样保留、接着改不变慢。代价是发送时多扫描一遍 payload；payload 不足 4 KiB
  或省不到 max(256 B, payload 的 1/16) 时自动退回稠密。同机共享内存或进程内传递保持默认。
  接收方不需要任何设置，三种 wire 都接受。
- 想让 wire 更小时可以用 `save_compact<Profile>()`（或 `save_compact_into<Profile>(out)`）：
  它直接按校验顺序遍历对象图写出紧凑 wire（无空洞），不建临时 buffer，源 buffer 不变。
  默认给不少于 8 个元素的容器（字符串除外）留 size/8+1 个余量，接收方修改时不必马上搬家。
  只读或归档的接收方传 `XCompactOptions{.slack_div = 0}`：容量等于长度、wire 最小，
  与 `XCompactor::compact<Profile>(b).save_verified<Profile>()` 逐字节相同，
  但接收方第一次增长任何容器都要搬家。
- 把收到的数据转发给第三方（例如服务端把客户端 A 的上行消息转给客户端 B）时，用
  `save_compact` 重建后再发，不要 `load_verified` 之后直接 `save_verified`：后者会把
  A 写在空洞和容器余量里的字节原样带给 B。规则和 `save_compact` 管不到的部分见
  [DEPLOYMENT.md](DEPLOYMENT.md) 4.2 节。
- 把几段现有状态拼成一条消息时，用 `save_compact_into` 的组合写出重载，不必先赋值：

  ```cpp
  auto& msg = req.root<BattleMsg>();          // 草稿，标量照常直接改
  msg.battle_id = id;
  req.save_compact_into<BattleMsg>(out, {}, xassign(msg.attacker, att), xassign(msg.defender, def));
  ```

  写出的 wire 等于先做 `msg.attacker = att; msg.defender = def;` 再 `save_compact_into`
  的结果（tight 和默认余量都一样），但草稿和 `att`、`def` 都不变，也不在草稿里分配。
  规则：每个 target 是根对象里的成员子对象或数组元素（任意层级，标量也可以），不能是
  容器里的元素；多个 target 不能重叠；source 与 target 同类型，可以在任何 buffer 里，
  也可以是草稿自己的另一部分（所有 source 都按调用前的值读取）；含容器的 source 必须
  是某个 XOffset buffer 里的有效对象，发送端不校验它。target 的隐式填充字节写成 0。
  违反规则或超过 wire 上限时抛 `XException`，`out` 为空串。
- 加载时的校验包括：头部与版本、schema 身份、空洞表、从根出发的整图遍历，以及
  「活块 + 空洞恰好铺满已用区」。任何一项不满足都会抛 `XException`，不会发布根对象。
  不想用异常时调用 `try_load_verified`，见第 6.1 节。
- 加载后的 buffer 可以继续修改再保存；保存前的空闲空间会随空洞表带过去并继续复用。
- `load_unverified` 跳过校验，只适合调用方已经信任的数据。
- 直接把数据收进最终 arena 的接收接口是 `begin_verified_receive<T>` /
  `finish_verified`（`xoffset_verified_receive.hpp`），校验强度与 `load_verified` 相同。

### 4.1 大量小消息：槽位，或用 `XBufferPool` 复用映射

VM 存储的 `XBuffer` 都有自己的虚拟内存预留：建立时预留 `maximum_capacity` 并开启宿主
前缀页和载荷页，销毁时整块释放。这套系统调用和缺页在 Apple M4 上约 4.4 µs，
与预留大小关系不大。消息只有几 KB、而且每条都新建、销毁一个 buffer 时（战斗服往返、
只读战报、客户端收包），这部分开销会远大于校验本身。

不带池、Explicit 增长（默认）的小消息落在槽位上（3.4 节），不走这套系统调用。
Automatic 增长且上限较大的用法仍是 VM，这类接收方和发送方应当共用一个池，让销毁后的
映射留给下一条消息复用：

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
XBuffer snapshot = buf.clone();     // 新 owner：大小、maximum_capacity、增长策略、池、存储种类都同 buf
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

### 4.3 设计约定：哪些开销是契约带来的

有两条约定是整块可搬（K0）和「发出去的就是 arena 字节」（K3、K5）的前提，它们的代价
落在固定的几类操作上，库不打算靠改契约把它们压掉。v1.0 检查点（Apple M4，SLG 原型 M 规模，
对 zpp_bits）上最明显的两处是战斗服往返里的 A.apply（约 2.0 倍）和 A.simulate（约 1.3 倍）；
A 场景整体仍快于 zpp_bits（上界 0.78）。下面是来源和使用方能做的事。

**约定一：每个 buffer 一个 arena，带自己的空闲表。** 空闲空间记在这个 buffer 自己的表里，
随空洞表一起发出、装载，所以整块搬走或发出之后，接着改不退化（K5），也不依赖进程全局的
分配器状态。代价是元数据按 buffer 分散：一个 buffer 第一次用到某个大小档时，那一档的
元数据是冷的。很多 buffer 各改一两次的负载（例如 SLG 原型里的 512 个玩家 buffer）几乎每次都碰到
冷元数据，这就是 A.apply 的差距。

- 对策：长期持有、逐条复用 buffer（`reload_verified` / `reset<T>()`，见
  [BUFFER_REUSE.md](BUFFER_REUSE.md)），空闲表和映射都留在原处；能合并的修改集中到少数
  buffer 上做。

**约定二：容器余量是 payload 的一部分。** 容器的余量和其他字节一样随 wire 发出，扩容新露出
的字节必须先清零（不能把未初始化的字节发出去）。所以库不替你多留：一个空容器第一次
`assign`、拷贝赋值都只分配到长度（之后的扩容才翻倍），`save_compact` 的
`slack_div = 0` 也让容量等于长度。代价是收到的消息在接收方被编辑时，容器第一次增长就要
扩容、搬元素并清零新露出的部分；每条新消息都会重来一次，这就是 A.simulate 的差距。

- 已知终态大小时先调 `XVector` / `XString` 的 `reserve`，一次分配到位，不走多次翻倍。
- 发给会编辑的接收方时用 `save_compact` 的默认余量（`slack_div = 8`：不少于 8 个元素的容器
  （字符串除外）留 size/8+1 个），接收方第一次插入不搬家；只读或归档的接收方用 `slack_div = 0`，wire 最小。
- 跨机器发送带余量的状态时用 `XWireEmission::zero_runs`（第 4 节），余量里不短于 64 字节的零段
  不占线路字节，接收方装载后余量照样在。
- 接收方复用 buffer 时，`reload_verified` 之后在以前用过的范围内每次只增长 4 KiB（第 3 节），
  清零按需进行。

两条约定都不影响正确性和 wire 格式；上面的对策也只改性能，不改语义和 wire 的接受集合。

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
| 苹果平台部署目标不低于 macOS 14、iOS 17（tvOS 17、watchOS 10）：校验载入等接口用系统 libc++ 的 `std::pmr`，苹果从这些版本起才提供，且 SDK 不做可用性标注 | 部署目标更低时编译期 `#error`（否则会在旧系统上启动失败） |

## 6. 错误处理

- 所有库错误都是 `XException`（派生自 `std::runtime_error`）；空间不足是它的子类
  `XBadAlloc`。每个 `XException` 都带错误码：`e.code()` 返回 `XErrc`
  （`xoffset_error.hpp`，例如 `bad_magic`、`schema_mismatch`、`invalid_payload`、
  `capacity_exceeded`，`to_string(code)` 给出名字）。

### 6.1 不抛异常的接收入口

接收入口都有一个不抛异常的版本，返回 `XResult<T>`（值或 `XError{code, message}`）或
`XStatus`。校验与抛出版本完全相同；失败时的错误码和错误文字就是抛出版本会抛的那个，
buffer 留下的状态也相同（reload 失败后为空、无根）。

```cpp
auto in = XBuffer::try_load_verified<Profile>(wire, cap);     // XResult<XBuffer>
if (!in) { log(to_string(in.code()), in.error().message); return; }
const Profile& r = in->root<Profile>();

if (auto s = buf.try_reload_verified<Profile>(wire); !s) drop(s.error());

auto token = buf.try_begin_verified_reload<Profile>(used, holes, extents);
// ... 写入 token->header_bytes() / payload_bytes() / hole_bytes()，complete(...)
XStatus done = token ? token->try_finish() : XStatus(token.error());
```

还有 `try_load_verified_with_resource`、`try_reload_verified_with_resource` 和
`TypedXBuffer<T>::try_load_verified / try_reload_verified`。`XResult::value()` 在没有值时
抛出（关异常构建里调用致命处理器）；`*r` / `r->` 不检查。

### 6.2 关异常构建（`-fno-exceptions`、MSVC `/EHs-c-`）

整个库可以在关异常的编译选项下使用，面向不开异常的客户端引擎。检测
`__cpp_exceptions` / `_CPPUNWIND`，也可以用 `-DXOFFSET_NO_EXCEPTIONS=1` 强制。不需要 RTTI。

- 接收路径（wire 头、extent 表、容量检查、OS 存储预留与拷贝、owner 登记、整图校验、
  空闲表装载）沿调用链返回第一个失败，由上面的 `try_*` 入口交给调用方。同一条 wire 在
  开、关异常两种构建里得到同样的结果和同样的错误文字（`tools/no_exceptions_differential.sh`
  对 20 万条变异 wire 逐条比对）。
- 其他失败（修改时 arena 空间不足、`root<T>()` 没有根、保存时超过 wire 上限、调用抛出版本
  的接收入口被拒等）没有调用方可以返回，调用致命处理器后 `abort()`。引擎用
  `set_fatal_handler(fn)` 接入自己的崩溃上报；`fn(XErrc, const char*)` 不能返回。
- 宿主 `operator new` 失败按标准库在关异常下的行为终止。
- 开异常时会从 `XBadAlloc` 恢复的两处，关异常时也是致命的：容器拷贝赋值的整块拷贝
  （开异常时退回逐元素拷贝）和 `XCompactor::compact` 的倍数重试（2 倍已用空间足够时不会
  走到）。客户端只接收、读取、做少量修改时不受影响。
- `receive_raw::validate` 和 `begin_verified_receive` / `finish_verified` 还没有不抛出的
  版本，关异常时它们的拒绝是致命的；客户端请用 `try_load_verified`、
  `try_reload_verified` 或 `try_begin_verified_reload`。
- 库容器的修改在分配失败时保留原有逻辑内容，但容量和分配器记账不一定回滚。多步
  业务操作不是事务。

## 7. schema 变更

3.1.0 **不支持 schema 演进**：

- wire 头里的 schema 身份必须与读取方类型完全一致。增加、删除、重排或改变任何
  一个字段，旧 wire 都会被拒绝。
- 适合收发双方使用同一版本类型定义的场景（同一次构建部署的进程之间）。客户端场景下，
  任何改了 schema 的服务端更新都必须和客户端同时发版。
- 需要长期保存或跨版本读取的数据，应保留能读取原版本的程序，自行导出再导入。

3.2（未发布，在 `next_cpp26` 上）加入 schema 演进：服务端保存一条版本链，收到旧版本
的 wire 先按旧版本完整校验再按字段名升级，发给旧客户端时降级后写出；客户端只多一次
握手。用法、发版规则和代价见 [SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)。wire 格式不变。
- 不同编译器、不同平台之间的 wire 兼容只在已验证的工具链上成立（见第 8 节 K7）。
  用 `XOFFSET_PIN_SCHEMA` 固定自己的 schema（[SCHEMA_PINS.md](SCHEMA_PINS.md)），
  任一构建算出的布局和固定文本不一致时编译失败；它发现布局变化，但不提供演进。

## 8. 核心承诺与 API 稳定性

版本之间不承诺源码 API 或 ABI 稳定。下面 K0–K7 是核心承诺，后续版本不得回退；
相关接口如有改名或签名变化，会在 CHANGELOG 中单独列出并给出替代写法。

| # | 承诺 | 接口 / 机制 |
| --- | --- | --- |
| K0 | 可重定位：payload 里没有任何绝对地址，任何修改之后仍可整块复制到任意地址接着读写；同一进程、跨进程、跨机器搬的是同一份字节。K2、K3、K5 都建立在它上面 | 编译期准入（`is_byte_copy_safe_v`：XOffset 沿用 TypeLayout 的规则，按字段接口递归，凡含裸指针、成员指针、引用和虚表的类型一律拒绝）。两条路径规则相同：服务端的 C++26 构建从反射拿字段，并与 TypeLayout 的判定交叉核对；客户端的 C++20/C++23 构建从 `XOFFSET_RECORD` 字段表拿字段，写不了字段表的联合体和非聚合体类一律不准入。容器头只存相对偏移，根用基址偏移 |
| K1 | 用普通 C++ 聚合体定义 schema，不用 IDL：服务端（C++26）由反射自动得出布局和指纹，不写样板；客户端（C++20/C++23）每个记录加一行字段表，由编译器证明列全（偏移严格递增 + 聚合初始化元数相符），C++26 构建同时用反射核对字段表，两种构建的 wire 字节相同（第 2.1 节） | `XOFFSET_REGISTER_SCHEMA_NAME`、`XOFFSET_RECORD`、`XOFFSET_RECORD_DERIVED`、`XString`、`XVector`、`XFixedVector`、`XSet`、`XMap` |
| K2 | 对象直接建在 arena 里，原地读、原地改（含变长修改），空闲块复用 | `XBuffer::create<T>`、`root<T>`、`has_root<T>`、容器的公开成员函数、`maximum_capacity`、`with_growth`、`XGrowPolicy` |
| K3 | 独立输出：发出的是调用方独占的副本，发完可以继续改 | `save_verified<T>` |
| K4 | 接收时完整结构校验：截断、越界、重叠、未铺满、非法值、schema 不符一律拒绝 | `load_verified<T>`、`XException` |
| K5 | 接收后接着改、再发出，空闲空间随空洞表带过去，不退化 | 同上 |
| K6 | schema 指纹精确匹配，不同就拒绝 | wire 头 `schema_hash` |
| K7 | 跨编译器、跨平台的对象表示一致 | 用户 schema：`XOFFSET_PIN_SCHEMA`；库自带类型：`tools/sigs/` 签名基线；跨编译器 wire 语料比对（`tools/platform_matrix.sh`） |

K0 的边界：搬的单位是整个区域，不是其中单个对象（容器头禁止拷贝和移动）。正在使用的
buffer 地址固定（在 VM 预留内增长），只在显式时间点搬：输出、接收、reload、`XCompactor`
压缩；`XHandle` 用 epoch 在搬动后重新定位。同 ABI、已信任的数据可以用 `load_unverified`
省掉校验，不可信输入走 K4。

K7 的现状：

- **用户 schema**：`XOFFSET_PIN_SCHEMA` 把类型的 wire 布局固定成一段签名文本，每个包含
  固定文件的构建都由它自己的编译器求值，不一致就编译失败（[SCHEMA_PINS.md](SCHEMA_PINS.md)，
  CTest `schema_pin_mismatch` 确认确实会失败）。服务端的 GCC 16、P2996 Clang 如此；客户端的
  字段表构建不需要反射，同一份 pin 也由原生编译器求值，NDK r29/r30 Clang、MSVC 19.44、
  clang-cl 19 和 MinGW GCC 13 的全量 CTest 都包含这项检查。Apple clang 上要等 iOS 构建跑通。
- **库自带类型**：x86-64 Linux、x86-64 Windows、arm64 iOS、arm64 Android 四个目标的签名
  基线在 `tools/sigs/`，在 CTest 里核对（`check_compat`、
  `check_signature_regeneration`）。
- **跨编译器 wire**：3.1.0 RC 的本地平台矩阵里，Linux x86-64（GCC 16、P2996）、
  Linux arm64（GCC 16、P2996）和 Windows x64（MinGW-w64 GCC 16）写出的 2.1 语料逐字节
  一致，互相接受。字段表构建合入后（3.2，未发布），Android arm64（NDK r29/r30）和
  Windows x64（MSVC 19.44 的 C++20 与 C++23、clang-cl 19、MinGW GCC 13）都读入服务端
  C++26 反射构建写出的参照语料并全部接受，写出的语料与它逐字节一致
  （[CLIENT_TARGETS.md](CLIENT_TARGETS.md)）。
- **边界**：只覆盖跑过上面检查的工具链。iOS（Apple clang + iOS SDK）还在等 Xcode，
  尚未覆盖。库自带类型的签名基线目前只在 CTest 里核对，不会让用户构建失败；目标是为用户
  schema 导出签名、不一致时构建失败，并把 GCC 16 并入同一门禁（实现中）。

K4 的前提：接收方必须设置 `maximum_capacity`。否则发送方可以发接近 payload 上限
（约 2 GiB）的数据，让接收方复制并做全图校验，造成资源耗尽。默认上限建议见
[DEPLOYMENT.md](DEPLOYMENT.md) 第 5 节。

校验的承诺边界：对不可信输入保证内存安全和 schema 一致，**不保证内容完整性**。
只改动某个标量字段值的数据会被接受，因此不能把校验当作防篡改手段。

**目标（尚未承诺，实现中）K8**：客户端（Windows、Android、iOS，都是 64 位小端）与服务端用同一套
库、同一份 schema 和同一 wire，客户端能接收、读写、校验、再发。客户端用字段表后端
（第 2.1 节），按 C++20 用各平台原生编译器构建（Apple clang、Android NDK Clang、Windows
编译器），服务端照旧用 C++26 反射。现状（3.2，未发布）：字段表后端已合入；Android
（NDK r29/r30）和 Windows（MSVC 19.44、clang-cl 19、MinGW）全量 CTest 都是 106/106，语料与
服务端逐字节一致；iOS 等 Xcode。升为核心承诺的条件：三个客户端目标都原生构建、CTest 通过
（iOS 含地址空间实测，并定下最低部署目标：系统 libc++ 的 `std::pmr` 在 macOS 14 / iOS 17
以下加载失败），跨目标 wire 语料一致，签名与 `tools/sigs/` 基线一致。详见
[CLIENT_TARGETS.md](CLIENT_TARGETS.md)。3.1.0 没有这条构建路径：Android、iOS 和 MSVC 编不了。

以下接口提供但属于可选或实验性质，可能在后续版本中改变：

| # | 接口 | 说明 |
| --- | --- | --- |
| E1 | `freeze_verified`、`begin_verified_receive` / `finish_verified` | 零拷贝分段发送、直接收进最终 arena |
| E2 | `save_verified_into`、`TypedXBuffer<T>` | 便利接口 |
| E3 | `XBufferPool`、预留与增长策略细节、`XStorage` / `storage_kind()`、`SLOT_MAX`、`trim_slot_storage` | 小 buffer 高频场景 |
| E4 | `load_unverified` | 只用于调用方已信任的数据 |
| E5 | `XCompactor` | 同类型压缩整理 |

`XHandle`、诊断与统计接口、`XBufferCore` 的底层成员不属于承诺范围。

## 9. 性能数据的口径

正式计时只来自一台 Apple M4、一个游戏服务器负载（`bench/slg`），六家在同一进程里对比
zpp_bits、cista、cereal、protobuf、FlatBuffers；每个版本的数字、测法和局限见
[PERFORMANCE.md](PERFORMANCE.md)。引用时请同时说明机器、负载和适配写法；
没有 Linux 或 Windows 的计时结论。第 3 节那组云端比例（GCC 16，Linux 虚拟机）只用来
说明声明普通记录的量级，不是正式测量。
