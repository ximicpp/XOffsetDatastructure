# v2 R1 / M2 独立统计政策预审

审查日期：2026-10-20（Asia/Shanghai）。审查者：`v2_stats_review`。本轮只读计划、历史回执和工具源码；编译、native、性能采样、重负载、旧窗口统计复算均为零。本文件是唯一新增产物。

**结论：固定统计族、单位和判定可继续沿用；下列两项继承细节必须写入后续工具与实际 freeze。尚无实际 R1/M2 screen freeze，本预审不授予 screen、Q32 或 F64 准入，也不证明任何性能收益。**

## 1. 已核对的政策与当前身份

v2 是目录和实施调度迭代，不能重置原四主目标/36辅助目标的统计族。M1 已执行失败窗口，仍消费其原登记位置；L1 取消不回收 alpha 或样本位置。v1 `FAILURE_REVIEW.json` 明确唯一 M1 attempt 为119 attempted、118 completed、25 unexecuted，summary=null；禁止换名重试、成功前缀推断、重新分配 alpha 或通过无关组合重验。R1/M2 不继承这些样本。

| 项目 | 固定政策 |
|---|---|
| R1 主目标 | `read_sum`，候选 `r1-canonical-hotcold-v1` 对独立 C14 控制 |
| M2 主目标 | `mutate`，独立 M2 候选对 C14 控制；不混入 R1/M1 |
| 统计单位 | 72个预先固定的配对，每对一个 control、一个 candidate；144个实际 child |
| 次序 | AB/BA各36对；新窗口事前固定算法、seed、平衡随机次序及完整144项 schedule |
| 重复与失败 | warmup=0、retry=0、replacement=0；失败封存前缀，不计算其收益 |
| 主目标校正 | 原主族4，单侧95%联合；df71，t=`2.289930854179058` |
| 辅助校正 | 每候选其余9项，原辅助族36，双侧95%联合；df71，t=`3.327935133019535` |

两个临界值直接复用 v1 `STATISTICAL_REVIEW.md` 的不完全 beta/CDF 二分与独立 Simpson 密度积分双路径数值证据；本次不重建数值工具、不调整系数。

固定八阶段为 `load`、`query_before`、`mutate`、`emit_after`、`retire_modified_state`、`reload`、`query_after`、`cleanup`。每个 child 内先计算 `read_sum=query_before+query_after`，`total=sum(八阶段)`，再计算配对比。原生 `total_ns` 含计时记账，另外保留，不能替代登记的 derived total。

R1 的九辅助为八阶段加 total；M2 的九辅助为除 mutate 外七阶段加 read_sum、total。不能因为 R1 的 read_sum 与两个 query 分项重叠而删除辅助或重新分配预算；Bonferroni 校正不要求这些指标独立。

## 2. 两项必须明确继承的细节

### A. 合法零时间与无定义 log

早期 `STATISTICAL_REVIEW.md` 的“输入时间必须有限正数”不能直接覆盖后来冻结的零值政策。实际稳定 worker、`screen/README.md`、最终 `SCREEN_WORKER_REVIEW.md` 和 `TOOL_CHECK_v2.json` 已明确：

- 原生合法阶段时间是规范的非负整数；合法零原样保留，72对与144个 child 不减少。
- 任一指标任一 control/candidate 为零，该指标标记 `undefined_log_contrast_zero_preserved`，不加 epsilon、不删点、不补样、不改变分辨率。
- 最终 v1 worker 用 `primary_improvement = not undefined_metrics and primary_upper < 1`：即使零仅出现在辅助项，也不能晋升该候选。v2 继续保留这个更保守的实际政策。
- 缺失、负值、非规范/非数、角色缺失、身份变化、native/真值失败与超时属于数据或窗口失败，不能当作合法零。全窗失败没有统计 summary。

v2 PLAN 当前未显式重述零值细节；后续工具和 freeze 必须绑定上述最终政策，而不是因目录更新变成删除零样本或只忽略辅助零。

### B. 已确认辅助回退仍是否决条件

v2 PLAN 写“主上界<1且全部工程门禁通过才能晋升”，随后说明“辅助下界>1为确认回退”。为避免被实现成仅报告回退、不参与晋升，实际判定必须沿用 v1 的完整合取：

```text
promote = 完整72对且所有指标可定义
          AND 主目标联合单侧上界严格<1
          AND 不存在辅助项联合双侧下界>1
          AND 全部冻结工程/语义/资源/恢复/独立审计门禁通过
```

辅助区间跨1只表示未确认回退，不能称为等价、无退化或成本不变；主上界等于或跨1均为未建立改善，不能凭点估计晋升。较小 total 不能抵销失败主项、已确认辅助回退或工程成本转移。

内存与资源不新增事后统计比较，而由事前冻结的工程门槛单独判定；构建、维护、释放、retained bytes 与活跃内存成本按实际发生阶段保留。定义不清的“全部工程门禁”必须在实际 freeze 明列，不能看完结果后决定可接受的转移。

## 3. 计算、随机化和组合边界

每个指标每对按实际角色还原 `x_i=log(candidate_i/control_i)`，不是 second/first；不得因 AB/BA 方向改变符号。n=72、df=71、Bessel sample stdev、`se=s/sqrt(72)`；point=`exp(mean(x))`，主 upper=`exp(mean+t_primary*se)`，辅助 lower/upper=`exp(mean±t_auxiliary*se)`。

不得将144个 child、八阶段、九个修改阶段、机制/诊断进程或旧窗口当成新增独立样本；不得先算阶段比再平均为 read_sum/total。新 schedule 的完整顺序及算法/runtime身份、seed应进入 freeze；AB/BA各36本身不是随机化证明，随机化也不证明72个配对 log 值满足独立性模型。执行异常和时间序列条件完整披露，不按结果删离群值、去趋势、切换检验或补样。

现有 `m1_screen.py` 明确 M1-only，schedule 为历史交替顺序，主项硬编码 mutate。它是政策/实现参考，不得修改或仅换 label 给 R1 使用；新 worker 必须正确参数化主项与辅助集合，并单独审查随机 schedule、数据验证和晋升合取。政策预审不能替代这些代码核查。

M2 的独立筛选只证明 M2 对 C14 的效果，不是 M2 对 R1 的增量。R1/M2 可能消除同一解析工作：不能叠加百分比、将两组样本当组合样本，或以任一候选结果为组合性能证据。只有各自独立获准保留的候选进入真实组合闭包；后续完整门禁和冻结正式 Q/F 单独验收。关闭的 M1 和 L1 不随组合恢复资格。

内部筛选上界<1只用于选候选，不是正式竞品验收。最终仍为 XOffset÷同批各固定竞品：read_sum≤1.05、mutate≤1.05、完整独立 owning emit≤0.20，12项主比较联合单侧95%；reload 完整校验并另列，无新增比例门槛。Q32 的30项 A/A 全部落入 `[1/1.05,1.05]` 后才可进入同闭包 F64；本次没有资格或正式采样。

## 4. 实际 freeze 仍待审的内容

执行前必须另审：最终新 worker/工具完整身份和已知答案检查；控制/候选全154成员和14生产闭包、实际 bare `-O3 -DNDEBUG` 二进制、profile、compiler/libc++/runtime、driver/adapter、输入/wire/trace；同候选完整功能、SAN、业务、V1、消费端、OOM和生命周期门禁；确切 seed/schedule/argv/输出路径、唯一 attempt 与错误/零值策略；完整进程与在线审计预算、allocated 峰值及双卷空间；原全局 lease、host quiet、停止/恢复及恢复先于离线审计；实际 freeze 外部 pin 与独立终审。诊断、插桩和 capture 均不能混入72对。

在这些证据闭合前，`screen_admitted=false`、`QF_admitted=false`、`performance_goal_complete=false`。本文件只批准继续准备，不批准运行任何计时 child。

## 审查输入字节身份

以下为读取时磁盘内容；其他 agent 若后续更新 PLAN/TASKS，需复核受影响条款，不能把此 SHA 结论转借给新内容。

| 输入（相对工程根） | bytes | SHA256 |
|---|---:|---|
| `docs/coordination/c14-iteration-20261020-v2/PLAN.md` | 9920 | `aa8bd6141d94ac0786cd3f99d114b06513b4e3fcf1294849e87281a6a3e27ca1` |
| `docs/coordination/c14-iteration-20261020-v2/TASKS.json` | 3546 | `2f18066d5dfb1cdf826b9fffd97e0ccea44266bc0e0ce269c596377a918cfac2` |
| `docs/coordination/c14-iteration-20261019-v1/PLAN.md` | 10571 | `1f3a0569f56b0256eba4204e76b5b038c677f0459e83f4ae0ac22d424964fb01` |
| `docs/coordination/c14-iteration-20261019-v1/validation/review/STATISTICAL_REVIEW.md` | 7845 | `bf4bae74e2b2abd062ff65a4ac69c899236c0a3a8eb072f4299956094cbdb363` |
| `docs/coordination/c14-iteration-20261019-v1/validation/review/SCREEN_WORKER_REVIEW.md` | 17980 | `bcbdb932b80b1a908782fd71aa7403efd3e0b2c922fa92c1f3283ca395103119` |
| `docs/coordination/c14-iteration-20261019-v1/validation/screen/README.md` | 6826 | `df224f515c5cec3b6da785e123c922fccc3c7884e452a664124cf205d1eb27f0` |
| `docs/coordination/c14-iteration-20261019-v1/validation/screen/FAILURE_REVIEW.json` | 9279 | `d591ca3aa1df27210c262bccab4da63df9b853b8ab82a5ad8db3b0ec353df53f` |
| `tools/c14_iteration_20261019/validation/screen/m1_screen.py` | 39510 | `8310364077d3c37e01b4ca521932d33c4eb69023ecf645d9a7e6a5b40fe659b7` |
| `tools/c14_iteration_20261019/validation/screen/campaign.py` | 54254 | `33dfc628a7202d4f08c31e463a4ac0c4fffc27ba5fd04dad3b4a991d402b99c2` |
| `tools/c14_iteration_20261019/validation/screen/core.py` | 7845 | `9ad9063aade365b9dc243f9cf1261df93483e3aa2e8bb7ab443898ae3396cada` |
| `tools/c14_iteration_20261019/validation/screen/check_tools.py` | 3209 | `d6f65c1c11b3c5e3b4307c5c2e3decbbce0d0d8a96ef191e72ef758b8f1e7d32` |
