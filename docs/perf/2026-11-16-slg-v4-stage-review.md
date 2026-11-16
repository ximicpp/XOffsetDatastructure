# SLG 4.0 延迟优化阶段验收

性能目标已暂停。优化分支是 `codex/xoffset-v4-latency-20261115`，起点为
`ee7a2c5f`。正式性能结果测自 `8d3cba8c6de448083bccb761b3cb27ee69519064`。
阶段收尾增加 Windows 脏模块存储回归，运行路径沿用被测提交。

## 性能验收

M 场景使用同机固定 zpp_bits 可执行文件，表中为 XOffset 除以 zpp_bits 的耗时比及配对
95% 置信区间。每项达标要求区间上限不超过 `1.00`。

| 原生平台 | `UL.send` | `UP.recv+apply` |
| --- | ---: | ---: |
| Windows MSVC | 0.9623 [0.9481, 0.9768] | 0.9131 [0.9068, 0.9193] |
| Mac mini M4 Pro | 1.0125 [1.0055, 1.0195] | 0.9771 [0.9728, 0.9814] |

两台机器均通过 S/M 常规和 D3 差分。被测提交在 Windows 通过 5 项原生 SLG 回归，
在 Mac 通过 4 项。Linux 原生存储专项通过 3 项，包括无异常版本。256 MiB 地址空间限制下，
有界上行回退的预留约为 1 MiB，旧预留策略在同一限制下失败。Linux 证据的覆盖范围为
存储和预留边界。

## 代码审查与候选决策

- 上行写入复用按长度预备的 V2 帧。现有差分测试用独立紧凑编码逐字节比较 2,048 笔
  指令，覆盖空部队、固定槽边界、大部队、种子回绕和两种接收校验。macOS 的值传递入口
  与保留 RNG 状态的入口由同一生成逻辑实现。
- macOS 同 arena 移动仅用于把尾部单位搬到待删除单位的位置，随后更新索引并移除尾部。
  M 场景 `US.apply` 相对该轮 XOffset 对照缩短 4.9%，登录 `L.recv` 增加约 1.3%。
  Windows 的 M 场景 `UL.recv` 增加约 2.7%，完整校验 `ULF.recv` 缩短约 15.1%。
  各阶段和 A/A 对照见原始审计。Mac `UL.send` 仍未达标，接收阶段的耗时回退仍需验证和处理。
- Windows 脏模块候选用已有原生部队复制取代逐字段复制。12 组复测中，M 场景
  `SD.save_encode` 缩短 24.1%，`SD.save_encode + SD.store_recv` 缩短 22.9%。同一候选的
  `UL.send` 对 zpp_bits 区间上限为 1.0082。暂停时，后续隔离复测收齐 M 的 5 组，S 的
  计时尚未开始。候选留在实验目录。本次提交把已有存储路径的差分检查纳入 Windows 回归，
  覆盖空模块、复用接收、字段摘要与独立完整校验。
- Mac 登录接收返回已绑定根对象，发送函数保留独立调用。暂停时收齐 M 的 13 组，候选
  `UL.send` 对 zpp_bits 为 0.9978 [0.9905, 1.0051]。S/M 常规和 D3 差分通过，S 场景
  计时尚未开始。单独调整校验工作区、接收帧长算式和发送缓存布局的初筛未确认收益。
  候选留在实验目录。

## 复跑和证据

每个正式计时批次独立启动四个进程，用同一个 XOffset A 二进制跑两次，另跑候选 B 和固定
zpp_bits。轮换顺序。每平台分别完成 S、M 各 16 批，固定种子 `20261021`，每进程 21 个周期，
丢弃第 0 周期。按周期计算耗时比，取对数并在批内平均，再用独立配对批次计算双侧
Student t 区间。正式计时设 `SLG_REUSE=1 SLG_CASES=demo SLG_NATIVE_OFF=1`。差分执行
`--mode verify --scale S|M --cycles 3 --seed 20261021`。D3 差分设 `SLG_D3=1`，清除
`SLG_NATIVE_OFF`，并在格式列表加入 `zpp_bits_dirty`。

原生回归的构建入口和命令见 [SLG README](../../bench/slg/README.md)。阶段收尾在装有
锁定依赖的独立 Windows 源码目录重新构建，5 项测试通过。实验归档中的 `environment.json`、
`commands.jsonl` 和逐进程 JSONL 记录二进制哈希、参数和耗时。实验目录
`xoffset-v4-parity-20261115` 中的 `verify_extreme_windows_release_prepared_overlay_16blocks.py`、
`verify_extreme_mac_release_16blocks.py`、`campaign.py` 和 `analyze.py` 可复跑采集和分析。

| 实验目录中的文件 | 内容 |
| --- | --- |
| `latency-8d3cba8c-exact-cross-platform-audit.json` | 双平台正式结果与证据哈希 |
| `windows-extreme-release-8d3cba8c-evidence.zip` | Windows 正式原始计时和检查日志 |
| `mac-extreme-release-8d3cba8c.tar.gz` | Mac 正式原始计时和检查日志 |
| `linux-fe17f084-storage-native-evidence.tar.gz` | Linux 地址空间、存储与回归证据 |
| `latency-8d3cba8c-stage-close-index.json` | 候选实验和中断试跑的数据索引 |
| `windows-stage-close-8d3cba8c-tests/ctest.log` | 阶段收尾的 Windows 原生回归输出 |
