# 固定请求 owner scaling：旧基准审查与最小设计建议

2026-10-16，design-only；仅只读分析和本文件写入。未实现 harness、编译、测试、运行负载或计时。由 root 合并后一次冻结；以下建议不构成执行授权。

## 旧矩阵实际测了什么

源码：`tools/bench_vm_improvements.cpp` SHA256 `5e40e681af29bcc442ca7b65696cfb5efab6a45aa388601851c2992532d41551`；它在行 3–13 文本包含 `bench_vm_compare.cpp`（SHA256 `e1da4e993f477770259e3c79a6b5fc097192f587dccdb9b30a972055857ffa2d`），后者行 48 固定 `allocations_per_buffer=16`。

- `run_vm_improvements.py:44–48` 是 owners `{1,100,1000,5000}` × threads `{1,2,4,8}` × order `{sequential,random}`，共 32 条；不是当前所需六条。`--cycles` 未在 `owner()` 中重复四阶段。
- `owner():171–210` 为每个 owner 分配 16 个 `uint64_t`，随后释放 16 个；总用户调用数为 `32*owners`（行 256），因此 owner 数量与实际工作量混在一起。每 buffer 初始 logical=4096、显式 cap=64KiB，无 pool。
- 四阶段 wall timer 是主线程从第一个 barrier 前到第二个 barrier 后（行 222–228），包含 rendezvous、调度和慢 worker；线程创建与行 230–247 的值校验在计时外。`operation_samples_ms={allocate_ms}` 是整个 allocation block，不是每 lookup 延迟。
- 旧 allocate 在行 204 把 payload value store 算进计时；新设计若要求“值写入/核验均在计时外”，须明确移出，而非宣称旧脚本原样已满足。
- 全局 random 顺序的 `i = stage==0 ? pos : order[pos]` 使 owner 创建线程为 `i % T`，后续线程却由 shuffled position 决定。阶段内仍然 disjoint，但并非整个 owner 寿命固定 worker。首版只取 sequential；若以后增加乱序，应在各 worker 自己的 owner 子集内 shuffle。
- 旧 worker 异常会写本 lane 的 `exception_ptr` 并 drain barrier；但主线程阶段间的 `capture`、`root()`、memory query 等仍可能抛出。新实现须处理这些异常和部分线程启动失败，不能在未 join 时直接离开或使 barrier 失配。

## 真实热路径与不能据此推出的结论

固定 F 双头仍为主头 `9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2`、VM 头 `22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad`。

`allocator<uint64_t>()`（主头 3574–3577）返回保存 arena 指针的 allocator；每次 `allocate(1)`（93–100）→ `arena_header::allocate_aligned`（156–157）→ `owner()`（176–177）→ `owner_from_payload_ptr`（442–448）：先锁全局 registry mutex，再精确基址 hash find。`deallocate`（103–105/171–172）同样每次查一次 owner。此锁只覆盖 owner 查找，不覆盖随后 Core 的分配/释放；因此必须让每个 buffer 的全部操作与析构固定在同一 worker，不能误当作 XBuffer 并发修改安全。

Core 分配（648–687）还包含 free-block 搜索、对齐、extent 检查和 allocation header 写入；释放（719–734）还包含宿主 `free_blocks_.push_back`，首次增长可能分配宿主内存，且 OOM 会 best-effort 丢弃回收记录。不要改成 fixed wire allocator/container 路径：其 freelist hit 可不经过 registry，已不是本实验热路径。也不要通过直接缓存 `XBufferCore*`/改头绕开锁；那会成为另一个候选。

构造中的 allocator/root 存储由 Core 内部直接分配（614–630、3184–3189、3341–3347），不会因为造根自动等价为一次 `owner_from_payload_ptr` 查找；create 的注册、destroy 的注销应与请求 lookup 分开。registry 仍是平均复杂度 hash，现事件不统计 hash probe 次数。timing block 也不能单独证明时间差全部来自 registry。

## 建议冻结的最小工作量

推荐首版 `R=1024` 次用户分配，再做 `R=1024` 次用户释放，总 2048 次，所有条件一致；R 表示**每阶段全进程总请求数**，不是每 owner、每线程或 alloc+free 合计。每帧只做一轮完整 create/allocate/free/destroy，不在 child 内自适应增 R 或重复到稳定。

| 精确条件 ID（建议） | owners O | threads T | 每 owner 分配 q=R/O | 每 worker owners / 分配数 |
|---|---:|---:|---:|---:|
| `owner_fixed_r1024_o8_t1` | 8 | 1 | 128 | 8 / 1024 |
| `owner_fixed_r1024_o64_t1` | 64 | 1 | 16 | 64 / 1024 |
| `owner_fixed_r1024_o256_t1` | 256 | 1 | 4 | 256 / 1024 |
| `owner_fixed_r1024_o256_t2` | 256 | 2 | 4 | 128 / 512 |
| `owner_fixed_r1024_o256_t4` | 256 | 4 | 4 | 64 / 256 |
| `owner_fixed_r1024_o256_t8` | 256 | 8 | 4 | 32 / 128 |

`o256_t1` 只登记一次。O/T、R/O、R/T 均整除；固定 `owner i -> worker i%T`，该规则覆盖 create、allocate、free、destroy 和失败清理。每个 worker 依次访问自己的 owner，每 owner q 次，不增加 random 维度。

每 owner 使用 no-pool、`logical=maximum_capacity=64KiB`、`Explicit` growth。create 阶段承担统一每 owner 的 reservation/enable/zero；allocate/free 阶段不得 grow、reclaim、load/save、move/swap、清池或构造其他 owner。最大 q=128 的小块占用远低于 64KiB；仍须按固定 F 的实际 sizeof/alignment 在冻结时证明容量，而非靠运行中扩大容量/自动回退。这样 O 改变时 create/destroy 本身工作量本来就是 O，须分别报告每 owner 成本；不得把四阶段总和/R 当成“相同工作量 registry lookup”结果。

R 小意味着短 block 的 barrier/scheduling 可能占较大比例，应由 W3 按预登记校准裁决；不能宣称已达到测量精度、逐 lookup 延迟或生产尾延迟。若决定提高 R，必须在一次最终冻结前同时解决以下 recorder 容量和固定逻辑容量约束，不自动延伸矩阵。

## 阶段语义与验证

在起线程前分配所有 harness 元数据：O 个 owner slot、R 个 pointer slot、worker 进度/错误槽和固定 owner 索引；这些不是库 registry/Core 成本。线程全部成功启动后才进入四阶段；部分启动失败需让已启动 worker 从启动等待直接退出并 join，不能等待一个永远缺席的 barrier participant。

1. **create timed：** 固定 worker 逐个构造自己的 XBuffer 并 `make<ScalarRoot>()`；只构造 allocator/root，不写 marker 值。全部 owner 创建完成并停在下一阶段入口后，主线程在计时外写 `marker(i)`，检查全部 O 个独立非空地址、64KiB logical/cap 和固定归属。不要在主线程把 owner 移动到新位置。
2. **allocate timed：** 每 worker 在自己的 buffer 上调用 `allocator<uint64_t>().allocate(1)` 恰好 R/T 次，只将返回 pointer 写到预分配 slot；不写 payload 值、不算 checksum、不查询 RSS、不输出。所有 O 个 owner 全程同时存活。结束后计时外写 `value(i,j)`，逐值再读核对，校验所有 R 个指针在所属 owner 的 logical 范围、对齐、相互不重叠且与 root 独立，marker 未变；完整核对之外可输出 convenience checksum。
3. **free timed：** 固定 worker 用原所属 allocator 逐一释放所有已分配 pointer，恰好 R/T 次；不在释放后再解引用。结束后在计时外确认仍有 O 个 owner、root marker 正确、used_end/logical/base 未意外改变。Core 的 host metadata 工作属于真实 free 成本，不能计时外预热并隐去。
4. **destroy timed：** 各 worker 只 reset 自己的 O/T 个 owner；结束后确认 slots 全空，再 join。timing JSON/trace dump/文件输出均在 join 和所有相应生命周期结束后。

复用旧的“主线程起钟→start barrier→end barrier→止钟”模式，并在 timer 名称中明确包含 barrier/调度。阶段间写值、核验、内存采样均在计时外、worker 已 rendezvous 后。错误只记录，不在 worker 或主线程中越过 barrier 抛出；失败时仍按固定 worker 清理已成功创建/分配的部分、drain 余下阶段并 join，最终样本非零退出且不可作为计时样本。不能因 cancel 标志而直接跳过最终清理。

## 诊断接入和 recorder 边界

可复用统筹已接受 W2 frozen implementation 的 generated F 双头、`n3_runtime.hpp` 和 `audit_events.py`，不重做 63 种事件。原始 F timing 构建无探针；诊断构建相同 R/O/T/顺序/值语义，worker 中四个静态 `n3::Phase` 标签；原始 counts 与诊断计时不混用。

预期 workload-specific 断言：allocate lookup_hit=R、free lookup_hit=R，lookup_miss=0；两阶段每个 owner 各 q 次、每个 worker 各 R/T 次；allocate `core_new=R`、`extent_request=R`、`core_reuse/core_scan/auto_growth=0`；free `core_return=R`、`core_return_failed=0`；create register=O、destroy erase=O、rebind=0，无 pool/wire/图验证事件。create 的 allocator/root Core 分配为 2O，单列不混进 R。销毁后无 active registry generation/VM owner/Core metadata；registry bucket 保留允许存在，单独列而非要求所有宿主分配归零。

free metadata 的累计分配次数与 transient peak 会随 q、O、标准库 capacity policy 改变；即使总用户 R 一样，它也不是纯 lookup 试验。诊断需要保留并分别报告这些精确事件、registry lock wait/hold 总和及 workload wall time；跨线程等待/持有之和可大于 wall time，不能相加推出端到端可省比例。

冻结 runtime 每线程仅 16384 records、全生命周期最多 16 recorder threads，且无 reset/清 trace。较大 R 不能直接接入：R=8192 时仅两阶段的 lookup_hit+registry_lock 已达 32768 条（T=1），必超限；R=4096 仅这四倍 R 即占满，实际还远多于此。推荐 R=1024 是为保留原 recorder：按固定成功路径预计最坏 O256/T1 在约 14k 条以内，但 root 最终必须给出保守、覆盖 registry rehash/host metadata 的明确静态上界并在运行时严格拒绝 valid=false/dropped>0；本文件没有执行来证明实际事件数。若换 runtime 容量必须做独立副本与身份/契约审查，不能改 W2 冻结材料或静默截断。

## 依赖闭包与最少负向控制

- 冻结新的独立 harness、runner/contract/auditor 和 F 双头。若文本包含旧 harness（不建议带入无关 workload），`bench_vm_improvements.cpp -> bench_vm_compare.cpp -> F -> VM/TypeLayout` 全部是依赖；只哈希入口 cpp 不充分。只复用其 barrier 思路可以缩小闭包，但仍须保留引用来源身份。没有独立的 `bench_common.hpp` 依赖。
- F 直接包含 `<experimental/meta>` 与 `external/typelayout/include/boost/typelayout.hpp`，递归到 config/fwd/fixed_string/signature/opaque/admission/layout_traits/type_map/signature_impl/reflect/sig_parser 等。记录 TypeLayout commit `9a6de1d73617631a9612ba9f572d6c0844a90b19`、实际用到的完整头树与编译依赖清单；不要借历史 harness 的 include 参数声称本次必须链接所有 Boost 库。Boost 当前 commit `da041154c6bac1a4aa98254a7d6819059e8ac0b0` 若仍在实际命令/闭包也列出；C++ 标准库、P2996 compiler/SDK、OS headers、-pthread、link/runtime 身份由最终准备材料记录。
- 离线审计负例至少拒绝：缺/重/换六条件、R 被当每线程而倍增、线程/owner 归属变化、丢分配或双 free 计数、错误 marker/value/pointer 范围、缺 phase 或 lookup 数不符、错误 header/harness/工具依赖 hash、trace overflow/dropped/非法 thread 数以及 end-state 存活 owner。改写证据副本的负控不得污染原始样本。
- 功能失败控制设计至少覆盖：一 lane 在 create 中途失败、allocate 中途失败、主线程阶段间核验失败、部分线程启动失败；须真实排空/join、不死锁、不误销毁他人 owner、不接受部分样本。保留精确失败原因和成功进度；不能任意异常算负控通过。重用已接受 runtime 的 overflow/control 证据可注明继承，不为本缺口重开全部事件矩阵。

本建议仅填补当前 F 固定请求、多 owner 与 registry 争用的设计缺口；不重跑历史 B/C/D 矩阵，不成为旧网络/I/O 候选的实现授权，也没有测量结果。
