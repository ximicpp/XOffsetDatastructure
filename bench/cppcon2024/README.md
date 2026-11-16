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
- std 模型的 map 用有序 vector（和 XOffset 的 `XMap` 一样是扁平的）：2024 的对手用 `std::map`，
  2026-11-03 的公平性审查已把所有 std 模型改成扁平的。cista 用自己的 `hash_map`（2024 也是）。
- 每种格式解码出的状态都和发送的状态逐字段比对一次（不计时），读写循环结束后的状态也和 std 模型比对。
- 字节数：原始大小和 zstd level 3（压缩不计时）。wire 格式不为缩小字节调整，字节只报告。
- 两组读写行。“2024 原样”：2024 的数据布局和逐个元素的写法（`rw_once`），所有库相同。“调优写法”：
  装备属性不超过 8 个时内联存放（更长的放进溢出 vector），8 个路径点一次追加、一次删掉，物品按插入
  返回的位置删除（`rw_tuned`，XOffset 是 `rw_xoffset_bulk`）。调优对每个能这样写的模型都做：XOffset
  （`xoffset inline`）、std 模型（`std inline`，也是 `zpp::bits inline` 的状态）、cista（`cista raw inline`、
  `cista offset inline`）。Protocol Buffers 的消息里没有内联的标量数组，`RemoveLast` 本来就留着清空的
  对象，能用的批量调用只有按范围添加属性（`Protocol Buffers bulk`）；试过再加上 `DeleteSubrange` 和按迭代器
  插入、删除物品，整体更慢（154 ns 对 67 ns），没有用。FlatBuffers 和 Cap'n Proto 的 schema 由 IDL 固定，读写用的是 std 模型。
- XOffset 的两组读写行都是每次迭代一个 `edit()`。一个 `edit()` 包住一轮全部迭代的写法只报告
  （`rw_one_scope`）。

## 结果（云端 x86-64，GCC 16.2，7 个进程取中位数，2026-11-16，next_cpp26 5c3b6817 + 本目录这次的改动）

“2024” 列是 2024 README 的数换算成每次的纳秒（10 万次的毫秒数 × 10）。2024 的 XOffset 发送+接收
0.5 ns 是测错的：循环第一次就把发送方的 buffer move 走了，之后计的是空 move；2024 FlatBuffers 和
Cap'n Proto 的那一行还把随机数和构建都算在里面。所以 2024 列只用来对照，不能和 4.0 列直接相除。

### Send + receive (ns per message)

| Library | 2024 encode+decode | 4.0 reuse | 4.0 fresh | XOffset ÷ lib (reuse) | XOffset ÷ lib (fresh) |
| --- | ---: | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 0.5 | 81.3 | 192.0 | 1.000 | 1.000 |
| zpp::bits | 4,531.4 | 277.6 | 913.1 | 0.293 | 0.210 |
| MessagePack | 9,577.0 | 5,765.5 | 6,113.1 | 0.014 | 0.031 |
| Protocol Buffers | 15,780.6 | 7,461.1 | 13,065.7 | 0.011 | 0.015 |
| FlatBuffers | 6,794.4 | 536.2 | 1,000.7 | 0.152 | 0.192 |
| Cap'n Proto | 2,592.2 | 2,050.0 | 2,370.5 | 0.040 | 0.081 |
| cista raw | 5,315.6 | 617.9 | 709.0 | 0.132 | 0.271 |
| cista offset | 5,845.5 | 637.2 | 734.1 | 0.128 | 0.262 |

Report-only rows:

| Row | reuse | fresh |
| --- | ---: | ---: |
| xoffset 4.0 full | 193.8 | 307.6 |
| FlatBuffers view | 432.8 |  |

### Read/write loop (ns per iteration)

| Library | Containers | 2024 | 4.0 | XOffset ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset 4.0 | XOffset (edit() per iteration) | 498.5 | 212.7 | 1.000 |
| zpp::bits | std::* (shared, items = sorted vector) | 753.2 | 274.8 | 0.774 |
| MessagePack | std::* (shared, items = sorted vector) | 680.4 | 274.8 | 0.774 |
| Protocol Buffers | google::protobuf message | 565.4 | 67.8 | 3.137 |
| FlatBuffers | std::* (shared, items = sorted vector) | 680.4 | 274.8 | 0.774 |
| Cap'n Proto | std::* (shared, items = sorted vector) | 680.4 | 274.8 | 0.774 |
| cista raw | cista::raw::* (items = hash_map) | 655.4 | 159.0 | 1.338 |
| cista offset | cista::offset::* (items = hash_map) | 692.2 | 182.4 | 1.166 |
| xoffset 4.0 plain_root (report-only) | XOffset, plain reference | | 179.7 | |

### Wire size (bytes)

| Library | 2024 raw | 2024 zlib | 4.0 raw | 4.0 zstd-3 |
| --- | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 4,184 | 3,075 | 3,232 | 3,001 |
| zpp::bits | 2,916 | 2,769 | 2,916 | 2,926 |
| MessagePack | 3,222 | 3,064 | 3,542 | 3,391 |
| Protocol Buffers | 4,100 | 3,759 | 4,095 | 3,803 |
| FlatBuffers | 3,672 | 3,247 | 3,680 | 3,128 |
| Cap'n Proto | 3,656 | 3,205 | 3,656 | 3,131 |
| cista raw | 5,392 | 3,294 | 5,392 | 3,634 |
| cista offset | 5,392 | 3,336 | 5,392 | 3,634 |

XOffset after the read/write rounds (report-only): save_verified 8,600 B, save_compact 3,552 B.

### Tuned layout and loop: read/write (ns per iteration)

| Library | Tuning applied | 2024 layout | Tuned | XOffset inline ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset inline | inline attributes, range append, erase at position; edit() per iteration | 212.7 | 57.2 | 1.000 |
| std inline | inline attributes, range insert, erase at position | 274.8 | 64.4 | 0.888 |
| Protocol Buffers bulk | attributes added as one range (no inline array in a message) | 67.8 | 55.5 | 1.031 |
| cista raw inline | inline attributes, range insert, erase at position | 159.0 | 22.1 | 2.588 |
| cista offset inline | inline attributes, range insert, erase at position | 182.4 | 26.2 | 2.183 |

Report-only rows:

| Row | ns |
| --- | ---: |
| xoffset inline, one edit() around all iterations of a round | 53.7 |
| xoffset bulk (2024 layout, bulk calls, edit() per iteration) | 113.0 |

### Tuned layout: send + receive (ns per message)

| Library | reuse | fresh | XOffset inline ÷ lib (reuse) | XOffset inline ÷ lib (fresh) | raw bytes | zstd-3 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| xoffset inline | 68.3 | 172.7 | 1.000 | 1.000 | 3,128 | 2,977 |
| zpp::bits inline | 256.9 | 689.1 | 0.266 | 0.251 | 2,948 | 2,958 |
| cista raw inline | 607.3 | 698.6 | 0.112 | 0.247 | 5,456 | 3,643 |
| cista offset inline | 619.4 | 716.1 | 0.110 | 0.241 | 5,456 | 3,643 |

XOffset inline after the read/write rounds (report-only): save_verified 9,048 B (zstd-3 3,059), save_compact 3,632 B (zstd-3 3,006).

### One read/write iteration + one send + receive (reuse), ns (sum of the medians above)

| Library | 2024 layout | Tuned | XOffset ÷ lib (2024 layout) | XOffset inline ÷ lib (tuned) |
| --- | ---: | ---: | ---: | ---: |
| xoffset | 294.0 | 125.5 | 1.000 | 1.000 |
| zpp::bits (std model) | 552.4 | 321.3 | 0.532 | 0.391 |
| Protocol Buffers | 7,528.9 | 7,516.6 | 0.039 | 0.017 |
| cista raw | 776.9 | 629.4 | 0.378 | 0.199 |
| cista offset | 819.6 | 645.6 | 0.359 | 0.194 |

云端计时会抖（同一项单个进程之间最多差约 30%），上面取 7 个进程的中位数；比值在 0.95–1.05 之间的
按持平看。

## 怎么读

- **发送+接收**：XOffset 4.0（信任档）reuse 每条约 81 ns，是 zpp::bits 的 0.29、FlatBuffers 的 0.15、
  cista 的 0.13、Cap'n Proto 的 0.04、MessagePack / Protocol Buffers 的 0.01。fresh 模式每条要新建接收
  buffer（新映射），XOffset 约 192 ns，对各家的比值 0.02–0.27。完整校验档 reuse 约 194 ns，仍是 zpp::bits
  的约 0.70。对手换成内联布局后结论不变（XOffset inline 对 zpp::bits inline 0.27，对 cista inline 0.11）。
- **读写，2024 原样**：XOffset 213 ns，快于 std 模型（0.77），慢于 cista（1.17–1.34）和 Protocol Buffers
  （3.14）。
- **读写，调优写法**：XOffset 57 ns，和 Protocol Buffers（56 ns，1.03）持平，快于 std 模型（0.89），慢于
  cista（22–26 ns，2.2–2.6 倍）。一个 `edit()` 包住一轮全部迭代只比每次迭代一个 `edit()` 快约 3 ns。
- **改一次 + 发一次 + 收一次**（两项中位数相加，只作参考）：XOffset 最低，调优写法下 126 ns，是
  zpp::bits 的 0.39、cista 的 0.20、Protocol Buffers 的 0.02。读写上输给 cista 的约 35 ns 远小于发送+接收
  赢回的约 540 ns。
- **大小**：XOffset 4.0 的 wire 从 2024 的 4,184 字节降到 3,232 字节（V2 格式），zstd-3 后 3,001 字节，
  和其他格式（2,926–3,803）同一量级。读写循环之后 `save_verified` 带着空洞表是 8,600 字节，`save_compact`
  是 3,552 字节（只报告）。

## 读写的时间花在哪

用 `-DCC24_PARTS` 把循环的三部分分开计时（云端，每个变体 3 个进程取中位数，ns/次；标量和位置的读写
每个变体都在，约 1–2 ns）：

| 行 | 全部 | 装备 ×4 | 路径点和属性 | 物品 |
| --- | ---: | ---: | ---: | ---: |
| XOffset，2024 原样 | 212.7 | 147.6 | 17.3 | 48.9 |
| XOffset，2024 原样，plain_root（只报告） | 179.7 | 118.1 | 15.5 | 49.3 |
| std，2024 原样 | 274.8 | 201.3 | 12.5 | 65.4 |
| Protocol Buffers，2024 原样 | 67.8 | 29.2 | 22.6 | 27.5 |
| cista raw，2024 原样 | 159.0 | 139.2 | 13.5 | 14.0 |
| cista offset，2024 原样 | 182.4 | 155.2 | 14.3 | 14.1 |
| XOffset，调优写法 | 57.2 | 22.3 | 9.9 | 37.8 |
| std，调优写法 | 64.4 | 12.5 | 8.4 | 55.8 |
| Protocol Buffers，范围添加 | 55.5 | 16.9 | 22.4 | 28.0 |
| cista raw，调优写法 | 22.1 | 15.9 | 8.2 | 9.5 |
| cista offset，调优写法 | 26.2 | 17.2 | 8.3 | 10.3 |

- 装备追加再删除，2024 原样：每件装备的属性 vector 要分配一次再释放。std 走 `new`/`delete`；XOffset 在
  `edit()` 里删掉的块要到 `edit()` 结束才复用（plain 引用仍然安全），所以每次迭代都要新分配；Protocol
  Buffers 的 `RemoveLast` 留着清空的对象，下一次 `add_` 直接复用，不分配。内联布局去掉了这次分配，
  各家都降到 12–22 ns。
- 物品插入再删除：扁平有序 map（XOffset、std）要挪动后面的元素，调优写法下 XOffset 约 38 ns，std 约
  56 ns；cista 的 hash map 约 10 ns，Protocol Buffers 的 map 约 28 ns。调优写法下 XOffset 输给 cista 的
  差距主要在这里（约 28 ns），其次是装备部分（约 5–6 ns）。
- 路径点：都在 8–23 ns。

所以读写的差距来自容器种类（扁平有序 map 对 hash map），不是 offset 指针本身的开销。

这次对比顺带改了库里一处：`edit()` 结束时归还待回收块的列表会把列表的主机内存也释放掉，下一个
`edit()` 又要重新分配。现在保留列表的容量（最多 1024 项）。2024 原样的循环每次迭代从 4,794 Ir 降到
4,121 Ir（GCC 16.2，−14%），计时从 260 ns 降到 213 ns（同机交替 7 对进程）；内联布局的行不受影响。

## 历史结果

`results/2026-11-16/` 是 PR #287 的记录（另一台 Linux x86-64 和 Mac mini M4 Pro，各 9 对进程）。那次的
`xoffset inline` 读写行是一个 `edit()` 包住一轮全部迭代，对手都是 2024 原样的布局和写法，所以它的
“读写对最快对手 0.889 / 0.755”不是同等写法下的比较，以本页的“调优写法”表为准。那次的发送+接收数据
（对 zpp::bits 0.19 / 0.31）和本页方向一致。

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
