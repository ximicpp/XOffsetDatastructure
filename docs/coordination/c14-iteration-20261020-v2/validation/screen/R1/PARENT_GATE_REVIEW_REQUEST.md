# R1 最终三项独立 gate 审查请求

唯一事实请求为同目录 `PARENT_GATE_REVIEW_REQUEST.json`（81,609 B，SHA256 `56d97e6cea2a4c9d8a49dab221635b7ca71a29936aa88383e4649e5ba04b0e8e`）。`GATES.json` 已列十一项：机制、定向5+5、full73+73、SML80+80、V1双向84+8/AB4/C4、消费者/共享runtime、生命周期52，共八项依现有真实独立接受记录通过；codegen、safety、engineering 三项保持 pending。此时执行freeze会被拒绝。

请 parent **新写** `reviews/R1_SCREEN_SOURCE_COST_GATES_ACCEPTANCE.json`；不得追加或重写 R1_FULL_ACCEPTANCE、B_ACCEPTANCE、C_EXECUTION_ACCEPTANCE、config、PRECONDITIONS 或任何07ebe/capture已pin输入。新记录须含：status=`passed`、candidate=`r1-canonical-hotcold-v1`、请求内精确candidate_identity/source154/production14、非空精确evidence refs，以及：

```json
"accepted_gates": {
  "codegen": {"candidate_compiles": 1},
  "safety": {"representation_and_lifetime_preserved": true},
  "engineering": {"predeclared_cost_policy_satisfied": true}
}
```

这三个布尔/计数字段必须是 parent 对下述范围的实际独立决定，不能仅复制请求。screen/performance/QF 仍未获准。

- Codegen：现有真实 old bare control/candidate asm 与原请求、admission、compile结果、同154/14以及当前裸profile/deps均已列exact refs。control16处outlined canonical调用、helper在ledger test前5个push；candidate14处内联acquire/load/test零分支跳过active helper，active helper仍outlined。现存原source review只批准当时codegen检查，动态checker review也明确bare codegen独立，因此需本次parent明确接受实际机器码机制。保留历史外层61B转发wrapper的before/after证据范围限制；现有汇编事实仍直接可审，不重编control，不以asm文本大小作速度证据。
- Safety：原独立 `core/REVIEW_R1.md` 记录cold body相等，未新增field/layout/registry/TLS/record/free操作、不跨调用缓存零状态或pointer，anchor优先及record退休/释放顺序保留；当前targeted/full/OOM/callback/lifecycle真实通过提供对应动态证据。本次脚本从const token到return末尾复核3961 ASCII B相等；包含首行原有12格缩进的完整相同范围为3973 ASCII B，与旧review相同，数字差是范围起点而非源码变化。需要parent明确接受这个固定支持域的representation/lifetime gate，不将SAN称为全部ISO规则证明。
- Engineering：`PREDECLARED_RESOURCE_COST_POLICY.json`、实际两次裸capture/queue/完整过程与full-body真值、`RESOURCE_PROJECTION.json`、统一input政策/实际staging、原R1设计/codegen成本披露及未来auxiliary否决条款均已列refs。实际保守投影约166.753894s、physical2,915,420,513B通过其资源范围；staging1.275457023s/62,218,240B已计入。它们不能单独声称全部工程成本通过。parent须明确接受屏幕前的build/maintenance/free/retained/active-memory成本界限，披露可能额外active call/frame及代码footprint；不得忽略原retire/reload/cleanup/total，不使用诊断或capture推出AB收益。未来任一九辅助lower>1确认回退即否决，任何metric零/undefined阻止晋升，当前决定不能豁免未来实际工程/统计复审。

parent 新记录通过后，下面命令只读既有事实、写三份新typed receipt和 `GATES_v2.json`，保留原pending文件；不执行freeze或重任务：

```text
/usr/bin/python3 -I -B <C14>/tools/c14_iteration_20261020_v2/validation/screen/bind_gates_r1.py --parent-review <C14>/docs/coordination/c14-iteration-20261020-v2/reviews/R1_SCREEN_SOURCE_COST_GATES_ACCEPTANCE.json
```

全部十一gate闭合后，由executor执行的 **source-only freeze** 精确CLI如下；本次没有执行：

```text
/usr/bin/python3 -B <C14>/tools/c14_iteration_20261020_v2/validation/screen/r1/screen.py freeze --config <C14>/docs/coordination/c14-iteration-20261020-v2/validation/screen/R1/config.json --capture /root/xoffset-c14-iteration/20261020/v2/validation/r1-screen-resource-capture-v1 --gates <C14>/docs/coordination/c14-iteration-20261020-v2/validation/screen/R1/GATES_v2.json --out <C14>/docs/coordination/c14-iteration-20261020-v2/validation/screen/R1/frozen-v1
```

`<C14>` 精确为 `/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发`。freeze重新检查实际154/14、input/compileddeps/tools、gate refs、成功两run capture与原queue/cleanup以及保守资源投影，再生成新freeze SHA。生成freeze不批准144次screen；最终仍需parent对精确freeze的独立准入、正式fresh host/quiet/恢复安排。预观察hostplan不复用为正式guard，不把已观察PID/handle写入源码。
