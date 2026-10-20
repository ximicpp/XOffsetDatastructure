# E Q32源码与资源有限预检（准备完成，实际准入未开始）

2026-10-20，Asia/Shanghai。范围仅为读取小型原policy/profile/统计与调度源码，核对固定定义并列出最终闭包义务；未执行这些工具、生成新候选/调度、遍历body、哈希大body或启动任何queue/native。M2未终态、最终分子未选择，故现在不能给出Q/F通过的实际投影或freeze。当前状态：Q32_admitted=false、F64_admitted=false、performance_goal_complete=false。

## 六角色与竞品模式

固定角色顺序为`old_xoffset,new_xoffset,cista,flatbuffers,capnproto,cereal`。本轮`old_xoffset`是当前C14控制，`new_xoffset`是root最终选择的R1或R1+M2实际分子；二者各自有同binary两次replica产生本角色A/A，不要求两个XOffset角色binary相同。历史frozen-qualification把old/new绑为V8/旧four-path，这些旧身份不能原样复用到本轮。

| 固定竞品 | 版本/commit | 原模式（全部detached） |
|---|---|---|
| cista | v0.16 / `015b94e66c4c0d7c1ef29742383a024c84fbbba1` | T-trusted-offset-mode-NONE-structural-checks-private-blob;U-not-established |
| FlatBuffers | v25.12.19 / `7e163021e59cca4f8e1e35a7c828b5c6b7915953` | trusted-object-api-no-upfront-verifier |
| Cap’n Proto | v1.5.0 / `373e61ec89e2359f1c362e9b2eadc552f4779306` | T-trusted-bounded-reader-builder-detached;U-not-established |
| cereal | v1.3.2 / `ebef1e929807629befafbb2918ea1a08c7194554` | T-trusted-binary-native-std-detached;U-not-established |

版本/mode来自原environment锁与实际旧六角色profile，仅核对定义；未将旧binary视为当前可直接运行。未来同身份二进制/runtime/依赖、原schema/model/固定M输入trace与原public路径须完整绑定。不得改成另一serializer模式、加/删upfront verifier、借borrowed/zero-copy替代detached、改九修改阶段/完整校验/首错误或新加生产接口成绩。原virtual CPU2 affinity与host资源/电源观察分列，不能宣称物理核独占或锁频。

## 原Q32/F64定义与可复用调度

已读实际旧frozen-qualification的`policy.json`、`envelope-policy.json`、`proposal.json`、`profile.json`及源`phase_protocol.py`和`comparison_protocol.py`。phase协议显式复用身份锁定的V5 schedule/t数学，并扩展完整阶段判定；不可直接改用只分析6总量A/A的旧V5分析器。调度seed是`sc-six-role-v5-fixed-schedule-v1`；本轮不重抽seed，也不把内部72配对的随机序列用于Q/F。

原schedule保留两warmup blocks、固定六角色顺序平衡基序(0,1,5,2,4,3)、每macrocycle六shift按固定SHA排序、奇偶cycle反向、每角色两replica AB/BA按原奇偶规则排列。每个primary之前有同角色conditioner；conditioner时间不用于结果选择、不进入推断。每个child原八阶段完整生命周期与独立owning emit/reload不变。只复用其不受身份变化影响的调度/统计/监督语义；历史executor的STAGE_BINDINGS/基线/候选闭包字面量与源路径需要在最终实际source后作可审的精确绑定派生，不能直接运行旧冻结目录或把旧资格借给当前候选。当前未生成这个派生层。

| 范围 | Q32 qualification | F64 conditional formal |
|---|---:|---:|
| 完整平衡macrocycle / df | 32 / 31 | 64 / 63 |
| comparison blocks | 192 | 384 |
| primary总数（含预定warmup24） | 2328 | 4632 |
| 入推断primary | 2304 | 4608 |
| conditioner | 2328 | 4632 |
| 实际child总数 | 4656 | 9264 |
| 每角色primary + conditioner | 388 + 388 | 772 + 772 |
| 完整command上限 | 2700s | 5400s |
| 每child / 独立离线audit上限 | 20s / 3600s | 20s / 3600s |

Q32只做30资格：6角色×(read_sum,mutate,emit_after,reload,total)。每角色本binary的chronological first/second构造log比值；不是new/old，也不是候选/竞品。每child先算read_sum=query_before+query_after；原total=八阶段和。按六blocks聚合成32个macrocycle，再以df31、t(1-.05/(2*30),31)构造30项联合双侧区间，全部在[1/1.05,1.05]内才合格（保留原1e-12 log roundoff口径）。不能以4656child或2304primary当独立统计单位。

合法阶段零不删样、不加epsilon，保留原undefined/no-log及资格未建立规则；全窗、模型/角色/源/真值/资源/恢复和离线审计都需通过。旧Q32的cereal mutate越界导致29/30，仅说明旧资格未建立；旧F64未启动且窗口封存，不回收或补样。新的最终candidate需要它自己的同六角色Q32，不能借R1内部read_sum U<1或旧资格。

F64只有同一完整闭包Q已合格、root对精确F freeze及当前资源另行准入后才执行。F自己也保留同30项A/A与df63；4竞品×3目标为12项联合单侧95%，t(1-.05/12,63)，最终候选/每竞品上界须read_sum≤1.05、mutate≤1.05、完整owning emit_after≤0.20。原40项辅助、单独误差族与against-C14的load/reload/retire/cleanup/total成本转移否决保留；reload没有新硬比例。区间跨1不是等价，不合格/未达标不放宽、删点、去趋势、变样本单位、补样或追加窗口；不同统计族不合成一个更强的整体95%主张。

## 最终source/profile/gate闭包清单

1. M2终态和root最终选择记录先到位；用`E_FINAL_CANDIDATE_DECISION_TEMPLATE.md`的条件分支，未选前不生成组合。选R1时复用同源实际门禁；选真实组合时先有组合154/14及自己的相关完整门禁，独立筛选不能冒充组合效果。
2. 控制C14与最终候选都固定完整154成员/14生产文件、实际bare `-O3 -DNDEBUG`原driver/adapter、compiler outer61B与inner/toolchain、libc++/runtime/依赖闭包、实际binary和metadata等同性；不含SAN、`-UNDEBUG`、diag/global-new capture。前后pin按真实证据范围记录，不追溯修补早期wrapper gap。
3. 四竞品保留上述版本/mode/schema/public动作与实际bin/closure，全部六角色输入wire/trace/fixture/oracle、完整各阶段/owning输出/reload真值、canonical/query/full-body审计和V1支持域按原合同绑定。相同身份既有控制/竞品结果可精确引用，不为凑闭包重跑历史大矩阵；改变身份的部分不能借通过。
4. 调度/统计/conditioner-envelope/runner/power-host/controller/resource/audit工具固定实际字节，独立审查身份派生与已验证错误路径；原global run_job.py/queue.lock单lease，不能新建并行锁或在held lease里再调用自取lease的dispatcher。
5. 新Q目录、actual argv、全部schedule、input staging归属与费用、候选/控制/竞品实际过程和产物投影、资源与host quiet/恢复程序、stop/seal预算、外部freeze pin和root独立接受全部闭合。恢复用户进程在离线audit之前；失败保留prefix，cleanup不为空不能继续。两角色内部screen/capture不是这份六角色投影。

## 不变预算与真实资源投影

每角色r按最终实际同profile、同conditioner/public工作/输入/产物审计测得的完整process maxima记`maxC_r`（conditioner）、`maxP_r`（primary），令`S=sum_r(maxC_r+maxP_r)`。不能使用native.total代替完整process，不能把单一R1capture推广为六角色，也不能用旧max保证未来tail。

- Q必要投影：`388*S + H_Q <= 2700`，`H_Q=300s`。
- F必要投影：`772*S + H_F <= 5400`，`H_F=300s`。
- 这只是必要可行性门槛；每次launch还要保留原当前child硬上限、seal/cleanup和controller prelude/恢复的真实余量，完整command deadline不顺延。离线audit各3600s单列，恢复先行。

原conditioned envelope的资源限额保留：Q/F实际allocated output上限4/8GiB，wire两类合计128/256GiB、其中primary wire64/128GiB，logical retained256/512GiB；metadata reserve分别372244480/674234368B，双卷free margin2GiB，真实RSS/host/backing准入按原执行器合同。不能因原full-byte验证后的canonical/query hardlink去重把逻辑生成量或wire量抹去；所有wire独立，staging/capture/中间产物/失败prefix/包与日志/metadata/封存临时空间都要归属到guest及C backing实测可分配空间。去重必须原sha/size/full-byte比较后原子替换，不能先假定未来全相同。不得删除历史腾空间或临时扩预算。

当前没有最终候选、六角色当前真实max/产物投影、当前host/lease/双卷空间或最终工具freeze，因此本文件没有计算“能装下/能完成”的结论，也没有建议先启动再看。旧资源记录只可作规划参照。若真实投影失败，保留候选与证据、说明具体阻塞，不缩Q/F样本、不改竞品模式、不自行降级测量、不多发control/candidate探针。

## 正式目标仍未完成

输出5倍目前没有新机制证据。R1自身改善、M2是否通过、首次emit诊断/缺页账本以及显式生产接口功能均不能证明owning emit≤0.20。即便Q通过也只是资格，不能宣布任何竞品胜负；只有最终同闭包F64的逐竞品12项目标与成本/语义/资源/审计全部成立，才可接受正式目标。两文件完成后本会话idle，等待root最终候选选择；收到quiet立即静默。

## 本次只读引用身份

以下仅固定本次读取的source/metadata字节，不追溯授予旧窗口或新Q/F资格；旧冻结角色身份仍须按正文重新绑定。未哈希binary或body。

| 引用 | 路径 | bytes | SHA256 |
|---|---|---:|---|
| Current v2 plan | `C:/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/docs/coordination/c14-iteration-20261020-v2/PLAN.md` | 10926 | `c4ae64dd49378fb61b6126952c235db87804d95714bb627fdafa38a805bc26ad` |
| Inherited authorization | `C:/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/handoff/continuation-20261019-v1/docs/coordination/performance-four-paths-20261018/authorization-and-acceptance.md` | 6431 | `408827c1176bebb76cdc1f0d5384bc22f290d0bce904317ec24b10e5fd47f64f` |
| Original qualification policy | `/root/xoffset-four-paths/20261018/v1/frozen-qualification-v1/policy.json` | 2641 | `77075d0a05a003461c7d04ab6bf140b51f524402d8ee3ddcb975d3deb8108e03` |
| Original conditioned envelope | `/root/xoffset-four-paths/20261018/v1/frozen-qualification-v1/envelope-policy.json` | 1048 | `0c886fc0d3e582de8ac5ea21a9b6f85a319e7c6fbe3edbcebbb838d70a993161` |
| Original formal/qualification proposal | `/root/xoffset-four-paths/20261018/v1/frozen-qualification-v1/proposal.json` | 65177 | `7e17bf9d9d178226cabe61a13567e1ee5a14b685f34e41d05277975e195ff1f7` |
| Original six-role profile | `/root/xoffset-four-paths/20261018/v1/frozen-qualification-v1/profile.json` | 151816 | `91599dd0481ec5ebc3e056435c5eec9161cde2b5053adb24aac74c7f6402cffd` |
| Phase protocol source | `/root/xoffset-four-paths/20261018/v1/frozen-qualification-v1/tools/four-path-plan-support-20261018/statistics/phase_protocol.py` | 17354 | `4be28ab149726a5dafef9ebbcc572a2462748ab58a74ae45e7ea98e7ad0b3a7f` |
| Frozen schedule/math source | `/root/xoffset-four-paths/20261018/v1/frozen-qualification-v1/tools/performance-v5/measurement/comparison_protocol.py` | 13491 | `444095c1d6640dd05e11b71e8f75ec2cbf7f3d150734623d015e9a4d7ec16852` |
| Frozen conditioned executor source | `/root/xoffset-four-paths/20261018/v1/frozen-qualification-v1/tools/six-role-campaign.py` | 115800 | `7e7bdebf06bc58df4a645d1b04fe237e807b2cb46447d7c276fc1bd1377857d3` |
| Frozen conditioner source | `/root/xoffset-four-paths/20261018/v1/frozen-qualification-v1/tools/conditioning_envelope.py` | 2915 | `c1ab6d1ad275b716500e269942969abb4ace4a4550a5eb890758fa1b9f1b6fa1` |
