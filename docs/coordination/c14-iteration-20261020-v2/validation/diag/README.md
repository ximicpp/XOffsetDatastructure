# V2 固定诊断监护工具

唯一写入树为C14 worktree。接口原件位于`tools/c14_iteration_20261020_v2/validation/diag/INTERFACE.json`，本目录保存同字节副本。root已于2026-10-19 17:03:35 UTC启动dummy-v1，18项实际全部通过，原queue清理为空、耗时3.274149708秒，见未修改的DUMMY_RESULT.json。已测工具及当时文档按原字节保存在tools与docs的`revisions/dummy-v1/`。当前deadline路径修订v2尚未自检，仍为0真实diag；作者没有自行执行dummy/C++/业务native。

核心事件：`XD1 <seq> <child_monotonic_ns> <label>\n`；只有`emit.pod0/1/2`额外携四个u64。每条≤128 ASCII字节、最多64条、每条一次非阻塞write，不重试。所有34个固定label均为完整诊断的必需边界，见INTERFACE。emit内部只记POD，t4后在emit之外flush三条POD，字段/单位由核心独立登记。时间为同WSL CLOCK_MONOTONIC；parent收到即以无缓冲write/flush保存原bytes，同时保存解析事件及接收时间，不执行fsync。只承诺child被kill后已接收前缀仍在外部文件中，不承诺掉电或内核崩溃持久性。kill路径先核对当前owned PID身份并发SIGKILL，再尽力记录，无同步持久化syscall挡在首次信号前。seq缺口、未知标签、半条尾部、溢出均保留且使observer不完整，不判为库故障。

wrapper使用fork/exec，child在新PGID但保持原共享runner session；设置Linux subreaper，仅回收本PGID的被收养后代。没有Popen、waitpid或后台reaper。direct child仅有一次成功wait4，全生命周期CPU/faults/maxRSS来自其终结rusage；被收养后代的回收另列，不把资源冒充direct child指标。ECHILD明确记监督器错误和资源unavailable，不换用poll/waitpid补造成功。每次wait4均WNOHANG；kill后的清理截止为原child deadline+500ms，无无限blocking wait。发信号前重新核对当前PGID/session/start_ticks及parent链，再逐PID重核身份并发SIGKILL；无当前已验证owned成员时不对旧数值PGID裸kill。

body单文件RLIMIT_FSIZE为128MiB；四个body合计128MiB由循环及退出检查，越界失败并保留实际文件（不截断证据；burst超量并不保证被采样前物理从未超过阈值）。stdout/stderr/events三通道持续排空，保存总量上限1MiB，越界kill且保留精确前缀。事件pipe为O_NONBLOCK，stdout/stderr pipe由同一deadline循环排空，不先wait再读。地址空间硬限制16GiB并记录实际maxRSS。正式child deadline固定20秒，从fork前开始，不随事件到达重置；单次wrapper外层原runner上限≤60秒，整个两diag job的600秒预算仍由统筹coordinator持有。

## root执行dummy自检

将下列路径展开为绝对路径。`T=/mnt/c/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发/tools/c14_iteration_20261020_v2/validation/diag`。

原共享run_job由root建立W3、kind validation或diagnostic、timeout30、wait0的新queue；精确command：

```text
/usr/bin/python3 -B T/diag_wrapper.py selfcheck --out /root/xoffset-c14-iteration/20261020/v2/validation/diag/dummy-v2 --queue /root/xoffset-c14-iteration/20261020/v2/validation/diag/dummy-v2-queue
```

queue与dummy-v2分开，后者必须不存在。dummy-v1的首轮18项通过保留，修订后的root只在独立dummy-v2路径执行一次固定自检；失败先读实际receipt和错误，不重复运行同一输出目录。原TOOLS_PREPARED.json和DUMMY_RESULT.json不覆盖，新身份见TOOLS_PREPARED_v2.json及REVISION_v2.json。

当前v2固定19个Python dummy各一次，无C++、无业务或正式diag：normal/nonzero/signal/truncated/EPIPE/reader-close/large-log/log-limit/seq-gap/event65/exec-failure/collector-error/register-error/ECHILD/descendant-FD的child cap均0.75秒；no-event-hang/event-hang/partial-hang各0.25秒；wakeups为0.12秒。每case再有最多0.5秒固定清理余量，全部cap之和加清理约21.7秒，外层30秒。每case body=0，log≤1MiB；保守产物预算24MiB。子进程AS上限沿用16GiB，实际Python RSS写入每份回执。新增register-error在第一个FD已注册、第二次注册时抛出EMFILE，验证同一finally覆盖kill/reap及全部已注册/未注册FD关闭；dummy关闭reader的记录写入也在该protected段内。

正常与明确非零/信号、无事件挂起、已有前缀挂起、半行挂起、截断、EPIPE、关闭reader、超过pipe容量的持续stdout/stderr、总日志上限、seq缺口、第65事件、不断唤醒但deadline不延长、exec失败、collector写错误、ECHILD、parent退出而后代持有FD均各有固定检查。EPIPE由dummy-only握手确认reader已关闭后再write，避免靠sleep制造竞争。ECHILD在唯一实际kernel reap之后故意丢弃返回值并注入错误，验证不会补第二种reaper/伪造资源；它不声称真实OS发生ECHILD。collector错误在已持久化首事件后注入ENOSPC，不填满用户磁盘。后代用例最后执行且也必须cleanup_remaining为空才自检通过；任何真实残留在receipt和原runner最终cleanup中显示，不能给下一实际diag准入。

## actual diag调用合同

每个role一个新wrapper目录及四个新的绝对body路径；outer coordinator仍先control后R1，至多两次，任一失败停止，不允许wrapper自重试。wrapper CLI可由原runner直接调用：

```text
/usr/bin/python3 -B T/diag_wrapper.py run --out OUT --environment ENV.json --queue QUEUE --binding BINDING.json --body WIRE --body CANONICAL --body QUERIES --body NATIVE_JSON -- /absolute/instrumented-binary run --wire FIXED_INPUT --trace FIXED_TRACE --wire-out WIRE --canonical-out CANONICAL --queries-out QUERIES --result NATIVE_JSON
```

ENV.json是明确字符串字典。BINDING.json为`role:"control"|"R1",instrumented:true,eligible_for_performance:false,files:{source_identity,driver,binary,compile_receipt,hook,...}`，每file为`{path,identity:{bytes:<int>,sha256}}`；父进程核对前后实际身份并单独写binding-after.json。生产14/完整154、原wire/trace、真实instrumented compiler/runtime/依赖闭包由上层冻结请求提供，不以一个main SHA代替。

wrapper保存launch-before、launch-return、kill请求、唯一wait4返回、原事件/stdout/stderr、parsed events、完整direct-child rusage、最后区段、cleanup残留、四body exists/identity、参数和原queue request。receipt的`complete`只表示observer及退出状态，`semantic_validation`始终是not_performed_by_observer；上层必须执行原完整native_result/wire/canonical/query审计及原runner cleanup审计，才能给诊断通过。两diag内部时钟与计数均受hook污染，不产生AB比值、置信区间或正式性能资格。

若现有600秒coordinator已经直接持有原全局lease，不嵌套抢锁：可加载同一个模块调用`supervise(command,out,environment,bodies,binding=...,queue_request=...,absolute_job_deadline_ns=...)`。调用者核对其当前lease及冻结请求，并将整个job绝对单调deadline传入；不够20秒child+0.5秒清理则启动前拒绝。此API本身不会启动第二次diag或编译，CLI的lease检查仅供独立原runner直接执行。

不可回收、磁盘/collector失败、ECHILD、原runner timeout等保留现有prefix并阻断后续diag。若wrapper本身被外watchdog杀掉，无最终receipt时，launch-before、已落事件及queue结果仍是失败证据；不得补造完整进程rusage或再运行一次。
