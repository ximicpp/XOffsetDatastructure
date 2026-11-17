# CppCon 2024 对比，用 4.0 重跑

CppCon 2024 分享（tag `v1.0.0`，`benchmark/xbenchmark3.cpp`，结果在当时的 README 第 3 节）用一个游戏
角色 `Character` 比了 8 个库：发送+接收时间、读写循环时间、消息大小。这里用 4.0.0 的库重做同一个对比：
数据形状、读写循环、对手都和 2024 一样，测量规则换成现在的。

## 怎么跑

```sh
bench/cppcon2024/run.sh <输出目录>          # RUNS=7 CPU=2 可调；QUICK=1 冒烟
```

需要 git、python3、docker（第一次构建镜像要联网）。脚本构建工具链镜像（`Dockerfile`：4.0 记分卡同一个
GCC 16.2 镜像，加 Debian 13 的 protobuf 3.21.12、Cap'n Proto 1.1.0、FlatBuffers 23.5.26、msgpack-cxx 7.0.0、
zstd 1.5.7），在主机上按记分卡固定的提交拉 zpp_bits v4.7.6 和 cista v0.16，生成三个 IDL 的代码
（`schema/` 是 2024 原样的 `.proto`、`.fbs`、`.capnp`），`-std=c++26 -freflection -O3 -DNDEBUG` 编译，
`--cpuset-cpus` 绑核跑 `RUNS` 个进程，`summarize.py` 对每项取中位数并和 2024 的数并列。

编译时加 `-DCC24_PARTS=1`（装备）、`2`（路径点和属性）或 `4`（物品）可以只跑读写循环的一部分，用来看时间花在哪。

## 场景和规则

- 数据：和 2024 同形状：64 个物品（map）、16 个路径点、16 个属性、8 件装备各 8 个属性、4 个技能、8 字节名字。
- 读写循环（每次迭代）：4 个标量 +1，改位置，4 次（追加一件 8 属性装备再删掉），追加 8 个路径点和 1 个属性，
  插入并删除一个物品，删掉那 8 个点和 1 个属性，4 个标量 -1。2024 在计时里每次迭代抽 85 个
  `mt19937_64` 随机数，开销和数据操作本身差不多大；这里随机数在计时前生成好，计时只含数据读写。
- 删掉的东西各家都真释放（2026-11-17 起）。Protocol Buffers 删装备和路径点用 `DeleteSubrange`，被删的
  message 会被 delete，和其他库每次迭代真分配、真释放一样。它默认的 `RemoveLast` 把清空的 message 留在
  repeated 字段里，下一次 `add_` 直接复用，整个循环不分配；这个写法另列一行只报告（`Protocol Buffers reuse`）。
- 物品的 key 每次迭代都不同（2026-11-17 起）：计时前按迭代号生成 10 万个 splitmix64 值，按顺序读，各家相同。
  其余值仍取自 1024 项的环。以前 key 也取自环，每轮每个 key 重复约 100 次，std 和 cista 的有分支二分查找
  （`std::lower_bound`）的 1024 条路径被分支预测器记住，XOffset `XMap` 的无分支查找不受益；只跑物品部分时，
  cista raw 调优写法从 28 ns 变成 53 ns，XOffset 从 41 ns 变成 49 ns。
- 发送 = 内存状态 → 别的进程能用的字节；接收 = 字节 → 接收方能读也能改的状态（2024 的说法）。
  - XOffset：`save_verified_into` / `save_verified`，接收用信任档 `XReceiver<XLink::server_peer>`
    （`reload` 进复用的 buffer / `load` 新建）。完整校验档（`reload_verified` / `load`）只报告。
  - zpp::bits、MessagePack：编码 std 对象，解码进 std 对象。
  - Protocol Buffers：和 2024 一样，状态就是可改的 protobuf message：`SerializeToString` + `ParseFromString`。
  - FlatBuffers、Cap'n Proto：从 std 对象构建消息，接收方读出到 std 对象（这两种格式的消息不能原地增删，
    要改就得拆出来，2024 README 第 1.2 节的论点）。FlatBuffers 不跑 Verifier；只读访问（不拆）另列一行只报告。
    Cap'n Proto 的读取自带边界检查，关不掉。
  - cista raw / offset：和 4.0 记分卡的 cista 一样，接收方把 wire 复制进自己的 buffer 再原地反序列化
    （raw 用 `mode::UNCHECKED`，offset 用 `mode::CAST`，都不校验），对象留在 buffer 里可以改。
- 两种发送+接收模式：reuse（收发两端都复用 buffer 和对象，服务器循环的常态）和 fresh（每条消息新建，2024 的写法）。
- 物品 map 的容器种类各家相同：std 模型和 cista 都用按 key 排序的 vector（和 XOffset 的 `XMap` 一样是
  扁平的），插入、删除走同一段代码（`common.hpp` 的 `items_upsert` / `items_erase_at`）。2024 的 std 对手用
  `std::map`，cista 用 `hash_map`；前者在 2026-11-03 的公平性审查里改成扁平的，后者在这一版去掉，不再拿
  hash map 和有序 map 比。Protocol Buffers 用 2024 原样的 `.proto`，物品是它的 `map<>` 字段。
- 每种格式解码出的状态都和发送的状态逐字段比对一次（不计时），读写循环结束后的状态也和 std 模型比对。
- 字节数：原始大小和 zstd level 3（压缩不计时）。wire 格式不为缩小字节调整，字节只报告。
- 两组读写行。“2024 原样”：2024 的数据布局和逐个元素的写法（`rw_once`），所有库相同。“调优写法”：
  装备属性不超过 8 个时内联存放（更长的放进溢出 vector），8 个路径点一次追加、一次删掉，物品按插入
  返回的位置删除（`rw_tuned`，XOffset 是 `rw_xoffset_bulk`）。调优对每个能这样写的模型都做：XOffset
  （`xoffset inline`）、std 模型（`std inline`，也是 `zpp::bits inline` 的状态）、cista（`cista raw inline`、
  `cista offset inline`）。Protocol Buffers 的消息里没有内联的标量数组，`RemoveLast` 本来就留着清空的
  对象（计分行改用 `DeleteSubrange` 真释放，见上），能用的批量调用只有按范围添加属性（`Protocol Buffers bulk`）。FlatBuffers 和 Cap'n Proto 的 schema 由 IDL 固定，读写用的是 std 模型。
- XOffset 的两组读写行都是每次迭代一个 `edit()`。一个 `edit()` 包住一轮全部迭代的写法只报告
  （`rw_one_scope`）。

## Latest results: XVector first block of at least 32 bytes (2026-11-17)

This set supersedes the next section as the one the top-level README quotes.

One library change (e36cf44): the block an empty `XVector` takes on its first append holds at least 32 bytes
(8 floats or 32-bit ids; it was 4 elements, 16 bytes), and a `push_back` of a small scalar that triggers growth
skips the staged copy and insert checks of the general path. The case is the fair case of the next section,
unchanged. The library before (main 837c9d4f) and after ran 15 processes each on the same cloud machine,
alternating; medians below. All tables and the raw output of every process:
[`results/2026-11-17-first-block/summary.md`](results/2026-11-17-first-block/summary.md) (after) and
[`before.md`](results/2026-11-17-first-block/before.md) (before). The machine was about 1.2 times slower this
time than in the next section, so compare ratios across sections, not nanoseconds.

| Row | Before | After |
| --- | ---: | ---: |
| Read/write, 2024 layout: XOffset ns | 202.6 | 143.5 |
| ÷ std / cista raw / cista offset / Protocol Buffers | 0.59 / 0.84 / 0.80 / 0.35 | 0.42 / 0.61 / 0.55 / 0.24 |
| Read/write, tuned layout: XOffset ns | 61.3 | 60.4 |
| ÷ std / cista raw / cista offset / Protocol Buffers | 0.72 / 0.96 / 0.91 / 0.17 | 0.72 / 0.91 / 0.90 / 0.16 |
| Send + receive reuse, 2024 layout: XOffset ns (÷ zpp::bits) | 102.7 (0.31) | 83.2 (0.24) |
| Send + receive reuse, full validation tier: XOffset ns (÷ zpp::bits) | 218.1 (0.65) | 170.5 (0.50) |
| One read/write + send + receive, tuned: XOffset ns (÷ zpp::bits / cista) | 145.4 (0.36 / 0.22) | 144.4 (0.37 / 0.21–0.22) |
| Wire bytes (zstd-3) | 3,112 (3,006) | 3,064 (2,986) |

- Read/write, 2024 layout: faster in every pair, median after ÷ before 0.71. Instructions per iteration
  (GCC 16.2, callgrind) 3,489 -> 2,187. Four times per iteration the loop appends an equipment record with 8
  float attributes and removes it; the attribute vector used to take 16 bytes and move to 32 bytes at the fifth
  attribute (one more allocation, a copy and a retirement), and now takes 32 bytes at once. The equipment step
  alone goes from 2,771 to 1,491 instructions per iteration.
- Read/write, tuned layout: instructions unchanged (813), timing unchanged (pair ratio median 1.00). The move
  from 0.96 to 0.91 against cista raw is jitter of the cista row (63.8 vs 66.3 ns). After the change XOffset
  wins both read/write rows against every rival in all 15 processes.
- Send + receive, 2024 layout: faster in every pair (median 0.81), 1,445 -> 994 instructions per message. The
  sent state is built with `push_back`; each attribute vector used to leave a retired 16-byte block that was
  saved and loaded with the wire's hole table. Those blocks are gone and the wire is 48 bytes smaller. The
  inline-layout rows have no such vector and do not change (84 ns).
- The report-only Protocol Buffers row that reuses removed messages (`RemoveLast`) takes 76.8 ns and is still
  faster than XOffset's 143.5 ns: its loop allocates nothing. Doing the same in the library would keep removed
  slots owning blocks, which changes the wire contract, so it is not done.
- The report-only row `xoffset bulk` (2024 layout, bulk calls) reads 113.8 -> 119.9 ns with the same
  instruction count (1,749). A separate program with only these rows, alternating on the same machine, gives
  115.4 vs 113.4 ns, so this is code placement in the bench binary, not a slower library.
- Cost: a vector grown one element at a time with `push_back` / `emplace_back` that ends with 1 to 4 four-byte
  scalars takes 16 bytes more than before (24 more for 1- and 2-byte elements). `reserve()` and the range
  assignments still size their block exactly. The wire format is unchanged.

## 最新结果：公平用例（2026-11-17）

用例改了两处（上面“场景和规则”里标了 2026-11-17 的两条）：Protocol Buffers 删掉的 message 真释放，物品 key
每次迭代不同。库是 main 18a37f39（PR #292、#293 之后，加固默认关闭）。云端 x86-64，GCC 16.2，15 个进程取中位数。
全部表格和每个进程的原始输出：[`results/2026-11-17-fair/`](results/2026-11-17-fair/summary.md)。这台机器和下面
几节的不同，绝对纳秒数不能跨节比，比值可以。

| 项 | XOffset ns | std | cista raw / offset | Protocol Buffers |
| --- | ---: | ---: | ---: | ---: |
| 读写，2024 原样：对手 ns | 170.5 | 273.9 | 184.3 / 207.0 | 450.7 |
| 　XOffset ÷ 对手 | | 0.62 | 0.92 / 0.82 | 0.38 |
| 读写，调优写法：对手 ns | 51.1 | 70.0 | 54.3 / 56.5 | 288.2 |
| 　XOffset ÷ 对手 | | 0.73 | 0.94 / 0.90 | 0.18 |

- 两种写法下 XOffset 的读写都是最快的。逐进程比，XOffset 对 cista raw 15 个进程赢 14 个（2024 原样和调优
  写法都是），对 cista offset 和 Protocol Buffers 全赢。
- Protocol Buffers 用 `RemoveLast` 复用对象时 2024 原样是 65.5 ns（只报告），真释放后 450.7 ns：以前 3 倍的
  领先全部来自对象复用。
- 发送+接收 reuse：XOffset 87 ns，zpp::bits 的 0.33，FlatBuffers 和 cista 的 0.16，Cap'n Proto 的 0.045，
  MessagePack 的 0.016，Protocol Buffers 的 0.013。完整校验档 202 ns（zpp::bits 的 0.76）。
- 读写一次 + 发送 + 接收（调优写法）：XOffset 122 ns，zpp::bits（std 容器）327 ns（0.37），cista 578–591 ns
  （0.21），Protocol Buffers 7,180 ns（0.017）。

## PR #292 之后（2026-11-16，旧用例）

PR #292（`emplace_back` 不再查 owner、`pop_back` 默认不清零、`erase` 快路径）合并前后的库，在同一台云端机器上
用本目录的同一套 bench 各跑 15 个进程、前后交替，取中位数。全部表格：
[`results/2026-11-16-pr292/after.md`](results/2026-11-16-pr292/after.md)（合并后）和
[`before.md`](results/2026-11-16-pr292/before.md)（合并前，同一台机器）。顶层 README 引用的是合并后这一组。

| 项（XOffset ÷ 对手） | 合并前 | 合并后 |
| --- | ---: | ---: |
| 读写，2024 原样：XOffset ns | 257.6 | 239.0 |
| 　÷ std / cista raw / cista offset / Protocol Buffers | 0.74 / 1.15 / 1.05 / 3.28 | 0.70 / 1.07 / 0.97 / 3.06 |
| 读写，调优写法：XOffset ns | 67.7 | 62.1 |
| 　÷ std / cista raw / cista offset / Protocol Buffers | 0.85 / 1.40 / 1.39 / 0.92 | 0.79 / 1.34 / 1.16 / 0.86 |
| 发送+接收 reuse：XOffset ns（÷ zpp::bits） | 99.4（0.31） | 95.8（0.28） |
| 读写一次 + 发送 + 接收，调优写法：XOffset ns（÷ zpp::bits / cista） | 148.4（0.38 / 0.21–0.22） | 143.1（0.36 / 0.20–0.21） |

这台机器和下一节那台不同，绝对纳秒数不能跨节比，比值可以。读写调优写法仍输 cista 1.16–1.34 倍。
#292 之后 PR #293 把加固措施改成默认关闭的编译开关，CHANGELOG 记录了默认构建对 `XOFFSET_HARDENED=1` 的指令数（读写循环 2024 原样 −14.4%、调优 −7.6%），
没有重新计时。

## 更早的结果（旧用例：Protocol Buffers 复用删掉的对象，物品 key 取自 1024 项的环；云端 x86-64，GCC 16.2，15 个进程取中位数，2026-11-16，next_cpp26 3f25b76c + 本目录这次的改动）

“2024” 列是 2024 README 的数换算成每次的纳秒（10 万次的毫秒数 × 10）。2024 的 XOffset 发送+接收
0.5 ns 是测错的：循环第一次就把发送方的 buffer move 走了，之后计的是空 move；2024 FlatBuffers 和
Cap'n Proto 的那一行还把随机数和构建都算在里面。所以 2024 列只用来对照，不能和 4.0 列直接相除。
2024 列里 cista 的读写和大小是它用 `hash_map` 时的数。

这一版的云端机器比上一版（PR #289）那台慢约 1.3 倍，所有行都在这台机器上重跑过；绝对纳秒数不能和
上一版相比，比值可以。

### Send + receive (ns per message)

| Library | 2024 encode+decode | 4.0 reuse | 4.0 fresh | XOffset ÷ lib (reuse) | XOffset ÷ lib (fresh) |
| --- | ---: | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 0.5 | 107.9 | 275.4 | 1.000 | 1.000 |
| zpp::bits | 4,531.4 | 409.8 | 1,298.2 | 0.263 | 0.212 |
| MessagePack | 9,577.0 | 7,594.7 | 7,718.3 | 0.014 | 0.036 |
| Protocol Buffers | 15,780.6 | 10,596.2 | 18,000.5 | 0.010 | 0.015 |
| FlatBuffers | 6,794.4 | 718.7 | 1,321.9 | 0.150 | 0.208 |
| Cap'n Proto | 2,592.2 | 2,610.3 | 3,232.1 | 0.041 | 0.085 |
| cista raw | 5,315.6 | 739.9 | 899.6 | 0.146 | 0.306 |
| cista offset | 5,845.5 | 736.1 | 882.0 | 0.147 | 0.312 |

Report-only rows:

| Row | reuse | fresh |
| --- | ---: | ---: |
| xoffset 4.0 full | 245.4 | 409.7 |
| FlatBuffers view | 571.0 |  |

### Read/write loop (ns per iteration)

| Library | Containers | 2024 | 4.0 | XOffset ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset 4.0 | XOffset (edit() per iteration) | 498.5 | 280.2 | 1.000 |
| zpp::bits | std::* (shared, items = sorted vector) | 753.2 | 406.7 | 0.689 |
| MessagePack | std::* (shared, items = sorted vector) | 680.4 | 406.7 | 0.689 |
| Protocol Buffers | google::protobuf message | 565.4 | 89.3 | 3.138 |
| FlatBuffers | std::* (shared, items = sorted vector) | 680.4 | 406.7 | 0.689 |
| Cap'n Proto | std::* (shared, items = sorted vector) | 680.4 | 406.7 | 0.689 |
| cista raw | cista::raw::* (items = sorted vector) | 655.4 | 275.3 | 1.018 |
| cista offset | cista::offset::* (items = sorted vector) | 692.2 | 291.8 | 0.960 |
| xoffset 4.0 plain_root (report-only) | XOffset, plain reference | | 246.9 | |

### Wire size (bytes)

| Library | 2024 raw | 2024 zlib | 4.0 raw | 4.0 zstd-3 |
| --- | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 4,184 | 3,075 | 3,232 | 3,001 |
| zpp::bits | 2,916 | 2,769 | 2,916 | 2,926 |
| MessagePack | 3,222 | 3,064 | 3,542 | 3,391 |
| Protocol Buffers | 4,100 | 3,759 | 4,095 | 3,799 |
| FlatBuffers | 3,672 | 3,247 | 3,680 | 3,128 |
| Cap'n Proto | 3,656 | 3,205 | 3,656 | 3,131 |
| cista raw | 5,392 | 3,294 | 3,216 | 2,992 |
| cista offset | 5,392 | 3,336 | 3,216 | 2,992 |

XOffset after the read/write rounds (report-only): save_verified 8,600 B, save_compact 3,552 B.

### Tuned layout and loop: read/write (ns per iteration)

| Library | Tuning applied | 2024 layout | Tuned | XOffset inline ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset inline | inline attributes, range append, erase at position; edit() per iteration | 280.2 | 80.1 | 1.000 |
| std inline | inline attributes, range insert, erase at position | 406.7 | 86.8 | 0.923 |
| Protocol Buffers bulk | attributes added as one range (no inline array in a message) | 89.3 | 82.6 | 0.970 |
| cista raw inline | inline attributes, range insert, erase at position | 275.3 | 53.5 | 1.497 |
| cista offset inline | inline attributes, range insert, erase at position | 291.8 | 55.6 | 1.441 |

Report-only rows:

| Row | ns |
| --- | ---: |
| xoffset inline, one edit() around all iterations of a round | 74.7 |
| xoffset bulk (2024 layout, bulk calls, edit() per iteration) | 157.3 |

### Tuned layout: send + receive (ns per message)

| Library | reuse | fresh | XOffset inline ÷ lib (reuse) | XOffset inline ÷ lib (fresh) | raw bytes | zstd-3 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| xoffset inline | 93.3 | 272.2 | 1.000 | 1.000 | 3,128 | 2,977 |
| zpp::bits inline | 368.0 | 927.7 | 0.254 | 0.293 | 2,948 | 2,958 |
| cista raw inline | 726.0 | 863.7 | 0.129 | 0.315 | 3,280 | 2,994 |
| cista offset inline | 710.2 | 851.7 | 0.131 | 0.320 | 3,280 | 2,994 |

XOffset inline after the read/write rounds (report-only): save_verified 9,048 B (zstd-3 3,059), save_compact 3,632 B (zstd-3 3,006).

### One read/write iteration + one send + receive (reuse), ns (sum of the medians above)

| Library | 2024 layout | Tuned | XOffset ÷ lib (2024 layout) | XOffset inline ÷ lib (tuned) |
| --- | ---: | ---: | ---: | ---: |
| xoffset | 388.1 | 173.4 | 1.000 | 1.000 |
| zpp::bits (std model) | 816.5 | 454.8 | 0.475 | 0.381 |
| Protocol Buffers | 10,685.5 | 10,678.8 | 0.036 | 0.016 |
| cista raw | 1,015.2 | 779.5 | 0.382 | 0.222 |
| cista offset | 1,027.9 | 765.8 | 0.378 | 0.226 |

云端计时会抖（这台机器约三分之一的进程整体慢 1.3–1.7 倍），上面取 15 个进程的中位数；比值在
0.95–1.05 之间的按持平看。

## 怎么读

（读的是“更早的结果”那组，旧用例；现在的结论见“最新结果”。）

- **发送+接收**：XOffset 4.0（信任档）reuse 每条约 108 ns，是 zpp::bits 的 0.26、FlatBuffers 和 cista 的
  0.15、Cap'n Proto 的 0.04、MessagePack / Protocol Buffers 的 0.01。fresh 模式每条要新建接收 buffer
  （新映射），XOffset 约 275 ns，对各家的比值 0.02–0.31。完整校验档 reuse 约 245 ns，仍是 zpp::bits 的
  约 0.60。对手换成内联布局后结论不变（XOffset inline 对 zpp::bits inline 0.25，对 cista inline 0.13）。
- **读写，2024 原样**：XOffset 280 ns，快于 std 模型（0.69），和 cista 持平（0.96–1.02），慢于
  Protocol Buffers（3.14）。
- **读写，调优写法**：XOffset 80 ns，和 Protocol Buffers（83 ns，0.97）持平，快于 std 模型（0.92），慢于
  cista（54–56 ns，1.4–1.5 倍）。一个 `edit()` 包住一轮全部迭代只比每次迭代一个 `edit()` 快约 5 ns。
- **改一次 + 发一次 + 收一次**（两项中位数相加，只作参考）：XOffset 最低，调优写法下 173 ns，是
  zpp::bits 的 0.38、cista 的 0.22、Protocol Buffers 的 0.02。读写上输给 cista 的约 25 ns 远小于发送+接收
  赢回的约 630 ns。
- **大小**：XOffset 4.0 的 wire 从 2024 的 4,184 字节降到 3,232 字节（V2 格式），zstd-3 后 3,001 字节，
  和其他格式（2,926–3,799）同一量级。读写循环之后 `save_verified` 带着空洞表是 8,600 字节，`save_compact`
  是 3,552 字节（只报告）。

## 读写的时间花在哪

（旧用例的数：Protocol Buffers 复用删掉的对象，物品 key 取自 1024 项的环。）

用 `-DCC24_PARTS` 把循环的三部分分开计时（云端，每个变体 3 个进程取中位数，ns/次；“全部”列是上面
15 个进程的数；标量和位置的读写每个变体都在，约 1–2 ns，所以三部分相加会略大于“全部”）：

| 行 | 全部 | 装备 ×4 | 路径点和属性 | 物品 |
| --- | ---: | ---: | ---: | ---: |
| XOffset，2024 原样 | 280.2 | 207.9 | 22.9 | 63.9 |
| XOffset，2024 原样，plain_root（只报告） | 246.9 | 160.4 | 21.2 | 63.2 |
| std，2024 原样 | 406.7 | 290.1 | 17.1 | 76.9 |
| Protocol Buffers，2024 原样 | 89.3 | 39.1 | 31.7 | 53.1 |
| cista raw，2024 原样 | 275.3 | 199.8 | 17.5 | 36.8 |
| cista offset，2024 原样 | 291.8 | 212.5 | 17.9 | 34.7 |
| XOffset，调优写法 | 80.1 | 28.1 | 11.6 | 46.7 |
| std，调优写法 | 86.8 | 23.4 | 11.3 | 66.0 |
| Protocol Buffers，范围添加 | 82.6 | 22.3 | 32.1 | 51.9 |
| cista raw，调优写法 | 53.5 | 19.6 | 11.6 | 31.7 |
| cista offset，调优写法 | 55.6 | 21.1 | 13.5 | 29.8 |

- 装备追加再删除，2024 原样：每件装备的属性 vector 要分配一次再释放。std 和 cista 走 `new`/`delete`；
  XOffset 在 `edit()` 里删掉的块要到 `edit()` 结束才复用（plain 引用仍然安全），所以每次迭代都要新分配；
  Protocol Buffers 的 `RemoveLast` 留着清空的对象，下一次 `add_` 直接复用，不分配。内联布局去掉了这次
  分配，各家都降到 20–28 ns。
- 物品插入再删除：XOffset、std、cista 都是有序扁平 map，都要挪动后面的元素。std 和 cista 跑同一段
  代码，cista 约 30–37 ns，std 约 66–77 ns；XOffset 的 `XMap` 约 47–64 ns；Protocol Buffers 的 `map<>`
  约 52 ns。调优写法下 XOffset 输给 cista 的约 25 ns 里，这里占约 15 ns，装备部分占约 8 ns。
- 装备，调优写法：std 约 23 ns，cista 约 20 ns，Protocol Buffers 约 22 ns，XOffset 约 28 ns。
- 路径点：都在 11–32 ns。

容器种类相同之后，2024 原样的读写 XOffset 和 cista 持平；调优写法下还差 1.4–1.5 倍，差距在 `XMap`
插入删除和 `edit()` 里追加装备这两处，还没有剖析到具体指令。

PR #289 顺带改了库里一处：`edit()` 结束时归还待回收块的列表会把列表的主机内存也释放掉，下一个
`edit()` 又要重新分配。现在保留列表的容量（最多 1024 项）。2024 原样的循环每次迭代从 4,794 Ir 降到
4,121 Ir（GCC 16.2，−14%），计时从 260 ns 降到 213 ns（当时那台机器，同机交替 7 对进程）；内联布局的行
不受影响。

## 历史结果

`results/2026-11-16/` 是 PR #287 的记录（另一台 Linux x86-64 和 Mac mini M4 Pro，各 9 对进程）。那次的
`xoffset inline` 读写行是一个 `edit()` 包住一轮全部迭代，对手都是 2024 原样的布局和写法，所以它的
“读写对最快对手 0.889 / 0.755”不是同等写法下的比较，以本页的“调优写法”表为准。那次的发送+接收数据
（对 zpp::bits 0.19 / 0.31）和本页方向一致。

到 PR #289 为止，cista 的物品 map 沿用 2024 的 `hash_map`，当时读写 XOffset ÷ cista 是 1.17–1.34
（2024 原样）和 2.18–2.59（调优写法）。那是拿有序扁平 map 和 hash map 比，这一版已去掉，cista 改用和
其他模型相同的有序 vector，以本页的表为准。

## 和 2024 的 demo

2024 的另一个 demo（`demo/xdemo.cpp`：一个平台写 `data.bin`，另一个平台读出来接着改）在 4.0 里对应
`examples/demo.cpp`、`examples/helloworld.cpp` 和跨设备互传 demo。本目录的 `character_demo.cpp` 用同一个
Character 做了写入、修改、回读（编译只需库头文件，C++20）：

```sh
c++ -std=c++20 -O2 -I. bench/cppcon2024/character_demo.cpp -o character_demo
checksum=$(./character_demo write character.xwire)
./character_demo modify character.xwire character.changed.xwire "$checksum"
./character_demo verify character.changed.xwire
```

修改进程可以在另一平台运行。回读进程使用首次写入的平台，按独立生成的参考对象校验完整状态。
