# XOffsetDatastructure 部署指南（3.1.0）

本文写给把 XOffset 接入服务端、客户端的工程师，说明发布、安全和网络上的前提。
API 用法见 [USER_GUIDE.md](USER_GUIDE.md)，输入边界见
[ALPHA_COMPATIBILITY.md](ALPHA_COMPATIBILITY.md)。

## 1. 适用范围

XOffset 用于**同一份 schema 的 C++ 程序之间**搬运可变状态：服务端之间，以及服务端
与客户端之间。编译器和平台可以不同，schema 必须相同。它不是跨语言协议，也不是
长期存储格式。

服务端与客户端之间的用法目前是目标，对外不宣传。3.1.0 的现状（详见
[CLIENT_TARGETS.md](CLIENT_TARGETS.md)）：

- Windows x64 客户端用 MinGW-w64 GCC 16 编译，全量测试通过，wire 与 Linux 服务端逐字节
  一致；MSVC 和 clang-cl 编不了。
- Android、iOS 客户端编译不了：NDK 和 Xcode 自带的 Clang 都没有反射。客户端的构建
  方式在 3.2 里补。
- 客户端上没有任何性能数据，现有计时都来自 Apple M4。

## 2. 同版本发布

- wire 头里的 schema 指纹必须与接收方类型完全一致（核心承诺 K6），不同就拒绝。
  当前版本不支持 schema 演进。
- 所有互相收发数据的程序（各服务端、各平台客户端）必须用**同一份 schema 定义**
  构建，并一起发布。
- **任何改了 schema 的服务端更新，都必须和客户端同时发版。** 手游客户端要过应用
  商店审核，发版周期要按客户端来排。
- 客户端版本落后时，应在登录或握手阶段用版本号拒绝，不要等到 `load_verified`
  报错。可以把 `wire_schema_hash_v<T>()` 作为握手的一部分，在建立会话时比较。
- 服务端之间滚动升级时，新旧版本不能混用同一类数据。可以分批切换（同一时间只让
  同版本的服务端互相通信），或在切换期间用旧版本程序导出、新版本程序导入。
- schema 演进（新旧版本互通）在 3.2 加入（`next_cpp26`，未发布），见下面 2.1 节。

### 2.1 3.2 起：版本链与三条发版规则

3.2 的服务端可以同时和多个版本的对端收发（[SCHEMA_EVOLUTION.md](SCHEMA_EVOLUTION.md)）：
连接建立时客户端发 `XSchemaHello`，服务端用 `XPeerSchemas` 判定并回 ack，之后按对端
的版本收发。用它时，上面“服务端与客户端同时发版”换成下面三条：

1. **服务端先于客户端发版。** 转换都在服务端，客户端只认识自己的版本。
2. **服务端的版本链覆盖线上还在用的所有客户端版本。** 保留几个由业务定；被移出链的
   版本在握手时得到 `reject_unknown`，客户端据此提示更新。
3. **要能回滚服务端时分两步发。** 先发一个认识新版本、但仍以旧版本工作的服务端，下一次
   再切工作版本。回滚到上一个服务端时，它仍认识已经升级的客户端。

发版时用 `schema_freeze freeze` 冻结当前版本，`schema_freeze check` 放进 CTest，
防止只改字段名不改布局、或忘了把上一版放进版本链。

## 3. 构建一致性

3.1.0 已验证的工具链（本地平台矩阵，3.1.0 RC e913d22，GitHub CI 自 2026-10-23 停用）：

| 用途 | 平台 | 编译器与标准库 |
| --- | --- | --- |
| 服务端 | Linux x86-64、Linux arm64 | GCC 16.2（libstdc++），或 clang-p2996 `d77eff1`（配套 libc++）。两者 wire 逐字节一致，可以混用 |
| 客户端 | Windows x64 | MinGW-w64 GCC 16.2（`tools/build_mingw_toolchain.sh`、`cmake/toolchains/mingw-w64-x86_64.cmake`）。GCC 的 MinGW TLS 是模拟的，每线程缓存在这个目标上关闭（`XOFFSET_THREAD_CACHES=0`） |
| 客户端 | Android、iOS | 3.1.0 无可用工具链 |

- 固定的编译器来源：clang-p2996 见 `toolchains/README.md`，GCC 16 镜像 digest 见
  `tools/local_validate.sh`，MinGW 工具链的构建配方是 `tools/build_mingw_toolchain.sh`。
  MSVC、clang-cl 不支持。
- MinGW 和 MSVC 的 C++ ABI 不同：MinGW 编的 XOffset 模块只能通过 C 接口给 MSVC 编的
  引擎使用。
- 同一可执行文件内，所有翻译单元的头文件版本、编译选项和依赖必须一致。
- 类型签名（K7）：用 `XOFFSET_PIN_SCHEMA` 固定每个收发的根类型
  （[SCHEMA_PINS.md](SCHEMA_PINS.md)），把固定文件放进服务端和每个客户端的构建。任何
  一个构建算出的布局不一致，编译直接失败，不必等到运行时被 `load_verified` 拒绝。
  有意改 schema 时重新生成固定文件，评审时看它的 diff。固定只发现布局变化，版本错位
  仍然按第 2 节处理。
- 只支持 64 位小端平台；32 位安卓渠道包不支持。

## 4. 不可信输入

客户端发给服务端的数据，以及任何来自网络的数据，都按不可信输入处理。

- **一律用 `load_verified`**（或 `TypedXBuffer<T>::load`、`begin_verified_receive`），
  **并且一定设置 `maximum_capacity`**（见第 5 节）。只检查 wire 的实际长度不够：
  稀疏 wire 可以用几百字节声明接近 payload 上限（约 2 GiB）的大小，让接收方按声明
  大小预留并做全图校验，造成资源耗尽。不传上限时，库按默认上限（64 MiB，可用
  `XOFFSET_DEFAULT_MAXIMUM_CAPACITY` 调整）在预留之前拒绝更大的 wire，报错里写明
  声明大小和上限；但默认值是兜底，不是给每个请求的预算，接收不可信数据时要按第 5
  节显式设置。结构安全的承诺（K4）以设置上限为前提。
- `load_unverified` 只用于进程内或已经认证过的数据。
- **校验保证什么**：通过校验后，对根对象图的类型化访问不会越界，结构与 schema 一致
  （截断、越界、重叠、未铺满、非法 bool、乱序集合、schema 不符都会被拒绝）。
- **校验不保证什么**：内容的真实性和完整性。只改动某个整数字段值的数据会被接受。
  wire 头（格式 2.1）没有校验和字段，也没有签名或 MAC。
- **因此必须在外层做**：
  - 来源认证和防篡改：TLS 或消息签名；
  - 业务合法性检查：字段值范围、权限、频率，按普通业务输入处理；
  - 资源上限（见第 5 节）。

### 4.1 发出的 wire 里不带旧数据

wire 是 arena 的原始字节，发给客户端之前要确认里面没有不该发的内容。威胁模型
（`docs/V2_THREAT_MODEL.md`）实测过三类来源，本版本的处理如下：

| 来源 | 本版本的行为 | 你需要做的 |
| --- | --- | --- |
| P1 已释放块（空洞）：字符串换成更长的值、删除元素、vector 迁移后留下的旧块 | 块在释放时清零，arena 里的空洞本身就是 0，所有输出路径照常整段复制 | 转发收到的数据时，发送方 wire 空洞里的字节会原样再发出（那是发送方的字节，不是本端内存）。转发前用 `save_compact` 重建，见 4.2 节 |
| P2 容器的空余容量：`clear()`、`resize()` 变小、`erase`、`pop_back` 之后 `[size, capacity)` 里的旧元素；字符串变短后的旧尾部 | 腾出的字节在缩小时清零（`clear()` 因此是 O(n)） | 无 |
| P3 结构体的隐式填充字节 | 库构造或搬移的容器元素（含 `XMap` 的键值对）在构造时把填充清零，同样内容总是得到同样的 wire | 直接给根对象或其成员赋值的结构体，库碰不到它的填充。发给客户端的 schema 建议加 `XOFFSET_REQUIRE_NO_PADDING(T)`，或全局定义 `XOFFSET_REJECT_IMPLICIT_PADDING`，有隐式填充就编译失败，按报错补显式字段 |

仍然需要注意：

- 同一个 buffer 里**当前还活着**的数据都会发出。服务端内部字段、其他玩家的数据
  不要和下发内容放在同一个 buffer 里，下发前单独构建一个只含可见数据的 buffer。
- 校验只保证结构安全，不检查内容；见上文「校验不保证什么」。

### 4.2 转发客户端上行数据：用 `save_compact` 重建

服务端把客户端 A 的上行数据转给客户端 B（聊天、组队、交易、观战）时，**不要**
`load_verified` 之后直接 `save_verified` 或 `freeze_verified`。校验只检查结构，不检查
空洞和容器余量里的字节是不是 0，所以 A 可以在这些字节里写任意内容，`load_verified`
照样接受，`save_verified` 再把它们原样发给 B。这不是内存安全问题，是一条绕过业务
检查的夹带通道。

转发前用 `save_compact` 重建：

```cpp
XBuffer in = XBuffer::load_verified<ChatMsg>(wire_from_a, XBuffer::maximum_capacity(1u << 20));
// ……业务检查……
std::string to_b = in.save_compact<ChatMsg>();   // 不要 in.save_verified<ChatMsg>()
```

`save_compact` 按对象图重新写出 wire，A 能写的位置里它会清掉这些：

- 空洞：不写出；
- 容器余量（`[size, capacity)`）：按新的容量重新分配，余量写成 0；
- 字符串长度之后的字节：清零（内联字符串的这部分，校验本来就要求是 0）。

它**管不到**两处，要另外处理：

- **结构体的隐式填充**：根对象和容器元素按字节整体复制，填充里的字节会原样带过去，
  而校验不检查填充。转发的类型要没有隐式填充：给转发的根类型加
  `XOFFSET_REQUIRE_NO_PADDING(T)`（连同它包含的记录和容器元素一起检查），或全局定义
  `XOFFSET_REJECT_IMPLICIT_PADDING`，有填充就编译失败，按报错补显式字段。
- **字段值本身**：标量、字符串内容都是 A 发的内容，`save_compact` 照原样转发。按
  普通业务输入检查（长度、范围、敏感词等）。

只读的转发可以传 `XCompactOptions{.slack_div = 0}`，wire 最小；B 会接着修改时用默认余量。

## 5. 容量与资源上限

`maximum_capacity` 同时决定每个 buffer 预留的虚拟地址空间。默认 64 MiB（或初始
长度，取较大者）；payload 的硬上限是 `0x7ffffff8`（约 2 GiB），这不是建议给每个
请求的预算。

| 场景 | 建议 | 依据 |
| --- | --- | --- |
| 服务端接收客户端数据 | 默认 `maximum_capacity` 设为 **1 MiB**；读取 wire 前先检查长度，超过就丢弃 | 客户端上行消息多为 KB 级；按业务最大合法消息的 2 倍向上调整 |
| 服务端之间 | 按该类状态的最大合法大小设上限（单块状态可以到数百 MB） | 服务端间数据视为已认证，但仍需防止异常数据拖垮进程 |
| 客户端接收或构建 | 设较小的 `maximum_capacity`（按最大下发消息的 2 倍）；频繁创建、销毁 buffer 时用 `XBufferPool` 复用空闲预留 | 每个存活的 buffer 默认预留 64 MiB 地址空间，池只复用空闲预留、不减少存活 buffer 的预留；移动端地址空间有限，iOS 上的实际上限还没有实测 |

另外：

- 单个 buffer 合法不等于总资源受控：还要限制并发 buffer 数、队列长度、总内存或
  虚拟地址预算，以及单个请求的处理时间。
- 每条 wire 有一个 40 字节的头，每个 buffer 有一个虚拟内存预留。百字节级的高频小消息
  成本没有测过，这类场景先用实测确认。
- 上表中的 1 MiB 和「2 倍」是建议起点，不是测量结果，接入时按业务数据调整。

## 6. 网络传输

- wire 是原始内存布局，比 varint 类格式大。SLG M 规模第 20 周期的玩家状态消息：
  3.1.0 稠密 wire 13,667 B，FlatBuffers 8,423 B；protobuf 约 4.5 KB（2026-10-21 原型）。
  大出来的主要是容器余量和空洞；只读下发用 `save_compact` 加
  `XCompactOptions{.slack_div = 0}` 可以去掉，估算与 FlatBuffers 持平（3.1.0 上没有实测）。
- 服务端机房内的链路通常不需要压缩。
- **客户端链路（尤其移动网络）建议在外层加 zstd 压缩**，或 lz4（更快、压缩率较低）。
  压缩在 `save_verified` 之后、`load_verified` 之前做，不影响库的校验。解压时同样
  要限制解压后的长度，防止压缩炸弹。
- 压缩后的大小、压缩耗时没有测过，接入时按实际数据实测并选择压缩级别。
- 数据只能整块发送，没有增量同步；需要增量时由上层协议拆分 schema。

## 7. 接入检查清单

- [ ] 所有收发端使用同一份 schema；改 schema 时服务端和客户端同时发版（3.2 起可改用 2.1 节的版本链和三条规则）
- [ ] 握手阶段比较版本或 schema 指纹（3.2 起用 `XSchemaHello` / `XPeerSchemas`）
- [ ] 每个平台使用第 3 节表里已验证的工具链构建，且只面向 64 位小端平台
- [ ] 收发的根类型都用 `XOFFSET_PIN_SCHEMA` 固定，固定文件进了服务端和每个客户端的构建
- [ ] 网络输入只走 `load_verified` 路径，并设置了 `maximum_capacity`
- [ ] 下发给客户端的 schema 没有隐式填充（`XOFFSET_REQUIRE_NO_PADDING` 或 `XOFFSET_REJECT_IMPLICIT_PADDING`），下发 buffer 只含客户端可见的数据
- [ ] 服务端接收客户端数据时有 wire 长度上限（建议起点 1 MiB）
- [ ] 转发客户端上行数据前用 `save_compact` 重建，转发的类型没有隐式填充（4.2 节）
- [ ] 客户端用较小的 `maximum_capacity` 或 `XBufferPool`
- [ ] 外层有认证（TLS 或签名）和业务字段检查
- [ ] 限制了并发 buffer 数和总内存预算
- [ ] 客户端链路评估过 zstd/lz4 压缩，并限制了解压后长度
