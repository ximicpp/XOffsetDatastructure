# V2 威胁模型（3.1.0；文中「0.3.0-beta.1」指该版起的状态）

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
- `receive_raw::validate<T>`（内部接口）。

`load_unverified` 不校验，**不是**不可信输入的入口。

## 3. 威胁与现状

| 编号 | 威胁 | 现状 | 依据 / 缺口 |
| --- | --- | --- | --- |
| T1 | 构造的 wire 让接收方越界读写 | **已防住** | 完整校验（V2_FORMAT 第 5 节）；Clang 与 GCC 16 的 ASan+UBSan CI；每个 PR 上的 libFuzzer（多 schema，第 5 节）；发布前 GCC 16 ASan+UBSan 差分 fuzz 480 万例无分歧。缺口：每日长时间 fuzz 尚未运行（第 5 节 F4） |
| T2 | 篡改字段值（A3） | **不防** | 只改标量值的 wire 能通过校验；wire 头没有校验和（格式 2.1 删除了固定为 0 的 `crc32c`），没有 MAC。外层必须用 TLS 或消息签名 |
| T3 | 伪造 schema 身份 | 不构成越权 | schema 指纹（64 位 FNV-1a）是一致性检查，不是认证：攻击者照抄指纹也只能让接收方按**接收方自己的类型**做完整校验 |
| T4 | 资源耗尽 | **默认有界**（M1：不传上限时默认 64 MiB） | 见 3.1；服务端接收客户端数据仍应设更小的上限（DEPLOYMENT 第 5 节） |
| T5 | 已删除数据的旧字节随 wire 发出（A4） | **已防住，有两处调用方责任** | P1、P2 由 M3、M4 清零；P3 由 M6 处理库构造的元素。调用方直接赋值的带填充记录、以及 load 进来的空洞原样转发，见 3.2 |
| T6 | 不同编译器对同一 wire 的理解不同 | 已发现一处并修复 | enum 键只定义 `operator<=>` 时，Clang 和 GCC 的集合顺序相反，两边互相拒收；修复和 CI 互读作业见 GCC 16 移植 PR |
| T7 | 业务层非法值（负数金币等） | **不防** | 不属于结构校验，按普通业务输入检查 |

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

**现状（0.3.0-beta.1）：**

- P1：块在释放时清零（M3），arena 里的空洞就是 0，所有输出路径仍是原样复制。
- P2：容器缩小时清零腾出的元素和字符字节（M4）。
- P3：库构造或搬移的容器元素（含 map 条目）填充字节清零（M6）。调用方直接赋值的
  带隐式填充的记录，库无法触及：用 `XOFFSET_REQUIRE_NO_PADDING(T)` 或
  `XOFFSET_REJECT_IMPLICIT_PADDING` 在编译期发现，改成显式字段。
- 转发：load 进来的 wire 里的空洞和容器余量（`[size, capacity)`）保留发送方字节，
  校验不要求它们是 0，load 后直接 save 会原样转发。这不是接收方自己的内存，但恶意
  发送方可借此给下游夹带字节。转发前用 `save_compact` 重建：它不写空洞、余量重写为 0、
  字符串尾部清零；记录的隐式填充仍按字节复制，所以转发的类型要没有隐式填充
  （[DEPLOYMENT.md](DEPLOYMENT.md) 4.2 节）。

回归测试：`test_wire_hole_zeroing`（P1、P2、转发、M1/M2）、`test_implicit_padding`（P3）。

## 4. 缓解措施（0.3.0-beta.1 全部实施）

M1–M6 已在 0.3.0-beta.1 实施，行为变化见 CHANGELOG。M3 的修改路径开销实测约 +5%。

| 编号 | 措施 | 影响 |
| --- | --- | --- |
| M1 | `load_verified` 和 `begin_verified_receive` 不传上限时，用默认上限（64 MiB，可用 `XOFFSET_DEFAULT_MAXIMUM_CAPACITY` 调整），超过就在预留前拒绝；需要更大的 wire 时显式传 `maximum_capacity` | 公开 API 行为变化：大于 64 MiB 的 wire 必须显式传上限。读路径性能不变 |
| M2 | 上限不足时的报错改为 `used_bytes … exceeds maximum_capacity …`（现在是笼统的 `XOffset arena allocation failed`） | 只改诊断 |
| M3 | 块释放时清零，空洞在 arena 里就是 0（P1），输出路径不变 | 改变含空洞 wire 的字节（golden 要更新）；开销在释放路径，与释放的字节数成正比。load 进来的 wire 空洞保留发送方字节，load 后直接 save 会原样转发（不泄漏本端内存，但恶意发送方可借此给下游夹带字节）。最初实现是输出时按空洞补零，实测正式输出慢约 22%，故改为释放时清零 |
| M4 | 容器缩小时（`clear`、`resize` 变小、`erase`、`pop_back`）把腾出的元素字节清零（P2） | 开销只在修改路径，与删除的字节数成正比；`clear()` 从 O(1) 变成 O(n)，需实测 |
| M5 | 文档：DEPLOYMENT 写明「只检查 wire 长度不够，必须设上限」，以及 P1–P3 | 无代码影响 |
| M6 | 填充字节（P3）：编译期检查可选开关，schema 有隐式填充就报错，让用户改成显式字段；或在输出前按反射得到的填充位置清零 | 开关方式没有运行时开销；清零方式要实测 |

## 5. 持续 fuzz

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
