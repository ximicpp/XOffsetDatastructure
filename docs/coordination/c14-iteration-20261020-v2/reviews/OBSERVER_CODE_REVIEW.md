# V2 观测器源码窄审查

日期：2026-10-20（Asia/Shanghai）。审查者：`runtime_observer_review`。

**结论：主体方案成立，当前 revision 尚需关闭两项具体监督问题。未执行 dummy、C++、编译或性能。** 此结论是源码审查，不替代实际稳定版 Python 自测、核心 producer 的 SIGPIPE 证明、两角色诊断或完整真值审计。

## 读取身份

| 文件 | bytes | SHA256 |
|---|---:|---|
| `validation/diag/diag_wrapper.py` | 23161 | `d6a9746a80e53ff05416126d719c89d7a0924ed742db67c9ff029f87955ed5d2` |
| `validation/diag/dummy_selfcheck.py` | 4939 | `3cf9209ae8276df44b777295b55f09031920b4d2803b33a19e3982f530427be5` |
| `validation/diag/dummy_child.py` | 2224 | `01cd1edb6379f9a3454855b5358d8596c7696848d2d9c5769575e375c102b2db` |
| `validation/diag/INTERFACE.json` | 2739 | `737bc78401caf58d0fd7300f8e669f1b48eb30595798d67625188fa5c55a6a5b` |
| `core/common_diag.hpp` | 2654 | `55221d97734679cfda69cef3f590b80574e54b313afbfa6918316d6d477ed689` |

工具路径前缀为 `tools/c14_iteration_20261020_v2/`。同轮 docs 的 `validation/diag/INTERFACE.json` 与工具副本字节相同。行号对应本表 revision，修改后不能借用旧身份宣称通过。

## 必须修正

**B1：逐批 fsync 在监督关键路径造成无必要阻塞。** `diag_wrapper.py:37` 对每份 raw chunk 和每条 parsed event 都执行 fsync，199、215行调用它；31行的 `exclusive` 也在 launched、wait4-return、kill-request 等活跃路径同步落盘，特别148行先 fsync 再向 child 发 SIGKILL。同步磁盘等待期间不检查20秒 deadline，也不排空 stdout/stderr PIPE；可能影响诊断，且日志恰好有问题时推迟终止。236行最终清理还在组清理之前 fsync。此任务只要求 child kill 后仍有外部可读 bytes，不要求掉电持久性。最小修复是即时 write/flush 保留 raw，移除逐批及监督/kill路径的 fsync，并同步修改 INTERFACE 的 consumer 合约。无需增加 transport 或异步日志框架；最终封存若保留 fsync，也须在 child/group 已清理之后且仍受外层预算监督。

**B2：fork 后部分 setup 绕过既有 finally。** 135行 `sel.register` 在 child 已启动之后、169行 try 之前；注册抛 OSError/MemoryError 会直接逃逸，未走 kill/reap。167–168行 dummy reader-close及落盘也在该保护之外。最小修复是将这些操作放进已有 try/finally，保证 setup/collector 失败都进入同一清理。用固定故障注入验证一次即可，无需新建监督层。

## 已成立的主要行为

- 实际使用 fork/exec，没有 Popen，也没有竞争 wait/poll；direct child 的唯一成功回收点是175或异常finally中的229行 wait4。status建立后不会二次回收。收养后代另按PID回收并单列，不掺入direct child计数。
- child事件fd为 O_NONBLOCK，seq计尝试事件；parent selector同步排空event/stdout/stderr，raw先保留再解析。缺seq、坏行、半行、超过64和缺终结均不能获得complete。kill后继续排空至EOF；持有FD的后代有固定退出后等待和清理路径。
- launch之前建立绝对child deadline，select、kill与finally均不因事件重置它；无blocking wait。group成员通过PGID/session和start ticks重新确认后按owned ancestry发信号，避免直接向陈旧数字PGID信号。清理残留会显式失败。
- wait4返回的CPU/faults明确命名完整fork/exec child lifetime，包含该child自身已wait的后代；maxrss单位为KiB。没有将stdout时间、coordinator资源或旧t0..t8资源冒称完整进程资源。
- `instrumented=true`、`eligible_for_performance=false`、`performance_inference=null`、`semantic_validation=not_performed_by_observer`明确。wrapper的complete仅表示监督观测完成，真实诊断成功仍须调用方校验四份body和原业务真值。
- `common_diag.hpp` 使用固定POD、thread_local计数和sample，没有在emit细分路径写日志；区间标记明确为嵌套区间，不能相加成独立成本。真正producer和完整driver hook不在本次窄审范围。

## 调用方必须证明的前提

- `supervise` 有 `absolute_job_deadline_ns` 的启动预算检查，但CLI main:322没有传它，且单wrapper队列只限制≤60秒。因此本文件不能宣称整个“两次编译+两run”600秒已实现；调用方需要实际600秒外队列/统一deadline证据，或把固定absolute deadline传入。不能只读取接口参数即宣称整批cap生效。
- `returned>deadline` 已使status失败，但 `timed_out` 目前仅看kill_reason。若未发kill而迟回收，应用 `child_deadline_exceeded` 明确解释为观察超限，不能仅以timed_out=false宣称未超时；建议将receipt另列deadline_exceeded，保留是否实际发kill的区别。
- 正式72对必须使用无observer的独立bare产物及实际编译/依赖闭包。该wrapper的source/binary前后绑定不自动完成“诊断产物不得进入screen”的审计。

## 自测覆盖审查（尚未执行）

18个固定Python用例确实分别产生正常、非零、signal、无事件挂起、事件后挂起、部分行、截断、EPIPE、关闭reader、大stdout/stderr、日志上限、seq缺口、第65事件、重复唤醒、exec失败、collector错误、ECHILD注入和持FD后代。每用例要求cleanup为空；失败即停；没有项目binary或C++入口。

其中ECHILD明确是在唯一真实回收之后丢弃结果的故障注入，用于检查“资源未知、监督失败”分类，不是声称真实竞争reaper已测试。`event65`目前只断言failed和len(events)≤64，最好进一步断言received_event_lines=65及对应count-limit错误，避免别处失败掩盖该界限。`large_logs`只验证退出码和原始字节计数，不声称缺少完整业务事件的程序是正常诊断。这两点无需扩大为通用框架。

修复B1、B2后，对稳定工具重新核对身份和对应行即可；实际自测由唯一验证者在原共享租约执行一次。本审查没有执行权限，也未运行任何dummy。

## B1/B2 修复后的稳定版窄复审

**结论：B1、B2 在下列实际 revision 的源码中已闭合；源码窄审通过。旧18项dummy结果不属于该revision，尚需仅补修改影响的用例，不能借旧passed标签准入真实诊断。** 本段仍只读，未运行dummy、项目native或任何编译。

| 实际文件 | bytes | SHA256 |
|---|---:|---|
| `validation/diag/diag_wrapper.py` | 24110 | `48ddd6262b371a96ec1eabd2c57c57aa392e909e831869c5dae634de1d92fc34` |
| `validation/diag/dummy_selfcheck.py` | 5222 | `cdcaa06338c7c9d247aa1a4af080738658d1aef4d6919063c951b2ef1f9aa130` |
| `validation/diag/dummy_child.py` | 2224 | `01cd1edb6379f9a3454855b5358d8596c7696848d2d9c5769575e375c102b2db` |
| `validation/diag/INTERFACE.json` | 2873 | `d551ac98c06e582e58ab6570ec3bbe43382b52e6a6ce6b18d41b32fd5d873ab9` |

统筹发来的中间wrapper身份`309e277a...`不是本段实际读取的文件；本结论只绑定上表，不追认中间版。

- B1：`persist`、`exclusive`均使用普通无缓冲write/flush，无fsync；partial write按实际进度继续，无进展明确报错。kill先向重新验证的owned PID发SIGKILL，再尽力留kill记录。close日志也已移除fsync。工具INTERFACE已明确只保留child kill后的收到前缀，不承诺掉电持久性。
- B2：selector注册和dummy reader-close已在同一try/finally中；finally关闭全部fd，包含尚未成功注册的fd。新增`register_error`固定注入第二次注册失败，可检查collector_error、SIGKILL、单次回收和无残留；无需新的监督框架。
- 600秒整批预算留给实际dispatch/freeze核对，CLI不需要扩成新框架。本窄审没有收到该执行闭包，不给出整批预算已验证的结论。

### 原18项自测身份与可复用边界

实际 `validation/diag/DUMMY_RESULT.json` 为8491B、SHA256 `b5e2e30d3c968f68ec91b81a951ea0993ba5491215b46ae4285fab88a8fbe1db`。报告18固定Python child全部passed，C++/业务/真实diagnostic均0；原queue `b215fe32df91429dab96655855441860` complete，3.274149708秒，cleanup三项全空。

该报告的`tools_after`与`DUMMY_DISPATCH_REQUEST.json`绑定 **旧wrapper d6a9746...、旧selfcheck 3cf9209...、旧INTERFACE 737bc784...**。进一步只读原始`/root/xoffset-c14-iteration/20261020/v2/validation/diag/dummy-v1/normal/launch-before.json`，wrapper/interface也确为旧SHA；normal原始receipt SHA `d25564531c97944a22944a7be1b65808aacc29d030861f79a052216ea6bb18d8`与报告一致，不能将18项改写成新revision的成绩。

原始event65收据实际记录received_event_lines=65、kill_reason=event_count_limit、event_count_over64、一次回收及空cleanup；旧selfcheck断言虽宽，实际边界确实被触发。原始event_hang为timeout、-9、last_section=mutate、一次回收及空cleanup，已保留的机制证据有效，不需要为了重新展示再跑同批18项。

最小补测建议固定 **normal、event_hang、large_logs、collector_error、register_error** 五类，分别覆盖去fsync后的正常写出、kill前缀/单reaper、持续排空、写失败清理及注册失败清理。由唯一验证者冻结新工具身份、一次执行并单独保存修改后证据；其他未改动类别保留旧原始回执及本次源码差异审查，不盲目重跑全套。若实际工具在补测前再改动，绑定最终实际SHA而非本段旧值。
