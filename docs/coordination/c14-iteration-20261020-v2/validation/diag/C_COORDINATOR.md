# C coordinator freeze 与执行

`phase_c_job.py`只提供source-only prepare和一次run，不发队列，不运行dummy。实际材料化目录已由root在原lease生成；本工具以其MATERIALIZATION与每role FINAL_SOURCE_IDENTITY为准。

## 一次 source-only prepare

原共享runner W3、kind validation、timeout180、wait0。展开绝对路径后command：

```text
/usr/bin/python3 -B /mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/tools/c14_iteration_20261020_v2/validation/diag/phase_c_job.py prepare --out /mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/docs/coordination/c14-iteration-20261020-v2/validation/diag/C_FREEZE_DRAFT.json --resources-out /mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/docs/coordination/c14-iteration-20261020-v2/validation/diag/C_RESOURCES_DRAFT.json
```

两文件均独占新建。输出实际freeze SHA，root/parent只审这个hash。prepare核对并记录source154/14、三role原始材料化链、最终driver/adapter/core hook/event header、所有observer/audit/core动态判据、61B launcher与真正compiler wrapper、compiler/runtime/linker/CRT、实际配置include头集合、Python词法/resolve、继承PATH解析的ps、原wire/trace及fixed-M完整oracle。`-MD/-MF`使编译后的实际依赖必须属于这个事前集合；不在编译后补身份。原/mnt/c输入不搬。

资源schema=`phase-c-resources-v1`，`policy`是同一固定限制，`observation`含实际guest/C backing空余与MemAvailable等。准入峰值固定4GiB，两个卷均要求空余≥4GiB+2GiB；AS硬16GiB、观测RSS≤16GiB。producer四body每role合计64MiB、两role128MiB；原wrapper每child20秒、日志1MiB；编译每child120秒、单file128MiB、编译日志保存≤1MiB。内存可用量如实记录，不把当前空闲当未来分配保证。

固定命令上限600秒，内部保守投影`2*(120+2+20.5+60)+100=505秒`，其中每role编译清理2秒、native清理0.5秒、完整离线role审计60秒，身份与封存余量100秒。实际运行前仍预留当前role的完整预算，并在重哈希后再次核对native/audit/seal余量；不fit不启动，不延时或补样。

## 具体同hash准入文件

root独立审核创建`C_ADMISSION.json`，至少：

```json
{
  "kind":"phase-c-independent-admission",
  "status":"passed",
  "freeze_sha256":"ACTUAL_FREEZE_SHA256",
  "R1_B_passed":true,
  "interface36_scope_accepted":true,
  "resources_passed":true,
  "final_source_and_tools_passed":true,
  "evidence":{
    "R1_B":{"path":"ACTUAL_ABSOLUTE_RECEIPT","identity":{"bytes":0,"sha256":"ACTUAL"}},
    "interface36_acceptance":{"path":"ACTUAL_C_OBSERVER_SCOPE_ACCEPTANCE_JSON","identity":{"bytes":0,"sha256":"ACTUAL"}},
    "source_review":{"path":"ACTUAL_REVIEW","identity":{"bytes":0,"sha256":"ACTUAL"}},
    "resource_review":{"path":"ACTUAL_RESOURCE_REVIEW","identity":{"bytes":0,"sha256":"ACTUAL"}}
  }
}
```

示例中的0/ACTUAL是待替换字段，不能直接执行。interface36是已接受的34→36纯数据桥接，既有18/6项真实自检按原scope引用；不要求或伪称新的normal36实际执行。freeze源检查两份interface除labels新增truth.replay.before/after以外完全一致。

## 唯一600秒实际 C job

OUTPUT固定`/root/xoffset-c14-iteration/20261020/v2/validation/phase-c-original-run-v1`。原runner W3、kind diagnostic、timeout600、wait0，queue=`OUTPUT/queue`，cwd显式C14。command：

```text
/usr/bin/python3 -B /mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/tools/c14_iteration_20261020_v2/validation/diag/phase_c_job.py run --frozen /mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/docs/coordination/c14-iteration-20261020-v2/validation/diag/C_FREEZE_DRAFT.json --freeze-sha256 ACTUAL_FREEZE_SHA256 --admission /mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/docs/coordination/c14-iteration-20261020-v2/validation/diag/C_ADMISSION.json --queue /root/xoffset-c14-iteration/20261020/v2/validation/phase-c-original-run-v1/queue
```

coordinator验证exact command、原lease当前holder/runid/parent PID/新session及600/0参数。输出起初只允许queue。C14编译→一次原run→全role审计→R1编译→一次原run→全role审计；最多2compile、2native，第一项失败停止后续并保留前缀。编译采用独立fork/wait4与受保护的pipe collector，native直接调用已验收48ddd observer API；不嵌套抢lease。

native20秒使用`started_monotonic_ns`至唯一wait4的`returned_monotonic_ns`。绝不使用较晚的wrapper finished/seal时间放宽或误判child。原完整campaign.native_result收到的finished就是returned；另保留wrapper finished供全job预算核对。固定36原始事件必须按真实顺序、seq、child/接收单调时钟及fork/reap界限通过。emit9样本逐个status0、seen511/invocation1、累积资源/POD/byte ledger一致；计数mask1022、P0=M0=0、M9=P3、stage1=M1-P2、九stage之和=mutate；原native和独立九stage replay/canonical/query完整通过。source/input/tools/runtime前后均绑定。

`AUDIT.json`通过只表示执行、真值、事件/资源和计数守恒成立。R1动态命中与bare代码生成是另外的门禁；已预登记的offline checker可在外部pin住两role AUDIT后执行，不新增native、重跑机制或改变C命令。t4后flush污染邻接诊断区段，peak_rss_after_audit包含额外replay/observer；不称裸driver峰值，不相加嵌套faults，不作速度或M1历史原因结论。RESULT仍需root核对原runner最终queue manifest/result/cleanup与actual600秒，才可验收整个C job。
