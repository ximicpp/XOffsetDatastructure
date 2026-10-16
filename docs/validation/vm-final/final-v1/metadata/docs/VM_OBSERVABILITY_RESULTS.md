# N3 独立诊断与交接结果（W2）

2026-10-15（中国时区）。本轮完成固定 F 的独立诊断副本、可预测语义/故障控制及离线审计，交付统筹复核后供 W3 接入。生产双头未修改；未运行正式计时、D/F 消融、远端 CI 或发布。默认路径归因仍等待 W3 的可靠复现，本报告不关闭整个 N3/路线图。

2026-10-16 增量：有界网络诊断已完成，见文末新增结果。前半部分的覆盖数和失败记录仍专指已验收 foundation，不追溯改写。

## 身份与入口

固定 F 双头 SHA-256：

- `xoffsetdatastructure.hpp`：`9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2`
- `xoffset_virtual_memory.hpp`：`22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad`

统筹已接受的 [contract](../experiments/vm_observability/contract.json)、[63 类 kind 目录](../experiments/vm_observability/event_kinds.json) 与 [设计覆盖表](../experiments/vm_observability/coverage.json) 保持冻结哈希。最终生成器验证 71 处锚点；复用固定身份的 N5 `instrument(F)` 事件位置，再补 owner 索引、实际 allocator 请求、复制和 OS 结果。普通全局 N6 计数器未用于并发。诊断头与生产头分离，donor、wire、释放与异常控制均使用同一个 F 实现。

最终构建为 `build/vm-observability/build-06/`，最终固定探针证据为 [collection-06](validation/vm-observability/collection-06/result.json)。每个构建和 C++ 执行都由 W4 的默认队列包住整个任务。构建顺序执行两个目标，使用 `/usr/local/bin/clang++`、显式 SDK `MacOSX26.5.sdk`、P2996 参数及冻结 Boost/TypeLayout 提交；未使用系统默认 SDK 27。构建记录包含实际依赖的 1,018 个外部文件指纹、原始/诊断双头、编译参数和两个二进制哈希。

`n3_original` 是原始 F 的语义对照，`n3_diagnostic` 是插桩程序；二者都不是 W3 的正式计时消费者。机器接入路径、冻结包、字段及版本见 [handoff.json](../experiments/vm_observability/handoff.json)。

## 本轮实际验证

最终固定集合为契约中的 10 个 case，各运行原始 F 与诊断程序一次，共 20 个进程。9 个普通诊断 trace 有效；`recorder_overflow` 必须生成无效 trace，审计拒绝后才算负控通过。每项逐值语义与预登记常量均核对，不能用任意异常充当失败成功。

| case | 关键实际检查 |
|---|---|
| `owner_basic` | 1 次注册/注销、2 次 owner 命中、一次 Core 分配/归还、4096 字节 owning copy extent |
| `owner_transfer` | move/swap 4 次 rebind；旧 handle 失效/恢复；原 allocator 跟随 arena；外层 Scope 下 4 个空 owner 快照仍为零身份；未注册 arena 精确报 owner lookup failed；空 copy 不伪造复制 |
| `core_metadata` | 2 个并存 owner；11 个新块、12 次归还、1 次首适配 whole-donor 复用；物理 donor 32 字节不被切分；domain1 的 7 次实际 allocator 请求共 352 字节 |
| `copy_wire` | verified round trip 逐值一致；一次保存复制 payload=600 字节；成功和坏图加载实际复制共 1200 字节；4 个完成迁移元素共 16 字节；string 迁移 3 字节；坏 magic 与坏 bool 按预期错误拒绝 |
| `pool_reclaim` | 真正跨完整页回收与再增长清零；冷/暖池 hit/miss=1/1；清池释放；本机 16 KiB 页的前提明确 |
| `threads` | 两个线程各自 owner；注册/注销、donor 归属、线程身份及并存容量可区分 |
| `failure_registry` | 既有 owner 不变；生产 one-shot 注册失败与诊断 allocator 实际失败分别验证；随后正常重试 |
| `failure_metadata` | Core 归还宿主分配失败不抛出，不虚报成功归还；get_free_memory 未虚增；重新归还后复用原 donor |
| `failure_vm` | Prepare/Reclaim 注入位依次为 4/8；失败保留旧地址/逻辑长度/数据；真正回收重试有 OS 成功事件，注入拒绝没有冒充失败 syscall |
| `recorder_overflow` | 单线程 16,384 条上限后丢事件并使整条 trace 无效；不将这条 trace 解释为成本 |

另有 [3 个实际 runtime 控制](validation/vm-observability/runtime-controls-01/result.json)：17 个生命周期线程耗尽 16 个槽、64 字节标签超限、分域 allocator 在一次成功后失败并正确释放。前两项仅证明无效诊断被拒绝；allocator 控制有 3 个匹配事件，存储值和账本均核对。这些不是网络或业务测试。

原 F 对照的边界单列：`failure_metadata` 的诊断 allocator 注入只存在于诊断副本，原 F 该 case 仅比较正常 checksum；`recorder_overflow` 的原 F 输出 trace 为 null，仅比较控制次数。`failure_registry` 的原 F 验证生产 one-shot 失败，诊断额外验证真实 map allocator 失败。不能把这些窄化对照称为两构建具有相同故障注入覆盖。

[57 项 Python 测试](validation/vm-observability/python-tests-final-02.log) 验证严格类型、未知 kind/domain、缺/重/乱序事件、线程/标签/容量上限、错误释放字节或域、错误 owner、缺失/重复注册、失效诊断、身份/原始 stdout/计时标签/case 集错误。它们使用合成轨迹和临时清单，不替代上述 C++ 运行。另以真正的源码准备结果确认连续审计和搬迁后冻结审计均不改写证据。

## 63 类目录与运行覆盖的区别

逐 kind 的输入、实际触发次数、证据文件和断言见 [event-validation-coverage.json](../experiments/vm_observability/event-validation-coverage.json)。分类如下：

- 29 类实际触发，并有固定数量/字节、部分快照或分配/注册账本断言；其中“部分快照”不表示该类所有取值均穷举。
- 11 类实际触发，仅接受结构、类型、合法值及通用账本审计；不宣称它们的每个计数已有独立预期值。
- 1 类仅在无效负控 trace 中触发，不能解释成本。
- 22 类声明在冻结目录中，本轮没有执行相应 C++ 路径。通用目录/合法值测试不等于这些路径运行通过。

未执行的 22 类包括 Automatic/extent 请求、部分尾扩展结果、wire flush/转移、vector 插入/删除移动、string fallback append，以及 Linux madvise/protect 和 OS 真正失败返回。既有 N4/N5/N6 观测为继承来源，未冒充本轮重跑；Windows 诊断未支持。精确清单以机器文件为准。

## 成本口径与解释限制

host ledger 记录五个限定 allocator 域的 backend 请求字节和释放，包含扩容时新旧请求短暂并存。domain 2/3/4 是共享 registry/pool 元数据的因果归属；domain 1/5 可分别查看，不能把共享项称为 owner 独占。Core vector 固定对象大小通过快照单列，不是 allocator 请求；registry 静态对象、malloc 开销、进程其余分配和 RSS 未纳入这些请求总量。

`core_metadata` 的本机可预测示例中，单个 origin owner 的非共享请求峰值为 192 字节，domain1 并存总峰值为 224 字节，全部受跟踪域合计峰值为 304 字节。这三个量不同。峰值按 recorder 的观察顺序计算，包括新旧 vector 分配重叠，不是物理内存峰值。owner raw base 可能在注销后复用，独占生命周期分析应使用 `(base, register sequence)` 代次；move/swap/rebind 不创建新的 arena 代次。deallocate 从原始 allocation pointer 账本归属，不跟随当前 Scope 猜测。

显式 memcpy/memmove/memset 字节是相应调用成功后的请求范围，vector 完成迁移量是逻辑元素大小；二者不可相加当作总复制。Core erase movement 为逻辑记录移动估算，容量为快照，`copy_bytes`/fallback append 是逻辑输出 extent。未计入的包含 header 小字段/内部 STL/libc/kernel 复制、递归反射 transfer 的完整物理字节、prune 元数据移动及进程总复制。结构摘要里的通用 `bytes/value` 累加只方便重算；是否可作成本必须服从 kind 目录，指针、容量、offset、domain/epoch 等 value 不可当作可加成本。

锁 wait/hold 是插桩后的时钟观测，互斥范围保留，但 recorder 原子序号和时钟会改变争用。累计线程等待可能超过墙钟，hold 不包含记录/解锁末尾开销。POSIX OS 事件区分调用成功/失败和范围，映射/权限/丢弃不等于 RSS。record/thread/label overflow 使整条记录失效，既不截取有效前缀也不称通过。

## 复现、失败记录与后续 gate

所有队列的 request/result/manifest 保存在 `docs/validation/vm-observability/*job*`。正式构建/探针命令全文在 build-06 和 collection-06 的 result 中；离线使用 `collection-06/tool/observe.py audit --record PATH`，仅核验文件与数据，不执行被测 C++。后续 checker 改版时必须使用冻结 checker。

早期 build-01/03/04/05 已被后续 review 与测试修订取代，不作为最终源码验证。build-job-02 在编译前拒绝：原 N5 import 在准备归档中生成了 Python cache，导致清单变化。已改为固定身份源码的无 cache 加载，并禁用冻结 checker 的 bytecode 写入；连续/搬迁审计负控证明已修正。失败与旧记录保留，未覆盖历史证据。

W3 可接入冻结诊断头和 runtime；初始化、Phase、线程 join、dump、预存资源限制和允许保留资源的规则在 contract 中。暖池准备阶段必须被记录，不能删前缀或 reset 后把已有资源当作零成本。自定义 workload 只获得结构/分配/registry 审计，仍须由 W3 加入其语义与预登记资源断言。正式 timing 继续使用独立未插桩构建。

独立 review 记录见 [REVIEW.md](../experiments/vm_observability/REVIEW.md)。本轮的采纳内容仅为独立实验诊断与验证工具；生产头保持 F。D/F 差异稳定复现前不做单变量消融，网络成本解释与性能采用决定由后续 W3/统筹 gate 决定。

## 2026-10-16 有界网络诊断增量

按统筹已接受的 [20-anchor 方案及正式派发](coordination/vm-followup/contracts/W2-network-v1/acceptance.json)，完成独立 consumer 生成、默认队列构建、原始 W1 对照、事件/资源审计、负控与封存。生产双头、W1/W3、已冻结 runtime/catalog/contract 均未修改。最终 [collection-v1](validation/vm-observability/network/collection-v1/result.json) 包含四个夹具和十二个网络条件，每项一对原始/诊断进程，恰好 **32 个 workload 进程**；每网络条件两次 work，warm 模式另记录一次完整 warm，共24次work+4次warm。所有进程正常结束，未重试 workload。

最终构建为 `build/vm-observability/network/build-v2`，诊断二进制 SHA-256 为 `12d97eb430a8d894a4491944827fce7b169835ebb9dec040b02b92a66f8ba2ed`。编译依赖验证三个头唯一解析到本构建的冻结诊断 source：数据结构头 `8d9447a8…`、VM头 `b1caeed2…`、runtime `0ee7e25b…`，逐项匹配已接受 foundation 清单。编译静态断言确认 header=88B、WireValidationRange=16B、u32=4B、size_t=8B。完整源码、编译器/SDK/外部依赖、生成器20锚点和原始/诊断binary身份均在 prepared/build/sidecar 及 [最终小型封存](validation/vm-observability/network/final-v1/result.json) 中。

四种名义尺寸及原始/诊断都一致的 frame 结果如下。这里的名义尺寸仅指 u32 元素字节数；payload 还包括 arena/allocator/root/label 等内容。

| 名义元素字节 | 元素数 | wire字节W | payload字节P=W−88 |
|---:|---:|---:|---:|
| 4,096 | 1,024 | 4,448 | 4,360 |
| 65,536 | 16,384 | 65,888 | 65,800 |
| 1,048,576 | 262,144 | 1,048,928 | 1,048,840 |
| 16,777,216 | 4,194,304 | 16,777,568 | 16,777,480 |

所有夹具、两次work输出、跨pool输出逐字节相等；warm拥有型输出对照同条件原始W1的work0，不伪称存在原始W1 warm输出文件。逐元素marker/label/count/value检查仍由原W1源码执行。W1 stdout原样保留，时间字段仅归档；对照检查所有非时间语义/预算/pool字段。

[机器摘要](validation/vm-observability/network/summary-v1/summary.json) 记录16条有效trace、2,263个事件、dropped0。网络每条件none/cold/warm分别136/164/211事件，每线程最大129，最长phase标签23UTF-8字节；均低于2048硬事件和63字节标签上限。四夹具为54/55事件、单线程。没有删warm前缀、reset、采样或截断。

| 每网络条件的完整进程 | none | cold | warm |
|---|---:|---:|---:|
| 交换数（含warm） | 2 | 2 | 3 |
| 注册/注销各自次数（含preflight） | 5 | 5 | 7 |
| reserve / enable / unmap各自次数 | 5 | 5 | 3 |
| work pool hit / miss | 0 / 0 | 0 / 4 | 4 / 0 |
| warmup pool hit / miss | 0 / 0 | 0 / 0 | 0 / 2 |
| pool扫描次数 | 0 | 0 | 4 |
| domain5分配/释放各自次数 | 20 | 20 | 28 |
| domain5累计请求字节 | 1,200 | 1,200 | 1,680 |

每次交换精确观察到两次load的header复制共176B、payload复制2P，一次save的header复制88B、payload复制P，以及一次label赋值复制21B。每次verified load的domain5实际请求序列16/32/64/128B，共240B，全部释放，局部观察序峰值192B。该峰值仅属于domain5；5 owned/0 free区间数依据冻结schema/source，而四次分配本身不能区分5–8个区间。每个网络进程另包含一次preflight load及21B处理，不计入单次交换。正常请求没有Core/wire分配/归还、增长、迁移或transfer事件。

网络同时活跃注册owner峰值为1；夹具为2。warm每条件7个owner代次对应3段mapping，mapping登记分布1/3/3。load构造前缀先复制/获取映射、再register，按相同thread/endpoint/cycle/load phase/base的唯一后续注册关联，避免把复用地址归给旧代。启用pool时final clear前各池恰有1个idle mapping，reserved=R(32MiB)、accessible=R(P)；clear后均为0。结束时domains1/3/4/5无保留，domain2只保留首个preflight创建的**唯一16B registry bucket**，逐项核对原分配subject、origin_owner/thread和phase，不是宽泛允许整个domain2。

receive字符串初始化长度2W、容量快照、原有header assembly两次共176B另列在native observation中。初始化长度是逻辑constructor请求，capacity排除终止符；它们不是malloc实用字节、实际libc memset、内核复制或RSS。server字符串释放完成点在`serve_one`返回后、worker完成发布前；dump在run返回且worker已join、pool已销毁后。warm短作用域为归档延长了拥有型输出寿命，文件I/O与额外`diag.pool.check`查询均是诊断开销，保留在证据中。

[覆盖增量](validation/vm-observability/network/summary-v1/coverage-increment.json) 仅将两个原先未执行kind增加为本次实际断言：四夹具`auto_growth=4`、`extent_request=8`；属于准备阶段，不能算请求成本。该批共23种实际kind；foundation与本批合计42种普通运行kind，仍有20种没有普通运行覆盖，另有原先1种仅无效负控。foundation历史29/11/1/22分类保持原样，不用本批局部断言全局升级旧kind。

冻结工具的[36项Python测试](validation/vm-observability/network/python-tests-v1.log)通过；[实际产物小型搬迁副本控制](validation/vm-observability/network/package-controls-v1/controls.json)为1正控、11负控，raw stdout、timing标签、join、work/warm/input字节、sidecar/二进制身份、冻结头、warm前缀和binary字节变化均按预期拒绝，未新增C++执行。四个W4队列receipt离线审计通过，包含失败构建。首构建因把clang++软链接解析为clang而丢失C++运行库自动链接；失败日志保留，第二次保持driver名称后通过。两次构建已用完本轮额度。独立[源码与原始证据复核](../experiments/vm_observability/network/REVIEW.md)通过，无未解决阻塞；ledger支持正常退出后的续跑，不承诺突然断电的严格持久性。完整数据只保留一份，最终预算数见小型封存，未复制整套大型输出。

## N3原始五类指标与现有证据的对应

下表对应 [N3原始口径](VM_NEXT_STAGE_PLAN.md#n3--测量基础与默认路径归因p1依赖-n0)，区分本W2与继承的N4/N5/N6证据；未新增sampler，未为填目录扩跑63种kind。

| 原始指标 | 本W2及既有证据 | 精确/局部/估算/进程采样边界 |
|---|---|---|
| Core/wire扫描、元数据移动/宿主分配与峰值、缓存转移、尾扩展失败原因 | W2 foundation固定探针、逐kind验证表与网络分域账本；[N5 summary](perf/2026-10-15-vm-stage2-automatic/summary.json)、[N6归档索引](perf/2026-10-15-vm-stage3-allocator/index.json) | W2是指定predicate扫描及五域实际allocator请求/释放，峰值按事件观察序；Core erase是逻辑移动估算。N6 fragment/trace按vector容量变化推定分配，micro global-new仅限active区间，容量峰值仍是检查点。不是全进程分配或malloc开销。 |
| 实际迁移/复制、增长、重试、系统调用与回收结果 | W2 copy_wire/pool_reclaim/failure_vm及本批显式复制/夹具增长；[N5布局结果](VM_AUTOMATIC_TRACE_RESULTS.md)、[N4 pool/lifecycle](perf/2026-10-15-vm-stage2-pool/summary.json) | 完成的显式调用范围、完成元素逻辑extent分别计；N5历史279,600B布局差与重试差可追溯。实际OS结果、注入拒绝和未执行失败路径分开；STL/libc/kernel及递归transfer完整物理流量未覆盖。 |
| used_end、逻辑长度、accessible、reserved、Core/wire空闲、池预算 | N5诊断、N6布局与[交叉检查](perf/2026-10-15-vm-stage3-allocator/diagnostic-timing-crosscheck.json)、N4 idle/cleared/budget；W2网络mapping/registry代次及pool前后状态 | 是指定检查点或独立生命周期账本。Core空闲含分配头，wire free与物理donor另列；pool预算不含live owner。不能合并为RSS或有效用户字节。 |
| RSS / working set独立采样 | N4 summary的rss_before/idle/cleared与归档lifecycle；[既有F回收结果](VM_MEMORY_REUSE_RESULTS.md) | 本机进程检查点采样，不是连续峰值或单arena独占。N4 reused字段在idle_pool/live_reclaim生命周期不同。W2无新增RSS sampler，capacity/accessible/请求字节均不能替代RSS；Windows working set未覆盖。 |
| 多buffer吞吐、owner索引争用、等工作量控制 | [N4等总预算线程矩阵](VM_POOL_WORKLOAD_RESULTS.md)；W2 threads/owner_basic/owner_transfer/core_metadata及锁账本；本网络单在途同步链 | N4吞吐是OS层池获取/归还，未含完整owner索引/反射；W2是限定诊断，锁计时受插桩扰动，线程等待累计可超墙钟。代表性完整库多buffer吞吐与索引争用的非插桩归因仍未由本批完成。 |

本次补齐的是冻结网络消费者的显式成本与生命周期证据。阶段时间、复制字节占比和warm映射复用均不能换算端到端可节省比例，也不构成候选采用依据。完整N3、可靠默认D/F归因和正式性能结论继续由各自gate管理。
