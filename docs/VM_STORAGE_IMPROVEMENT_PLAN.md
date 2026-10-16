# XBuffer 改进方案审查与实施顺序

审查日期：2026-10-13。分支 `next_cpp26`，HEAD 为
`07d649909d7f43ca0d11b85accb6d9dab991e284`；VM 实现、实验与文档为未提交的工作区改动。
本文件保留审查时的方案与验收边界。A–D 已实施，E 的四项策略已依据实验作出决定；
[实施结果](VM_STORAGE_IMPROVEMENTS_RESULTS.md) 记录最终验收、收益与限制。
下文各阶段中的“现状/当前”描述的是审查时的问题或设计动机，不代表仍未修复。

## 审查结论

稳定 VM 存储的方向成立。已有实测支持大 arena 增长与首次保存的收益，但不能据此认为功能验收已经充分、所有负载均有提升。审查时有两处已对照旧版复现的功能回归；实施先修复它们，再冻结可靠的 VM 基线用于后续优化。

| 阶段 | 改进 | 判断与交付边界 |
|---|---|---|
| A：合并前必须完成 | registry 析构寿命、const handle | 两项确认回归；修复并补真正覆盖程序退出的测试 |
| B：优先优化 | load 直接填充、verified save 单输出副本 | 明确的重复写入/复制路径；保持现有格式、返回类型和验证 |
| C：优先优化 | arena 起点地址的 owner 索引 | 当前唯一查询入口已知起点；先保留 mutex，独立替换线性查找 |
| D：功能通过后试验 | 容器尾块原地扩大 | 减少容器搬迁和空间放大；首版只使用已经可用的逻辑空间 |
| E：按负载决定 | 两层空闲块管理、小 VM 池、尾页回收 | 各自独立设计和验证，避免把存储改造扩大成 allocator 重写 |

每项单独形成可审查的变更和结果，不同时改变定位算法、锁模型、分配策略与传输格式。源代码修复、性能收益及未验证平台分别记录。

## 依据与保留契约

依据是第二份实现要求、当前源码、已完成的独立代码审查，以及
[18 组性能结果](VM_PERFORMANCE_COMPARISON_RESULTS.md)。代表性时间为每版七次运行的中位数：

| 负载 | 旧 vector → 当前 VM | 对方案的影响 |
|---|---:|---|
| 128 MiB 小起点增长，旧默认 / VM 目标上限 | 13.336 → 5.504 ms | 保留不搬迁的存储路径 |
| 128 MiB 小起点增长，相同预留 | 6.705 → 5.577 ms | 将预留策略收益与存储本身收益分开 |
| 千万次追加，显式增长 | 52.569 → 35.110 ms | 有增长收益，逻辑 arena 仍达到 256 MiB |
| 千万次追加，预分配 | 24.392 → 23.940 ms | 区间跨过 1，不能声称 push 本身提速 |
| 64 MiB verified load | 3.547 → 4.112 ms | 优先消除完整清零后再覆盖的路径 |
| 64 MiB 首次 verified save | 12.333 → 7.915 ms | 已减少 arena 搬迁，仍有两个 payload 副本 |
| 5000 个默认小 arena | 572.444 → 808.310 ms | 查找和映射开销需要分别改善 |
| 5000 个 arena，均初始预留 64 KiB | 569.515 → 740.450 ms | 只降低预留量不能消除回退 |

必须继续保持：

- 同一存储的 base 在 grow、逻辑 shrink、save 中稳定；上限或启用失败时明确报告，已有 logical/payload/allocator 仍可使用。
- `logical <= accessible <= reserved`，`logical <= maximum`；页取整不增加逻辑硬上限。accessible、实际 RSS 和 Windows commit charge 分别表述。
- 默认和 legacy hint 的容量选择仍为 `max(initial, default, hint)`；明确的 `maximum_capacity` 是硬上限。没有隐藏的 vector/malloc payload 退路，也不迁址增长。
- 容器业务语义、类型准入、v1 wire 字段及布局签名保持兼容。存储地址、owner 指针、索引和池信息仅存进程内。
- 普通 grow 继续清零新暴露范围，包括 shrink 后再次暴露的旧字节。内部元素依然可能因容器自己的扩容、插删或重建而移动。
- grow/shrink/save 不推进对象 epoch；move、swap、replacement 推进各自 C++ 对象的 epoch。checked handle 继续拒绝不同根类型和未验证替换。
- 不同 buffer 可由不同线程独立使用；同一 buffer 的修改、move 和销毁仍需外部同步。本轮不增加同一 buffer 的并发修改能力。

初始长度大于默认 64 MiB 时，现行普通构造的硬上限等于初始长度，没有额外增长余量。需要继续增长的调用方应显式选择更大的上限；方案与示例应说明这一点。

## A：先修复已确认功能回归

### A1. registry 同步状态寿命

现状：`registry_mutex()` 的函数静态 `std::mutex` 可能先于全局 XBuffer 析构。
全局 `XBuffer b;` 在 `main` 中赋值，程序正常退出时会锁定已销毁的 mutex；当前 macOS/libc++ 退出码为 `-6`，旧版为 `0`。两个从未使用的独立全局默认 buffer 也能复现。

修复同步状态，使其覆盖所有 buffer 析构，并且不在程序退出过程中被提前销毁。最小方案是在静态对齐存储中构造 mutex，不登记其析构；初始化继续由线程安全的函数静态初始化机制保护。后续索引和池的同步状态也必须满足这一寿命要求。

不能只在空 buffer 上跳过注销：这不解决 `main` 中已取得存储的全局对象。不能在析构失败时捕获异常继续退出来掩盖状态寿命错误。

验收用独立进程测试，断言**正常退出码**：

1. 全局默认对象在 main 中取得 VM 存储。
2. 两个独立全局默认对象从未使用。
3. 函数静态对象、不同构造顺序的多个全局对象、退出时的 move/replacement。
4. 独立 buffer 多线程创建/销毁；保持原有 TSan 覆盖。

普通局部变量测试以及 main 中的 assert 不足以检出 main 返回后的析构崩溃。

### A2. 进程内类型身份的 cv 兼容

现状：`type_identity<T>` 区分 `T` 与 `const T`。未注册 schema 的 Root 经
`make<Root>()` 创建后，当前 checked、unsafe 和直接 Core 的 `const Root` handle 全为空；旧版三者均有效，当前 `root<const Root>()` 仍有效。

绑定和比较根类型时统一使用 `std::remove_cv_t<T>` 的身份，句柄结果保持调用方请求的 const 属性。仅规范化进程内身份，不顺带改动 schema 名称、hash 或 wire 类型规则；注册 schema 的 cv 查询原有行为不在本修复中扩展。

验收：三个只读 handle 入口，普通 grow/save，同类型 replacement、不同类型 replacement、raw replacement、Core/derived move 和 swap。正向只读访问与反向类型拒绝必须同时通过，且不能因为 base 稳定删除 epoch 检查。

### A3. 功能基线

两项修复后重新执行完整 P2996 CTest 与 signature 检查、受影响 ASan/UBSan、独立 buffer TSan、旧新 wire 互通，并记录新增测试确实被 CTest 执行。

原 33/33 测试通过的记录保留为历史证据；不能将它解释成上述退出顺序和 cv 情形已经覆盖。以修复后的源码 SHA 冻结后续 VM 功能/性能基线。

## B：减少 load/save 的重复写入

### B1. 从字节初始化全新的 VM owner

当前 `init_from_bytes` 先构造并清零整个 logical 范围，随后 memcpy 完整覆盖。新增内部、仅用于新 owner 的字节工厂：

1. 检查 input、size、页取整、offset 和硬上限。
2. 局部 RAII owner 以 logical=0 预留，启用需要的页，记录 accessible。
3. 保留 Prepare 故障点；失败释放尚未完成的新 owner。
4. 完整复制 `[0, size)`，再发布 logical=size。
5. 维持现有 header 和完整 verified graph/type/schema 校验；typed access 仅在验证成功后开放。

使用现有输入 span/string；输入和旧 owner 在复制完成前保持存活。replacement 继续通过临时 XBuffer 成功后 move/swap。失败不能损坏旧对象或提前登记未完成存储。header-valid、typed-unverified 的现有中间状态可按当前流程存在；随后 graph 校验拒绝时需注销并释放临时 owner。

不公开通用的“不初始化 grow”开关。池复用及 shrink/regrow 的新暴露范围仍可能含旧数据，应维持清零。这里依赖完整复制覆盖逻辑范围，不能依赖所有平台或复用页天然为零。

内部零长度路径跳过 memcpy，XBuffer 输入的最小 payload 长度规则保持不变。

测试包含跨页/部分页、非零输入图样、硬上限、Reserve/Enable/Prepare 失败、坏 wire、失败 replacement、别的存活 buffer 作为输入、load 后多轮 shrink/regrow。代码确认 load 没有额外整段 memset，正确性测试确认所有逻辑字节由输入决定。

### B2. verified save 直接生成最终 string

当前 `save_verified` 先生成临时 payload string，再复制到 header+payload 的输出 string。
保持现有规范化顺序、header 和返回类型，删除临时 payload，直接从 arena 复制到最终输出。

本机 libc++ 已提供 `__cpp_lib_string_resize_and_overwrite=202110L`。实施时用实际 P2996 工具链做最小构建验证；该路径可直接填充输出，避免先初始化整段输出。回调只做非抛出写入，返回准确长度，并预先检查 header+payload 总长度。其它支持工具链可使用一次 reserve 后 append header 与 arena，仍保证没有中间 payload string。

成功结果与当前实现逐字节相同。输出分配失败沿用现有 save 已进行逻辑收缩但 buffer 仍可用的行为，不新增“失败时逻辑长度完全不变”的保证。没有新增分段协议或借用输出生命周期，返回的 string 仍是独立拥有的副本。

量化分开记录：删除临时 payload 确定消除一次 payload memcpy；减少输出预初始化的收益依赖实际路径。没有实测前不承诺加速倍率。

## C：替换线性 owner 查找

当前私有 `owner_from_payload_ptr` 唯一调用来自 `arena_header::owner(this)`；传入的是 arena 起点，而不是任意内部指针。优先采用 `arena 起点地址 → XBufferCore owner` 的进程内哈希索引，无需扩大 VA 对齐或在 payload 写入控制指针。

首个候选保留 mutex，使用有控制负载因子的地址哈希表，只改变定位算法：

- grow/shrink/save 不改 key，也不重新注册。
- 构造/load 注册新 key 可以分配失败；由正常构造路径回滚，释放新 VM 并报告 `XBadAlloc`。
- move/swap 对现存 base 的条目原位更新 owner，不 erase 后重新 insert，不使用可能新增条目的 `operator[]`。
- 析构只删除已有条目；move/swap/析构不分配索引节点或 rehash，保持 `noexcept`。
- 哈希与相等比较本身不抛出；全部 membership/owner 变更仍在锁内完成。
- 同步和索引采用进程寿命状态，保留 A1 的修复；不缓存可因 VA 重用而过期的 TLS owner 指针。

需要针对注册失败注入、move/swap 无分配、旧 key 删除、VA 重用、空对象及线程退出验证。哈希仅保证平均查找复杂度；检查实际地址混合与碰撞，避免页/预留对齐使低位高度一致。

预分配桶加侵入节点可作为第二个候选，降低每条索引的分配成本。但固定桶数量下，负载增长会增加链长，不能无条件宣称任意规模 O(1)。桶增长只能在允许失败的创建阶段执行，且不能破坏现有条目。两种方案按实测再选，不在首批引入 shared_mutex、无锁缓存或锁分片。

有序区间表可支持通用内部指针查询，复杂度 O(log N)，但当前并不需要；对齐控制页掩码会额外增加平台、容量及地址空间复杂度，后置。

测量 1/100/1000/5000 个同时存活 arena，顺序和随机访问，以及 1/2/4/8 个线程、每线程独占自己的 buffer。分别记录创建、分配、释放、销毁及索引内存，检查成本随存活 buffer 数量的变化。索引效果确认后，才评估锁竞争的独立优化。

实施测量调整：正式计时未给热路径插入探测计数，原始输出将 `owner_probe_count`
明确记为 null。用规模/阶段耗时验证查找策略收益，另以独立的 `operator new`
实验记录索引申请与保留字节；不把耗时推算成实际探测次数或单次查找延迟。

## D：容器尾块原地扩展

当前千万次追加仅约 38.15 MiB 有效数据，却增长到 256 MiB logical arena；基地址稳定并没有消除容器自身搬迁、旧块与翻倍增长的放大。

首版 `try_expand` 只处理**确实位于 arena 尾端的存活分配块**，且只使用已有 logical 范围。不吞并邻接 free blocks、不自动启用页、不移动 arena。失败无副作用，沿用现有 reserve 分配/迁移/显式 grow 重试路径。

必须同时考虑真实分配块大小与容器 capacity 的差异，并更新：

- Core 的 `AllocationHeader` 和 `used_end`。
- payload 中的 `XAllocatorStateV1::arena_end` 及相关统计。
- 容器 capacity；数据偏移及已有元素地址保持不变。

先完成全部 bounds、alignment、rel32 与 size 计算检查，再提交元数据，禁止后半段更新后抛错。成功只扩大容量；元素构造、赋值和异常保证保持原逻辑。

测试单尾部 vector/string、非尾部容器、两个容器交错增长、嵌套对象分配、较大对齐、复用的超尺寸块、容量边界、失败后继续修改、load/save 后继续扩展。单尾向量的零迁移是该场景目标，不能推广为所有 vector reserve 永不移动。

## E：分开评估的后续项目

### E1. 自由块切分/合并

源码有两层：Core 的进程内 `free_blocks_`，以及 wire 内的 16 类 freelist 缓存。
只改 Core 的最佳适配并不会自动消除内层缓存和碎片。先建立小/大/混合块的碎片重用负载，再选择改哪一层。

切分需保留合法 header、对齐与最小剩余块尺寸；合并需把内层缓存和外层节点协调到一致所有权，防止重叠分配或重复释放。保持 `XAllocatorStateV1`、FreeBlockHeader 和已冻结 container 的 wire 布局及验证规则，继续测试旧 reader 读取新输出。超出兼容范围的 allocator 重写另行立项。

### E2. 小 arena 显式容量与有限 VM 池

先用已有 `maximum_capacity(64 KiB)` 一类显式预算服务已知小对象。5000 个默认 VM arena 预留 312.5 GiB VA；相同数量、64 KiB 预算约 0.305 GiB。降低预算不能改进线性查找本身，也不能为活 storage 增长时迁移到更大池槽。

保持默认和 legacy hint 语义，不静默把默认上限降为 64 KiB。池作为显式策略，按 reservation 大小和容量策略匹配；同时限制缓存数、预留字节和保留可访问字节，超预算真正 release。新 owner 重建 header/allocator/type 关联，归还先注销，池状态寿命和跨线程归还需独立设计。

当前默认多 arena 的创建+销毁中位数合计约 85 ms，约为总耗时的 11%；分配/释放仍约 722 ms。优先解决查找，再判断池是否值得引入。测冷池/热池、短寿命 churn/大量同时存活、归还后的 RSS 与 VA；保留可访问页和回收后缓存两种模式分别测量。

池命中时必须定义 Reserve/Enable/Prepare 故障注入的适用点，测试强制冷路径或明确命中语义，不能使失败测试因缓存状态随机通过。

### E3. 可选尾页回收

保留现有逻辑 shrink 的默认行为。若增加回收，只能处理
`ceil(logical/page) * page` 之后的完整尾页，并先保证尾部没有 live allocation、freelist 或 validator 需要访问的元数据。保留最后一个部分页、base 和整份 reservation。

- Windows 可用 MEM_DECOMMIT，让尾页返回 reserved 状态；仅整个 owner 结束时 MEM_RELEASE。[VirtualFree 官方契约](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree)
- Linux 私有匿名 mapping 的 MADV_DONTNEED 可丢弃页，后续读取由零页重填。若另用 mprotect 撤销权限，两步的部分成功状态分别记录；保护成功更新 accessible，丢弃失败不能宣称物理回收完成。[Linux madvise](https://man7.org/linux/man-pages/man2/madvise.2.html)
- macOS 的 advisory 回收没有相同的需求零页保证；继续主动 regrow 清零，并实测回收/再访问行为。未验证前可明确返回不支持或仅提供 best-effort，不声称即时 RSS 下降。[Apple madvise](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/madvise.2.html)

禁止先 munmap 尾部再尝试抢回原地址，也不把 live 数据页标成可丢弃。回收减少驻留/提交开销的目标与再增长的缺页停顿存在取舍，分别测 idle 与 save/grow 循环，不能把回收设成默认而掩盖循环负载回退。

### E4. 自动增长独立处理

当前显式增长及失败重试保持兼容。若之后需要 Automatic，作为独立、显式配置的功能，定义几何增长、硬上限、异常语义与分配后元数据提交顺序；超限仍失败，不迁址。它不是修复当前回归或交付 B/C 的依赖，不与 tail expand 或自由表重写捆绑实施。

## 验证、性能门槛与实验改造

功能门槛高于速度：任何旧值、相对引用、类型拒绝、对齐、失败可用性、释放或 wire 互通失败，都阻止采用优化。持续用实际 P2996 工具链，不能以关闭 reflection/验证的构建代替集成检查。

后续应保留三个明确版本：原 vector V0、修复后的 VM 基线 V1、每项优化 V2。
以 V1→V2 判断新增优化，V0 仅作历史参照。单项与最终组合都测，继续冻结源码/依赖/编译参数和原始样本，使用独立进程、交替顺序和完整输入校验。

现有 runner 强制两侧增长次数、XBadAlloc 重试数、final logical、newly exposed 字节和 wire hash 一致，适合这次存储对照。D/E 的正确优化恰好可能改变它们，实施前必须分模式调整：

- 保持 payload 全值校验、用户请求操作数、输入长度与语义结果一致；结构计数、增长数、迁移数和内存指标作为结果。
- B/C 的内部存储/复制优化仍要求同样对象和历史操作产生完全相同的 wire 字节。
- D/E 的合法布局变化不要求 frame 长度或字节 hash 相同；要求 v1 格式/签名不变，旧新 reader 都能读出完全相同的对象值，再修改、保存和加载。
- 不放宽损坏输入拒绝、base 稳定或失败语义，不删验证来迁就优化。

| 项目 | 增加的测量 | 功能验收重点 |
|---|---|---|
| load/save | 1 KiB、64 KiB、8/64 MiB，首次及重复（小消息 64 次、大消息 4 次）；时间、复制路径和 RSS | 同输入 wire 一致、坏帧拒绝、跨页、失败与 regrow |
| owner 索引 | 1–5000 arena，顺序/随机、1–8 独立线程；四阶段耗时、独立索引内存（探测次数未插桩） | 注册失败回滚，transfer 无分配，注销与寿命 |
| 容器扩展/碎片 | 单尾部、交错、嵌套、碎片；迁移数、logical、grow 数、RSS、最大暂停 | 两层元数据一致、不重叠、容量边界、旧新互通 |
| 池/回收 | cold/warm、churn/live、idle/regrow；VA、accessible、RSS、提交与暂停 | 重用清零、故障点、部分页及平台失败状态 |

另外保留 8 次 save/regrow 诊断。未实现池/物理回收时，不把普通 churn 和保留页
结果称为冷/热池、回收收益或 idle RSS 验证；这些测量在采用该策略时才需要完成。

默认/同预留条件继续分开；RSS 与 logical/access/capacity 分开；峰值取采样时的进程历史峰值。增长与批次样本不足时不宣称生产 p99。

目标组收益应大于实际采样波动，并在独立重复运行中保持方向一致。改进目标没有可重复收益，或原增长/首次保存优势、预分配追加及失败路径出现持续回退时，先诊断，再决定是否采用该项。不能因为收益未达预期退回可搬迁 payload 或降低检查。

平台覆盖仍需补齐：macOS 原生已有基线；Linux 完整库目前为 x86_64 CPU 模拟，原生 arm64 仅 OS 层；Windows 尚未编译执行。新 OS 行为首先跑 native 独立层测试，再跑平台实际可用的完整 P2996 集成。平台缺失的结果明确保留为空，不用其它平台的性能推算。

## 交付清单与状态

- [x] A1：进程寿命 registry 与退出回归。
- [x] A2：cv 身份修复，正向只读及反向类型拒绝回归。
- [x] A3：完整功能/工具链验收，冻结 V1。
- [x] B1/B2：load 直接填充、save 单副本，分别正确性及性能验收。
- [x] C：owner 索引，注册失败/noexcept transfer/线程及性能验收。
- [x] D：尾块原地扩展，先修改实验校验模式，再验证布局兼容与收益。
- [x] E：依据单独负载决定自由块、池、回收和 Automatic 是否进入实施。

两处确认回归已修复。最终采用 A–D；E 经分别评估后保留显式容量与增长，
不引入两层自由表重写、VM 池或物理回收。实施及验收证据以
[结果报告](VM_STORAGE_IMPROVEMENTS_RESULTS.md) 为准；Windows 未编译执行的限制继续保留。
未发布包、推送分支或触发远端 CI。
