> 「可修改可重定位定位」线程的探讨稿（设计讨论，xhigh）。分支 `claude/mutable-relocatable-positioning` 专门放这个方向的文档和原型。定稿的定位仍以项目资料 `PRODUCT_POSITIONING.md` 为准，由「产品定位」线程更新；本文是讨论用的工作稿，结论定下来后再同步过去。

# 可修改、可重定位的数据结构：定位探讨

2026-10-22 · susu 发起 · 状态：Q1–Q6 已定（见第 6 节），后续原型继续· 分支基点 next_cpp26 @ 4bc922b

标注沿用项目习惯：
- **代码**：在仓库里核对过。
- **实测**：本项目测的，注明机器。
- **公开**：第三方资料。
- **推断**：分析，需要验证。

## 0. 这条分支放什么

- `docs/positioning/MUTABLE_RELOCATABLE.md`：本文。讨论的问题、推荐和决定记录都在这里。
- `experiments/mutable_relocatable/`：原型和微基准（第 5 节）。只在云端跑，只看比例，不占用 Mac。
- 这条分支不合并进 next_cpp26。某一项定下来、需要改库或改仓库文档时，单独从 next_cpp26 开 PR。

## 1. 一句话：现状和 susu 的新说法

现在的一句话（`PRODUCT_POSITIONING.md`，10-22 14:50Z 定）：

> XOffset V2 是可重定位的 C++ 对象图：对象直接建在一块位置无关的内存里，任何修改之后仍能整块搬到同一进程的另一处、另一个进程或另一台机器，一次复制发出；对端完整结构校验后原地读写，改完再发。它用作同一份 schema 的 C++ 程序之间搬运可变状态的数据层……

susu 的新说法（10-22 15:53Z）：**可修改、可 relocate 的数据结构。**

**更新（10-22 21:37Z）**：按 Q1、Q2 改写后，`PRODUCT_POSITIONING.md` 正本的一句话已经是：

> XOffset V2 是可修改、可重定位的 C++ 数据结构：程序直接拿它当工作状态，任何修改之后都能整块搬到同一进程的另一处、另一个进程或另一台机器，搬的是同一份字节。它最主要的用途是在同一份 schema 的 C++ 程序之间搬运可变状态，覆盖服务端之间、服务端与客户端之间，跨编译器和平台；两端的对象表示是否一致，用类型签名判定（目标是编译期判定，现状见 K7）。

同时改了第 5 节非目标"数据从不离开进程"那一行和第 7 节（加重定位税）。改前的版本备份在项目资料 `mutable-relocatable-20261022/PRODUCT_POSITIONING_before_Q1Q2_20261022.md`。下面第 1 节其余文字保留讨论时的原样。

两种说法描述的机制相同，差别在身份。现在的说法把它当作"数据层"，用途是在程序之间搬状态，主指标都是搬运场景（SLG A/B/C 对序列化库）。新说法把它当作"数据结构"：程序直接拿它当工作状态的容器，可重定位是这种数据结构的性质，跨机器发送只是重定位的一种。第 3 节列出这个差别会改动什么。

## 2. 现有结论梳理

### 2.1 已经定下来的

| 结论 | 出处 | 时间 |
| --- | --- | --- |
| K0：payload 里没有绝对地址，任何修改之后仍可整块复制到任意地址接着读写；同一进程、跨进程、跨机器搬的是同一份字节。由编译期准入保证：TypeLayout 拒绝布局签名里带 `ptr`、`vptr` 等记号的类型 | `PRODUCT_POSITIONING.md` 3.1；代码 `external/typelayout/include/boost/typelayout/admission.hpp` | 10-22 14:50Z |
| 地址契约：存活的 buffer 只在显式点搬（输出、接收、reload、`XCompactor`、显式 `grow()`）。R1 之后，默认（Explicit）buffer 放在可搬槽位上，显式 `grow()` 可能搬家；要地址不变就用 `with_storage(XStorage::Pinned)`；带池或 Automatic 大上限的 buffer 仍在 VM 预留里 | `PRODUCT_POSITIONING.md` 3.1「K0 的边界」；R1 设计第 0 节 | 10-22 15:09Z |
| "兼容"指编译器、平台可以不同，schema 必须相同；schema 版本互通是 TODO | `PRODUCT_POSITIONING.md` 第 6 节决定 0 | 10-21 12:41Z |
| 不可信输入的结构校验是核心承诺（内存安全 + schema 一致，不含内容完整性），前提是接收方设了 `maximum_capacity` | K4；review R6 | 10-21 12:58Z、13:38Z |
| 极简核心满足后，性能优先于功能和限制 | 项目记录 | 10-22 01:33Z |
| zpp_bits、cista 纳入对手：A/B/C 对两家的 UB 都 ≤ 1.0（G8） | `PERF_GOAL.md` | 10-22 15:05Z |

### 2.2 概念框架（「可重定位本质」，`RELOCATABLE_ESSENCE_20261022.md`）

1. **susu 的两个词正好是分类的两条轴。**

   | | 不可重定位（内存里有绝对地址） | 可重定位 |
   | --- | --- | --- |
   | **可修改** | R0：std 容器；protobuf、cereal、zpp_bits 解码出来的对象。过边界要编码、解码 | R2：Boost.Interprocess 托管段、PMDK、Lite³、iceoryx2 定容容器、**XOffset** |
   | **只读，或只能改很少** | 不讨论 | R1：FlatBuffers、Cap'n Proto 读端、rkyv、cista（修改前）、Unity BlobAsset |

   R1 和 R2 的分界是"分配器在不在区域里"。cista 能扩容，但扩出去的部分在普通堆上，改完就不再是一块。

2. **"可修改 + 可重定位"是一个类，不是 XOffset 独有的。** Boost.Interprocess 的 `managed_heap_memory` / `managed_external_buffer` 是 main 分支的底座，它的文档也推荐把数据经网络发给同 ABI 的进程（公开）。XOffset 在 R2 里的差别有四点：对不可信输入做完整结构校验；核对不同编译器给出的实际布局；空闲空间以空洞表随 wire 走，接收方能用精确覆盖证伪它；rel32 只占 4 字节。对外说"在对比过的库里没有第二个"，不说"唯一"。
3. **搬运 = 搬字节 + 在目的地重建宿主绑定 + 按边界补证明。** 字节在所有边界上都一样。不同的是两件事：
   - 要重建的绑定：owner 槽、登记、存储、空闲表。
   - 要补的证明，即 O1–O8 八条义务：位置无关、对齐、布局一致、结构完整、空闲空间可信、外部引用、独占、生存期。

   边界 B0–B6 里，不可信边界（B3 跨信任、B4 跨编译器）已经覆盖。缺口在可信的 B1（同进程换位置）和 B2（同机另一进程），现在都只能"导出再导入"。
4. **区域可重定位和对象可重定位是对偶的。** XOffset 的整块区域怎么搬都行，单个容器头却不能单独搬：`XFixedString`、`XFixedVector` 的拷贝和移动构造函数都是 `= delete`（代码）。C++ 标准讨论的"平凡重定位"（P2786）讲的是对象级，2025-11 的 Kona 会议把它从 C++26 移除，目标改为 C++29（公开，P4197R0）。
5. **所有权是一棵树。** 精确覆盖要求每个块恰好被一个头认领，所以合法 payload 里没有共享，也没有环。这带来三个好处：
   - 在不可信数据上原地修改，不会"改一处、坏两处"。
   - 子树可以作为一个单位搬走。
   - 整块可以原地滑动压实，不需要转发表（R5）。
6. **存活时钉住，静止时可搬。** main 分支（CppCon 2024 那一代）增长时整块 realloc 搬家。改成 VM 钉住后，128 MiB 小起点增长的最大单次停顿（中位）从 3.690 ms 降到 0.207 ms，RSS 从 254 MiB 降到 130 MiB（实测，`docs/VM_PERFORMANCE_COMPARISON_RESULTS.md`）。不要回到隐式搬家。

### 2.3 证据：搬运强，操作参差

按"数据结构"来看，证据要分两类：
- 搬一次花多少，对手是编码解码或深拷贝。
- 平时的操作比 std 容器慢多少。下文把这部分额外开销叫"重定位税"。

| 类 | 项 | 数字 | 出处 |
| --- | --- | --- | --- |
| 搬运 | SLG B 跨服迁移（用户侧用池，口径 c） | UB ÷cereal 0.312、÷FlatBuffers 0.417、÷protobuf 0.113 | `PERF_GOAL.md` G1（M4） |
| 搬运 | SLG C 快照本身 | 0.028 ms，cereal 0.576 ms | `PRODUCT_POSITIONING.md` 第 2 节（M4，原型） |
| 搬运 | 正式负载输出 12.6 MB | 0.71 ms；加载 2.04 ms | 同上 |
| 操作 | 元素平移（erase / insert 的 SHIFT 路径） | 每元素 1.14 ns，std 1.16 ns（#18 之前 5.66 ns） | `G3_RESULT_20261021.md`（M4） |
| 操作 | C.ticks 每 tick 批量删除 | 对 std UB 0.81–0.88，比 std 快 | `PERF_GOAL.md`（10-21 16:29Z） |
| 操作 | 逐条中间删除（压力项） | UB 1.06–1.11，慢 5–10% | `PERF_GOAL.md` G3 |
| 操作 | 正式口径修改阶段 | 对 cereal（std 容器）1.053 | `PRODUCT_POSITIONING.md` 第 2 节 |
| 混合 | SLG A 对 zpp_bits，按阶段（ms/周期） | req_encode +0.175、apply +0.104、res_decode +0.051、simulate +0.046、req_decode −0.067、res_encode −0.072；合计 1.07 对 0.82 | `PERF_GOAL.md`（10-22 15:43Z，M4） |
| 存储 | 一个小 buffer 的存储生命周期 | VM 约 4.4 µs，堆槽位 28 ns | `SMALL_MSG_COST_20261021.md`（M4）；R1 由此而来 |

"混合"那一行值得单独说。zpp_bits 把消息解码进 std 容器，所以 SLG A 里 zpp_bits 的 apply、simulate 就是 std 容器上的操作。这两项是纯粹的容器操作，XOffset 合计慢 0.150 ms。req_encode 在 XOffset 一侧是在 buffer 里原地建请求再输出，在 zpp_bits 一侧是建 std 对象再编码，其中也含建对象的开销（推断，未拆分）。纯搬运的几项有快有慢：req_decode、res_encode 更快，res_decode 慢 0.051。

所以 **A 对 zpp_bits 的差距大头在建对象和改对象，不在搬运。** 按"数据结构"定位，G8 的 A 检验的正是"XOffset 容器的建、改是否和 std 容器一样快"。

### 2.5 本分支的云端实测（X1、X2）

详见 `experiments/mutable_relocatable/RESULT_X1_X2_20261022.md`。云端 GCC 16，只看比例。

- **重定位税不均匀。** 读（遍历、下标、查询）和整段搬移（中间插入删除、批量删除、map 删除）已经和 std 持平或更快。税集中在几条写路径：`push_back` 约 2 倍，短字符串赋值 2–3 倍，`reset<T>()` 4.4 倍，元素带容器头的中间删除 1.6 倍，XMap 查找对 `std::flat_map` 约 2 倍。
- **SLG 逻辑原地跑（没有任何收发）：** simulate、season_edits 1.13 倍，apply 稳态 1.38 倍，ticks 0.95–0.99 倍，query 1.02 倍。
- **同进程整块复制：** 用本分支的 `clone_into()` 原型，玩家（11.5 KB）是 std 拷贝赋值的 0.24 倍，地图分片（890 KB）0.36 倍，接近 memcpy 下限。现有 API 里最快的同类做法（save_verified_into + reload_verified）是 std 的 1.6–2.3 倍。

### 2.4 正在做的，和本方向直接相关

- R1 可搬存储：I1「槽位存储」排在 T6（PR #50）之后；I2「owner 登记瘦身」是 PR #59。
- T6 `begin_verified_reload`：PR #50。
- R4：容器头去掉 `arena_delta`，缩到 12 B，排进 V2.1 格式修订。
- 还没开：R2 同进程 `clone()`、R3 同机页交接（memfd + `F_SEAL_WRITE`）、R5 原地滑动压实。

## 3. 按"数据结构"定位，会改动什么

| 方面 | 现在（数据层） | 按"数据结构" |
| --- | --- | --- |
| 和谁比 | 序列化库：cereal、FlatBuffers、protobuf、zpp_bits、cista | 平时的操作和 std 容器比；过边界时和序列化库、std 深拷贝比 |
| 主指标 | SLG A/B/C 搬运场景的比值 | 两组：重定位税（操作 ÷ std，目标约 1.0）；搬运成本（÷ 编码解码或深拷贝，越小越好）。现有 SLG 指标仍是后一组的主体 |
| 地址契约 | 「K0 的边界」一段话 | 一张像 std 那样的失效规则表：每个操作会不会让元素引用、迭代器、root、区域地址失效，epoch 会不会加一 |
| 同进程搬 | 候选场景（分叉、回滚），没有专门的 API | 一等用法：`clone()`（R2）、修改前留底、回滚快照，成本是一次 memcpy |
| 容器 | XString、XVector、XSet、XMap、XBlob。XSet / XMap 是有序平铺数组，插入和删除 O(n)，查找 O(log n)（代码：`XFixedFlatSet`、`XFixedFlatMap`，`lower_bound_index`） | 容器种类和复杂度成为定位的一部分，用户会拿 `std::map`、`std::unordered_map` 来比。可重定位的哈希表是自然的下一种（cista 的 offset hash map 是先例，竞品调研 B5），但按"性能优先于功能"排在后面 |
| 非目标"数据从不离开进程" | 不服务，理由是修改和 std 打平、没有收益 | 仍然不服务；但"和 std 打平"从一句理由变成一条要守住的要求 |

## 4. 讨论的问题

按 susu 的习惯，一次一张卡片。Q1 先发，其余等 Q1 定了再逐个发。每条都写了推荐。

### Q1 身份：数据结构，还是数据层？（已定：数据结构，susu 10-22 16:08Z）

- **推荐**：以"数据结构"为身份，搬运是它最主要的用途。
- **理由**：
  - 这是 susu 的原话。
  - G8 A 的差距有了清楚的归属，就是重定位税。
  - 同进程的整块复制、回滚自然成为一等用法。
- **代价**：要多守一组指标（对 std 的操作比值）；容器的种类和复杂度会被拿来比较。
- **定了以后要做的**：
  - 一句话改写，经「产品定位」更新 `PRODUCT_POSITIONING.md`。
  - 重定位税指标交「性能优化」决定是否进 `PERF_GOAL.md`。
  - 失效规则表、`clone()` 原型放在本分支。

### Q2 要不要第三个词："可校验"（已定：不要，一句话也不提校验，susu 10-22 21:34Z）

可修改 + 可重定位是 R2 这一类，Boost.Interprocess 托管段也满足。XOffset 和它的差别在校验和布局核对。

**推荐**：标题保留 susu 的两个词；定义句里必须写出"跨信任边界时，接收方完整校验后接着改"，否则这句话对 Boost.Interprocess 也成立。

### Q3 英文怎么说（已定：position-independent，susu 10-22 21:41Z）

C++ 圈里的 "relocatable" 默认指对象级的平凡重定位（P1144、P2786），而 XOffset 的容器头恰好不是：它的拷贝和移动都删了。CppCon 的听众会先问这个。

**推荐**：
- 英文主词用 "position-independent"，对照 PIC：位置无关代码让共享库可以装到任意地址，XOffset 让可变的应用状态可以放到任意地址。
- 说明里写 "relocatable as a whole region, after any mutation"。
- 中文继续用"可重定位"，第一次出现时注明"整块"。

### Q4 "可修改"包括什么，不包括什么（已定：只写进用户指南，susu 10-22 21:43Z）

- **包括**：任意字段写入、变长修改、插入删除、空间复用、接收后接着改（K2、K5）。
- **不包括**：
  - 改 schema：新增字段要等 schema 演进。
  - 多个写者并发：单 owner。
  - 对象之间共享子对象或成环：所有权是树，要表达图只能用下标或 ID。

**推荐**：这三条作为数据结构的限制写进定位，口气和 std 容器的"不是线程安全的"一样。

### Q5 "可重定位"指哪一层（已定：按推荐，susu 10-22 21:43Z）

- 字节级（K0）在任何两次操作之间都成立。
- 活对象（XBuffer）只在显式点换存储。
- 边界上，跨机器和静止（快照）已经有了；同进程（B1）和同机另一进程（B2）只能导出再导入。

**推荐**：
- B1 补一个一等 API `clone()`，改动小。
- B2 的页交接先在本分支做 Linux 原型，作为 CppCon 演示的候选。

### Q6 度量（已定：按推荐，susu 10-22 21:43Z）

如果 Q1 选"数据结构"，建议在 `PERF_GOAL.md` 加两组：
- **重定位税微指标**：向量的 push_back、中间插入删除、遍历；字符串的赋值、追加；有序 map 的插入、查找、删除；嵌套对象构建。比值是 XOffset ÷ std，门槛建议 UB ≤ 1.05。
- **同进程整块复制 ÷ std 深拷贝。**

怎么排由「性能优化」决定。正式数字照旧只在 Mac 上测。

## 5. 原型计划（本分支，云端，不占 Mac）

| # | 内容 | 用来回答 | 状态 |
| --- | --- | --- | --- |
| X1 | 重定位税微基准：Q6 列的操作，XOffset 容器对 std 容器，用 GCC 16 docker 构建 | Q1、Q6：差距出在哪些操作上 | 云端完成（2.5 节） |
| X2 | 同进程整块复制：嵌套状态的 std 深拷贝，对 XOffset 现有的 save + load_unverified，再对 `clone()` 原型（一次 memcpy + 复制空闲表） | Q5：B1 值不值得做成一等 API | 云端完成（2.5 节）；原型在本分支的库头文件 |
| X3 | 失效规则表：逐个操作按代码核对 | 第 3 节的地址契约 | 草案完成：`docs/positioning/INVALIDATION_RULES.md` |
| X4 | R3 Linux memfd 封印页交接原型 | Q5 的 B2 | 云端完成：`experiments/mutable_relocatable/RESULT_X4_20261022.md`。共享 arena 整块交给另一个进程，两边都不复制，接收方只读时是 socket 流式的 0.39–0.55 倍（890 KB、3.55 MB）；接收方大面积修改时写时复制吃回收益；11.5 KB 的小状态不划算 |
| X5 | R5 原地滑动压实 | 长期运行后的碎片整理 | 研究项 |

云端是 Firecracker 虚拟机，只看比例和量级。要对外引用的数字仍按 `PERF_GOAL.md` 的口径在 Mac 上测，经协调者排队。

## 6. 决定记录

| 时间 | 问题 | susu 的选择 |
| --- | --- | --- |
| 10-22 16:08Z | Q1 身份 | 以"数据结构"为身份，搬运是它最主要的用途；加对 std 的操作比值指标（卡片选项 0，susu 在项目聊天回复"按推荐来"） |
| 10-22 21:34Z | Q2 第三个词 | 不要"可校验"：一句话只讲可修改、可重定位，校验只留在 K4（susu 原话"不需要可校验"，记为卡片选项 2）。改写建议交「产品定位」：`POSITIONING_REWRITE_Q1Q2.md`（项目资料），21:37Z 已更新正本 |
| 10-22 21:41Z | Q3 英文术语 | 主词用 position-independent："a mutable, position-independent C++ data structure"，说明里写 "relocatable as a whole region, after any mutation"；不单用 relocatable，避免和 P2786 的对象级平凡重定位混淆（卡片选项 0） |
| 10-22 21:43Z | Q4 "可修改"的限制 | 定位不动；三条限制（不能改 schema、单写者、所有权是树）和 XMap/XSet 对标 `std::flat_map` 的失效规则，与失效规则表一起写进 `docs/USER_GUIDE.md`（卡片选项 1）。只写 next_cpp26 已有的行为，R1、`clone()` 落地后再补 |
| 10-22 21:43Z | Q5 可重定位的层次 | 按推荐（susu "后续按推荐答案来"）：B1 同进程补一等 API `clone()` / `clone_into()`，以本分支原型为起点单独实现；B2 同机另一进程的页交接先在本分支做 Linux memfd 封印原型（X4），作为 CppCon 演示候选 |
| 10-22 21:43Z | Q6 度量 | 按推荐：`PERF_GOAL.md` 加两组，重定位税微指标（XOffset ÷ std，门槛 UB ≤ 1.05）和同进程整块复制 ÷ std 深拷贝；怎么排、怎么测由「性能优化」定，正式数字只在 Mac 上测 |

## 来源

项目资料（项目共享文件夹，不在仓库里）：
- `PRODUCT_POSITIONING.md`
- `PERF_GOAL.md`
- `relocatable-20261022/RELOCATABLE_ESSENCE_20261022.md`、`R1_MOVABLE_STORAGE_DESIGN_20261022.md`、`FOLLOWUP_SPLIT_20261022.md`
- `competitors-20261022/COMPETITORS_20261022.md`
- `inplace-mutate-20261021/G3_RESULT_20261021.md`
- `small-msg-20261021/SMALL_MSG_COST_20261021.md`

仓库（next_cpp26 @ 4bc922b）：
- `xoffsetdatastructure.hpp`：`XFixedFlatSet`、`XFixedFlatMap`、`XHandle`、`XBuffer`
- `external/typelayout/include/boost/typelayout/admission.hpp`
- `docs/V2_FORMAT.md`、`docs/VM_PERFORMANCE_COMPARISON_RESULTS.md`、`README.md`
