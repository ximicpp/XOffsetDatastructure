# R1 / M2 独立 screen source-ready v2

当前入口为 `SOURCE_READY_v2.json` → 各自 `REQUEST_DRAFT_v2.json` / `TOOL_MANIFEST_v2.json`。旧无后缀 draft、manifest、SOURCE_READY 的字节保留；它们对应未执行中间版，不能给当前代码准入。v2 的十项 metrics 直接取各自冻结 design，原八阶段为 load/query_before/mutate/emit_after/retire_modified_state/reload/query_after/cleanup，另有 read_sum 和阶段和 total。

R1 执行代码已经稳定：`tools/c14_iteration_20261020_v2/validation/screen/r1/screen.py` 为 59,903 B / `0ae713213e7f5cfeba5cba8cd7dded155fe44a3114fbcecabf30f23f6f651a21`。当前 exact 有限资源请求 `R1/RESOURCE_CAPTURE_REQUEST.json` 为 1,145,648 B / `07ebe1dcc9933faff07a3fa438a4a484c1026be1db536043acc310781cb2bdfa`；`R1/RESOURCE_CAPTURE_DISPATCH.json` 为 2,452 B / `e3cea2f351c952bf3bf6e38749eec0cb8a4685fc3ff998030041fe244188026e`，其中 argv 即原 Documents 全局 runner 的完整命令、cwd 为唯一 C14 根。

请求只允许 control 后 candidate 各一个完整裸 run：child20s、outer120s、0 compile、0 warmup、0 retry、0 timing-inference samples。运行还需统筹独立写 `R1/RESOURCE_CAPTURE_ADMISSION.json`，kind=`independent-resource-capture-admission`、status=`passed`、candidate=`r1-canonical-hotcold-v1`、request_sha256 精确为上述 07ebe…；本次准备未创建该准入，也未运行请求。

请求已实际只读绑定 source154 / production14、两份真实 profile/binary/compile argv/compiled dependency 文件、61 B launcher、P2996/libc++/实际 headers/runtime、原全局 runner 和 Python、当前CPU2 affinity、完整 R1 functional closure、input policy 及当前 power 观测。两份 bare 实际 argv 均为 -O3 -DNDEBUG、P2996 flags，无 -UNDEBUG/SAN/diagnostic hook。legacy adapter commit 7539433…仅为沿袭的 benchmark base metadata；实际候选身份依154/14、compiled deps 和 profile content SHA，不能声称其为当前候选 Git commit。

control 的实际编译树与 materialized154 仅 SOURCE_MANIFEST 包装元数据不同；其余153及14生产文件逐项一致。capture 绑定 executor 的已 passed 真实差分 review `validation/review/CONTROL_METADATA_EQUIVALENCE_REVIEW.json`；统筹的 `reviews/CONTROL_METADATA_EQUIVALENCE.json` 又独立核308项并批准同一有限例外。exact capture admission 及最终 freeze review 需同时引用该统筹记录，不能将元数据例外扩展至候选或生产字节。

统一 input policy 已由 `reviews/SCREEN_INPUT_POLICY_ACCEPTANCE.json` 批准。C_RESULT 精确 SHA 为 c13e761ba41a67a7abcac600fbcea2a1edfe517032c782ccc98f1d41cbeab2c4；各 `INPUT_POLICY.json` 引用两份实际 observer receipt，自动按 child monotonic event 的 input.read.after-before 得到 C14=887,414,204 ns、R1=1,101,559,283 ns。它们是带 observer/replay 的归因证据，不是 AB 速度、不是旧 M1 原因。parent 的 staging 已逐项核原/新18个 hash：九文件共62,191,438 B，copy1.275457023s、retained allocated62,218,240B，0 native preview。

control/candidate 在 capture 与新 screen 都使用同一 `/root/xoffset-c14-iteration/20261020/v2/validation/screen-inputs-v1/initial-wire.bin` 与 fixed-M；每次 before/after 检查九个原/副本文件。原 /mnt/c 输入保持原字节。旧 /root functional M 的约0.366s只能证明已有裸完整 run 收据可读，不直接代表原 /mnt/c 输入或600s可行性。本 exact 请求用统一政策下两份新完整资源收据解决该缺口，不选快者、不计算差值。

capture 完成后，独立只读投影命令为：

```text
/usr/bin/python3 -I -B <C14>/tools/c14_iteration_20261020_v2/validation/screen/report_projection.py r1 --result <C14>/docs/coordination/c14-iteration-20261020-v2/validation/screen/R1/RESOURCE_PROJECTION.json
```

这里 `<C14>` 为 `/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发`。投影核完整 capture manifest、原120s queue/cleanup、原始process/完整body真值收据和 before/after closure。每 child 使用实测完整进程加完整 native_result/逐字节 oracle 审计墙时，再加事前固定0.25s tail margin 覆盖 dedup/ledger；取两 role 最大值乘144，加 nonchild60s 和 staging1.275457023s，须≤570s，另外保留seal30s。物理投影假定144个wire全部独有，只允许完整 oracle byte-exact hardlink dedup canonical/queries，另计临时7个最大文件、metadata64MiB、retained capture 和 staging allocated；须≤4GiB且实际 guest/C backing 另有2GiB reserve。projection 不通过即不启动 screen，不追加资源采样或重试。

最终 screen freeze 入口见各 REQUEST_DRAFT_v2 的 argv。`GATES.json` 要引用十一份已完成同候选154/14的真实 typed receipt：codegen、真实命中机制、定向（R1 5+5 / M2 4+4）、normal73、SAN73、业务80+80、V1双向84+8/AB4/C4、安装/共享runtime、生命周期52、safety、事前engineering成本。每项必须有实际 evidence refs 与独立审查；现有 gate-drafts 仍明确 pending，不能借 M1 或另一候选结果。freeze 自行重新算实际资源投影，之后仍需统筹独立精确freeze SHA准入。

每候选新公开seed的平衡随机72 pairs/144 children、AB/BA36、df71、原4主/36辅助族和原 t 常数不变。R1 主 read_sum、M2 主 mutate；任何 metric 零值保留、该 log contrast undefined且禁止晋升。缺失、负值、非整数、非有限或身份/真值/timeout失败关闭窗口，失败summary=null，无前缀推断、epsilon、删点、补样或重试。

执行 controller 的 host prelude60s、handoff≤30s、独立watchdog780s均在全新各候选目录。实际授权仅原两进程名，新plan实际PID/image/SID/creation及新held handles/token；未复用旧PIN/guard/PID。controller 读当前冻结power而不切换；实际静默与host review在run前新提供。成功/失败先stop/resume用户进程并写恢复，再做离线audit3600s；完整成功还核实际pause覆盖整个timing queue。此次未plan、未pause、未queue、未compile、未native。

M2 的独立工具/统一input政策已 source-ready；其实际 own business bare profile/config、完整功能/机制/成本 receipts、有限资源请求与最终 host/freeze 仍待各自闭合，不复用R1执行结果。
