# 网络成本诊断最小接入方案（设计，未实施）

启动：2026-10-15；本次设计交接：2026-10-16（Asia/Shanghai）。状态：设计待统筹审查。本项不依赖默认 D/F 复现；本轮仅阅读源码、核对哈希和编写设计，没有编译、运行网络、改动 W1/W3 或冻结包。W3 的独立网络计时准备不依赖本设计。

## 固定输入与最小复用

消费端唯一输入为 `build/vm-network/prepared-v2/source/`，其 prepared result SHA-256 为 `2af642fcd617b1fa193873b46f2892549e8d9dabcd457953bae2b8f7d5ac65fd`。W1 契约采用统筹 accepted revision 2（contract `496791aaf988cfe3aa72a8cdc0d120e2e0384434f8fc1bf628a2e79223191718`），不是读取今后变化的工作文件。源/锚点/依赖身份见 `plan.json` 和 `anchor-census.json`。

复用 W2 已验收的 `frozen-implementation-v1`：137 文件 snapshot `fc1780b5ce1f6cb7cfbbdfb85896d9746740c20b5e3df59bddedee5a42bb3c8d`、生成诊断双头、runtime、63-kind catalog 及结构 auditor。W1 的 F 双头与它逐字节匹配。只在 W2 独占目录生成一份消费者副本，替换其 include 解析到已冻结诊断头；`frame_model.hpp`、测试头、schema、传输和处理代码原样复制。不改生产头，不把诊断开关加入主 F，不建立通用接入框架。

保留 `N7_NET_DIAGNOSTICS=1`，让 W1 原 JSON 明确为 diagnostic，输出仍是原来的一行 JSON；另存 N3 trace 和 sidecar。W1 原阶段时钟字段只归档，不进入成本结论或任何端到端节省比例。新增 sidecar 是本设计的独立 W2 输出草案，不改变 W1 revision 2 或已冻结的 W2 事件契约；若需新增 kind、改变 runtime、字段意义或采样，先报契约修订，不能复用不相符的旧 kind 名。

可复用工具只有三组：W1 冻结 `fixture/run`、逐值验证和输出字节；W2 `n3::Phase/Scope/dump`、固定头与 `audit_events.validate_trace`；W4 `run_job.py` 的 build/functional/diagnostic 队列。W2 的固定探针 `check_case` 不适用于网络自定义 workload。已有独立验收的固定探针不重跑。

## 拟执行条件（尚未运行）

| 名义 u32 数据量 | elements | work 请求 | warm 模式额外 warm-up |
|---|---:|---:|---:|
| 4 KiB | 1024 | 2 | 1 |
| 64 KiB | 16384 | 2 | 1 |
| 1 MiB | 262144 | 2 | 1 |
| 16 MiB | 4194304 | 2 | 1 |

每尺寸分别 none/cold/warm，一个条件一个新进程，共 12 个 N3 网络条件；不在同一 recorder 中累积 12 个条件。与 W3 当前预登记相同的硬预算：max-wire=32 MiB、owner-capacity=32 MiB、inflight logical credit=128 MiB、两个独立一槽池各 reserved/accessibility idle budget=32 MiB+64 KiB，一条复用 TCP 回环连接、一个在途请求、chunk=64 KiB。实际输入/输出 wire 长度另记。每次操作 timeout-ms=30000；每个队列 job 外限 180 秒，超时算失败，不为了凑事件重试。

另拟运行 4 个独立、单线程的 instrumented W1 `fixture` 进程，用同一 elements 捕获准备成本，比较未改 W1 生成的 fixture 字节。这能以原样业务准备步骤触发目前未运行的 Automatic/extent 路径，无需人为改网络请求制造覆盖。它们与网络 run 分开，不能算作请求成本。网络对照使用 W1 已冻结原生消费者逐条件跑相同的两次 work 和 warm-up（只取功能/字节，不作为正式计时）。若已有同输入、同契约、同迭代和模式且身份完整的 W1 功能输出，可只读复用；不从不匹配的旧 smoke 凑对照。

最多新建 4 个 N3 fixture + 4 个 W1 fixture + 12 个 N3 run + 12 个 W1 功能对照，即 32 个短进程；两组网络各 28 次完整交换。按本 schema 的名义 payload 估算原始输出约 257 MiB，包含最多四份 warm 响应观测副本（实际 wire 以生成后检查为准），先设 512 MiB 新证据文件上限，fixture/输出不无限复制，超出先停并报告。这个文件预算不是 RSS 或总物理内存保证；所有 owner/池上限和约 44 MiB recorder 固定对象空间另列。

## 最小插入点与生命周期

精确源窗口见 `anchor-census.json`：20 个安全锚点对应下列 7 处逻辑窗口，并非 20 个新探针。未来生成前逐项匹配原文件哈希及预期出现次数，不按模糊行号替换。

1. `run` 入口：在所有被观察资源之前声明 `optional<n3::Phase>`，先标 `init.input`，将已有 preflight 块标 `init.preflight`，分别标两次 pool 建立为 `init.pool.server/client`。原 request、ledger、owner 生命周期保持原顺序。
2. `Worker`：保留现有持久线程及 start/finish/stop/ready/done 同步。只给复制类增加不变的 `has_warmup` 和 worker 局部 ordinal，依次给 server 生成 warm 或 work.0/work.1 标签；不共享无同步的诊断序号，不为每请求新建线程；不变标志必须在 thread_ 成员启动前完成初始化。client 在 run 的两个既有调用点用同一序号。serve_one/exchange 的诊断副本最多增加一个只读 cycle 标签参数，不改报文字节或调度条件。server wire/saved 已结束生命周期的检查点放在 Worker::loop 的 result_=serve_one(...) 返回后、既有 done 同步发布前，或写入 caller-owned 观察存储；不能让析构 guard 修改正在按值返回的局部 ServerResult 来传递完成标志，否则依赖 NRVO。该检查只表示 C++ 对象生命周期结束，不是 std::string backend free 字节探针。
3. `serve_one`：依次标 `server.{cycle}.receive/load/process/save/release/send/strings_release`。阶段 holder 必须声明在 wire/saved/owner 之前；用 `holder.emplace(label)` 切换，设置 release 后让原 `auto owner` 按原位置析构。若 Phase 在 owner 后声明，它会先析构，释放事件会归错阶段。禁止为标注而把 server owner 改成 optional/reset，避免额外 owner moves。需要的 Scope holder 也在 owner 前声明，加载后设置 base，保持至 owner 原有析构完成。
4. `exchange`：标 `client.{cycle}.send/receive/load/validate/release/server_wait`，保留原有 optional verifier、verified.reset、worker.finish 的先后。在原 reset 处标 release。每次交换按原逻辑等待 server 完成，避免把 server 字符串销毁遗留到下一请求。
5. run 循环：保留一次 warm-up 的完整事件和输出逐值校验，标 `warm` 而不是删掉；在短作用域中接收其已有返回 Sample，将 owning warm 输出存为独立观测文件，在任何 work 前销毁。此处延长了 warm 响应字符串寿命并增加文件 I/O，明确属于诊断准备开销，不声称该字符串寿命与原 W1 完全相同，也不解释 warm 时间；标 warm.output_archive/release。它没有新增 arena owner 或 arena moves。cold 的两次 clear、work.0/1、output 写入与释放、final.clear、request.release 分别标注。已有 run 返回前最后设置 `run.teardown`，让声明在所有资源前的 holder 覆盖 worker 析构/join、连接关闭和 pool control/slot 析构。
6. main 的 `run_json(run(...))` 改为等价三步：接收原 run 结果（此时 Worker 已析构且 join，池 handles 也已析构）、dump 到新的 n3-events.json、输出原 run_json。dump 前没有活跃 emitter，不在 run 内或仅 worker.finish 后提前 dump。失败时仅在已有资源已栈展开且 worker 已 join、输出目录可用时写无成功结论的诊断；缺 trace、坏 trace 或失败都拒绝成本解释。
7. `fixture`：阶段 holder 同样先于 owner，标 build/reserve/save/readback_verify/teardown；保存与已有 readback 验证不删。此进程没有 worker，函数返回后再 dump。fixture 计入单独 preparation 表。

不修改 `frame_model.hpp`，不插桩 read_exact/send_exact 的每次循环，不把 recv/send 字节称为 memcpy。已有 IOProgress 提供实际端点 read/written/receive-allocation/terminal 计数。为直接 staging 与输出复制方向补三个有界 consumer 观察即可：receive_frame 中原 header memcpy 完成后的 calls/bytes；receive string 的 size/capacity/构造请求长度；server saved 与 client output 的 size/capacity 和原释放检查点。复制的 IOProgress/ServerResult/Sample 可携带这些少量标量，借既有 finish 同步回 main。它们写入独立 sidecar，不挤进 63-kind trace 或改变原 JSON 字段。

receive string 构造 `std::string(W,'\0')` 只报告逻辑初始化请求 W 和观察到的 capacity；不改成 `grow_zero`（那个 kind 属于 VM），不把请求长度叫作实际 libc memset 或全进程分配。已有 std::memcpy(header) 的完成调用可单独核对 88 字节；无 owner 的传输 scratch 不能伪装成 arena identity。std::string malloc、allocator overhead、kernel socket copying、Worker 自身 mutex/condition_variable 等待仍未覆盖；不能把它们重命名成 pool/registry lock。

sidecar 标量采用 uint64 bytes/count 或明确 boolean：receive size/逻辑初始化 extent 包含 88 字节 wire header；capacity() 为存活字符串的字符容量，不含末尾 NUL，不能当作 backend allocated/usable bytes。header assembly 只在原 memcpy 完成后计一次 88 字节，独立于 IOProgress socket bytes。server saved capacity 在 save 完成且字符串存活时取样，client output 在 receive assignment 完成后取样，server_strings_released 在 worker 调用返回检查点置 true。消费端只写 raw observation；进程完成后的收集器再生成绑定 stdout/trace/outputs 的最终 sidecar，不改原 W1 JSON。若为 final-zero 检查额外调用 stats()，必须标 diag.pool.check，保留新增锁事件且单列为诊断观察开销，不归入原 W1 可消除锁数。

## 可预测事件与资源断言

令 H=88，Wi/Wo 是同一冻结 fixture/reference 对照实际 wire 长度，Pi=Wi-H、Po=Wo-H。所有尺寸 N>0，输入/输出标签均为 21 字节，值数不变，process 原地改值；本 schema 预期 Pi=Po、Wi=Wo，必须用原 W1 字节确认，不能只据推测写入期待表。

每个 work 或 warm exchange 的预期（均是待执行断言，尚不是结果）：

| 路径 | 预期 |
|---|---|
| server/client verified load | register 各 1、erase 各 1；load_header_copy 合计 2H；load_payload_copy 分别 Pi、Po；三个字段逐值核验保留 |
| server process | string_assign_copy 一次 21 字节；无 vector relocation、Core/wire 新分配/转移、Automatic 增长 |
| server save | save_header_copy 一次 H；save_payload_copy 一次 Po；原 owner 在 send 前注销，拥有型输出仍发送完整 Wo |
| consumer receive | 两个 endpoint 各一次 header memcpy H，各一次 owning string 准入；请求/响应端点 IOProgress 与 Wi/Wo 精确匹配 |
| graph metadata domain 5 | 每个 loader 5 个 owned range：arena header、allocator、root、values、label，无 free ranges；固定 libc++ 容量 1/2/4/8，4 次请求共 240 字节、4 次匹配释放；domain5 单次加载局部观察顺序峰值 192 字节（此严格同步链的 domain5 峰值，非全域峰值）；与 N 无关 |
| move/rebind | 由 TypedXBuffer 及 optional 的原转移形成合法注册代次，不能改变 root/base；NRVO 相关总数不先伪造固定值，按冻结构建实际 transfer 账本验证；注册/注销数量与归属是硬断言 |

preflight 独立多一个 verified load 和一次 21 字节 process，没有 save。若 K=2，w=warm?1:0，E=K+w，则每 run 总 register=erase=1+2E，load-header 次数/载荷-copy 次数均 1+2E，save-header/save-payload 次数均 E，string_assign 次数 1+E；domain5 总分配/释放各 4(1+2E)，总请求 bytes=240(1+2E)。正常 run 注册表最大同时活跃 owner 为 1：preflight 已释放，server owner 在响应发送前释放，然后 client 才装载。跨线程不意味着两个 owner 必然同时活跃。

| 模式 | work 命中/未命中 | work 期间新 reserve/enable | run 总 reserve/enable/unmap（含 preflight/warm/final clear） | pool_scan |
|---|---:|---:|---:|---:|
| none | 0/0 | 各 4 | 各 5 | 0 |
| cold | 0/4 | 各 4 | 各 5 | 0 |
| warm | 4/0；warm 自己 0/2 | 各 0 | 各 3 | work 合计 4；warm 0 |

此表要求 P<C=32 MiB、页面前缀行为和无失败（本四个输入满足设计上限，运行前仍检查）。范围为 R(C) 预留、R(P,page) 可访问；不是 RSS。这里只预测 no-failure 成功 syscall，不保证未来 OS 一定成功；任何真实错误都使该条件失败。none 不创建 pool metadata。cold/warm 各创建两个 domain3 slot 请求、两个 domain4 state/control 请求；实际请求字节从固定 allocator 记录，不猜 private control block 大小。全程无 reclaim_tail 调用，因此不能宣称执行了 reclaim 事件路径。pool_lock 包含统计/清池/借还而非请求专属时间；不为了整数漂亮忽略 stats 锁。

before_final_clear 检查：none idle=0；cold/warm 两池各 idle=1、reserved 合计 2R(C)、accessible 合计 R(Pi)+R(Po)，且仍无活跃 arena owner。最终 clear 后 idle 三量均零；run 返回且 worker join/pool handles 析构后 domain1/3/4/5 outstanding=0，domain2 只允许已登记的共享 registry retained bucket：固定 libc++ 在活跃峰值 1 时预期唯一 2-pointer bucket 请求（16 字节），不得泛放行任何 domain2 泄漏。若实际 ABI/几何不符合，失败并说明，不静默放宽。对 retained 请求必须以 allocation subject/bytes/domain 对照其初始化 phase 与释放账本，不能用当前 Scope 重归属。原 W1 ResourceLedger 最终 slots/bytes=0，峰值=4*maxwire；这仍是逻辑 credit。

owner 代次使用 `(base, registry_register.sequence)`，rebind 改 C++ object 关联而不生成 arena 代次。warm 池可反复给同一 base，新请求仍注册新代次；不得按 raw base 合并生命周期后称 owner 复用。OS mapping 的 reserve/unmap lifetime 可跨多次 register/erase，并由 pool 终止。phase+代次+thread 关联请求；不能把 shared domain2/3/4 全计作某一个 owner 独占。F 的 load_payload_copy 和 OS/pool 获取发生在成功 registry_register 之前：用同一 endpoint/cycle/load phase、线程及 base 关联到随后唯一一次成功 register，作为该代次的构造前缀，不能误归给已结束的上一代。无 register、多个候选或 phase/base 不一致就拒绝该请求的成本解释。OS reserve/unmap 仍单独按 mapping lifetime 归属。

## 63-kind 覆盖增量与 recorder 预算

成功的网络 run 预计只使用已有 40 类已执行类型的子集；它不会自然触发此前 22 类中的 wire flush、container shift、tail-growth 等。它增加的是 server/client/preflight/warm/cold-clear 上的成本断言，不冒称新类型已执行。单独 fixture 预计新增 `extent_request` 两次（不允许增长的第一次请求、Automatic 重试）和 `auto_growth` 一次；每个 fixture 先创建 4096 字节 arena，再 reserve 4N 字节，N>=1024 必须跨初始 extent。无 free cache，cache_flush/transfer 仍为零。fixture 读回时原 owner 尚存，registry peak=2；与 run peak=1 分开。fixture 若不满足上述次数就失败并复核源码，不按实际结果改期望。

range metadata 是 5 个固定区间，vector 在 reserve(N) 后才 push，已有插桩不会为 4,194,304 个 u32 各发事件；process/verify 的逐元素循环也不新增探针。最小插入点不记录每次 socket chunk 或每个数值。因此事件数量与消息字节长、短读写次数无直接线性关系。验证/处理仍逐元素执行 O(N) 工作，只是没有逐元素事件；固定的 domain5 请求数不是完整校验成本。某个 receive/send/CPU phase 没有 N3 事件不表示该阶段成本为零。未来若发现按元素发事件的代码，先拒绝该生成物，不靠 sampling/删前缀/增大 recorder 掩盖。

源码预期和硬验收门槛分别登记，数值统一为更小的 2048：每 run main 预计 <=2048、server 预计 <=2048 条，fixture 单线程预计 <=2048；sidecar 每 run <=3 exchanges、每 exchange 两个 endpoint、固定字段，无逐字节数组。硬断言为 per-thread <=2048、thread_count<=2（fixture<=1）、labels<=63 UTF-8 字节、valid=true、dropped=0；runtime 原上限仍是 16 lifetime slots、每槽 16384。2048 的源码预测与硬门槛是不同口径字段，当前数值相同，均不是已实测计数。超过 2048 或任一其他硬条件失败即拒绝该条件全部成本解释，停止后续条件并交统筹复核；完整原始记录保留，不自动重跑或改期望。任何放宽都须新的明确设计修订和实施派发，不能在看到结果后静默放宽。failure 中可有不完整代次，但不提供成功成本结果。fixture、run、对照分别新进程，warm-up 从完整 trace 第一事件起保留。

## 逐值、字节与候选依据

保留 W1 marker、21 字节 label、count 和每个 fixture_value/processed_value 检查。N3 fixture 与同 N 的未改 W1 fixture SHA-256/长度/字节完全一致；每个 run 的 output-0/1.wire 与同输入、模式、预算、迭代的 W1 对照逐字节相同，各模式之间也相同。N3 warm 观测文件逐字节比对同条件原 W1 的 work0 响应；称为“warm 对 canonical W1 work0 oracle”，不声称存在原 W1 没有保存的 warm 字节证据。sidecar 绑定 stdout 原文哈希、输入/输出 hashes、schema/version、prepared result/contract、生成 anchor manifest、实际二进制和 trace hash。单一 checksum 不足以通过；丢/重/换 condition 或迭代、trace 缺失/溢出、错误计数/owner、未配对释放、warm 前缀缺失、身份/stdout/bytes 不符均拒绝。

三条候选只拿到以下成本依据：owner 复用看每 exchange 两次注册/注销、domain2/5 请求及 none/cold/warm 映射差别；direct staging 看 receive owning string 的逻辑初始化/容量与 load 的 Pi/Po 真实显式复制；减少输出复制看 save 的 Po/H 和拥有型输出在 owner 释放后仍完整发送。它们没有获得实现或采用授权。验证、失败原子性、旧引用失效、取消和发送完成生命周期都仍须按候选单独证明；F warm 已消除的映射不能再次算作 owner-reuse 收益。

trace 字节量只描述这些作用点，不是生产网络收益。不能将 server/client 时间或重叠 send/receive/release 时间相加，也不能将复制字节占比、阶段时长或 warm 池差值换算成端到端“可省比例”。后续性能、噪声与采用仍完全由 W3 的独立计时和统筹 gate 决定。

下一步仅请统筹审查本设计与精确插入点。获派实施窗口后，才复制/生成源、冻结新 wrapper 身份、通过 W4 队列构建与有界功能/诊断执行；本轮没有这些结果。
