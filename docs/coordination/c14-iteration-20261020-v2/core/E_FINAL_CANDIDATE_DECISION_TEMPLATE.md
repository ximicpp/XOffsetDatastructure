# E 最终候选决策模板（待root填写，不是集成决定）

2026-10-20，Asia/Shanghai。本文件仅准备决策字段和两条条件路径；未选择最终候选、未生成新源码、未修改默认C14。M2继续由验证会话独占执行，本会话不推断其终态。完成本文件与配套预检后保持idle，等待root最终选择；收到quiet立即停止读取/写入。

## 当前证据边界

root报告R1唯一72配对已完成144/144并经审计：read_sum point0.8428788466688211、联合单侧U0.8546632724186217；mutate辅助point0.764263918106265、区间[0.7365515038437574,0.7930189993109338]，无confirmed辅助回退。这些是R1对独立C14的内部筛选结果；最终工程接受由root另写精确证据，模板不替代该决定。R1自身full/mechanism/source/cost binding不自动授予Q/F。

M2终态、是否保留以及对应实际完整功能/机制/唯一筛选/工程接受均留空。不从其静态4→1或控制侧计数推定效果，也不根据R1表现取消既定M2验证。M1永久关闭、L1拒绝，不能通过组合恢复；不追加或转移任何72配对预算。

## root最终记录字段

以下为待填写结构，null是尚未决定，禁止当作false或成功执行结果。实际决定应新写root拥有的接受记录，引用最终文件SHA，而不是改写已封存证据。

```json
{
  "status": "awaiting_M2_terminal_and_root_final_decision",
  "R1_terminal_evidence": null,
  "R1_engineering_acceptance": null,
  "R1_retained": null,
  "M2_terminal_evidence": null,
  "M2_engineering_acceptance": null,
  "M2_retained": null,
  "selected_branch": null,
  "final_source154_identity": null,
  "final_production14_identity": null,
  "final_bare_profile_binary_dependencies": null,
  "final_functional_and_cost_closure": null,
  "six_role_Q32_resource_projection": null,
  "Q32_exact_freeze_and_independent_admission": null,
  "Q32_qualified": null,
  "F64_same_closure_admission": null,
  "formal_per_competitor_12_results": null,
  "performance_goal_complete": false
}
```

root须先核对各候选实际全窗、统计和工程终态，记录保留/拒绝/未知/工具失败及范围；不能把计数命中、SAN、单个上界或无confirmed回退分别当作全部接受。只有明确保留的候选才可进入最终分子。下列两分支均以root最终接受R1为前提；若此前提不成立，停止本模板的两分支选择，不自动切换其他候选。

## 条件A：仅R1获准保留

- 最小最终源码即现有独立`r1-canonical-hotcold-v1`，main `b4c61b2d286d6d4ffdfbd44af3ff7151b1640dd07f8b9a2c9bfcfa01a6a9c23f`、patch `0bfbad574e9d9d5bdd6e326b5ac3a495812f9042b74d40a957b3c3faf44b67ad`。无新生产patch，无M1/M2混入，不因改名创建另一个实验候选。
- 若最终154/14、编译器/runtime/依赖、原driver/adapter/public调用、flags和binary与现有R1完整门禁/裸profile完全相同，则只补精确身份桥接及最终root决定，复用实际同闭包门禁，不为目录名再跑完整平台。物化/复制必须有实际成员等同性，metadata字段变化仍须明确其范围，不能把文字替换当作原before/after证据。
- 保留R1自己的full73 normal+73 SAN、定向owner/canonical/thread、SML80+80、V1 84+8/AB4/C4、安装/子项目/relocated/archive/共享runtime、OOM/callback/lifecycle52等实际证据及最终闭包。不能借M1通过。发生真实源码、ABI、profile、adapter或runtime变化时，原证据仅能按不受影响范围复用，缺项须先有新精确有限门禁。
- Q32的`new_xoffset`绑定此实际R1裸binary；`old_xoffset`绑定当前C14控制。两者都需当前精确profile/source/runtime/dependency/input身份与完整工作审计，不能只把旧Q的V8/旧four-path标签改成新名字。

## 条件B：R1与M2均独立获准保留

- 现在不创建组合。root明确选择后，才以同一C14为基底合入已接受的R1函数边界变化与M2单节点data捕获变化，生成新的独立最终组合身份。只能包含这两份既定生产差异；核对patch相容、源差异及154/14，无顺带重构、缓存、layout或output契约变化。
- 真实组合是新闭包；R1/M2各自full与筛选不能证明组合正确性或效果，不能直接复用任一独立binary冒充组合。交互安全核对至少覆盖M2递归data捕获与R1canonical wrapper/active helper、每次acquire、冷路径record/TLS/owner生命周期、OOM与回调边界、双向owns predicate和遍历顺序。
- 最小验收为一次组合实际相关完整normal/SAN、原SML/query/九阶段canonical/owning emit/reload真值、V1双向、全部消费端/共享runtime、OOM/callback/生命周期闭包。定向用例按R1与M2已注册源码/断言/模式求并集；只有同一组合full已真实覆盖且profile/断言口径等同的用例才能引用，full以外的线程交接/特有定向用例须补齐。不能按名字相同就省略，也不为路径改名重复未变化的控制/竞品历史门禁。
- 组合裸原driver/profile须实际构建并绑定真实依赖/binary；这属于root选择后的另行有限验证，当前没有执行或授权。不存在新的组合72配对预算，不能自动加窗。最终Q/F只观察真实组合，不将R1/M2点估计相乘或节省比例相加，不把两组独立样本拼成组合收益。

## 两分支共同的正式验收入口

仅在M2已终态、root已选择最终分子、最终实际源码/门禁/裸六角色闭包已完成后，才以配套`E_Q32_SOURCE_RESOURCE_PRECHECK.md`核对真实资源投影和新freeze。没有满足投影、独立准入、当前host/原lease/quiet/恢复条件时不启动Q。Q32全部30项A/A合格且完整审计通过，才允许同一闭包条件F64；不根据Q结果改F样本量、竞品模式、阈值或工具。

12项正式目标仍是最终候选除以同批四竞品：read_sum≤1.05、mutate≤1.05、完整独立owning emit≤0.20的联合单侧上界，reload完整校验另报。R1自身改善不能证明任何竞品目标；当前没有支持输出5倍目标的新机制或正式证据。完整fresh17.83MB物化不能换成容量复用/segments/freeze/shared输出，不以生产API收益替代旧demo。只有最终正式全部目标及成本门禁实证成立，root才能将performance_goal_complete标记true。

本文件没有执行源码生成、编译、native、queue、body哈希或对外消息；没有改变任何预算、竞品模式、已审工具或候选。

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
| R1 existing gate binding | `C:/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/docs/coordination/c14-iteration-20261020-v2/core/R1_SCREEN_GATE_BINDING.json` | 106959 | `21d9ad5def7ead829692f3bd16b67a2cd010236aca389127fcff111a1900585d` |
| R1 candidate identity | `C:/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/build/c14-iteration-20261019-v1/candidates/r1-canonical-hotcold-v1/IDENTITY.json` | 44342 | `52570679ffd1a479bdf39d3854c8424aa8b29a51e67339d2f93e6fd3c6072948` |
| M2 candidate identity (not terminal evidence) | `C:/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/build/c14-iteration-20261020-v2/candidates/m2-exact-owns-v1/IDENTITY.json` | 44233 | `4a62016c53b103964d4a0b14ce56e3b717b59b54b011efbd52c021b7114b944c` |
