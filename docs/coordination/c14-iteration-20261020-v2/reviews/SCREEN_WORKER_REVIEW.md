# R1 / M2 screen worker 统计与调度接入窄审

日期：2026-10-20（Asia/Shanghai）；审查者 `v2_stats_review`。范围为实际 worker 的统计/次序、保留原八阶段、有限资源观测与推断隔离，以及提交元数据的一致性。仅读源码、JSON和轻量纯函数已知答案；编译、native、采样、队列及宿主操作均为零。

**结论：下列实际 worker 的统计和调度实现通过窄审；现有 SOURCE_READY/REQUEST_DRAFT/TOOL_MANIFEST 的发布链未通过，存在两项需修正的不一致。本结论不批准任何实际 screen、Q32 或 F64。**

## 精确读取身份

| 文件 | bytes | SHA256 |
|---|---:|---|
| `tools/c14_iteration_20261020_v2/validation/screen/r1/screen.py` | 59903 | `0ae713213e7f5cfeba5cba8cd7dded155fe44a3114fbcecabf30f23f6f651a21` |
| `tools/c14_iteration_20261020_v2/validation/screen/m2/screen.py` | 59817 | `9dc7c7808c3cb8e5fc9c77ed5ed4cf34e6f51b82a5ca490c6751ba17a7b07aaf` |
| `validation/screen/SOURCE_READY.json` | 2958 | `eafe31990d6206b1683167d392507b87290c1fa811acf0267ef69c0c50e447bf` |
| `validation/screen/R1/REQUEST_DRAFT.json` | 68567 | `be8363c5903bb0929d5acde3b5cdd5486f780c03eb6ba18b7db18ee8105a5681` |
| `validation/screen/M2/REQUEST_DRAFT.json` | 58616 | `b3cc5ca04dd30cafe9775d49fdf5362f88084d183396def8f175fbe97e9ab888` |
| `validation/screen/R1/TOOL_MANIFEST.json` | 4257 | `ca25d33b2f9d32a3fa4b512a1c6283da54220ff3d5e61d71c6b01be74328d04d` |
| `validation/screen/M2/TOOL_MANIFEST.json` | 4250 | `a58e1975adea2e5dc08faa06c0a0e9f60b6a8451b089dc41c01614b094c24191` |

表中 `validation/` 相对 `docs/coordination/c14-iteration-20261020-v2/`。actual worker 在纯函数检查前后 bytes/SHA 不变。

## 必须修正的发布不一致

1. **两个 TOOL_MANIFEST 的 worker 记录均过期。** R1记录47522 bytes/SHA `d197dcee48bc4cd7faaaec543bb65e6e35a7dc645cc2a3fe4988fbe305224f08`，M2记录47471 bytes/SHA `72301556761d37a9411d1ed4b3c1d3e75a817d58e3ef526eb1ded25939d6417b`，均不同于本次审查的实际 worker。SOURCE_READY→REQUEST_DRAFT→manifest的JSON引用本身匹配，并不证明其末端代码匹配。manifest其余列举文件核对一致。`DERIVATION.json`恰记录本次实际 worker hash，但不能替代请求实际绑定的 manifest。
2. **两个 REQUEST_DRAFT 的 statistics.metrics 均不是冻结八阶段。** 当前列为 `load,query_before,mutate,query_after,validate,emit,reload,release,read_sum,total`；冻结 design 和实际 worker 为 `load,query_before,mutate,emit_after,retire_modified_state,reload,query_after,cleanup,read_sum,total`。其中 validate/emit/release 无对应原计时阶段，不能靠名字解释为等价。

修复应保留已发布旧 draft，发布明确新 revision 的 manifest/request/ready，或给出可追踪的新版本链；不得称旧 SHA 已经绑定新代码。修正实际请求指标后，复核新引用身份及无其他变化即可，不需重做正确性或申请新的采样。若实际 worker 本身继续变化，按新 hash 重审受影响函数。

## 实际 worker 已通过的内容

- 两份 `campaign.py/core.py/protocol.py` 与已审 v1稳定版本分别逐字节相同，SHA依次为 `33dfc628a7202d4f08c31e463a4ac0c4fffc27ba5fd04dad3b4a991d402b99c2`、`9ad9063aade365b9dc243f9cf1261df93483e3aa2e8bb7ab443898ae3396cada`、`17c3eaf1d880e61e89a29cebad3df33fe4af5b2dc9a44f1c946df4bc125a9537`。本次复用原 execute/native_result 完整真值检查的既有代码审查，不重复审平台控制。
- `native()` AST与旧worker完全相同：保留原八阶段非负整数，先在child内计算 `read_sum=query_before+query_after`、`total=sum(八阶段)`，原生total_ns另存。retire、reload、cleanup未丢弃；完整native结果与RSS记录仍存在原文件，不只保留主项。
- `schedule()`重新实现已指定 SHA256-counter-unbiased-Fisher-Yates，核对与固定 design 的全部 pairs/children、71 draws、完整 schedule digest相同。R1固定设计SHA `3c835b01d1b84dbd06a28d0660a932fc97b4148f68bc71703a674f146309410d`，M2为 `9b6737a6a0d4a1962c3183e4da995fb60ef1d9c0e15f2f158b63bd49bbf31151`；144不同child、72完整配对、AB/BA各36、每角色72。run按该单一列表依次执行，失败只封存前缀。
- R1主read_sum，M2主mutate；POLICY保留原主族4/辅族36、n72/df71，t分别 `2.289930854179058`、`3.327935133019535`。summary先要求144项job列表与schedule完全相同，再按角色计算log(candidate/control)，使用Bessel样本标准差除sqrt72。每候选主项之外恰九辅助，未回收M1或L1位置。
- 合法零保留为 `undefined_log_contrast_zero_preserved`；任一主/辅助指标undefined均不晋升。非法数据关闭；没有epsilon、drop、离群值剔除、补样或改分辨率。主上界严格<1与无辅助下界>1分开记录后合取为 `statistically_eligible`。区间跨1不解释为等价。
- run从空completed列表开始，只写144项schedule中的实际计时child；audit从原窗口attempted/原生回执重建summary，仅完整成功144项才推断，失败summary=null。资源capture的values不进入该列表；既有functional projection亦只用于预算，不混入统计。单独capture需独立精确请求/准入，其预算不是本次draft自动授权。
- 工程/lifecycle/safety等十一类门禁必须在freeze时另绑定，summary和audit始终 `performance_accepted=false`。finalizer还要求独立raw144重算、工程通过和恢复覆盖，最终只是 `eligible_pending_parent_promotion`。本次不替代这些实际证据，也不把较小total用于抵销确认回退或资源失败。

## 独立纯函数已知答案

通过AST只提取实际文件的POLICY、schedule、summary到无I/O环境，未import/执行worker入口和native：

- 两种pair顺序均按角色设candidate为control的0.8：十项point/upper均为0.8，恰九辅助lower，统计资格为true；相等输入不晋升。
- 对每个指标分别在control和candidate放一个零，每候选20种情形，全部保留n72、只标对应undefined并禁止晋升。
- 主项0.8、单一辅助2.0：主改善仍记录true，但确认辅助回退并使统计资格false。
- 36对比2、36对比0.5：point1、标准误为 `ln(2)/sqrt(71)=0.08226143603173261`，与实现一致。
- 每候选9种非法/次序情形均拒绝：负数、bool、float、None、NaN、inf、超uint64，以及143项、颠倒schedule。检查前后实际worker身份不变。

这些是程序算术已知答案，不是业务或性能样本。新manifest/request身份链修正后仍须由root审实际source/profile/功能门禁、完整进程/产物投影、输入staging成本、原lease、host quiet和恢复、唯一attempt以及最终freeze/pin；本次没有授予任何执行准入。
