> **历史草稿勘误（D018 / D022，2026-10-16）**
> 本文原始设计已由 D018 冻结并下发实施。末节资源数以 D018 的 26 workload children、30秒/child、180秒/队列job、512MiB evidence、构建并行≤2 为准。
> D022 已确认本次 Apple arm64 profile 的有效 alignment=8，而本文原推导误设16。uint64 用户地址为 `base+64+24*j`，end=`72+24*j`，Q次后 `used_end=48+24*Q`；Q1024最大24624，仍小于65536。空owner used_end=48，free后回收每请求24字节。
> 本文原有32-byte stride/32808静态推导及对应旧 checker/synthetic fixture 不能用于本profile；事件数量、metadata/vector、bucket lineage、生命周期和所有预算保持不变。原harness使用真实alignof/sizeof且已正确，无需重编。
> 首次原始记录及失败收集保持历史结论；新审计身份通过补充材料单列，不重跑首例。

# N3 owner-scaling：待一次冻结的最小设计 v1

2026-10-16。仅设计准备；尚无新 harness、构建、运行或性能结果。本设计采用统筹选定的小方案，
不启动正式计时，不改 F/W2/W3 文件。旧审查的 R1024/typed root 方案未选，不属于本契约。

## 固定工作量与六条件

每个全新 child 只运行一批。直接使用生产 `XBufferCore` 和
`XArenaAllocator<uint64_t>(owner.arena())`：无 typed root、wire、pool、growth、reclaim、move/swap。
这是真实 **Core-backed allocator → arena → owner-index** 路径，不是网络负载或纯 hash 查找。
实际 `allocate(1)`/`deallocate(p,1)`各查一次 registry；禁止缓存 Core* 绕开查找。

| 唯一条件 ID | live owners C | workers T | R256 每 owner Q | R8192 每 owner Q | R256 整进程事件上界 | 任一 worker 上界 |
|---|---:|---:|---:|---:|---:|---:|
| `o8_t1` | 8 | 1 | 32 | 1024 | 1991 | 1991 |
| `o64_t1` | 64 | 1 | 4 | 128 | 3007 | 3007 |
| `o256_t1` | 256 | 1 | 1 | 32 | 5631 | 5631 |
| `o256_t2` | 256 | 2 | 1 | 32 | 5631 | 3071 |
| `o256_t4` | 256 | 4 | 1 | 32 | 5631 | 1791 |
| `o256_t8` | 256 | 8 | 1 | 32 | 5631 | 1151 |

R 是**全进程每阶段总请求数**，每条件有 R 次成功分配及 R 次对应释放；不是每线程/owner 的 R。
每 owner 初始 logical=显式 maximum=65536 bytes，Explicit、无池。固定 `i → worker i%T` 覆盖
完整 create/allocate/fill/free/destroy 及失败清理；owner 放稳定 heap 对象，外层 slots 预分配。
每 worker 顺序处理自己的 owner，每 owner Q=R/C 次；只选 sequential，避免引入第二顺序维度。
全部 C owner 注册完成之后才开始 allocate，全部 free 后才 destroy，故操作阶段 live owners=C。

成功功能集合恰为18进程：六条 F/R256 + 六条 diagnostic/R256 + 六条 F/R8192容量/语义检查。
F/R256 与 diagnostic/R256逐值/计数一致。诊断禁止R8192；未来正式计时只由W3对F/R8192派发。
R256 的诊断不能推算 R8192 的精确时间占比；短 batch/barrier 噪声仍交W3校准，无自动增R/重跑。

## 小 harness 与阶段边界

拟议文件仅 `owner_scaling.cpp`、包内薄 runner/checker；不引入通用框架。
只移植旧 `tools/bench_vm_improvements.cpp:166–253` 的固定 worker、barrier 与错误排空思路，
**不文本 include 旧 benchmark/compare.cpp**，避免其他 workload、root和计数进入闭包。
旧 random 把 create 与后续 owner 移给不同 lane；本设计不沿用它。

一个 T-worker team，main coordinator。起线程、预分配 metadata/计数槽、生成输入顺序均在计时外。
启动门闩先确认完整 team；部分启动失败时唤醒已起线程退出并 join，不进入缺 participant 的 barrier。
成功四段各由 main `clock → start barrier → end barrier → clock`，明确包含 rendezvous/调度：

1. **create**：worker 构造其 C/T 个 Core（不 make root），记录实际成功数、Core/base/容量归属。
2. **allocate**：每次真实 allocator 返回后存 pointer、增加实际完成计数；不写 payload、不算 checksum、
   不查RSS/输出。首批无free blocks，不能偷偷预热或reserve Core宿主vector。
3. **free**：验证结束后，原worker释放原owner的所有 pointer，每次返回后增加完成计数；不再解引用。
4. **destroy**：原worker reset 自己的 owner，记录实际成功销毁数，随后 join；dump/JSON在join后。

allocate 与 free 之间另设**不计时**的 fill/barrier/main-verify：worker 写 `value(k)=constant^k`，
main 在worker静止时逐值核对 R 个值、归属区间、实际对齐及全局不重叠，按 k 顺序算便利checksum。
k=i*Q+j，故同R各条件/构建的值集合和checksum相同。main只读payload及预先保存快照，不调用
allocator/registry；free后由worker在计时外核对base/logical/used_end和`get_free_memory`，证明
void/noexcept free未因宿主OOM静默丢失回收（预期tail + Q*(sizeof(AllocationHeader)+8)）。
最大Q1024，固定64位/16-byte alignment下end=32808<65536；无需增长或扩大cap。

所有异常仅先记录，不越过未完成barrier直接抛出。main核验失败、worker失败均让固定team继续
drain并在所属worker清理已分配/已创建部分，join后非零退出；cancel标志不能跳过清理。
不长期持registry全局锁，不并发修改同一Core。

## 诊断：精确事件与完整保留

只使用已接受 `frozen-implementation-v1/build/input/diagnostic/` 双头和runtime，
以及包内 `source/audit_events.py`、相邻catalog；不取W2当前网络生成副本，不新增探针。
16 lifetime slots、每slot16384events不变；只有T个worker emit，main dump不emit，所以槽数T≤8。
保留所有 setup/create/allocate/fill/verify/free/destroy/teardown；零事件阶段仍列于harness phase ledger。
不预热、不reset、不截尾、不采样；trace invalid/dropped、超预算或意外kind使整个child不可解释。

成功R256的硬断言：

- register=erase=C；完整generation=(base,register sequence)恰C，峰active=C，结束0；rebind/miss/失败0。
- lookup_hit=2R=512：allocate/free各R，每owner各Q、每worker各R/T；registry_lock=2C+2R。
  每lookup必须处于对应generation，lane→recorder-slot从register重建且全寿命一致。
- core_new=core_return=R；metadata snapshots=C+R；Core scan/reuse、wire、pool、extent/growth/
  expand、copy及失败kind均0。计数来自实际trace，不从声明请求数伪造。
- reserve/enable/grow_zero/unmap各C成功，zero总量65536*C；无reclaim/protect/replace。
- domain1 allocate/deallocate各C*g（g=1+log2(Q)），结束0；domain3/4/5均0。
  domain2 node分配/释放各C，桶完整记录，最终只允许一个共享registry bucket。

源码导出预算：每owner setup 7固定事件、teardown 4固定事件；每allocate 3、每free 4；
domain1共2*C*g；bucket增减B≤2C−1。因此 `E=7R+C*(11+2g)+B`，上表为保守上界，
最大5631<16384。冻结libc++按2→5→11→23→47→97→197→397扩桶，C8/64/256末桶分别
11/97/397指针，即88/776/3176 bytes；B实际预期5/11/15，E预期1981/2891/5135。
它们是**依赖限定、待首次trace核对的静态预期**，不是已运行结果。偏离则失败，不能放宽到通过。

Core free-vector growth及共享nodes/buckets都是真实成本，不把whole-phase wall差异全归registry。
host bytes为allocator请求量，不含malloc overhead/static registry对象/RSS。共享domain2只作因果归属，
不作单owner独占峰值；allocation pointer ledger保留原分配归属，完整保留擦除generation后的释放。
registry_lock wait/hold仅诊断，受探针扰动；逐phase、逐实际lock分布单列，不能相加或除whole-phase
冒充每lookup延迟/可省端到端比例。

## 两构建、闭包及原始输出

同一新source，两角色：`N3_DIAGNOSTIC=0`用快照F双头、无n3_runtime依赖；`=1`用接受的W2生成
双头/runtime。W2 `n3_original`不是本harness正式timing binary。仅阶段wall clocks在F构建内，
无inner-loop/lock诊断时钟。参考 [诊断预算复核](diagnostic-budget-review.md) 的现有profile argv，
新命令仅替换为自己prepared快照的include/source及新output；禁止把active工作树头作为最终include。

固定 `/usr/local/bin/clang++` **执行拼写和argv0保持不变**；可resolve其目标来算hash，不能改成
`clang`/`clang-21`执行。保留-O2、C++26反射/expansion、libc++、pthread、arm64、SDK26.5和
既有Boost/TypeLayout include及libc++链接profile。新构建各用`-MD -MF`记录包含system的完整.d，
把真实传递头、所有新helper、F/生成头/runtime、auditor/catalog/contract、compiler/SDK/builtin
headers、stdlib/OS/link runtime逐文件哈希纳入闭包；旧1018文件清单只作参考，不替代新.d。
依赖内容只记身份，不复制整套SDK/历史包。两编译顺序执行，不自动换driver或“修复”链接路径。

拟议CLI：`owner_scaling --owners C --threads T --requests 256|8192 --control none|...`；
只接受六条件，diagnostic只接受R256。每child stdout恰一JSON，stderr原样保留，退出码0成功、
2为明确失败；失败给reason、实际阶段进度/清理计数，外层识别预期control而不当普通成功。
原始输出包括profile/条件/R/Q/固定worker规则、实际counts、owner/lane/base ledger、四段
wall_ns、untimed setup/fill/verify/join、逐值结论/checksum、容量/回收快照及diagnostic完整trace。
F trace=null，`timing_evidence=false`；正式资格由W3另记。归一化仅称batch amortized wall/R，
create/destroy分别除实际C，不能称lookup latency。raw stdout/stderr、argv、退出码、source/binary/
dependency identities、通用及workload审计结果、排他新目录manifest均保存；离线搬迁可重算。

## 少量负控与总预算（批准后才执行）

四类失败各F/diagnostic一进程，共8；全部R256，精确expected reason及实际进度另列control表：
部分线程启动失败（C256/T4，lane2起前故意失败：已起2、Core0）；worker create前失败
（C8/T1，第4个Core前：create3、requests0、destroy3）；allocate前失败（C8/T1，第8次前：
alloc7、cleanup free7、destroy8）；计时外坏值核验（C8/T1：alloc/free256、destroy8）。
它们是harness控制异常，不冒充OS/allocator OOM。失败trace仍完整保留、审计，超时/任意异常
不能算通过。另在离线自有副本做3个拒绝控制：声明R倍增、删lookup后重编号、遗留owner/残留
类错误；不改原证据。既有recorder溢出控制仅引用继承，不重跑全事件矩阵。

- **原生运行最多26进程**：18成功+8预期失败，无额外自动重试；未来W3正式窗口另行派发/预算。
  两次编译、一次compiler元信息probe、两次薄Python工作入口后，共≤31工作进程，
  不含共享队列自身监督器；离线审计在收集入口内完成，不spawn C++或再开worker。
- 两个默认队列job：build≤300s，validation≤360s；每编译≤120s、每原生child≤10s，
  整包≤660s（安全回收可越过期限并记失败）。一次仅一个child/编译，最多T8+main，
  不嵌套同锁，不操作其他进程；繁忙返回busy，等待统筹窗口，不自行抢占W2/W3。
- 显式VM峰值16MiB；diagnostic recorder静态约44MiB（非全进程RSS）；pointer/Core metadata
  等计入已知数据额度≤64MiB。原生maxRSS验收上限256MiB，build参考预算2GiB/单编译，
  不假称OS硬限；超限停止本包并保留失败，不自动降R/C。输出/快照/归档合计≤64MiB。

下一步只请求统筹冻结本设计、身份与预算；获得实施/执行派发后才写harness或占用队列。
