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

## 内联属性布局

`xoffset inline` 行使用新的 Character wire schema，保留同样的逻辑字段和有序物品表。
装备属性数量 ≤8 时内联，更长的序列放入 arena vector。路径用 append_range 批量追加，物品插入后通过返回的迭代器删除。每轮以一次 edit() 容纳全部操作，计时涵盖 pin 的开启和关闭。
原始 `xoffset 4.0` 行保留原 schema 和容器操作，便于在同一进程复测。

可以独立运行 Character 写入、修改、回读 demo，编译只需库头文件。

```sh
c++ -std=c++20 -O2 -I. bench/cppcon2024/character_demo.cpp -o character_demo
checksum=$(./character_demo write character.xwire)
./character_demo modify character.xwire character.changed.xwire "$checksum"
./character_demo verify character.changed.xwire
```

修改进程可以在另一平台运行。回读进程使用首次写入的平台，按独立生成的参考对象校验完整状态。

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

## 结果（云端 x86-64，GCC 16.2，7 个进程取中位数，2026-11-15，next_cpp26 78446f64 = 4.0.0 之后）

“2024” 列是 2024 README 的数换算成每次的纳秒（10 万次的毫秒数 × 10）。2024 的 XOffset 发送+接收
0.5 ns 是测错的：循环第一次就把发送方的 buffer move 走了，之后计的是空 move；2024 FlatBuffers 和
Cap'n Proto 的那一行还把随机数和构建都算在里面。所以 2024 列只用来对照，不能和 4.0 列直接相除。

### Send + receive (ns per message)

| Library | 2024 encode+decode | 4.0 reuse | 4.0 fresh | XOffset ÷ lib (reuse) | XOffset ÷ lib (fresh) |
| --- | ---: | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 0.5 | 101.5 | 283.0 | 1.000 | 1.000 |
| zpp::bits | 4,531.4 | 353.2 | 1,174.4 | 0.287 | 0.241 |
| MessagePack | 9,577.0 | 6,699.9 | 7,619.2 | 0.015 | 0.037 |
| Protocol Buffers | 15,780.6 | 9,462.5 | 15,841.8 | 0.011 | 0.018 |
| FlatBuffers | 6,794.4 | 661.3 | 1,173.9 | 0.153 | 0.241 |
| Cap'n Proto | 2,592.2 | 2,317.3 | 2,772.1 | 0.044 | 0.102 |
| cista raw | 5,315.6 | 720.3 | 849.4 | 0.141 | 0.333 |
| cista offset | 5,845.5 | 595.3 | 685.1 | 0.171 | 0.413 |

Report-only rows:

| Row | reuse | fresh |
| --- | ---: | ---: |
| xoffset 4.0 full | 224.6 | 352.1 |
| FlatBuffers view | 517.5 |  |

### Read/write loop (ns per iteration)

| Library | Containers | 2024 | 4.0 | XOffset ÷ lib |
| --- | --- | ---: | ---: | ---: |
| xoffset 4.0 | XOffset (edit() per iteration) | 498.5 | 301.9 | 1.000 |
| zpp::bits | std::* (shared, items = sorted vector) | 753.2 | 318.9 | 0.947 |
| MessagePack | std::* (shared, items = sorted vector) | 680.4 | 318.9 | 0.947 |
| Protocol Buffers | google::protobuf message | 565.4 | 81.3 | 3.713 |
| FlatBuffers | std::* (shared, items = sorted vector) | 680.4 | 318.9 | 0.947 |
| Cap'n Proto | std::* (shared, items = sorted vector) | 680.4 | 318.9 | 0.947 |
| cista raw | cista::raw::* (items = hash_map) | 655.4 | 170.5 | 1.771 |
| cista offset | cista::offset::* (items = hash_map) | 692.2 | 180.0 | 1.677 |
| xoffset 4.0 plain_root (report-only) | XOffset, plain reference | | 233.4 | |

### Wire size (bytes)

| Library | 2024 raw | 2024 zlib | 4.0 raw | 4.0 zstd-3 |
| --- | ---: | ---: | ---: | ---: |
| xoffset 4.0 | 4,184 | 3,075 | 3,232 | 3,001 |
| zpp::bits | 2,916 | 2,769 | 2,916 | 2,926 |
| MessagePack | 3,222 | 3,064 | 3,542 | 3,391 |
| Protocol Buffers | 4,100 | 3,759 | 4,095 | 3,797 |
| FlatBuffers | 3,672 | 3,247 | 3,680 | 3,128 |
| Cap'n Proto | 3,656 | 3,205 | 3,656 | 3,131 |
| cista raw | 5,392 | 3,294 | 5,392 | 3,540 |
| cista offset | 5,392 | 3,336 | 5,392 | 3,540 |

XOffset after the read/write rounds (report-only): save_verified 8,600 B, save_compact 3,552 B.

云端计时会抖（同一项单个进程之间最多差约 30%），上面取 7 个进程的中位数；比值在 0.95–1.05 之间的
按持平看。

## 怎么读

- **发送+接收**：XOffset 4.0（信任档）reuse 每条约 100 ns，是 zpp::bits 的 0.29、FlatBuffers 的 0.15、
  cista 的 0.14–0.17、Cap'n Proto 的 0.04、MessagePack / Protocol Buffers 的 0.01–0.02。fresh 模式每条要新建
  接收 buffer（新映射），XOffset 约 280 ns，对各家的比值 0.02–0.41。完整校验档 reuse 约 225 ns，仍是
  zpp::bits 的约 0.64。
- **读写循环**：XOffset 和 std 模型持平（0.95），慢于 cista（约 1.7 倍）和 Protocol Buffers（约 3.7 倍）。
  按循环里的三部分拆开量（单个进程，只作说明）：
  - 装备追加再删除：std 每次 `new` 一个属性 vector 约 360 ns；XOffset plain 引用约 145 ns，`edit()` 里约
    240 ns（pin scope 里删掉的块要到 scope 结束才复用，所以每次迭代都新分配）；cista 约 160–195 ns；
    Protocol Buffers 的 `RemoveLast` 会留着清空的对象，下一次 `add_` 直接复用，不分配。
  - 物品插入再删除：扁平 map（XOffset 约 58 ns，std 约 105 ns）要挪动后面的元素，cista 的 hash map 约 16 ns。
  - 路径点：都在 15–30 ns。
  所以读写的差距来自容器种类（扁平 map 对 hash map）和“加了马上删”这个写法正好命中 protobuf 的对象缓存，
  不是 offset 指针本身的开销。`plain_root()` 引用（2024 用裸指针就是这样）约 233 ns，只报告。
- **大小**：XOffset 4.0 的 wire 从 2024 的 4,184 字节降到 3,232 字节（V2 格式），zstd-3 后 3,001 字节，
  和其他格式（2,926–3,797）同一量级。读写循环之后 `save_verified` 带着空洞表是 8,600 字节，`save_compact`
  是 3,552 字节（只报告）。

## 和 2024 的 demo

2024 的另一个 demo（`demo/xdemo.cpp`：一个平台写 `data.bin`，另一个平台读出来接着改）在 4.0 里对应
`examples/demo.cpp`、`examples/helloworld.cpp` 和跨设备互传 demo，这里不重复。
