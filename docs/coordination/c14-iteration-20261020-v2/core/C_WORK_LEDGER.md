# 固定 C 诊断的有限工作账本

本账本只分析已经完成的 C14→R1 两次固定诊断，不启动编译、native、warmup、补 run 或筛选。结论是：R1 注册目标确实经过 inactive 路径；首次 owning emit 主要观察区间仍是完整 payload 复制及同时发生的 minor faults；整条 child 还包含大量输入与独立审计工作。下面是单次插桩观察值，不计算候选速度比、置信区间、精确 CPU 占比或5×收益。

机器可复核数据是 `C_WORK_LEDGER.json`；生成器 `tools/c14_iteration_20261020_v2/core/analyze_c_ledger.py` 只读冻结回执并以新文件输出。它校验 C_RESULT 中两角色 AUDIT/observer/compile 哈希、observer 所列全部原始日志哈希、36条 raw event 与解析结果逐项相同，以及原 stdout 的三条 JSON 与审计中 emit/count/replay 完全相同。语义通过来自独立 AUDIT；observer 自身的 `semantic_validation=not_performed_by_observer` 没有被冒充为真值通过。未重新读取大 body 冒充一次新语义验证。

## 完整进程、边界与资源

原全局监督器 job 共26.272397881s，包含两次编译、两次 child、依赖/身份/资源检查、审计和封存。两编译实际 wall 合计10.884084354s，两 child 合计4.982224939s，其余外层步骤合计10.406088588s；这个外层差额不能全称为审计或库运行时间。退出后 cleanup 的 live/process_groups/zombies 均为空，6459个 source/runtime/input 由已通过 C 回执证明前后不变。

| 互不重叠的 wall 范围 | C14 ms | R1 ms |
|---|---|---|
| 输入 wire+trace 读取 | 887.414204 | 1101.559283 |
| trace decode | 14.624223 | 19.617766 |
| 输入两份 SHA | 67.453079 | 68.650325 |
| primary t0..t8（事件边界） | 87.135628 | 75.492739 |
| 原 post-audit load | 15.285579 | 15.069626 |
| 原 post-audit export/encode/retire | 34.548071 | 22.615419 |
| 原 post-audit canonical hash | 22.801537 | 23.118096 |
| 新增九阶段 truth replay（事件边界） | 1049.144719 | 1153.334977 |
| replay 后至首写：query 编码/结果哈希等，未细分 | 112.209426 | 122.014221 |
| wire 文件写入 | 7.229289 | 10.280708 |
| canonical 文件写入 | 3.122664 | 6.821047 |
| queries 文件写入 | 4.759797 | 5.863197 |
| native result 文件写入 | 0.049823 | 0.063828 |
| 其余启动/退出/flush/边界间隙 | 22.816878 | 29.128790 |
| 完整 child | 2328.594917 | 2653.630022 |

表中 wall 均使用同一 CLOCK_MONOTONIC 域的 **child 产生时间**；不使用 monitor 收到时间替代执行边界。选定区间互不重叠，其余间隙单列；JSON 另给出了 launch→36事件→wait4 的完整相邻分区，其和严格等于 child wall。input.read 包含原 wire 13,702,104B 与 trace 14,482,121B 的读取、缓冲分配等；虽然路径是 /mnt/c，现有事件不能证明其中多少是磁盘等待、调度或内存工作。

| 资源采样范围 | C14 CPU ms / minor | R1 CPU ms / minor |
|---|---|---|
| 完整 child wait4 | 1091.909 / 54923 | 1161.907 / 54933 |
| primary aggregate | 86.734 / 16726 | 76.429 / 16726 |
| replay sample，末尾 JSON flush 前 | 648.610 / 10678 | 693.341 / 10678 |
| 其余资源差额，未细分 | 356.565 / 27519 | 392.137 / 27529 |

所有已采样范围 major faults 均为0。完整 child 的 user/system 分别为 C14 909.263/182.646ms、R1 977.152/184.755ms；不包括 Python observer 的 CPU。CPU/缺页差额由完整 child 减去互不重叠的 primary 与 replay 采样得到，覆盖输入/启动、原 post-audit、query 编码与哈希、写文件、观察和结束等；**没有逐项 rusage 边界，不能按 wall 比例分配给它们**。这也不是精确 CPU 份额估计。sample 的时钟和 rusage 是顺序 syscall，CPU 微秒记账与纳秒 wall 边界并不严格同步，极短区间不可据此求利用率。

replay 的资源采样 wall 为 C14 1049.070657ms、R1 1153.284761ms，位于 replay 外部事件范围内；其最终 JSON 构造/flush 在资源结束采样之后。replay 重新读一次原输入、load一次、initial export/check_trace一次、执行9组 mutate+export+hash，以及两次各55,299条 query，消耗28,673个 mutation operations；owning emit 为0，主计数关闭。它是另列的完整正确性工作，不能误算进 primary mutate 或首次 owning 输出。

## 原八阶段

| 原 primary 阶段 | C14 ms | R1 ms |
|---|---|---|
| load | 13.393245 | 9.107055 |
| query_before | 9.285592 | 8.387430 |
| mutate | 30.065781 | 23.250897 |
| emit_after | 5.782268 | 5.811768 |
| retire_modified_state | 0.831167 | 0.911894 |
| reload | 16.294800 | 17.494209 |
| query_after | 10.869904 | 9.834470 |
| cleanup | 0.611364 | 0.693737 |
| 合计（native 内部 clock） | 87.134121 | 75.491460 |

native 内部 t0..t8 与外部 event(t0)..event(t8) 边界位置有微小差异，两者是替代观察视图，不能相加。各阶段没有单独 CPU/缺页采样；只有 primary 合计与 emit 内嵌采样。t4 后的 emit POD/JSON flush 位于 retire 段开始前部，因此此诊断的 retire 值包含 observer 成本。t8 后的 count flush 不在 primary clock 内。读表中 R1 数值不能当作随机配对加速实验；固定先后两次运行、插桩、缓存、系统状态均不提供性能推断。

## 控制首次 owning emit：相邻区间与嵌套边界

C14 原始一次 owning emit 输出17,830,872B=88B header+17,830,784B payload；invocations=1，seen=511，9个 sample 的 clock/rusage status 全0。未在 t0 前 validate/export/owning warmup。完整 emit_after 为5.782268ms，内部 sample0→8为5.741193ms，差额0.041075ms处于调用/返回等未细分部分；这不是另一笔可另加的 emit。

| C14 相邻 sample 区间 | wall μs | user+system μs | minor/major faults |
|---|---|---|---|
| 0→1 normalization_root_allocator | 3.315 | 3 | 0/0 |
| 1→2 header_preparation | 0.355 | 0 | 0/0 |
| 2→3 string_and_checks | 0.220 | 0 | 0/0 |
| 3→4 resize_to_callback_string_preparation | 60.724 | 60 | 2/0 |
| 4→5 header_copy | 0.313 | 1 | 0/0 |
| 5→6 payload_copy | 5672.399 | 5648 | 4352/0 |
| 6→7 callback_return_and_resize_return | 3.642 | 2 | 0/0 |
| 7→8 save_return_tail | 0.225 | 0 | 0/0 |

相邻区间在 sample0→8内部可作同一次观察的分割；sample0→8、emit_after、primary、whole child 是包含关系，**不能把这些总量再相加**。0→1包含 normalization、root/allocator 准备；3→4包含 string resize/allocator/库和observer工作，不能称为纯 malloc；5→6是完整 payload memcpy 所在区间，也包含缺页服务及采样边界开销。

控制 string 准备3→4为60.724μs、2次 minor faults，payload复制5→6为5.672399ms、4,352次 minor faults；总0→8为4,354次 minor、0 major。4,352次 minor发生在复制区间，符合触及 fresh output pages 的现象，但没有故障地址或内核调用栈，不能将每个 fault 精确归因给目标页；也不能在复制时间之外再加一份“fault耗时”。此观察只证明页物化与完整复制确实在发生，不是复制纯带宽或分配器纯耗时测量。

R1对应 string准备49.043μs/2 minor、payload复制5.711476ms/4,352 minor、内部emit总5.769483ms/4,354 minor；数据口径相同，仍不建立速度差。部分 rusage 增量甚至可能大于对应 wall 的微秒换算，这进一步禁止把顺序采样的短区间比值称为精确 CPU 占比。完整 child peak RSS 为 C14 144,536KiB、R1 144,540KiB；包含 replay、audit及observer，不能当作裸 benchmark 峰值或未来尾部保证。

## 已命中的工作与仍待证明的候选

R1 read_sum 的2,439,240次 canonical 调用全部 inactive，其中 query_before 1,216,548、query_after 1,222,692；主运行合计12,081,262次均inactive，active为0。两角色所有相同逻辑计数保持一致，九阶段守恒和真值通过。结合已审裸 codegen，这说明原 inactive wrapper 路径有真实命中；不跨调用缓存 acquire，不将减少的调用机会换算为已测速度。emit本身 canonical计数为0，因此不能把 R1命中解释为消除了这次17.83MB物化。

共同 C14 控制在主 mutate、第7修改阶段观察到 owns320,465、vectors154,575、capacity_hits0、nontrivial_misses28,652、nonempty_misses16,131、vector_data_reads228,010。其他修改阶段这些owns/vector指标为0。M2的目标因此有控制侧真实 eligible 工作，但候选实际减少解析仍须唯一 M2观察通过，不能只凭控制或静态4→1宣布命中。

在固定闭合 source/type 模型下，控制 data_reads = vectors+2*N+K，得到 K=16,131；模型预期 M2可移除73,435次 const-data调用与48,393次 canonical解析。这是 **运行候选前的源码推算**，不是候选观测或耗时收益；实际准入仍使用事前冻结的完整计数守恒/正命中判据和相同真值审计，不因推算改变规则。R1和M2减少的解析工作可能重叠，不能叠加百分比或拼成组合样本。

首次 owning 输出仍要完成原约定的fresh独立string、header与完整payload复制；本次控制 normalization只观测到微秒量级，没有证据支持通过删 normalization得到正式emit比例目标。copy期间的minor faults、输入读取及replay工作不能通过偷换owning契约、预触页、改driver审计、预热或移出注册成本来消除。实际可用优化机会必须保持原公共调用/真值/生命周期，并由各自裸筛选与正式竞品验收证明。既有M1超时原因仍未知，本次未复现不追认其原因，也不恢复其资格。

## M2精确请求桥接

`M2_MECHANISM_EXACT_BRIDGE.json` 保留原条件草案并绑定已执行 C freeze、控制真实binary/compile/observer/AUDIT、原M2预声明、实际M2 FINAL_SOURCE_IDENTITY，以及候选唯一compile+run argv。仅将相同final C14 source前缀改为已冻结M2 source，及输出/TMPDIR/dependency/body前缀改为新的M2目录；flags、wire/trace、hook/driver/channel/wrapper/audit不变。outer61B launcher显式继承原pin。此文件为验证者完成有限freeze/当前资源/原lease/独立准入提供桥接，**不是执行授权**，未创建运行目录，也未运行候选。

候选上限为1次compile120s、1次native20s、outer600s，control新compile/run均0，retry/warmup均0。验证者仍须绑定自己的单候选dispatch工具、前后完整身份与编译依赖、当前资源和fresh输出、原监督器lease、同口径event/truth/body/cleanup审计及实际两AUDIT外部pin。失败按原注册关闭并封存，不重跑控制或补候选。screen/QF/正式性能目标仍未获准。

## M2机会的整体计数尺度（补充）

控制 mutate 有8,421,400次 canonical调用、28,652个 nontrivial misses、16,131个 nonempty misses和228,010次 const-data调用。按已审的闭合源码计数模型 K=data_reads-vectors-2*N=16,131，候选预期移除2*N+K=73,435次 data调用；其中实际 nonnull路径可移除的 canonical解析最多为3*K=48,393次，约占控制 mutate canonical计数的 **0.574643%（约0.575%）**。这是控制侧计数与源码模型给出的机会量级，M2候选观测尚未发生。

局部非空miss的4→1只描述该节点的重复解析，不能扩大成整个修改路径工作或耗时下降75%。不同解析调用的实际成本未由此计数测量，因此0.575%也不能当作耗时收益上限、精确CPU占比或预先否决候选的理由。R1在修改阶段覆盖更广的inactive命中范围，同样必须由各自独立screen决定性能。当前候选、协议、既定验证和样本预算保持不变，不追加样本、不跳过门禁，也不把两候选的机会相加。
