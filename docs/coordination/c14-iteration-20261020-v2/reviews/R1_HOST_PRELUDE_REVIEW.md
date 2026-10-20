# R1 宿主 prelude 接入窄审

日期：2026-10-20（Asia/Shanghai）。审查者：`runtime_observer_review`。

**结论：相对已验v1的宿主安全接入差分通过，未发现新实质阻断。** 只读核对HOST请求、实际只读观察、预审plan、watchdog推导及新controller关键路径；未pause、未start guard、未执行新测试。此记录不替代root对原human授权/最新事实的核实，也不批准尚未绑定实际freeze的暂停或计时。

## 工具与静态复用

| 文件 | bytes | SHA256 |
|---|---:|---|
| `screen/r1/execute.py` | 10295 | `a19950ed50850ebbcfa1ce8d1da00ac570fc89a1bdbef8f54f8896dea3630870` |
| `screen/r1/host/process_pause.py` | 16269 | `ce898a67023bdc0ad69057f3cc9a2558b9090e1818689f9bef99983ffc6f48ec` |
| `screen/r1/host/pause-control.ps1` | 12668 | `cb976637024caacead844dfd5e40277304f9a7bd3ef750fe06f9cd0078b60f44` |

工具路径前缀为 `tools/c14_iteration_20261020_v2/validation/`。新PowerShell helper与已验v1逐字节相同；新Python pause模块与v1静态差分只有独立STATE输出路径一行。已有held-handle/SID/FILETIME、异常恢复和独立watchdog安全证明可按此精确差分复用，不要求重做模拟暂停测试。

## 授权范围与当前观察

- HOST请求仅列 `client_ue4-Win64-Shipping`、`TGitCache`。standing记录固定为1532B/`b97fa587...`，指向原775B/`504243b5...`记录；二者保留human回答“允许临时暂停并恢复”，禁止kill/restart及暂停无关Git或其他程序。
- standing说明原human问题针对“测量期间临时暂停两程序并恢复”，原记录cursor-only性能标签属于root添加；没有声称收到新human回复。本窄审不把其他agent消息变成授权来源；root仍以原human记录核实持续授权。新候选不扩大目标程序、动作或恢复责任。
- `HOST_OBSERVATION.json`为只读观察，UTC `2026-10-19T18:09:20.700777+00:00`，实际原租约run_id `8e572da7adc0458bab29ab786ed6df15`。其中paused=0、guard_started=false、power_changed=false，不代表现在已有暂停或计时许可。
- 预审plan记录TGitCache PID49116，image为`C:\Program Files\TortoiseGit\bin\TGitCache.exe`，creation FILETIME `134359155653323608`；游戏PID133148，image为`D:\WhLauncher\games\wh\client_ue4\Binaries\Win64\client_ue4-Win64-Shipping.exe`，FILETIME `134368861706454844`。两者owner SID与controller相同，完整SID及start_time保留在原plan。
- 以上PID只是该只读观察时刻的证据。真正prelude重新调用`prepare_plan`生成新的`process-plan-v1.json`，不采用预审plan/旧PID/state。helper先为全部fresh目标打开durable handles，再逐字段核PID、name、image、owner SID、FILETIME及start_time，全部通过后才开始暂停。已有STATE或attempt文件会拒绝，不能借旧token/handle继续。

## 不改power与恢复边界

- controller的power操作只有`powercfg.exe /getactivescheme`，核当前GUID `381b4222-f694-41f0-9685-ff5bb260df2e`。没有切换电源策略、kill/restart、改优先级或暂停其他程序的路径。helper启动使用隐藏PowerShell窗口。
- 独立Windows helper持有真正handles，最大780秒。推导为剩余prelude60 + handoff30 + timing600 + 普通清理/恢复余量90 =780；它是有界恢复保护，不承诺不可中断WSL任务必在90秒内消失。
- helper成功/异常均在finally按持有handle执行各一次owned resume，然后关闭handles；watchdog deadline也触发恢复，但其结果不能获得正常计时覆盖资格。原进程外部退出、恢复错误或状态未知不能假称“恢复且仍live”。
- execute的finally先`P.stop()`恢复并验证guard/token，再记录前后power与RESTORED。恢复失败先保留证据，不启动audit。成功timing还调用原`verify_coverage`：ready-observed≤实际timing queue≤stop-request、正常requested_stop、逐目标owned resume成功且保持live，不靠Windows/WSL时钟偏移假设。
- `screen.py`的成功audit和failure audit都必须先读取同freeze的`RESTORED.status=passed/user_processes_restored=true`；离线audit使用另一个原共享validation租约。600秒计时窗口不包含暂停期间的独立3600秒audit。

## 最终接入条件

当前HOST_ADMISSION_REQUEST的freeze_sha256仍为null，quiet尚未建立，screen_admitted=false。因此此刻只完成源码/范围/已有只读事实复核。root应把最终同一freeze、当前all-chat/agent quiet、真实授权来源和fresh prelude/guard证据接入既有准入流程。无需新增工具层、新测试或扩大授权；实际暂停、timing和恢复必须以随后真实回执验收。
