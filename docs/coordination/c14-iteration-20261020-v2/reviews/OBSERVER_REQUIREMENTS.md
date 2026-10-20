# V2 原 run 观测器：最小要求与失效检查

日期：2026-10-20（Asia/Shanghai）。独立审查：`runtime_observer_review`。

本次只读审查 HANDOFF、M1 失败封存、旧 worker 审查和 Python 执行工具；没有构建、运行 C++、执行性能或修改工具。本文不是新工具通过回执，也不是实验启动许可。仅本文件为本审查新增产物。

## 1. 已发现的边界

- 旧 timeout wrapper 实际位于 `tools/c14_iteration_20261019/validation/screen/campaign.py:245` 的 `execute`；`screen/core.py` 仅含证据原语。旧 wrapper 使用 `Popen.wait`、超时后 kill/wait，stdout/stderr 直接落盘，收据在最后写入。
- M1 的 p059-1-candidate 证明完整进程超过固定 20 秒并被杀；没有 native/body 产物且 stdout/stderr 空，不能判断当时位于输入、八阶段或后置真值审计。旧 `cpu_ns` 和 faults 只覆盖 t0..t8，不能解释完整进程的墙钟差额。
- V2 只允许一个 C14、一个 R1 原 run，按冻结顺序执行；每 child 20 秒、总 600 秒，失败即停止。新增观测不能变成额外 native、采样或重试入口。

## 2. 最小 API 与事件通路

建议最小 Python API 为 `observe_run(argv, env, output_prefix, absolute_deadline_ns, role) -> receipt`；总预算 deadline 在整个 diagnostic job 开始时建立，child deadline 为启动前 monotonic 时间 + 20 秒且不超过总 deadline。所有输出以独占创建方式建立，不覆盖或追加旧 run。

### 事件

- 每 child 最多 64 个固定尺寸 POD 事件，事件必须包含 version、连续 seq、stage、enter/leave、child monotonic timestamp；不在事件路径分配、构造 JSON、格式化字符串或输出对象内容。
- 最小可行通路是 parent 预先独占打开普通事件文件并将 FD 显式继承给 child。每次事件使用无用户态缓冲的 `write`，不能等 child 正常退出才批量写出；单事件不超过 256 字节，总事件体不超过 16 KiB。stdout/stderr 使用各自的普通文件，不使用无人排空的 PIPE。
- 若已实现为 PIPE，必须由 deadline 循环同步排空，接收端即时追加到外部原始文件，且 child 的写入不能无限阻塞。不能先 `wait4` 再读 pipe；必须检查完整、部分、溢出和 EOF。外部文件保留原 bytes，不把损坏尾部静默截断后写成完整日志。
- `write` 可能部分成功或失败。部分记录、seq 缺口、超过 64、未知 stage 或事件写错误都使 `observer_complete=false`；它们不自动证明库故障。正常完整运行必须包含预登记的终结事件。中止运行保留最后一条完整事件及原始损坏尾部。
- kill child 后已成功写入内核的文件 bytes 应仍可读取；这只承诺 child kill 现场保留，不承诺掉电/内核崩溃持久性，不要求每个事件 fsync。
- 事件时钟与 parent monotonic 时钟只在同一 WSL/Linux 时钟域内关联；不得拿 Windows 时间戳直接做阶段相减。事件 syscall/计数成本被明确标为诊断污染，不进入正式性能结论。

事件至少覆盖：开始、输入准备完成、t0 和后续八阶段边界、后置真值审计开始/完成、各输出文件写入开始/完成、成功终结。emit 内部只增加必要的开始/完成事件，细分计数保留无分配 POD；不得改变原业务调用顺序、提前 oracle 或预热输出。

### 唯一 reaper 与清理

- 选择 `os.wait4(pid, WNOHANG)` 为唯一 reaper；只有该循环能回收 child。禁止同时调用 `Popen.wait/poll/communicate`、上下文退出的隐式 wait，或后台线程 wait。成功 wait4 后立刻由 wait status 设置 Popen 的 returncode，防止后续析构路径再次回收；`ECHILD` 不是成功回退，必须保存为 supervisor error。
- `wait4` 给出该 child 的终结 rusage。记录 user/system CPU、minor/major faults、max RSS；不得用 coordinator 的 `RUSAGE_CHILDREN` 差分代替精确 child，也不得将这组数据命名为八阶段资源。max RSS 是 Linux KiB 高水位，不能把两个高水位相减解释成阶段分配量。
- child 用独立、已记录的 session/process group；deadline 到达或 observer 出错时只向已证明属于本 run 的 group 发信号。kill 请求、ESRCH、实际 wait status 分别记录，不把“调用 kill 成功”当成已回收。
- 使用绝对 monotonic deadline，不因事件到达、EINTR、空轮询或 EOF 重置。读取与收据封存必须有固定清理截止，不能在末尾无限 blocking wait 或等继承 FD 的后代关闭。
- `Popen` 创建本身不被普通 wait timeout 覆盖。启动前即计时；返回时若已过 child deadline，不能记为正常成功。不要在 preexec 增加阻塞操作；外层 600 秒 watchdog 仍必须覆盖创建/收据/清理路径。若不可回收或处于不可中断状态，收据必须记录 cleanup 未完成并禁止启动下一 child，不能假称硬 cap 和空残留成立。
- 最后确认 owned child/group/已知后代无 live/zombie；正常 parent 退出而后代仍活着同样属于 supervisor failure。保持原共享租约和外层 runner 的最终清理证据。

## 3. 收据最小字段

可以沿用现有身份字段，不必引入新的通用框架；至少增加如下可审计字段组：

| 字段组 | 必需信息 |
|---|---|
| 身份 | schema/kind=`diagnostic`、role、原 run argv/env、source/driver/binary/build/tool observer SHA、原共享 queue run_id；明确 instrumented=true、eligible_for_performance=false |
| 生命周期 | started/deadline/launch-return/reaped/finished monotonic ns、PID/PGID、launch_error、wait_status、returncode、wait4_calls/reap_count、reaper_error |
| 限制与终止 | child/outer cap、timed_out、deadline_exceeded、kill_requested及结果、primary failure、全部 secondary errors、cleanup_deadline 与实际残留 |
| 全进程资源 | scope=`wait4-child-lifetime`、utime/stime 秒及来源精度、minflt/majflt、maxrss与 KiB 单位；失败未回收时写 unavailable，不能补零 |
| 观测 | 原 event bytes 路径/长度/SHA、event_count、完整/部分记录数、last_complete_event、observer_complete/error、实际使用的 file/pipe 通路 |
| 产物与结论 | stdout/stderr 原 bytes 身份、native/wire/canonical/query 的 exists/validated/identity、semantic validation 状态、diagnostic_only、retry=0 |

成功需要 exit=0、未超时、观测完整、原完整业务真值通过、source/tool前后一致及清理完整。非零退出、signal、timeout、真值失败、observer失败分别记录，不能将普通失败自动写为“性能退化”；任一都不得生成比值、置信区间或晋升标记。仅最后到达阶段并不等于该阶段导致卡顿；结论最多定位在已记录边界之间。

## 4. 必要的轻量失效检查

使用微型 Python 子进程和临时测试目录，不运行项目 native；这些工具自测与仅两次 C++ 原 run 的预算分别记录。每类只做一次固定用例，短超时即可，不扩成压力测试框架。

| 用例 | 必须核对的结果 |
|---|---|
| 1. 正常 0 退出 | 完整事件顺序、终结事件、rusage存在，reap_count=1；测试成功也只标 tool self-test |
| 2. 明确非零退出/异常信号 | 保留退出码或信号和已写事件；不是 timeout，不记成功或性能退化 |
| 3. 写事件后挂起 | deadline杀组并单次wait4，原前缀立即可读，last_complete_event正确，无native结果也有外部收据 |
| 4. 先写半条再挂起 | 保留完整前缀和半条尾部，observer incomplete；不靠EOF补出记录 |
| 5. stdout/stderr超出典型pipe容量 | 直接文件无堵塞且文件上限生效；若PIPE则证明持续排空和固定缓存界限 |
| 6. 无终结/seq缺口/第65事件 | 按协议失败且不越界；不是删除坏事件后宣告成功 |
| 7. parent正常退出、后代持有FD继续活 | 不无限等待EOF； owned group 清理完整且记录后代，reaper不抢wait |
| 8. deadline接近时退出与反复唤醒 | 不因事件、EINTR/轮询重置deadline；超过观察deadline的exit0仍超限，退出/kill竞争不触发双wait |
| 9. 启动失败与注入ECHILD | 启动失败收据存在且资源unavailable；ECHILD明示supervisor error，不以poll或第二wait补救 |
| 10. 日志写失败/监督器异常 | 注入write错误或collector异常，保留已有bytes、执行finally清理；不能因写收据异常遗留child或继续第二run |

磁盘故障无需真的填满卷；用可控错误注入或子进程文件限制完成。deadline创建阻塞和600秒外watchdog可以做单元级固定模拟，不应故意挂住宿主。运行前确认原始测试工具不会调用项目二进制。

## 5. 防止诊断污染正式性能

- 两个诊断角色使用相同 hook 和 profile，所有生成源码、编译receipt、强制include/宏、二进制与正式bare构建独立路径及SHA绑定；不得仅因为候选生产14文件相同就复用诊断二进制做72对。
- 正式screen在自己的编译命令和依赖闭包中证明没有observer include、计数宏或event runtime；检查的是实际生效编译产物，不只CMake选项文本。
- 仅收集此次两run的机制与成本账本；不将诊断原八阶段数值、全进程差或静态调用点计算成R1加速。M1失败窗口仍关闭；此次未复现不能回填历史超时原因。

参考：Python [wait4](https://docs.python.org/3/library/os.html#os.wait4) 提供单child的状态和资源；[subprocess wait](https://docs.python.org/3/library/subprocess.html#subprocess.Popen.wait) 说明无人排空PIPE可能死锁、创建不一定可被timeout中断；Linux [getrusage](https://man7.org/linux/man-pages/man2/getrusage.2.html) 定义资源范围和RSS单位；[write](https://man7.org/linux/man-pages/man2/write.2.html) 明确部分写与持久性边界。本文件未验证v2具体工具实现，实施后只需对稳定revision做一次对应窄复审。
