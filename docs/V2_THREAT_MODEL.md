# V2 威胁模型（4.0.0；文中「0.3.0-beta.1」指该版起的状态）

本文说明 XOffset V2 在接收**不可信 wire** 时防住了什么、没防住什么，以及调用方必须
在外层补上什么。格式细节见 [V2_FORMAT.md](V2_FORMAT.md)，接入要求见
[DEPLOYMENT.md](DEPLOYMENT.md)。文中的实测数据都可以用第 6 节的方法复现。

## 1. 保护对象与攻击者

**保护对象**

- 接收方进程的内存安全：读取通过校验的数据时，不越界、不读写 arena 以外的内存。
- 接收方进程的可用性：一条消息消耗的内存、地址空间和 CPU 有界。
- 发送方数据的保密性：wire 里只出现发送方当前对象图里的内容。

**攻击者**

| 编号 | 攻击者 | 能力 |
| --- | --- | --- |
| A1 | 任意网络发送者（例如恶意客户端） | 完全控制发给服务端的 wire 字节，知道 schema 和公开的 schema 指纹 |
| A2 | 被攻破的对端服务端 | 同 A1，并且能发很大的消息 |
| A3 | 链路上的中间人（没有 TLS 时） | 读取和改写 wire |
| A4 | 收到服务端数据的客户端 | 读取发给自己的 wire 的全部字节 |

不在范围内：与接收方同进程的攻击者、侧信道、编译器或标准库自身的缺陷。

## 2. 入口

不可信字节只能从下面的接口进入，它们的校验强度相同：

- `XBuffer::load_verified<T>`、`TypedXBuffer<T>::load`；
- `begin_verified_receive<T>` / `finish_verified`；
- `receive_raw::validate<T>`（内部接口）；
- 3.2 schema 演进：`XPeerSchemas::load_verified<W>`、`evolve_load_verified<W>`（按 wire
  头选出链上的版本，再走上面的 `load_verified`）、`XPeerSchemas::accept` 和
  `read_schema_ack`（握手消息，同样走 `load_verified`，另有 64 KiB 和条目数上限）。

- 重新加载进已有 buffer：`reload_verified` / `try_reload_verified`、`begin_verified_reload`
  （直接收进已有 buffer 的存储），以及可搬槽位存储（R1）上的同样入口；
- `load_verified_with_resource<T>` / `load_verified_with_scratch<T>`（校验用的位图来自调用方给的
  内存资源，校验本身与 `load_verified` 相同）；
- 客户端构建：以上入口在 C++20 字段表构建和关异常构建（`try_` 入口）里各是一份独立编译的代码。

`load_unverified` 不校验，**不是**不可信输入的入口。

接收检查分三档（2026-11-04 定，信任档和边界档的接口 3.4 加入）：信任档只查 wire 头准入和空洞表边界，
用于服务端之间、服务端发往客户端和自己的存档，**不是**不可信输入的入口（调试和 sanitizer 构建里照样
完整校验）。整块接收和增量的信任档是 `XReceiver<XLink::server_peer / from_server>::reload / load /
apply_delta`（`xoffset_receiver.hpp`），`XReceiver<XLink::from_client>` 在编译期拒绝信任档；把客户端连接误建成可信
链路的接收器靠代码审查发现。客户端是自己的代码、链路受保护的部署，可以用 `XOFFSET_CHECK_UPLINK=0` 编译
（2026-11-17 起的开关，默认 1，见 4.1）：这时客户端链路也默认信任档，只有调用处写明 `XCheck::bounds` /
`XCheck::full` 的才检查。信任档按定义不接不可信输入，不保证对任意输入给出可用的结果；fuzz 和
`test_receiver` 的差分测试只要求两件事：它自己的解析和写入对任何输入都不越界（ASan），对完整档接受的
输入给出相同结果（第 5 节表中的 admit 一行）。边界档（3.5）用于客户端上行：`XReceiver<XLink::from_client>::view`
在 wire 字节上给出只读视图 `XView<T>`，只有 const 访问。它保证的是：任何 const 访问都不读出 wire 之外，
遍历的工作量不超过负载大小的线性倍数（块可以重叠，所以按负载大小限遍历的元素字节总数），bool 不会是
0、1 以外的值（避免未定义行为）。它不保证值有意义：块重叠、set/map 键乱序、填充字节非零的输入可能被接受，
读出来的是垃圾值或查找不到。要原地修改客户端数据时走完整档，即上面这些入口。harness 见第 5 节表中的
bounds 一行。

## 3. 威胁与现状

| 编号 | 威胁 | 现状 | 依据 / 缺口 |
| --- | --- | --- | --- |
| T1 | 构造的 wire 让接收方越界读写 | **已防住**（用 `XOFFSET_CHECK_UPLINK=0` 编译的服务端对客户端上行不防，见 4.1） | 完整校验（V2_FORMAT 第 5 节）；Clang 与 GCC 16 的 ASan+UBSan CI；每个 PR 上的 libFuzzer（多 schema，第 5 节）；发布前 GCC 16 ASan+UBSan 差分 fuzz 480 万例无分歧。缺口：每日长时间 fuzz 尚未运行（第 5 节 F4） |
| T2 | 篡改字段值（A3） | **不防** | 只改标量值的 wire 能通过校验；wire 头没有校验和（格式 2.1 删除了固定为 0 的 `crc32c`），没有 MAC。外层必须用 TLS 或消息签名 |
| T3 | 伪造 schema 身份 | 不构成越权 | schema 指纹（64 位 FNV-1a）是一致性检查，不是认证：攻击者照抄指纹也只能让接收方按**接收方自己的类型**做完整校验 |
| T4 | 资源耗尽 | **默认有界**（M1：不传上限时默认 64 MiB） | 见 3.1；服务端接收客户端数据仍应设更小的上限（DEPLOYMENT 第 5 节） |
| T5 | 已删除数据的旧字节随 wire 发出（A4） | **部分防住**（2026-11-16 起，性能优先）；`XOFFSET_HARDENED=1` 或 `save_compact` 时基本防住 | 释放的块仍然清零（P1，M3）。默认构建不清零容器腾出的字节（P2）和填充字节（P3），`save_verified`、`freeze_verified` 和增量同步会把它们原样发出；2026-11-17 起 `reset` 也不清零，复用的 buffer 会把上一份内容的字节带进下一份内容的块余量和填充里（P4）。`XOFFSET_HARDENED=1` 打开 M4、M6（4.0.0 及之前的默认行为）；`save_compact` 在任何构建下都不带容器余量里的旧字节。见 3.2 和 4.1 |
| T6 | 不同编译器对同一 wire 的理解不同 | 已发现一处并修复 | enum 键只定义 `operator<=>` 时，Clang 和 GCC 的集合顺序相反，两边互相拒收；修复和 CI 互读作业见 GCC 16 移植 PR |
| T7 | 业务层非法值（负数金币等） | **不防** | 不属于结构校验，按普通业务输入检查 |
| T8 | 用旧版本 wire 放大服务端开销（3.2 schema 演进） | **有界** | 攻击者可以选链上最旧的版本，让服务端多做一次完整校验加每级一次转换；转换后的对象可能比 wire 大（新增字段、更宽的类型）。转换目标 buffer 的上限默认等于传给 `load_verified` 的 `maximum_capacity`，超过即失败。按链长估算放大倍数，必要时缩短链。`fuzz_schema_evolution` 覆盖这条入口 |

### 3.1 资源耗尽（T4）

**R1 稀疏 wire 放大。** 稀疏 wire 的空隙不占 wire 字节，只要求落在空洞里。一个
184 字节的 wire 可以声明接近 2 GiB 的 `used_bytes`：

| wire | 声明的 used_bytes | 不设上限 | 设 `maximum_capacity(1 MiB)` |
| ---: | ---: | --- | --- |
| 184 B | 64 MiB | 接受，预留 64 MiB，4 ms | 拒绝 |
| 184 B | 1 GiB | 接受，预留 1 GiB，67 ms | 拒绝 |
| 184 B | 2047 MiB | 接受，预留 2 GiB，140 ms，RSS +8 MiB | 拒绝 |

不设上限时，`load_verified` 的预留等于 wire 声明的大小；`begin_verified_receive`
不传上限时同样以声明的大小为准。后果：

- 服务端：每条 184 字节的消息占 2 GiB 地址空间和约 140 ms CPU。并发几十条就可能
  耗尽提交额度（`vm.overcommit_memory=2` 时直接失败）或拖慢服务。
- 客户端（尤其 iOS）：地址空间预算小，几条这样的消息就可能让进程崩溃。
- **只检查 wire 长度不够**：184 字节远小于任何长度上限。

**R2 空洞表放大。** 空洞表每项 8 字节，加载时每项都进入宿主侧空闲表，校验时还要
登记到字节索引：

| 声明的 used_bytes | 空洞大小 | wire | 加载耗时 | 空闲块数 | 峰值 RSS |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 GiB | 4096 B | 2 MiB | 182 ms | 262,144 | 141 MiB |
| 1 GiB | 64 B | 128 MiB | 797 ms | 16,777,215 | 267 MiB |
| 1 MiB | 8 B | 1 MiB | 5 ms | 131,063 | 5 MiB |

宿主内存和耗时随 `used_bytes` 和空洞数线性增长，不会超线性。设置
`maximum_capacity` 后，二者都被上限约束（1 MiB 上限时峰值约 5 MiB）。

**R3 其他。** 并发 buffer 数、队列长度、解压后长度、单请求耗时都在库外，由调用方
限制（DEPLOYMENT 第 5、6 节）。

**结论：** 上面的数据是缓解前的实测。0.3.0-beta.1 起，不传上限时按默认上限（64 MiB，
M1）在预留前拒绝，R1、R2 的放大被约束在默认上限内；超限的报错写明声明大小和上限
（M2）。对客户端上行数据，64 MiB 仍然偏大，应按 DEPLOYMENT 第 5 节设更小的上限。

### 3.2 旧内容泄露（T5）

下表是缓解前的实测：当时库不清零不再使用的字节，而 wire 是 arena 字节的原样复制，
所以有三条泄露途径。0.3.0-beta.1 的处理见表后的「现状」。

| 途径 | 例子 | 实测 |
| --- | --- | --- |
| P1 已释放块（空洞） | 字符串换成更长的值、删除含堆字符串的元素、vector 迁移后，旧块进入空闲表，内容不变 | 旧字符串（`SECRET-TOKEN-…`、`TOPSECRET-player-name`）出现在 wire 里，并且经过 `load_verified` 再保存后仍然保留 |
| P2 容器的空余容量 | `vector.clear()`、`resize()` 变小之后，`[size, capacity)` 里还是旧元素 | 旧元素字节出现在 wire 里 |
| P3 对象的填充字节 | 带隐式填充的结构体（如 `{uint8_t; uint64_t;}`）。从空闲块重新分配，或从栈上临时对象复制进来时，填充字节会带上旧的 arena 内容或栈内容 | 已实测（2026-10-21，Mac）：`alignas(8)` 的 `Troop` 数据 12 字节、`sizeof` 16，`troops.push_back({1, 2, 3})` 的临时对象带 4 字节不确定的填充，被原样写进 wire；同一份数据的两次构建在偏移 3884 处相差 1 字节（2 对 0）。加显式 `std::int32_t pad{};` 后完全一致。所以 P3 既会让 wire 不确定，也会带出发送方栈上的字节 |

字符串被更短的值覆盖时，实测旧尾部没有发出。

缓解前的后果：服务端发给客户端的 wire，可能带有这个 buffer 里**曾经存过、已经删掉**
的数据（其他玩家的信息、服务端内部字段等），P3 还可能带出服务端栈上的数据。

**现状（2026-11-16 起）：** susu 决定性能优先，P2、P3 两项清零改成编译开关，**默认关闭**；
2026-11-17 起重置时的清零（下面的 P4）也是。
`XOFFSET_HARDENED=1` 全部打开（4.0.0 及之前的默认行为），也可以单独打开（4.1 节）。
默认构建下，`save_verified`、`freeze_verified` 和增量同步发出的字节，可能带有容器里已经
删掉的元素，以及这个 buffer 重置之前存过的内容。`save_compact` 不受影响：它不写空洞，余量和字符串尾部重写为 0。

- P1：块在释放时清零（M3），arena 里的空洞就是 0，所有输出路径仍是原样复制。这一项
  **不是开关**：从 touch 标记生成的增量只在空洞出现或消失时发送它的字节，收发两端都依赖
  空洞里是 0，关掉会让副本和发送方的字节不一致。
- P2：默认不清零，`clear()`、`resize()` 变小、`erase`、`pop_back()`、赋更短的值之后，
  旧元素和旧字符留在 `[size, capacity)` 里，直到下一次在这个位置追加时被覆盖（字符串
  仍然写结尾的 NUL）。被删元素自己拥有的块照常释放并清零（M3），所以留下的是元素本身的
  内联字节（标量字段、内联数组、不超过 10 字节的内联字符串）和堆字符串缩短后的旧尾部。
  `XOFFSET_SCRUB_ERASED=1` 清零 `clear`、`resize`、`erase` 和赋值腾出的字节，
  `XOFFSET_SCRUB_POPPED=1` 清零 `pop_back` 删掉的元素（M4）。
- P3：默认不清零，带隐式填充的容器元素会带上构造它的那个值的填充字节，通常是发送方
  栈上的字节；同样的内容两次构建，wire 也可能不同。`XOFFSET_SCRUB_PADDING=1` 时库构造
  或搬移的容器元素（含 map 条目）填充字节清零（M6）。更省事的办法是让 wire 类型没有
  隐式填充：用 `XOFFSET_REQUIRE_NO_PADDING(T)` 或 `XOFFSET_REJECT_IMPLICIT_PADDING` 在
  编译期发现，改成显式字段。调用方直接赋值的带填充记录，库在任何构建下都无法触及。
- P4 重置（2026-11-17 起默认不清零）：`reset<T>()` 只把 buffer 标成空的，上一份内容的字节
  留在新的 `used_end` 之后。新内容的块从这段区域里分配，写入的值覆盖旧字节，但块里没被
  写到的部分（vector 的余量、字符串结尾之后到块尾、记录的隐式填充）仍是上一份内容。
  所以**轮流给不同接收方构建消息的同一个发送 buffer，会把上一个接收方的消息内容夹在
  下一条 wire 里**（2026-11-16 实测：同一个 buffer 轮流构建 40 条消息，3 条带出了上一条的
  内容）。`XOFFSET_SCRUB_RESET=1` 时 `reset` 把用过的字节全部清零（4.0.0 及之前的行为）。
  `save_compact` 在任何构建下都不带这些字节。库内部仍然保持「某个偏移之后全是 0」：没清零的
  范围记在 buffer 上，原地重载和增量接收照常工作。
- 转发：load 进来的 wire 里的空洞和容器余量（`[size, capacity)`）保留发送方字节，
  校验不要求它们是 0，load 后直接 save 会原样转发。这不是接收方自己的内存，但恶意
  发送方可借此给下游夹带字节。转发前用 `save_compact` 重建：它不写空洞、余量重写为 0、
  字符串尾部清零；记录的隐式填充仍按字节复制，所以转发的类型要没有隐式填充
  （[DEPLOYMENT.md](DEPLOYMENT.md) 4.2 节）。

回归测试：`test_wire_hole_zeroing`（P1、P2、转发、M1/M2）、`test_implicit_padding`（P3）、
`test_reset_used_range`（P4）。
两个测试各构建两次：默认构建检查空洞为 0、`save_compact` 不带旧字节、内容正确；
`XOFFSET_HARDENED=1` 构建（`*_hardened`）检查 wire 里没有任何旧字节、填充为 0、wire 确定。

## 4. 缓解措施（0.3.0-beta.1 全部实施）

M1–M6 已在 0.3.0-beta.1 实施，行为变化见 CHANGELOG。M3 的修改路径开销实测约 +5%。
2026-11-16 起 M4 和 M6 的清零默认关闭，2026-11-17 起重置清零也默认关闭，
都由 4.1 节的开关打开；M1、M2、M3、M5 不变。

| 编号 | 措施 | 影响 |
| --- | --- | --- |
| M1 | `load_verified` 和 `begin_verified_receive` 不传上限时，用默认上限（64 MiB，可用 `XOFFSET_DEFAULT_MAXIMUM_CAPACITY` 调整），超过就在预留前拒绝；需要更大的 wire 时显式传 `maximum_capacity` | 公开 API 行为变化：大于 64 MiB 的 wire 必须显式传上限。读路径性能不变 |
| M2 | 上限不足时的报错改为 `used_bytes … exceeds maximum_capacity …`（现在是笼统的 `XOffset arena allocation failed`） | 只改诊断 |
| M3 | 块释放时清零，空洞在 arena 里就是 0（P1），输出路径不变 | 改变含空洞 wire 的字节（golden 要更新）；开销在释放路径，与释放的字节数成正比。load 进来的 wire 空洞保留发送方字节，load 后直接 save 会原样转发（不泄漏本端内存，但恶意发送方可借此给下游夹带字节）。最初实现是输出时按空洞补零，实测正式输出慢约 22%，故改为释放时清零 |
| M4 | （默认关闭）容器缩小时把腾出的元素字节清零（P2）：`XOFFSET_SCRUB_ERASED=1` 管 `clear`、`resize` 变小、`erase`、赋更短的值，`XOFFSET_SCRUB_POPPED=1` 管 `pop_back` | 开销只在修改路径，与删除的字节数成正比；打开后 `clear()` 是 O(n) |
| M5 | 文档：DEPLOYMENT 写明「只检查 wire 长度不够，必须设上限」，以及 P1–P3 | 无代码影响 |
| M6 | 填充字节（P3）：编译期检查开关（`XOFFSET_REQUIRE_NO_PADDING`、`XOFFSET_REJECT_IMPLICIT_PADDING`），schema 有隐式填充就报错，让用户改成显式字段；`XOFFSET_SCRUB_PADDING=1`（默认关闭）时库在构造或搬移容器元素时按填充位置清零 | 编译期检查没有运行时开销；清零的开销见 4.1 |

### 4.1 加固开关（2026-11-16 起默认关闭）

下面的措施花在修改和发送的热路径上，防的是旧字节随 wire 发出（T5 的 P2、P3、P4），
或者调用方自己的用法错误。susu 2026-11-16 决定性能优先，2026-11-17
确认重置清零也在内：每一项都是编译开关，默认 0；`-DXOFFSET_<名字>=1`
打开一项，`-DXOFFSET_HARDENED=1`（CMake 选项 `XOFFSET_HARDENED=ON`）打开所有没有单独
指定的项。开关改变内联函数的行为，同一个程序的所有翻译单元要用同样的值。

| 开关 | 打开后 | 默认（关闭）时 | 关闭省下的指令数 |
| --- | --- | --- | --- |
| `XOFFSET_SCRUB_POPPED` | M4：`pop_back()` 把删掉的元素清零 | 元素的内联字节留在余量里，下一次追加时覆盖 | CppCon 2024 调优布局读写 7.6%；SLG UP.apply 1.9% |
| `XOFFSET_SCRUB_ERASED` | M4：`clear()`、`resize()` 变小、`erase`、赋更短的值，把腾出的字节清零 | 旧元素、旧字符留在余量里 | CppCon 调优布局读写再少约 4%；SLG LD.apply 3.8%、LDW.recv 3.4%；原生增量字节 NB.delta 少 9.9%（清零也是改动，要随增量发出） |
| `XOFFSET_SCRUB_PADDING` | M6：库构造或搬移的容器元素，隐式填充清零 | 填充带着构造它的值里的字节（通常来自栈），同样内容的 wire 可能不同 | SLG A.apply 1.6%、SD 1.3% |
| `XOFFSET_PIN_QUARANTINE` | `edit()` / `read()` 作用域里回收的块，到作用域结束才复用 | 块立刻可以复用。作用域里一个已经失效的引用（容器扩容或删除之前取的）可能读到后来写进去的别的数据；失效规则本身和 `std::vector` 一样 | CppCon 2024 布局读写 12.6%（计时约 18%）；SLG 各格不到 0.5%。反过来的一面：块立刻复用后 buffer 更小，但空洞可能更碎。CppCon 2024 的角色数据在一次 `edit()` 里填好后，wire 从 3232 字节、1 个空洞变成 3112 字节、2 个空洞，收发一轮多 123 条指令（9%） |
| `XOFFSET_SAVE_SELF_CHECK` | 小消息（4 KiB 以下）每次保存前，再查一次 arena 头的魔数和大小，以及输出 `std::string` 不和 buffer 自己的存储重叠 | 这两项是发送方自查：把 buffer 自己的存储当输出传入是未定义行为；arena 头被外部写坏时不报错。较大的消息走通用路径，两项检查仍然做 | SLG UP.send 5.7%、UC.send 1.8% |
| `XOFFSET_SCRUB_RESET` | `reset()` 把用过的字节全部清零，重置后的 buffer 和新建的逐字节相同 | `reset()` 不清零。上一份内容留在新 `used_end` 之后，新内容的块余量和填充里带着它；轮流给不同接收方构建消息的发送 buffer 会把上一条消息的字节带进下一条（3.2 的 P4） | SLG UP.send 4.1%、UC.send 2.6%、EB.send 2.4%、SD 2.2%、LD.send 2.0%；单独一次 `reset`（`reloc.reset`）43%。buffer 之间交换或移动时要多带一个字段：每次 `std::swap` 多 9 条指令、每次移动多 3 条 |

数字是 P2996 Clang x86-64 上 callgrind 的指令数（2026-11-16、2026-11-17，
`relax-survey-20261116/`）。

**默认打开的一项：客户端上行检查 `XOFFSET_CHECK_UPLINK`（默认 1，`XOFFSET_HARDENED` 不影响它）。**
`XReceiver<XLink::from_client>` 默认检查：`view()` 走边界档，`reload()` / `load()` / `apply_delta()`
走完整档，在这条链路上写 `XCheck::admit` 编译不过。用 `-DXOFFSET_CHECK_UPLINK=0` 编译时，客户端链路
和可信链路一样默认信任档，只查 wire 头和根在负载内，**任何客户端发一条构造的消息，就能让服务端越界读
（`view`）或越界写（修改 `reload` 的结果），崩溃或读到、改到服务端内存**；调用处写明 `XCheck::bounds` /
`XCheck::full` 的不受影响。它只适合客户端也是自己的代码、链路受保护的部署。实测省得不多：SLG 上行指令
UL.recv 少 9.6%（每条指令约 46 条机器指令），所以 susu 2026-11-17 决定这一项默认打开。

不在开关里、保持不变的：

- 块释放时清零（M3）：从 touch 标记生成的增量依赖「空洞是 0」，见 3.2。关掉 M3 实测只省
  LD.apply 1.5%、LDW.recv 1.3% 的指令，而且增量同步会出错。
- 可信链路仍是线头检查；任何链路都可以在调用处写明 `XCheck::bounds` 或 `XCheck::full`。
- M1 默认上限、参数检查、`XOFFSET_FORCE_FULL_CHECK`（调试构建和 sanitizer 下默认 1，这时
  信任档照样跑完整校验）。

发给不该看到已删数据的接收方时，两个办法任选：用 `XOFFSET_HARDENED=1`（或其中的四个
清零开关 `XOFFSET_SCRUB_ERASED`、`XOFFSET_SCRUB_POPPED`、`XOFFSET_SCRUB_PADDING`、
`XOFFSET_SCRUB_RESET`）编译，或者这条路径用 `save_compact` 发送。

## 5. 持续 fuzz

现状（2026-11-04 起，susu 11-04 06:15Z）：每晚长跑自 2026-10-28 停，不再恢复；改为**每次发版前**
在云端跑一轮 `tools/fuzz_prerelease.sh`（[RELEASING.md](RELEASING.md) 第 3 节），覆盖第 2 节的
每个入口，无发现才发版。入口与 harness 的对应：

| 入口 | harness（ASan+UBSan） |
| --- | --- |
| `load_verified`、`TypedXBuffer<T>::load`、`try_load_verified`、`load_verified_with_resource` | `fuzz_v2_wire`、`fuzz_wire_schemas`（libFuzzer）；`test_v2_fuzz`（GCC 16 差分，带独立 oracle） |
| `begin_verified_receive` / `finish_verified`、`receive_raw::validate` | 同上（`v2_fuzz_core.hpp` 的 receive、raw 两列，与 oracle 比较） |
| `reload_verified`、`begin_verified_reload`、可搬槽位存储 | 同上（reload、direct、slot 各列，与新 buffer 的加载逐字节比较） |
| schema 演进：`evolve_load_verified`、`XPeerSchemas` 握手、`read_schema_ack` | `fuzz_schema_evolution` |
| 增量 apply：`try_apply_delta_verified` / `apply_delta_verified`（3.3） | `fuzz_delta_sync`（反射和字段表两种构建）：快路径（none、in_place）与克隆副本上的全量校验比较结论、错误和结果；拒绝时副本和游标不变；接受的结果能再校验加载 |
| 信任档（3.4）：`XReceiver<server_peer / from_server>` 的 `reload`、`load`、`apply_delta`（admit） | 整块：`v2_fuzz_core.hpp` 第 8 列（`fuzz_v2_wire`、`test_v2_fuzz`），完整档接受的必须接受且结果逐字节相同，oracle 在线头阶段拒绝的必须拒绝；增量：`fuzz_delta_sync` 在另一份克隆上走 admit，完整档接受的必须接受且结果相同，解析阶段的错误必须相同，拒绝时副本和游标不变。admit 多接受的输入不读 |
| 共享增量（3.5）：`XReceiver<server_peer / from_server>::apply_sync`（4.0 起 XS 2.0，full 和 admit） | `fuzz_shared_delta`（反射和字段表两种构建）：原始根记录和操作段、真实消息流加篡改、整条消息、关键消息四种输入；每条先走 full，拒绝时记录和游标不变，接受的结果能再校验加载；full 接受的再在副本上走 admit，记录和游标必须相同。admit 不读 full 拒绝的输入（消息体信任发送方） |
| 边界档（3.5）：`XReceiver<from_client>::view`（bounds） | `v2_fuzz_core.hpp` 第 9 列（`fuzz_v2_wire`、`test_v2_fuzz`）：wire 复制进一个大小正好的堆块，完整档接受的稠密 wire 必须接受且读出的每个字段相同，稀疏 wire 和 oracle 在线头阶段拒绝的必须拒绝；多接受的输入通过视图读完全部字段，不能越界（ASan）。`test_receiver_view`：构造的越界块、超长内联字符串、缺 NUL、bool 2、重叠放大和随机变异 |
| 客户端字段表构建、关异常构建 | `fuzz_v2_wire`、`fuzz_wire_schemas` 的 C++20 字段表构建（libFuzzer），`tools/fuzz_outcome_diff.sh`（三种构建逐条比较结果和错误文字） |

各阶段与预算见脚本开头的说明；一轮发版 fuzz 约 4 小时（GCC 45 分钟，每个 libFuzzer 目标每种构建
20 分钟）。下面是更早的记录。

现状（0.3.0-beta.1）：F1–F3 已实现，F4 的 PR 短跑已在 CI 运行；每日长跑的 workflow
已写好，但 GitHub 定时任务只在默认分支触发，目前没有运行，发布前以一次 480 万例的
GCC 16 ASan+UBSan 差分 fuzz 代替。

现状（beta.2，2026-10-23 起）：项目不再使用 GitHub Actions，`fuzz.yml` 和 `ci.yml` 保留但不运行。
每日长跑改在云端 Docker 主机上执行 `tools/fuzz_nightly.sh`（北京时间每天 02:23）：

- GCC 16 ASan+UBSan 差分 fuzz：`test_v2_fuzz` 4 个进程，seed 按日期轮换
  （`YYYYMMDD×10+i`），用例数按约 45 分钟标定；失败时写出 wire 和 `--replay` 命令。
- GCC 16 ASan+UBSan 回放：`fuzz_<target>_replay` 回放 `tests/fuzz/regressions/` 和两个
  harness 的种子语料。
- 发现崩溃时，`tools/fuzz_report.py` 按签名开 GitHub issue（标签 `fuzz-crash`，同一签名只
  追加评论，已关闭的重新打开并标注回归），并把输入推到 `fuzz-crash/<签名>` 分支，作为
  `tests/fuzz/regressions/<target>/<签名>`。修复 PR 带上这个输入，issue 由修复关闭。没有发现
  却失败的运行记在唯一的 `fuzz-infra` issue 上。结果同时写进项目文件。
- libFuzzer 覆盖率引导 fuzz（F1、F2，2026-10-27 起）：同一晚接着执行
  `tools/fuzz_libfuzzer.sh`，用云端自编的 P2996 工具链（clang-p2996 d77eff1，带
  compiler-rt 的 libFuzzer 运行库）在主机上原生构建 `fuzz_v2_wire` 和 `fuzz_wire_schemas`
  （ASan+UBSan），步骤与 `fuzz.yml` 的 libfuzzer 作业相同：先单进程回放已有语料和种子，
  再 `-fork=4` 跑约 20 分钟，然后合并语料、复现并最小化发现。语料以 tar 包跨夜保存在
  项目文件里；调试信息里的路径映射为 `/workspace`，签名与 Docker 作业一致。首晚即发现
  #130（harness 对短于头部的输入检查过严，已修复，输入入回归目录）。
- 客户端构建（2026-10-28 起）：同一晚在 C++20 字段表构建上跑 libFuzzer（`v2_wire`、
  `wire_schemas`），再用 `tools/fuzz_outcome_diff.sh` 让反射、字段表、关异常三种构建回放
  同一批语料，逐条比较各入口的结果和错误文字。
- 仍有的缺口：没有 PR 短跑（PR 的合并证据是 `tools/local_validate.sh`）。

- **F1 覆盖率引导 fuzz**：libFuzzer harness（`LLVMFuzzerTestOneInput`），入口为
  `load_verified`、`finish_verified`、`receive_raw::validate`，用 P2996 Clang 和 CI 里已经
  固定的 compiler-rt 构建；种子用跨编译器 wire 语料。
- **F2 多 schema**：除现有的差分 fuzz schema，加入跨编译器语料的 schema（enum 键、
  定长数组、浮点、bool、嵌套 vector）。
- **F3 资源断言**：harness 里设 `maximum_capacity`，并断言单例的预留和峰值内存不超过
  上限的固定倍数，把 R1/R2 变成可回归的检查。
- **F4 运行方式**：PR 上每次跑短时间（例如 5 分钟），每天定时跑长时间；崩溃输入
  自动加入回归语料。

## 6. 复现方法

实测环境：云端 x86-64 容器（Intel Xeon 2.1 GHz，4 核），GCC 16.2.0，`-O2`，单次运行，
不是正式性能测量。做法：取一个合法的小 wire，把 `used_bytes` 和 payload 头的
`used_end` 改大，加一个覆盖新增区间的空洞（R1），或用许多小空洞铺满（R2），改成
单个范围的稀疏 wire 后调用 `TypedXBuffer<T>::load`，记录耗时、`reserved_bytes()` 和
`/proc/self/status` 的 RSS。T5 的做法：替换字符串或迁移 vector 后保存，在 wire 中
搜索旧内容。
