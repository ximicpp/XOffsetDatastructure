# I/O 候选：G3 逐方向拟议决定

2026-10-16。**三个方向均拟议为保留 F、前提未满足，本轮不启动候选实现；最终由
统筹裁定。** 没有候选被实现、实验拒绝或采用，真实消费者及业务收益仍为未评估。
测量窗口不合格不证明候选收益不存在。

[机器可读提案](../experiments/vm_io_candidates/candidate-preregister.json)记录成本、
寿命和预算要求；其中预算上限不构成后续运行或实现授权。生产双头与 V1 wire 不变。

## 已有证据和本次测量的边界

W1 的[静态审查](../experiments/vm_network/candidate-feasibility/REVIEW.md)已按其
历史提案版本获[统筹接受](coordination/vm-followup/contracts/W1-candidate-static-v1/acceptance.json)。
其作用是明确可描述的内部实验落点和事务/完成/资源缺口，没有给出性能比例或实现授权。

W2 [有界网络诊断](coordination/vm-followup/W2-network-acceptance.json)已验收，
[phase/计数表](validation/vm-observability/network/final-v1/summary/summary.json)覆盖
32 个进程、16 份有效 trace。none/cold 每条件含 preflight 的总注册、reserve/unmap
均为 5；warm 含 preflight/预热为 7 次注册、3 次 reserve/unmap。后续 work 已复用
映射，加载、注册和 payload 复制仍存在。候选不能再次计算已被 F 池化消除的工作。
domain5 的 192 B 是局部 allocator 请求峰值，不能换算物理内存或耗时比例。
final-v1 加 references 才是该诊断包范围。无需为本轮决定追加 W2 运行。

唯一 112 进程原生 F/F 校准中，只有 1 MiB 满足本次筛选校准，
[统筹 D021](coordination/vm-followup/network-aa-1-acceptance.json)据此仅允许新的独立
1 MiB 池筛选。该[唯一 88 进程窗口](perf/vm-network/POOL_SCREENING_1M_RESULTS.md)
已全部执行成功并完成冻结字节审计和独立汇总重算，
[统筹 D026](coordination/vm-followup/network-pool-1m-1-acceptance.json)已接受原始结果；
完整归档和复制后冻结审计也已通过。该两对照结果为：

| F 对照 | 前/中/后局部 A/A | 左 / 右 / 合并绝对漂移 | 完整资格 |
|---|---|---|---|
| none / cold | 全部通过 | 6.547% / 5.883% / 6.300% | 不合格 |
| none / warm | 全部通过 | 2.476% / 1.090% / 1.951% | 不合格 |

两对照均超出当窗 1% 漂移预算。先前 1 MiB 合格不能替代本窗口校准，因此描述性的
配对比值不能当作池化收益、回退或候选可消除耗时。这个窗口只比较既有 F 池模式，
没有实现以下任一候选。没有重跑、新窗口或其他尺寸，后续也未获授权。

## 逐方向拟议决定

| 方向 | 已有 scoped 成本事实 | 尚缺的具体依据与契约 | 拟议决定 |
|---|---|---|---|
| owner reuse | warm 已复用部分映射，注册/元数据及加载仍在；没有残余可消除耗时比例 | 选定复用 C++ 对象、reservation 还是 Core；完整 load 提交/失败范围和 spare/rollback；保留旧值时给 F 同等 pooled load+swap 工作量，计入旧、新、暂存 owner 及 registry/epoch 成本 | 保留 F，前提未满足；未实验拒绝 |
| 新 staging 直接读 | 存在 owning receive string→owner 的 payload 复制；不能将整个 load 阶段计作可省成本 | 一次只选一端；明确普通 grow 清零或未发布填入 seam；V1 准入/全部图校验、发布、短读/取消实际完成及以后 grow 的清零契约 | 保留 F，前提未满足；未实验拒绝 |
| 减少输出复制 | 存在响应 string 分配/materialization；F save_verified 没有完整图扫描 | 保留规范化、头构造、既有检查和两端 load 扫描；稳定 owned header、独占 payload、短写进度、真实 completion、背压下 owner/registry/页保留和延迟释放 | 保留 F，前提未满足；未实验拒绝 |

“旧值保留”目前只能覆盖明确的 load 事务，不能扩张到 process/save/发送全部操作。
save_verified 会先 shrink 再分配输出；若未来承诺整次操作强失败保证，还须单列
logical extent 和 Core 宿主状态，而不只是恢复值。新 staging 若用普通 grow，可能
把 memcpy 换成等量 memset，不能据此预设复制消除收益。

每 owner 上限、逻辑 string 准入和 idle pool 限额均不等于总活跃资源预算。旧/新/
spare/rollback 状态、header/描述符以及背压持有期必须进入相同资源账本。只返回
两个借用 span、只请求取消、只保持数值基址相同，都不足以形成完成/寿命契约。

## 本轮收口与仍开放的采用前提

主指标只校正已明确记录且不重叠的回收/剩余等待。pool prepare、connection setup
和 warmup 另报；客户端字符串销毁、worker 创建/最终 join 等仍未全部计量。完整
生命周期成本不可用，诊断阶段重叠时间不能相加成端到端可节省比例。

没有可信的候选端到端可消除成本比例，三个 cost_fraction 均保持 null。提案的 5%
目标、2% 默认回退预算、零 wire/验证/容量放宽，仅是未来若重新授权时的门槛；
小幅采用仍需 1% 偏移/离散/漂移、至少三个独立合格窗口和逐窗口预登记的 95% 配对
log-ratio 区间下界至少 `1/0.95`。单窗 5% 筛选门槛不构成采用资格。

本轮建议至此保留 F，不授权额外计时、诊断、候选实现或采用。真实消费者协议、
尺寸分布、异步完成与生产资源要求未定，业务收益继续开放。独立固定请求 owner
研究由 W1/W2 的另行设计/功能/精确提案 gate 管理，不是本轮网络研究的重试。
