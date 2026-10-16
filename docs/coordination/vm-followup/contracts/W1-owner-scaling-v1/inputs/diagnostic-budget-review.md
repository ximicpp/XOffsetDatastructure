# N3 owner scaling：冻结诊断入口与事件预算审查

2026-10-16；设计准备，仅阅读源码/JSON和轻量文件身份检查，未实现、构建、测试、执行 workload 或计时。下列命令为待统筹冻结的精确拟议入口，不是已存在的新 harness 或执行结果。

## 1. 固定工作量及线程寿命

采用统筹指定的最小 one-batch：每个新进程选择唯一 `(owners=C, threads=T)`，固定诊断总请求 `R=256`；六条件为 `(8,1),(64,1),(256,1),(256,2),(256,4),(256,8)`。每 owner 分配 `Q=R/C` 个独立 `uint64_t`，先完成全部分配/写值，再验证，最后逐个释放；不是每 owner 固定轮数，也不插入复用轮次。每条件都有 **256 成功 allocate + 256 对应 deallocate**。

每 owner 为直接构造的 `XBufferCore(65536, maximum_capacity(65536))`：无 pool、Automatic、typed root、wire、load/save、move/swap。每线程固定持有 `C/T` 个互不重叠 owner，`XArenaAllocator<uint64_t>(owner.arena()).allocate(1)`/`deallocate(p,1)`各触发一次 registry lookup（F 主头 76–106、157–178）。allocator/owner 的调用路径不能换成绕过 owner lookup 的私有方法。使用稳定 owner 归属，owner 对象不因外层容器增长而移动；同一 worker 完成其 owner 的 create/allocate/validate/free/destroy。

仅建立一次 T-worker team，按固定 owner 分区和固定 owner/request 顺序操作；`create → allocate → validate → free → destroy` 的各全局阶段边界同步，特别是全部 C owner 已注册之后才 allocate、全部请求验证/释放完成后才 destroy。这样全操作阶段 active owner 数确为 C。主线程不调用库操作、不发自定义事件，只在全部 worker 已结束其 owner/allocator 生命周期并 join 后 dump。每个 worker 使用五个静态 Phase 标签；Phase/Scope 自身不 emit。`thread_count=T<=8`，即使以后主线程加事件也只能额外一个 slot；不可每阶段换一批线程（16 是进程寿命线程槽，退出不归还）。

建议 timing 固定总请求 `R=8192`，仍一批、同六条件，最多 `Q=1024`。F 的 alignment 至少16、AllocationHeader 16字节，首次 uint64 block end 为72，此后每次增加32，`Q=1024` 最终约32808，低于65536；`Q=2048` 已约65576，不能把 Rtiming=16384 误称容量内。失败不得被计作成功请求。诊断 R=256 与 timing R=8192 用于不同目的；诊断只能提供有范围的操作计数/成本结构，不证明两种请求规模的性能等价。

## 2. 接受的诊断身份与观测语义

父 gate：`docs/coordination/vm-followup/W2-G1-acceptance.json`，状态 `accepted_scoped_foundation`，冻结包 `docs/validation/vm-observability/frozen-implementation-v1/`，snapshot SHA256 `fc1780b5ce1f6cb7cfbbdfb85896d9746740c20b5e3df59bddedee5a42bb3c8d`。只用包内 **build/input/diagnostic/** 的 generated 双头和 runtime，不从 W2 当前开发目录另生成或拷贝新探针。

| 文件 | 接受的 SHA256 |
|---|---|
| generated `xoffsetdatastructure.hpp` | `8d9447a81ca07f758fa813fbcc0823f8e8910d94cc2d9214bd7fda757a2964e6` |
| generated `xoffset_virtual_memory.hpp` | `b1caeed243f6ba412c3f68104730df21e0ad580193adc1c3186749c76c1c4e57` |
| `n3_runtime.hpp` | `0ee7e25b9a76f0e7b06acc25554be57219cd17982e3d6e1898a376ddacfba10a` |
| `source/audit_events.py` | `91ac07d1c54f306f965235b156a7ff95f18040c8eb65660434654342a626ee08` |
| `source/event_kinds.json` | `7bc7b0a77d24a8557a601a793daead2690406603fed26d78931cd3b942bb2e2c` |

Runtime 56–58 固定16 lifetime threads、每线程16384events、标签最多63 UTF-8字节；209–235 每次 emit 一个事件。线程/事件/标签溢出使整条 trace invalid/dropped，**不能采样、截尾或 reset 后继续解释**。从全新进程开始记录全部 setup/teardown，禁止不记录的 warm-up。dump 必须在 emitter quiescent 之后；raw JSON、完整 events 和审计结果均保留。

registry register/erase/rebind 的 owner 是 C++ Core 对象，subject 是 arena base；成功 register 的 sequence 定义 generation，erase 结束，不能只按裸地址把多个寿命合并。registry lookup_hit 指向当前注册对象。domain1 是 Core free-vector allocator；domain2 是共享 registry nodes/buckets，owner 只表因果来源，不是该 owner 的独占成本。host_deallocate 在实际后端释放前记录，其原始 owner/domain/bytes 由 allocation pointer ledger 配对；不能用析构时 Scope=0 覆盖分配来源。registry_lock 的 bytes/value 为受探针扰动的 wait_ns/hold_ns，不是内存字节，也不能把跨线程时长相加后称端到端耗时。

## 3. 源码导出的完整保守预算

以下是成功普通路径的**静态预期**，必须由首次实际 trace核对；不是本次执行结果。落点为 frozen generated H:460–479/538–567（setup/registry/teardown）、H:665–709（新分配）、H:743–780（free/vector metadata）、VM:141–182/241–249/373–378（reserve/enable/zero/unmap）。

每 owner setup 固定7events：reserve、enable、grow_zero、registry node host_allocate、register、metadata、registry_lock；另有 bucket 增长事件。每个 allocate 为 lookup_hit、registry_lock、core_new 共3；每个 free 为 lookup_hit、registry_lock、core_return、metadata 共4，另计 free-vector reallocation 的 allocator 事件。每 owner teardown 固定4events（node host_deallocate、erase、registry_lock、unmap），另计 free-vector最后释放。

当前已接受 libc++ vector doubling（`__vector/vector.h`:900–907）使 Q 为2的幂时每 owner 有 `g=1+log2(Q)` 次 domain1 allocation和同样次数 deallocation（含最后析构）；没有 reserve、free-list复用或中途 shrink。registry 单次 emplace 最多一次 bucket rehash（`__hash_table`:1419–1438/1790–1793），C次插入最多C次bucket分配及C−1次旧bucket释放，故 `B<=2*C−1`；最后bucket保留。由此：

`E_process = 7*R + C*(11+2*g) + B <= 7*R + C*(13+2*g) − 1`。

每 worker的非bucket事件均匀为 `[7*R+C*(11+2*g)]/T`；保守地把**全部全局 B**压在同一个worker，也不越界：

| C | T | Q | g | 整进程events上界 | 任一worker events上界 |
|---:|---:|---:|---:|---:|---:|
| 8 | 1 | 32 | 6 | 1991 | 1991 |
| 64 | 1 | 4 | 3 | 3007 | 3007 |
| 256 | 1 | 1 | 1 | 5631 | 5631 |
| 256 | 2 | 1 | 1 | 5631 | 3071 |
| 256 | 4 | 1 | 1 | 5631 | 1791 |
| 256 | 8 | 1 | 1 | 5631 | 1151 |

全部setup/teardown合计也能塞入单slot的16384，不需要扩大 runtime、拆trace或抽样。未来若增添 owner move、额外 rounds、typed/schema、pool、reclaim、故障注入或额外 snapshot，此预算即失效，必须重新冻结。

### 自定义 workload 必须断言的实际计数和残留

- `registry_register=C`、`registry_erase=C`、rebind/miss/register_failed均0；恰好C个完整 generation，最大active=C、末尾active=0。
- `registry_lookup_hit=2*R=512`，`registry_lock=2*C+2*R`；每phase分布必须匹配真实路径，lookup不得发生于未注册/已结束generation。
- `core_new=core_return=R=256`；`core_metadata_capacity=C+R`；core_scan/reuse/erase_movement、core_return_failed、extent/auto_growth/expand、wire/pool/copy类事件均0。
- `os_reserve=os_enable=grow_zero=os_unmap=C`，各自成功，grow_zero总bytes=`65536*C`；无失败、reclaim、protect或replace。reserve/unmap的bytes按实际OS page rounding核对，不混成RSS。
- domain1 allocate/deallocate各`C*g`，末尾live=0；domain3/4/5没有事件/残留。domain2 node分配/释放各C、bucket增长完整保留；末尾**恰好一个共享registry bucket allocation**，不能强迫所有host allocations归零或忽略该残留。在当前64位、默认max_load_factor=1且无reserve的已固定libc++，桶增长2→5→11→23→47→97→197→397，预期C8/C64/C256最终11/97/397个指针，即88/776/3176字节；这是待核对的依赖限定预期，实际 ledger须匹配，不可只凭大小给分配对象分类。
- `validate_trace(trace)`负责通用顺序/类型/allocator/registry审计，自定义层再断言上述计数、phase、生成寿命、精确成功数/逐值语义和资源残留。通过固定W2 probe审计不等于通过此workload。所有断言失败、invalid、dropped>0、事件超预算都使该process不可解释。

## 4. 拟议 CLI、编译角色及依赖闭包

拟议源文件是 `experiments/vm_owner_scaling/owner_scaling.cpp`（本审查不创建它）。CLI固定为 `--owners C --threads T --requests R`，输出单个JSON（包含条件/成功计数/语义结果；diagnostic带完整trace，timing无trace）。六条diagnostic argv仅将下列 `(C,T)`代入，不自动调R或重跑直到通过：

```text
owner_scaling_diagnostic --owners 8 --threads 1 --requests 256
owner_scaling_diagnostic --owners 64 --threads 1 --requests 256
owner_scaling_diagnostic --owners 256 --threads 1 --requests 256
owner_scaling_diagnostic --owners 256 --threads 2 --requests 256
owner_scaling_diagnostic --owners 256 --threads 4 --requests 256
owner_scaling_diagnostic --owners 256 --threads 8 --requests 256
```

采用 accepted build-06 的编译基础 argv（它们是既有可用profile，不是对新harness已经成功构建的声明）：

```text
/usr/local/bin/clang++ -O2 -std=c++26 -freflection -fexpansion-statements
  -stdlib=libc++ -pthread -arch arm64
  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk
  -mmacosx-version-min=10.15
  -isystem /Users/fanchensu/XOffsetDatastructure/external/boost/libs/container/include
  -isystem /Users/fanchensu/XOffsetDatastructure/external/boost/libs/core/include
  -isystem /Users/fanchensu/XOffsetDatastructure/external/boost/libs/config/include
  -isystem /Users/fanchensu/XOffsetDatastructure/external/boost/libs/throw_exception/include
  -isystem /Users/fanchensu/XOffsetDatastructure/external/boost/libs/static_assert/include
  -isystem /Users/fanchensu/XOffsetDatastructure/external/boost/libs/container_hash/include
  -isystem /Users/fanchensu/XOffsetDatastructure/external/boost/libs/type_traits/include
  -isystem /Users/fanchensu/XOffsetDatastructure/external/boost/libs/winapi/include
  -isystem /Users/fanchensu/XOffsetDatastructure/external/boost/libs/intrusive/include
  -isystem /Users/fanchensu/XOffsetDatastructure/external/boost/libs/predef/include
  -isystem /Users/fanchensu/XOffsetDatastructure/external/typelayout/include
  -L/usr/local/lib -Wl,-rpath,/usr/local/lib
```

两角色append以下精确拟议argv（源/输出目录待统筹冻结，不能覆盖旧产物）：

```text
timing: -DN3_DIAGNOSTIC=0
  -I /Users/fanchensu/XOffsetDatastructure
  -MD -MF /Users/fanchensu/XOffsetDatastructure/build/vm-owner-scaling/build-v1/dependencies-timing.d
  /Users/fanchensu/XOffsetDatastructure/experiments/vm_owner_scaling/owner_scaling.cpp
  -o /Users/fanchensu/XOffsetDatastructure/build/vm-owner-scaling/build-v1/owner_scaling_timing
diagnostic: -DN3_DIAGNOSTIC=1
  -I /Users/fanchensu/XOffsetDatastructure/docs/validation/vm-observability/frozen-implementation-v1/build/input/diagnostic
  -MD -MF /Users/fanchensu/XOffsetDatastructure/build/vm-owner-scaling/build-v1/dependencies-diagnostic.d
  /Users/fanchensu/XOffsetDatastructure/experiments/vm_owner_scaling/owner_scaling.cpp
  -o /Users/fanchensu/XOffsetDatastructure/build/vm-owner-scaling/build-v1/owner_scaling_diagnostic
```

新source必须在diagnostic分支先include frozen runtime再include generated头，timing分支只include实际F，**不include runtime/探针/诊断Phase**。W2 `n3_original` 虽为N3_DIAGNOSTIC=0，只是semantic comparator，不能直接充当正式timing。timing需绑定当前F双头SHA `9bc8f1f5…7640bb2`/`22f181b5…8239ad`，与新source/timer边界单独冻结。

父接受的build结果 `.../frozen-implementation-v1/build/result.json` 保存两条成功编译argv及1018份external依赖hash：boost commit `da041154c6bac1a4aa98254a7d6819059e8ac0b0`，typelayout `9a6de1d73617631a9612ba9f572d6c0844a90b19`；compiler实际目标 `/usr/local/bin/clang-21` SHA `b63aa0e51a70e4569e75ed49c575361aa20cbe5d9c3ef7457fb8b5db83763a8f`；libc++ `55a5a76021ef1d69f1049f39da0b5d7de357a442485b0c18eaa93fe9707e67bc`，libc++abi `608567f0ee857f57f0dfb6a3f180be5d48d82556409bf197419fe1e8cb1385a6`。上述vector/hash表源码hash亦在该closed manifest中。

新harness包含新同步/容器/时钟头时，旧1018文件清单**不自动等于新依赖闭包**；未来构建必须从自己的两份`.d`收集实际传递头与compiler/SDK/runtime链接身份、所有本地helper、两角色source及原始/生成双头/runtime/auditor/catalog/contracts的hash，冻结argv和binary。审计器邻接`event_kinds.json`，不可只带单个`.py`。新结论只适用于核实过的闭包。

获统筹实施/执行派发后，整个构建/诊断任务应作为 `experiments/vm_coordination/run_job.py run --owner W3 --kind build|diagnostic --out NEW_RECEIPT --timeout-seconds N -- COMMAND...` 的单个COMMAND，通过默认queue.lock；具体owner可由统筹重新派定。不能嵌套同锁。formal timing仍只由W3启动。当前文档不授权上述命令，不读取或改写W3实验文件，也不占用执行队列。
