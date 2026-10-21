# 第三轮新格式（V2）设计（2026-10-21）

基线：next_cpp26 `1d2b182`（库头文件与 W9a 逐字节相同）。分支 `claude/perf-round3-newformat-20261021`，工作克隆 `/Volumes/XOffsetCS/r3`。
依据：susu 批准的 N1–N13（版本 B，格式与兼容放开）。代码依据见文末“V1 现状”两份梳理（文件:行号）。

## 1. 结论先行

- **校验能力不变弱**（N13 的停止条件没有触发）。V1 每个块的 16 字节块头提供的检查，在 V2 里由“容器头推出的块范围 + 空洞表 + 精确覆盖”承担；精确覆盖额外能抓到 V1 抓不到的不可达死区。逐项对照见第 6 节。
- V2 的四个结构变化：**无块头**、**分配器状态移出 arena**（空闲表放宿主侧，输出时以空洞表随 wire 带出）、**短串内联**（≤10 字节）、**宿主前缀页直取 owner**（不再经 TLS 注册表）。
- 字节账（推断，M 规模初始快照）：每条记录 V1 约 800 B，V2 约 576 B，少约 28%；变更后加上空洞表约少 22–26%。

## 2. Wire V2 布局

```
[XWireHeaderV2 96 B][payload: used_bytes][空洞表: hole_count × {u32 offset, u32 bytes}]
```

**XWireHeaderV2**：magic `XOFFV2\0\0`，format_major=2，format_minor=0，header_size，used_bytes，root_offset，hole_count，hole_bytes（空洞总字节，校验用），root_type_id，schema_hash_lo/hi，flags=0，endian_tag，crc32c=0，reserved=0。去掉 `reserved_bytes`/`allocator_offset`。读者先读 magic 和 major：magic 为 `XOFFV1` 或 major≠2 一律报“unsupported wire format version (V1 input is not accepted)”（N3）。

**payload 头**（arena header，48 B）：magic `XBUFV2\0\0`（必须改，否则旧版 `load_unverified` 会接受 V2 payload），header_size，flags=0，root_offset，used_end，reserved=0。不再有 allocator_offset。

**块**：一律 8 字节对齐、长度为 8 的倍数（字符串 `round8(cap+1)`，向量 `round8(cap*sizeof(T))`，根对象 `round8(sizeof(T))`）。与平台无关（V1 在 x86-64 上按 16 对齐，在 Apple arm64 上按 8）。

**不变式（精确铺满）**：`[0,48)` ∪ 根 ∪ 所有可达活块 ∪ 空洞表各项 恰好铺满 `[0, used_end)`，两两不重叠。

## 3. 容器头

仍为 16 字节，`alignas(8)`：`arena_delta`(i32) `data_delta`(i32) `size_`(u32) `capacity_`(u32)。

- `arena_delta` 只有一种含义：rel32 指向 payload 基址（XArena）。去掉 V1 的“指向 XAllocatorStateV1 / 带标记位指向段”两种绑定，也去掉 88 B 的 XAllocatorStateV1 及其块。
- **短串内联（I1，仅字符串）**：第 15 字节（`capacity_` 最高字节）最高位为 1 表示内联。内联时第 4–14 字节存字符，长度 = 第 15 字节低 7 位（≤10），`data[len]=0`，其余字节必须为 0。堆模式下 `capacity_ < 2^31`，最高位天然为 0。`view()`/`data()` 多一个可预测分支；键 `k0000` 这类短串不再分配。
- `capacity_` 精确等于块容量：分配器永远不交出比请求大的块（复用时拆分，余量回空闲表），所以块范围总能由容器头推出。

## 4. 分配器（宿主侧空闲表）

- 位置：`XBufferCore` 内（宿主内存，不进 wire）。精确尺寸桶 8–4096 B（512 个，LIFO，`std::vector<u32>`），更大的块放按大小排序的表（最佳适配）。
- 分配：同尺寸桶命中 → O(1) 弹出；否则在已提交容量内顶端分配；再否则从更大的块拆分；最后按增长策略扩容后顶端分配。V1 的 Core `free_blocks_` 线性扫描和 `release_cached_blocks` 整体删除——**reload 后 O(n²) 的来源随之消失**。
- 退还：调用方提供块大小（由容器头推出），压入对应桶，O(1)。宿主内存不足时（push 失败）记一个标志，`save_verified` 会退回到一次可达性遍历重算空洞表，不会悄悄输出不完整的 wire。
- 原地扩容：调用方提供旧块大小，条件是 `data + B_old == used_end`。V1 的“扩进块头记录的余量”不再存在。
- 加载：空洞表直接灌入空闲桶，变更前保存的空闲空间在 reload 后继续可用（V1 中 Core 空闲块在 reload 后成为永久死区）。
- 输出：payload 原样复制 + 按桶顺序写出空洞表；写读再写字节稳定（加载按表顺序入桶，输出按桶顺序写出）。

## 5. owner 直取

`VirtualMemoryStorage` 在 payload 前多保留一个宿主前缀页，页尾 8 字节存当前 `XBufferCore*`。容器从 `arena_delta` 得到 payload 基址后，`*(base-8)` 一次读就拿到 owner。前缀页不属于 payload，不会被复制、输出或进入 `get_buffer()`。`swap`/移动时更新；存储离开 `XBufferCore`（冻结、销毁、回池）时清零。全局注册表保留给 `canonical_bytes_at`、`stable_erase_if` 见证等原有用途，热路径不再使用。

## 6. 新校验器与 V1 对照

遍历方式沿用现有的根出发、按成员声明顺序的反射遍历（类型化与 raw 两条路径），注册区间改为由容器头推出：

| V1 检查 | V2 对应 | 强弱 |
| --- | --- | --- |
| 块头 `user_offset == 地址`、`user_bytes ≥ 需求`、对齐字段 | 块范围 `[data, data+B)` 由 `capacity_` 推出；8 对齐；精确铺满 | 不弱：指针被改到别处时，原块失去覆盖或与他块重叠，铺满检查必报错；V1 的自检只能发现改到“非块起点”的情况 |
| 活块互不重叠、不越过 used_end | 同（字节位图不变） | 相同 |
| 空闲链表节点：对齐、尺寸类、与活块/彼此不重叠、`free_bytes` 求和 | 空洞表项：8 对齐、8 倍数、在界内、与活块/彼此不重叠、求和等于 `hole_bytes` | 相同 |
| XAllocatorStateV1 绑定、arena_begin/end 包络 | 状态已删除；容器 `arena_delta` 必须精确指向 payload 基址 | 相同 |
| （无） | 不可达死区：V1 不检查 Core 空闲块和 >256 KiB 退还块 | **V2 更强** |
| 字符串结尾 NUL、size ≤ cap、set/map 严格有序、bool 0/1 | 同；内联串另查 长度 ≤10、`data[len]==0`、尾部字节全 0 | 相同 |
| 块头本身被改坏 | 块头不存在 | 不适用 |

V1 还有一个现存缺陷顺带消失：活块位于 allocator state 之前的 wire 能通过校验，但第一次退还就 `std::terminate`（`retire_dynamic_bytes` 是 noexcept 却会抛异常）。

## 7. 其他决定项

- **N6**：移动赋值的双向所有权检查（`move_assignment_owns_address*`）只在调试构建（未定义 `NDEBUG`）里做；同分配器、头在 arena 内两项廉价检查保留。
- **N7**：`XFixedVector::assign(first, last)`：连续且平凡可复制时走 `assign_range`；一般前向迭代器走 `rebuild(n, n)`。demo 适配器第 4 阶段（及同形的 `assign_sequence`）改为一行 `target.assign(source.begin(), source.end())`。
- **N8**：输出 ≥1 MiB 时在 Darwin 上每 256 KiB 一段 `madvise(MADV_WILLNEED)`（P1 形态）。
- **N10**：先只统计 ≥4 KiB 空洞字节占 wire 的比例；≥3% 才做稀疏 wire。

## 8. 字节账（推断，M 规模初始快照，每条记录）

| 项 | V1 | V2 |
| --- | ---: | ---: |
| Record 本体（records 数组元素） | 88 | 88 |
| name（平均 36 字节） | 40 + 16 块头 | 40 |
| samples 128 B | 128 + 16 | 128 |
| payload 128 B | 128 + 16 | 128 |
| attrs 数组 4×32 B | 128 + 16 | 128 |
| 4 个键 `k000x` | 4 × (8 + 16) | 0（内联） |
| 4 个值 16 B | 4 × (16 + 16) | 4 × 16 |
| 合计 | ≈ 800 | ≈ 576（−28%） |

另省：88 B allocator state 及其块头、根对象块头。变更后多出空洞表（每个空洞 8 B，估计约占 wire 1–2%）。

## 9. 公开 API 变化（N2，实现后逐项补全）

- 删除：`XAllocatorStateV1`、`XBuffer::allocator_state()`、容器的 `XAllocatorStateV1*` 构造、`allocator_state()`、`stored_allocator_proxy::state`、`XBufferCore::AllocationHeader`、`arena_header::freelist_runtime_owner/reset_freelist_runtime_cache`、`XWireHeaderV1`/`XWIRE_*_V1`、FreelistIndex 诊断。
- 改变：`arena_header` 字段（无 allocator_offset）、`try_expand`/`deallocate` 需要块大小、`XArenaAllocator::deallocate` 使用 `n`。
- 新增：`XFixedVector::assign(first,last)`、`XWireHeaderV2`、空洞统计接口（N10）。
- 不可达的 arena 内存（例如用 `XBuffer::allocator<T>()` 分配后未释放）在 `save_verified` 时不在空洞表里，加载校验会因铺满失败而拒绝；需要在保存前释放。

## 10. 测试与门禁（N4）

V1 专用测试（块头、XAllocatorStateV1、freelist index、range hint、V1 wire 位图细节）按新格式改写或退役，逐个列入最终报告。新增：金标准 wire（写-读-再写字节稳定）、S/M/L 模型真值和查询结果与 W9a 完全相同、校验器突变 fuzz（新旧语义差分：V2 拒绝集 ⊇ 不安全输入）、V1 拒绝测试。

## V1 现状依据

分配器与块头梳理、容器/校验器/wire 梳理两份报告的要点已并入本文；关键行号：AllocationHeader X:627-634，Core 线性扫描 X:1042-1050，release_cached_blocks X:1536-1563，块头读点 X:1095/1120/1693/4110，校验入口 X:4349-4379，raw 校验 RR:708-733，owner 查找 X:743-782。
