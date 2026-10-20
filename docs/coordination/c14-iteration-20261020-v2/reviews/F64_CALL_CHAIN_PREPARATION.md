# F64 调用链准备交接

本记录仅保存已完成的只读源码核对。交接时本次 Q32 原 offline audit 尚未返回；独立原始统计30/30 A/A通过不能替代完整 Q 审计。本记录不生成实际 qualified-Q acceptance，不准入 F freeze、host 操作、租约或计时。

主根为 `C:/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发`。核心执行器 `tools/c14_iteration_20261020_v2/validation/final_formal_bound_v1/six-role-campaign.py` 实际118612 B、SHA256 `6c3319cb3502ce380d3ddd022be33de9da79483ea7ecbd911695fc215b4964c8`，读前后稳定，已经支持 formal。原 bound11 不需要修改。

## Q 资格记录及其实际读取合同

`require_qualified_q()`（502–557行）读取当前 `COORD/qualification-acceptance.json`，其中 COORD 为主根下 `docs/coordination/c14-iteration-20261020-v2/validation/final_formal_v1`。本次核对时该文件不存在。仅当原 Q 审计完整通过且独立实际接受后，才可由负责会话生成它。

下面只说明最低读取字段，不是可执行模板或实际接受结果：

```json
{
  "status": "passed",
  "measurement_qualified": true,
  "complete": true,
  "audit_report": {
    "path": "实际原Q审计报告的绝对路径",
    "identity": {"bytes": "实际十进制字符串", "sha256": "实际SHA256"}
  },
  "queue_request": {
    "path": "本次Q原queue/request.json绝对路径",
    "identity": {"bytes": "实际十进制字符串", "sha256": "实际SHA256"}
  },
  "queue_result": {
    "path": "本次Q原queue/result.json绝对路径",
    "identity": {"bytes": "实际十进制字符串", "sha256": "实际SHA256"}
  },
  "postrun_pin": "b3db8adafebb4079f65f7557e00917ac108a71184dfcb93ed7bfc2df992449ce"
}
```

`K.file_id()` 输出的 bytes 是字符串；`K.validate_id()` 也要求 decimal 字符串，不能直接把其他索引中的整数 bytes 搬入该合同。path/identity 对象只含这两个字段。顶层可以附加真实独立接受的 provenance，但不能以此替代以下原读取检查。

核心要求实际 audit report 的 status=passed、campaign=qualification、complete=true、measurement_qualified=true、successful_actual_children=4656、successful_primary_children=2328、budget_violations=[]，且其 queue_result 身份等于 acceptance 的实际 queue_result。analysis.policy 必须等于原 Q policy，qualification_passed=true，六角色×五指标30项 A/A均明确 equivalence_established=true。

核心同时核原 Q queue：W3/timing、exit0、cleanup 三列表全空、command elapsed和从request.created_at到result.finished_at的完整wall均不超过2700秒；六成员 queue manifest 与实际 inventory一致，request 必须位于同一 queue。还核 Q run manifest、Q run内 frozen manifest、postrun复合pin、固定二进制和 candidate profile/domain，以及 Q 的完整11工具与当前工具逐字节身份相同。

本次 Q run 为 `/root/xoffset-c14-iteration/20261020/v2/validation/final-formal-v1/qualification-v1`。既有小执行请求登记的 run manifest 为4849967 B、SHA256 `bc110e793dd37d4da1cf32f83a56e7c590f75b68b12db0bb85745298bbd0c3ca`，queue manifest为913 B、SHA256 `a5ddc3ffcefc8ff481d1c2094aa22cf73bc67377739663f2675029ae22f436a8`。本次只用该小执行请求重算复合pin，得到了上列 `b3db8ada…`；没有重新 hash 大 run manifest，也没有提前接受未返回的审计。实际原审计报告目标为主根下 `docs/coordination/c14-iteration-20261020-v2/validation/q32_freeze_preparation/Q32_OFFLINE_AUDIT_RESULT_v1.json`，其身份必须在真实生成后绑定。

## freeze 与 run 的边界

F `freeze()`（559–611行）先读取 `combined-gates.json` 的 passed 状态，再在562–563行调用 `require_qualified_q()`。随后复制当前 qualification acceptance，生成新的 formal runtime/profile/schedules/policies/status/manifest，并执行 maintained `verify_frozen()`。

F `run()`（652行起）不重新调用 `require_qualified_q()`；它按传入 manifest pin验证冻结包、输出路径和当前runtime，验证原 W3/timing直接子进程与原共享租约后，复制冻结包再开始窗口。核心本身不读取 root REVIEW、HOST_REVIEW、GLOBAL_QUIET 或外部 once ledger；这些闭合由外部 launcher 和 root 准入承担，不能把 core 的 freeze/run 成功当成这些条件已经通过。

因此必须新建 `frozen-formal-v1`，使用真实 F manifest；不能把 Q frozen改名或修改其manifest。原七份 registration/proposal 引用继续按已接受链使用；新的即时容量观察和 root records应使用独立新路径。F freeze复用原业务输入、fixture、profiles、record和竞品模式，不需要新编译或新的 native capture；其原 maintained校验和实际物化成本仍保留。

## 原 F 协议与资源

| 项目 | 固定 F64 条件 |
|---|---:|
| macrocycles / blocks | 64 / 384 |
| 每block | 六角色各A/B两次，共12个primary |
| primary / conditioning | 4632 / 4632 |
| primary warmup / inference | 24 / 4608 |
| 实际children / 单child上限 | 9264 / 20秒 |
| 唯一完整窗口 | 5400秒，零重试 |
| allocated / logical retained上限 | 8 GiB / 512 GiB |
| 总wire / primary wire上限 | 256 GiB / 128 GiB |
| metadata reserve / disk margin | 674234368 B / 2 GiB |

保留六角色、原 seed/schedule、30项 A/A、12项主目标与40项辅助、既有统计修正和解释限制。正式逐竞品目标仍为 XOffset÷同批各竞品的读取合计≤1.05、修改≤1.05、独立输出≤0.20；重载完整报告，不增加比例门槛。Q不计算跨库对比，F也不能改样本、追加有利批次或变更竞品模式。

仅按现有固定wire大小和F调用数计算，总wire为157351335928 B，primary wire为78675667964 B；最坏child peak加reserve为909630704 B，逻辑固定部分（不含新的实际F base）为324076960840 B。这些是工作量/容量算术，不是性能结果。实际新F freeze后须重新绑定其base、manifest/apparent/staging、完整retained投影、allocated及guest/backing即时空闲；原8 GiB cap加2 GiB margin要求至少10 GiB空闲，历史Q容量快照不能充当当前准入。

## 新 formal_host_v1 的必要薄派生

现有 `q32_host_v1/execute.py` 绑定Q的manifest、qualification目录、records kind、2700秒及2820/2760时长；`process_pause.py` 的状态目录固定且max上限2820，`pause-control.ps1` 也为 `ValidateRange(1,2820)`。不能将原Q launcher原样用于5400秒F，也不能仅改调用参数。保留全部Q源码/结果，在新 `formal_host_v1` 中做必要薄派生，并独立审查真实差分。

建议保留Q的同一裕量结构：timing5400秒，独立process watchdog5520秒，紧邻dispatch时保守剩余量≥5460秒。5520/5460是待新源码接受的薄派生设计，不是当前Q工具已有值；Python/PS两个上限、所有request/ready/completed/coverage和root host字段必须一致。原核心 `PowerWindow(MAX_SECONDS+60)` 对F自然为5460秒，现有7200秒上限支持，无需修改bound11或原power源。

新 host/controller/prelude/process-state/power目录须与Q分离，采用formal路径并绑定实际F pin。保留W1/validation/60秒prelude、≤30秒handoff、dispatch前再次核剩余量、W3/timing/wait0、fresh PID/creation/SID/持有handle/token、成功失败恢复、实际power ready/coverage/completion及恢复后才audit。只允许已有明确授权的 `client_ue4-Win64-Shipping`、`TGitCache`，不扩大到Editor/zenserver或其他进程；不复用旧PID、旧state或旧quiet。

原实际timing子命令合同为 `/usr/bin/python3 -B <原bound six-role-campaign.py> run --campaign formal --out <…/formal-v1> --frozen <…/frozen-formal-v1> --frozen-sha256 <实际F pin> --queue <…/formal-v1/queue>`。原runner使用W3/timing/5400/wait0，必须直接启动该子进程，不能插入 `-I` 或包一层timing子进程。准备型freeze与metadata操作另循各自准入。

## 交给原验证会话的最小顺序

1. 等待本次原Q完整审计结束，独立核真实report/queue/manifest/power/pause/预算闭环；失败则保留失败，不形成qualified acceptance、不进入F。
2. 通过后写真实 `qualification-acceptance.json`，仅绑定本次Q；保持bound11不变，使原Q仍资格覆盖当前完整工具闭包。
3. 新formal_host_v1源码/草稿可先准备；在有限静态差分和调用闭合接受前，不执行host操作或占用计时窗口。
4. 获明确的有限准备准入后，原验证者执行唯一新的F freeze；记录原队列、maintained verifier、真实manifest和资源投影，独立接受实际冻结产物。
5. 以实际F pin生成新timing request、root REVIEW、HOST_REVIEW、≤900秒GLOBAL_QUIET及外部attempt ledger（maximum_windows=1、retries=0），全部固定新launcher/source/request/host/实际freeze证据。随后才可单独考虑F计时准入。
6. 唯一F窗口完成后先恢复，再原队列审计和完整offline body audit；不把子进程结束或统计阶段完成当作正式目标已经通过。

本次审查未修改源码或冻结文件；项目Python、编译、native、capture、租约、freeze、host操作和跨库统计均为0。该文档是唯一新增文件。
