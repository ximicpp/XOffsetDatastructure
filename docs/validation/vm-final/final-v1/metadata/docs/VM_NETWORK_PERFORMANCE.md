# 网络与默认路径测量状态

2026-10-16。**默认路径唯一 F/F 校准四指标不满足小差异前提；网络唯一 112 次
F/F 校准中仅 1 MiB 合格。随后独立获批的 1 MiB、88 次池筛选已完成，两对照局部
A/A 均通过但绝对漂移均失败，不能支持收益或回退结论。没有重跑、D/F 或候选实现。**

生产双头保持[统筹方案](VM_FOLLOWUP_ORCHESTRATION.md)的 F 身份，历史证据只读。
既有[文件筛选](VM_IO_SCREENING_RESULTS.md)四尺寸 A/A 失败，不作为新优化依据。

## 网络：112 个真实原生进程，1/4 尺寸合格

network-aa-1 按统筹 D020 批准恰好执行一次，16 个预热及 96 个校准进程全部成功。
客户端时间 00:54:18.310699–00:54:39.672260（Asia/Shanghai），整体 21.361 秒；
计时锁释放，无残留子进程。所有调用均为同一 W1 timing-v2 的 F、pool none，
四个名义尺寸为 4 KiB/64 KiB/1 MiB/16 MiB，每进程迭代 32/16/4/1。

| 名义尺寸 | 前/中/后局部 A/A | 左/右/合并跨锚漂移 | 本尺寸窗口 |
|---|---|---|---|
| 4 KiB | 失败 / 通过 / 通过 | 失败 | 不合格 |
| 64 KiB | 全部通过 | 失败 | 不合格 |
| 1 MiB | 全部通过 | 通过 | 合格，仅本次筛选校准 |
| 16 MiB | 通过 / 通过 / 失败 | 通过 | 不合格 |

局部偏移/离散预算为 5%，绝对前/中/后锚漂移分别对左、右、合并约束为 1%。
`aa_qualified` 只检查局部，完整资格是 `stable_window_qualified`。1 MiB 通过不构成
5% 收益的采用资格：采用仍需 1% 偏移/离散、至少三个独立合格窗口及预登记收益区间。
本次有 1484 次进程内操作，不能称为独立窗口或生产 p99。

完整原始输入/输出、stdout、stderr、112 份回执、环境和精确调用保留；
逐次输出共 620,755,072 B，限额 2 GiB。冻结 `seal_native` 从完整字节重算并通过，
独立审查另从小回执/原始报告重算摘要，结果一致。原始 summary 的通用 synthetic
说明不改写；本次确为 native 执行，整体资格未过也不能表述为“没有执行”。
详情、阈值和噪声限制见[网络结果](perf/vm-network/RESULTS.md)。

主指标为 `(end_to_end_ns + cold_clear_outside_ns + final_clear_outside_ns +
client_owner_release_outside_ns + server_completion_wait_outside_ns) / iterations`。
最后一项只计区间外剩余等待，不重复相加重叠发送/销毁阶段。客户端字符串销毁、
worker 创建及最终 join、池/连接销毁等仍未完整计量，完整生命周期成本不可用。
不将校正指标称为全部应用成本。

原始八次功能准备保持 c53 执行来源不变，加固离线审计区分执行与审计协议；
历史 Python 摘要/版本未记录。新计时解释器路径、实际路径、SHA/大小和版本已固定，
W1 G1→manifest→build/smoke 的原路径、SHA、大小均绑定。
[冻结工具与 52 项检查](perf/vm-network/adapter-v2/README.md)通过普通与优化 Python
及独立审查；[通用协议的 31 项检查](perf/vm-network/framework-v2/README.md)是工具证据。

## 默认路径：四指标小差异校准不合格

D/F 桥通过队列编译，六个原生功能等价进程及完整输出核验成功。default-aa-1 唯一
78 进程 F/F 校准全部执行成功，但四指标、12 个局部锚点未通过 1% 偏移/离散/绝对
跨锚漂移联合 gate。未重跑、D/F 或消融，不推断 F 回退、等价或硬件永久限制。

冻结工具完整审计、独立摘要复核和复制后归档审计均通过。统筹接受为
`accepted_executed_noise_unqualified`；N3 对应路线图项继续开放。
详见[默认结果与完整归档](perf/vm-default-attribution/RESULTS.md)。

## 成本与下一道 gate

W2 的[有界网络诊断](coordination/vm-followup/W2-network-acceptance.json)已获统筹验收，
提供指定路径的复制、注册、映射及寿命计数，诊断时间不构成可消除耗时比例。
[候选成本/事务/完成契约](VM_IO_CANDIDATE_DECISIONS.md)已纳入 W1 已接受的静态审查；
三个方向仍是前提未满足，真实业务消费者为未评估。保留 F，候选 G3 由统筹结合
本次校准和 scoped 成本统一裁决。

任何新池筛选、候选或固定请求 owner 研究的正式计时均需独立批准。本次批准不延伸
为重试失败尺寸或自动比较。所有 C++、矩阵、诊断和重 I/O 继续使用已验收的 W4 队列，
W3 是正式计时唯一执行者。

## 1 MiB 池筛选：88 个进程，两对照均未获得完整资格

D025 单独批准 `network-pool-1m-1`，88 个原生进程全部成功，352 份完整输出
共 369,222,656 B，冻结完整字节审计和独立小记录重算通过。01:21:55.033257 至
01:22:04.783198（Asia/Shanghai），计时段 9.749539 秒，退出 0、锁释放、清理为空。

| 对照 | 三锚点局部 A/A | 左 / 右 / 合并漂移 | 完整资格 |
|---|---|---|---|
| none / cold | 全部通过 | 6.547% / 5.883% / 6.300% | 不合格 |
| none / warm | 全部通过 | 2.476% / 1.090% / 1.951% | 不合格 |

独立的 1% 漂移门槛仍必须通过，不能把旧 1 MiB 合格转移到本窗口，
也不能将本窗描述性比值称为池化收益、回退或候选效果。
[完整结果与可搬迁证据](perf/vm-network/POOL_SCREENING_1M_RESULTS.md)保留真实执行、
原始结果和边界；[绑定器增量及 22 项检查](perf/vm-network/adapter-v3/README.md)
只扩展精确 1 MiB 模式，N93/P406 和全部预算/阈值不变。

网络研究本轮据此收口，不再追加窗口或诊断。三个尚未实施候选拟议为保留 F、
前提未满足，最终由统筹裁定；这不是候选实验拒绝。真实业务及采用前提保持开放，
固定请求 owner 研究的功能/提案/正式计时由其独立 gate 管理。

统筹 [D026](coordination/vm-followup/network-pool-1m-1-acceptance.json) 接受本次 88 次
结果为 `accepted_executed_noise_unqualified`，G2 为 `accepted_scoped_with_noise_limits`。
[完整池筛选归档](perf/vm-network/pool-screening-1m-v1/EVIDENCE.json)及复制后冻结
审计已完成；原始证据不变，所有 352 个逐次完整输出路径保留。N7 网络测量本轮
有界执行/审计交付完成，性能/业务采用未满足；G3 三方向拟议决定见候选文档，
最终由统筹裁定。
