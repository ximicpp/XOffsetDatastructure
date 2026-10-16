# XBuffer 本轮研究最终交付

2026-10-16（上海时间）。本轮有界研究结果已验收，保留 F。本文进入 cut 时，最终封包与独立审计尚未执行；实际完成结果由文末的胶囊、回执与外置 seal 绑定。G4 父最终验收以统筹记录为准。

本轮研究交付保留 F。W1 网络功能参考、W2 基础及有界网络诊断、owner 26 项功能/诊断、
网络 112 进程校准及 88 进程池比较和相应便携档案均已有范围验收。默认 F/F 和网络池
比较虽执行成功，但噪声资格不合格；G3 三方向均以前提未满足保留 F，决议交付已完成。
owner 78 个进程及 D031/D032 离线补证已由 D033 接受：24 个 cell 中 7 个合格，
24 个比较中 2 个合格（含一个自比），所有多线程比较均不确定。原失败与原件/派生差异保留。

统筹的 [D015 补证验收](coordination/vm-followup/G4-supplement-acceptance.json) 已接受
历史配置的补充来源证明，当前无条件新增基础 C++ 验证任务为 **0**。D016 新增的
owner 争用研究另列，不计作基础兼容矩阵重跑。没有因“最终交付”
名称重跑基础矩阵。统筹现已授权本轮最终冻结与独立审查，没有 Docker 实现或容器任务授权。
实时 gate 以
[统筹派发记录](coordination/vm-followup/dispatch.json) 为准；本文的已验收部分与待项分开记录。

## 基线与已有交付

工作分支为 `next_cpp26`。F 包含未提交修改，Git 基点
`07d649909d7f43ca0d11b85accb6d9dab991e284` 不能代表 F 或最终交付源码。生产头的
固定身份为：

| 文件 | SHA-256 |
| --- | --- |
| [xoffsetdatastructure.hpp](../xoffsetdatastructure.hpp) | `9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2` |
| [xoffset_virtual_memory.hpp](../xoffset_virtual_memory.hpp) | `22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad` |

F 保持固定最大容量内的基址稳定、默认 Explicit 增长及可选 Automatic、显式回收与
池预算、现有指针/异常和 V1 wire 契约。arena 基址稳定不代表所有容器元素始终不移动。
后续研究没有采用新的 allocator 或 I/O 生产 API。

此前阶段已交付以下能力，原始结论和限制仍按各自档案解释：

- [N0/N1 本地验证基础](VM_STAGE1_RESULTS.md)：固定工具链与依赖、独立 C++17 storage
  入口、真实反射能力和编译拒绝检查、精确测试清单及可移植验证工具。工作流配置已调整，
  没有据此声称远端 CI 已运行。
- [N2 消费端打包](VM_STAGE2_RESULTS.md)：`XOffset::storage` / `XOffset::xoffset`、
  可重定位安装、源码子项目、多翻译单元消费及固定依赖/许可证；使用方式见
  [消费指南](VM_PACKAGING.md)。动态插件、跨 CRT 或不同头版本混用不在这些保证内。
- [N4/N5 研究](VM_STAGE2_RESULTS.md)：完成池/回收负载与 Automatic 轨迹研究，保留 F；
  Automatic 窄缓存转移候选因明确反例拒绝。历史合成测量不改称业务收益或安静环境确认。
- [N6 研究](VM_STAGE3_RESULTS.md)：六个 allocator 候选均拒绝，未启动不满足前提的
  selected 组合或 D-batch。它们是已完成的历史研究，不能与本轮 G3 I/O 前提决议混同。

## 已接受的验证配置与继承范围

下表记录**既有运行及其已接受的继承范围**，不是一轮新 G4 通过清单。稳定输入已通过
默认 validation 队列暂存；最终 selected source 清单另加入 D032 已接受的 owner 工具，
封存时将复核完整相关输入及精确 case 集，完成情况以外置回执为准。
双头哈希相同不足以自动覆盖后加源码、工具或新消费者。

| 范围 | 已有证据 | 可保留的结论与边界 |
| --- | --- | --- |
| macOS arm64 full | Stage III 48/48 CTest；配置检查 3 项另列 | 38 程序 + 7 子进程 + 3 tooling；签名 3 项已包含其中。相关闭包不变的继承基础已接受 |
| macOS standalone storage | 2/2 | 实际 Apple Clang 21、C++17 配置；与 full 中的 P2996 storage 子目标分别记录 |
| Linux amd64 普通 full | 历史 46/46 | ARM64 宿主上的仿真功能证据；不声称当前普通 Linux 48 已运行。新增 allocator 两项仅在统筹明确需要该主张时作为条件增量 |
| Linux amd64 sanitizer | 历史 26 项与 Stage III 新增 2 项分别通过 | ASan/UBSan/LSan 的逐 case 并集为 28；来自两次运行及不同 CMake 版本，不是一轮新 28 项通过；不提供原生性能结论 |
| macOS TSan | 历史 15/15 | 实际在 macOS 26.6.2 arm64 运行，编译记录含 `-mmacosx-version-min=10.15`；不证明 macOS 10.15 运行、Mach-O 有效最低版本或当前 unset deployment 配置通过 |
| wire 互通 | 84 个历史 frame 检查 + 420 个新操作，共 504 | 保留历史直接编译闭包、输入与命令链；84 个新生成输出未保留完整字节，只保留其哈希，不补造输出或二进制 seal |
| macOS full package | installed 3 + subproject 3 | P2996 full 配置；安装/导出/consumer/依赖闭包不变，继承原记录范围 |
| macOS storage package | installed 3 + subproject 3 | Apple Clang 21、C++17；不存在的 full component 预期拒绝另列，不算 CTest |
| Linux full package | installed 3 + subproject 3 | amd64/P2996 Clang 21、CMake 4.2.3，ARM64 宿主仿真 |
| Linux storage package | installed 3 + subproject 3 | 本地 VM/container 内 ARM64、GNU 13.3、CMake 3.28.3；不是 amd64 仿真或独立物理设备性能 |

原始运行入口见 [Stage I 验证索引](validation/2026-10-15-vm-stage1/README.md)、
[Stage II 验证索引](validation/2026-10-15-vm-stage2/README.md) 和
[Stage III 验证索引](validation/2026-10-15-vm-stage3/README.md)。原生 Linux ARM64
GNU storage 的既有 2 项功能检查单列于 Stage I；它不能替代完整 P2996 库或独立设备性能。

继承依据为 [初次验收](coordination/vm-followup/G4-inheritance-acceptance.json) 与
[最终补证验收](coordination/vm-followup/G4-supplement-acceptance.json)。后者补足 Linux
19 个旧 sanitizer 目标的有效参数沿革和 package 外层 profile，明确保留 TSan 的真实
deployment 选项差异。已接受的不可变说明见
[身份说明快照](coordination/vm-followup/platform-supplement-reviewed-v1/identity-notes.md)。

历史 source manifest、归档工具、实际命令、JUnit 与日志构成保留的来源链。未保存的
原始被测二进制、编译器、Python、完整 SDK/runtime/sysroot 字节 seal，以及完整外层
Docker 请求/响应和每容器 daemon 身份，不能用事后观察补成当时证据。保留这些限制
不等于把全部历史结果作废，也不自动产生全量重跑要求。候选矩阵和条件最小增量见
[G4 准备清单](../experiments/vm_platform_validation/g4-preparation/G4_MATRIX.md)。

## 本轮已经验收的功能与诊断基础

W1 的 [G1 验收](coordination/vm-followup/W1-G1-acceptance.json) 覆盖冻结修订 2 的
原生 macOS 同步 TCP 回环参考及独立生命周期模型。timing/diagnostic 两构建分别通过
34 组、182 子例，保留逐值语义、V1 字节、失败/截止/清理证据；还有 16 条 CLI 检查、
12 份处理后输出字节相等，以及每个 Python 模式 29 项工具检查。源码和二进制身份及
311 个常规归档成员见 [最终证据索引](validation/vm-network/final-v2/README.md)。

W1 的 timing 构建用于隔离诊断开销，名称本身不表示已经测得性能。当前交付不涵盖
生产异步 transport、TLS/远端网络或代表性业务 consumer；逻辑 admission credits
不等于物理内存或 RSS。P2996 要求 ASan v8，而本机 Apple Clang 21 runtime 提供
Apple 专用 ABI，实际链接失败证据已保留；sanitizer runtime 测试未运行，不绕过版本
检查，也不重复相同失败组合。详情见 [实现报告](VM_NETWORK_IMPLEMENTATION.md)。

W2 的 [G1 验收](coordination/vm-followup/W2-G1-acceptance.json) 覆盖独立固定 F
诊断基础及可预测语义/失败控制：10 个固定 case、原始/诊断各一次，共 20 个进程；
9 个普通有效 trace、480 个事件，另有必须被拒绝的 overflow 负控。最终实现冻结面为
137 文件，57 项 Python 检查及 3 个 runtime 控制有证据。

在该基础批次的 63 类事件目录中，29 类有计数、账本或部分快照断言，11 类仅结构覆盖，
1 类仅无效负控，22 类未执行。目录完整不等于所有运行路径已验证。共享 domain 的因果归属
不等于独占 owner 成本，观测峰值不等于物理同时峰值，不同含义的复制范围不能累加为
进程总流量；锁探针也会扰动计时。详情及原始记录见 [诊断结果](VM_OBSERVABILITY_RESULTS.md)。

W2 的 [D019 有界网络诊断验收](coordination/vm-followup/W2-network-acceptance.json)
已接受 4 个 fixture、12 个网络条件的原始/诊断对照，共 32 个 workload 进程、16 个
有效 trace，dropped events 为 0，线程事件数最多 129，低于硬上限 2048。36 项 Python
检查、11 个产物篡改负控及 1 个搬迁正控有记录；取证量 380,768,236 字节，未超过
512 MiB 预算。

网络证据覆盖 phase、资源、owner generation、复制及 warm 生命周期断言。none/cold
各记录 5 次注册、reserve 和 unmap；warm 含完整预热/预检时为 7 次注册、3 次 reserve
和 unmap。五类原始指标的证据/限制映射已接受，但诊断时钟不提供正式耗时收益，局部
元数据请求量/观察序峰值不等于 RSS 或进程总复制，网络 peak-owner 为 1 也不覆盖争用。
本批执行 23 类事件；与基础批次合并后为 42 类普通执行、20 类未执行、1 类仅无效负控，
不表示 42 类均有完整断言或全部 63 类路径已验证。

[network/final-v1](validation/vm-observability/network/final-v1/result.json) 的接受范围为
20 文件小包及 1,044 个外部原始证据引用。小包本身不是完整可移动归档；搬迁正控仅覆盖
冻结输入和一个 4 KiB warm case，不能扩展为整组搬迁通过。完整 N3、候选采用及原生
平台/sanitizer/业务前提没有因此关闭。

统筹 [D016 范围决定](coordination/vm-followup/dispatch.json) 另明确了 N3 的缺口：
历史多 owner 矩阵的总工作量随 owner 数增长，尚不能隔离当前 F 的 registry 争用。
W1 的 [D027 owner 功能/build 验收](coordination/vm-followup/W1-owner-functional-acceptance.json)
已接受精确 26 个 workload 进程：18 个成功配置、8 个指定失败控制。六个诊断 R256 与
六个原始 F R256 核对等工作语义，另六个 F R8192 检查正式工作量的功能；这些不是
正式测量样本。失败控制在两个 profile 中分别覆盖启动、创建、分配和阶段间核验，
预期退出 2，原因/进度/守恒/清理符合才算通过，不列为实际失败。研究固定请求量，
分别控制 live-owner 规模、线程数和实际工作。
[D022 布局勘误](coordination/vm-followup/contracts/W1-owner-scaling-v1/layout-correction.json)
要求修正 auditor 的 Apple arm64 布局假设、保留并离线重审首个已完成记录，只执行剩余
25 项，不重建 C++ 或重试首项。原 group00 checker 拒绝及 group01 零 native 的 preflight
失败均保留，补充审计接受原有效记录；它们不是生产/OS 失败。
[D029 owner 便携归档](coordination/vm-followup/W1-owner-archive-acceptance.json) 已接受
462 个唯一普通成员及搬迁审计，压缩 3,151,435 字节；没有新增 native 运行。

[D023 owner 测量设计](coordination/vm-followup/contracts/owner-measurement-v1/design.json)
冻结描述性 fixed-request scaling 协议，W2 准备工具、W3 唯一正式执行。
[D030 精确 proposal](coordination/vm-followup/contracts/owner-measurement-v1/proposal-acceptance.json)
已接受 D027 producer 绑定及 41 项普通/41 项优化 Python 工具测试，并单独批准 78 进程
窗口（6 prerun、72 measured、24 cells）。该窗口已经执行，与下节 default F/F 的
78 进程是不同工作，也不是 2% 回退或优化采用研究。

[D031 工具缺陷补证](coordination/vm-followup/contracts/owner-measurement-v1/approval-copy-supplement.json)
记录冻结 collector 重新序列化 approval 副本，触发离线哈希拒绝。原 proposal、执行工具、
窗口、78 个进程的原始记录（6 prerun、72 measured）和失败档案均保留；W2 修正未来 live 工具以保存原字节并补回归，W3 用
明确原件/派生审计视图作最小离线补证。没有 native 重跑或重编，也不能直接忽略哈希。
[D032 工具与离线执行验收](coordination/vm-followup/contracts/owner-measurement-v1/supplement-tool-acceptance.json)
接受保存原 approval 字节的 live collector、最小补证器及普通/优化 Python 模式各 51 项
检查（保留原 41 项，新增 10 项回归）。原 D030 collector、auditor、测试及执行入口字节
单独保存；修正后的 live 工具不会替代原测量工具的身份。

[D033 owner 研究验收](coordination/vm-followup/owner-measurement-acceptance.json) 接受原 78
个进程（6 prerun、72 measured）的完整执行/协议审计及显式部分资格。24 个 cell 中 7 个
合格、17 个不确定；24 个比较中 2 个合格、22 个不确定，两个合格项为 A/free 对 C/free
及 C/free 自比。唯一非自比合格项的六 block 比值中位数为 0.574711100，观察范围为
0.562113826–0.584546044；这是本窗固定请求的 phase 观察，owner 数同时改变每 owner
工作量与元数据，不能单独归因于 registry。所有多线程比较均不确定，不提供线程扩展、
锁/哈希因果、lookup latency、完整应用生命周期或生产尾延迟结论。原生 producer 的
`timing_evidence=false` 保持不变；也不产生默认 D/F、2% 回退、优化或采用结论。

[完整离线补证](perf/vm-owner-scaling/measurement-approval-supplement-v1/result.json)
接受 698 个 payload 文件、8,891,188 字节（result/COMPLETE 另计），完整搬迁审计通过。
`original-window` 保留原执行字节；`derived-window` 的唯一差异是 688 字节的 approval.json
恢复为外部原批准的精确字节，不能称派生副本与原窗口逐字节相同。原窗口、summary、
冻结工具/proposal、原失败档案和两份完整回执均保留。完整原始 inventory 首次在 D031
补证中产生，不冒充 D030 事前 pin。已完成 validation 回执闭合封包捕获的 running
metadata；没有新增 native、构建、样本或测量窗口。详细资格与限制见
[owner 结果报告](perf/vm-owner-scaling/RESULTS.md)。

## 默认路径测量与网络研究的现状

统筹已接受一次预登记的原生 macOS F/F 校准为
[`accepted_executed_noise_unqualified`](coordination/vm-followup/default-aa-1-acceptance.json)。
78 个进程（6 预热、72 测量）全部成功，保留完整 wire、原始 stdout/回执并通过冻结
离线审计。上海时间 2026-10-16 00:02:21.701–00:03:02.853 为本次队列窗口；重复
save 的两个指标来自同批进程，不能多算为独立窗口。

| 指标 | 预登记噪声资格 |
| --- | --- |
| `save_first_64m / first_ms` | 不合格 |
| `save_repeated_64m / first_ms` | 不合格 |
| `save_repeated_64m / repeated_mean_ms` | 不合格 |
| `vector_tail_1m / elapsed_ms` | 不合格 |

四项指标及各 anchor 都未满足偏移/顺序分层/离散和跨锚漂移要求。本窗口不建立 2%
检出能力，不证明 F 回退、D/F 等价或此设备永久不可测。按验收决定不重试该校准，
不启动 D/F 比较或归因消融。N3 的默认路径稳定复现与归因继续开放。完整方法、排除项、
原始数据和摘要见 [校准报告](perf/vm-default-attribution/RESULTS.md)。

网络 [D021 校准验收](coordination/vm-followup/network-aa-1-acceptance.json) 已接受
`network-aa-1` 的实际原生执行：112 个进程（16 预热、96 校准）全部退出 0，无超时、
stderr 为空，清理后没有存活进程。队列窗口为上海时间 2026-10-16
00:54:18.311–00:54:39.672；原始记录包含 1,932 个命令文件和 1,484 份 wire，合计
620,755,072 字节。它是已执行的 F/F 校准，不是池策略或候选收益比较。

| 名义尺寸 | 局部 A/A | 跨锚漂移 | 完整稳定窗口资格 |
| --- | --- | --- | --- |
| 4 KiB | 不合格（pre anchor） | 不合格 | 不合格 |
| 64 KiB | 合格 | 不合格 | 不合格 |
| 1 MiB | 合格 | 合格 | 合格 |
| 16 MiB | 不合格（post anchor） | 合格 | 不合格 |

12 个局部 anchor 中 10 个合格，不能据此把四个尺寸都记为稳定；整体 false 也不能写成
未执行或全部尺寸失败。统筹只允许准备新的 1 MiB 池筛选，不重试其余三个失败尺寸。
新的 88 进程 1 MiB none/cold、none/warm 窗口已有
[D026 验收](coordination/vm-followup/network-pool-1m-1-acceptance.json)：全部进程退出 0，
无超时、stderr 为空、清理为空。每进程 4 次迭代，每 contrast 8 对；两个 local A/A 都
合格，但 left/right/pooled 的完整跨锚漂移门槛都不合格：最差单侧漂移分别为 6.5467%
与 2.4762%，阈值 1%。这是一轮已完成的研究及噪声限制交付，不提供优越、等价或候选
实验拒绝结论；不重跑、扩尺寸、实施或采用。

[D029 网络88归档验收](coordination/vm-followup/network-pool-1m-1-archive-acceptance.json)
接受 865 个 window 文件、494 个内部 hardlink group、与原执行目录 0 共享 inode 及复制
后冻结审计。父复核只读元数据/关键文件，没有冒称重读 369,222,656 字节 wire 或重复
88 进程统计。档案见 [pool-screening-1m-v1](perf/vm-network/pool-screening-1m-v1/EVIDENCE.json)。

D021 也已接受 [calibration-v1 可移动归档](perf/vm-network/calibration-v1/EVIDENCE.json)：
复制后 audit 通过，2,096 个复制文件、1,656 个归档内部 hardlink，与原执行树共享 inode
为 0，关键 manifest/result/summary/approval/schedule 字节一致。父审阅核对文件元数据，
没有冒称重复完整 wire/statistics 审计。该验收不扩大测量或采用范围。
默认线的噪声结论也不能替代网络线校准。历史
[文件筛选](VM_IO_SCREENING_RESULTS.md) 的四尺寸 A/A 也全部未达门槛，不能当作网络
收益或新候选的实验拒绝依据。

G3 的复用 owner、直接读新 staging owner、减少输出复制三个 I/O 方向已按
[D028 决议](coordination/vm-followup/G3-acceptance.json) 全部记为
`prerequisite_unmet_retain_F`，本轮决议交付完成。实施、实验性拒绝和采用均为 0。
仍缺完整事务/生命周期/等工作比较及可解释的代表性端到端依据，可移除成本占比保持
null，不能从重叠诊断 phase 或噪声不合格池比较推算。
[候选预算和生命周期要求](VM_IO_CANDIDATE_DECISIONS.md) 保留。
真实消息尺寸分布、协议/库、同步或异步完成及取消契约、业务代表性仍未明确，生产
收益为未评估。当前保留 F，不凭合成参考或文件噪声结果推进生产 API。

## 平台及执行边界

[平台前提验收](coordination/vm-followup/platform-acceptance.json) 仅接受 24 条只读
命令和 38 文件取证，以及既有 Windows 包完整性，不产生新的编译、测试或性能结果。
本机为 macOS 26.6.2 arm64；固定 P2996 Clang 提交
`d77eff1cbd78fd065668acf93b1f5f400d39134d`、libc++/libc++abi 与 TSan runtime 匹配已记录清单。
SDK 26.5 存在，当前默认 SDK 为 27.0；使用历史 profile 时须显式选择 26.5。

| 前提/平台 | 当前真实状态 |
| --- | --- |
| 原生 Windows storage/package | `prerequisite_unavailable`：未建立 x64 MSVC/SDK 执行渠道；storage 2 项、installed/subproject 各 3 项尚未原生运行 |
| Windows 完整 P2996 库 | 独立工具链评估已完成，结果为运行前提未满足；原生完整库执行/支持仍未验证，不能由 storage 包替代 |
| Windows ZIP | 126 成员、121 源文件及 CRC/哈希完整性通过；`prepared-not-windows-tested`，最终使用前仍需比较 storage/package 全部相关输入 |
| 独立原生 Linux 性能 | 未确认可用设备/渠道；现有 `sfcdev` 别名一次有界连接超时，不能推断远端 OS 或工具链能力 |
| 本地 Linux | ARM64 VM/container 功能证据与 amd64 P2996 仿真证据分别保留；不提供独立物理设备性能结论 |
| 真实业务网络 consumer | 参数与代表性前提未明确；同步回环参考不能关闭此项 |

详细平台记录及 Windows 前置工具要求见 [平台报告](VM_PLATFORM_FOLLOWUP.md) 和
[Windows 交接](VM_WINDOWS_HANDOFF.md)。安装工作流和包内字节完整性不能替代操作系统原生执行。

W4 已接受的 [本机 POSIX 入口](../experiments/vm_coordination/HANDOFF.md) 使用默认稳定
锁串行协调参与任务，编译并行上限 2；正式计时由 W3 管理。17 项真实进程检查及
4 项 daemon 拒绝控制已有验收证据。其保证限于同 session 后代，不能隐藏 Docker、
远端或脱离 session 的任务，也不能据此保证没有其他后台活动。
[Docker 提案](../experiments/vm_coordination/docker-adapter-design/PROPOSAL.md) 仅为已接受
的受限设计储备：没有实现、adapter 测试或容器工作，本轮无需实施。

## 最终封存、适用验证与开放前提

逐轴结果与证据分别见 [verification-matrix.json](validation/vm-final/verification-matrix.json)
和 [evidence-index.json](validation/vm-final/evidence-index.json)，实际构建/生成输入身份见
[build-identities.json](validation/vm-final/build-identities.json)。各轴不能相加为测试数量：

- `new_passed`：本轮已验收的功能、预期负例控制、工具与便携审计；不把归档复制计为新 native 运行。
- `inherited_same_identity`：相关闭包不变且 profile 边界明确的既有验证；不是新跑或当前默认配置通过。
- `failed`：指定对象的噪声资格拒绝、真实工具/构建/审计失败历史；分别说明已解决或待补证，不指代全部库正确性失败。
- `prerequisite_unavailable`：原生平台/默认 D/F 归因/兼容 ASan ABI/真实业务与候选机制前提未满足，保持具体缺口。
- `not_applicable_with_reason`：未实施或采用候选的变体验证等没有适用对象；不用于掩盖未满足的外部平台或业务任务。

测量噪声独列：default F/F78 不合格；network F/F112 仅 1 MiB 完整合格；network pool88
两个比较均跨锚漂移不合格。执行/协议审计成功与统计资格分别记录，不能改写为收益、等价
或候选实验拒绝。owner78 的 7/24 cell、2/24 comparison 部分资格按 D033 单列，
包含 C/free 自比；多线程比较仍全部不确定。

| 项目 | 当前状态 | 进入最终交付所需结果 |
| --- | --- | --- |
| G0 | 已接受 | 保留本机入口、角色边界及冻结接口验收 |
| G1 | 有范围接受 | 保留 W1 功能、W2 诊断基础和平台前提的各自限制 |
| G2 | 限定测量/审计交付已接受 | 默认 F/F、网络112及网络88的执行/审计与噪声限制保留；没有 D/F 归因或性能采用结论 |
| W2 网络诊断 | D019 有界范围已接受 | 32 进程的成本/生命周期及五类指标映射保留；小包/外部引用边界明确，不产生正式收益或完整 N3 结论 |
| N3 固定请求量 owner 研究 | D027 功能26、D029归档及D033测量78/补证有范围完成 | 保留7/24 cell、2/24比较（含1自比）资格；多线程全部不确定，执行工具与live修正分开，无重跑/重编 |
| G3 | 本轮决议已接受 | 三方向均 prerequisite_unmet_retain_F，保留具体重开前提；没有候选实施/实验拒绝/采用 |
| G4 | 封包及独立审计执行结果待外置回执确认，父最终验收待统筹 | 一次源/元数据 cut、逐文件封包校验、独立只读审计；不重审或重复计数历史 native |
| 外部前提 | 继续开放 | Windows 原生层、独立 Linux 性能、真实业务 consumer；前提缺失不能记为通过 |

本轮 G4 只处理上述本地研究交付。最初稳定源为 225 文件、2,970,586 字节，
21 个基础相关输入与已接受闭包相同，29 个实验声明输入及 9 个实际生成/原执行输入
已有父稳定源复核。Stage III 的 129 条历史输入比较仅有 README 的 D024 文本澄清、
两个已接受且不在 CTest 闭包中的 allocator 离线工具差异；129 是比较记录数，并非都
进入源码包。未变化的 `tools/sigs/.gitkeep` 是 110 字节注释占位文件，未入选 225 文件。
最终再加入已接受的 owner 工具及封包工具增量，完整成员以
[source-manifest.json](validation/vm-final/final-v1/source-manifest.json) 为准。

封存程序将以 [source.tar.gz](validation/vm-final/final-v1/source.tar.gz) 固定选定的
生产/测试/工具/消费者、生成规则与已接受生成头，以及分开保存的 D030 原执行工具；
[metadata-manifest.json](validation/vm-final/final-v1/metadata-manifest.json) 将记录同一 cut
的 contract、验收、执行入口、角色 handoff、最新计划、构建身份、依赖/许可证和证据引用。
这是源码与依赖引用包，未复制 compiler/SDK/runtime、依赖源码、被测二进制或大型历史/wire
档案，不是完整离线自足构建环境；原始证据仍通过已接受的身份与位置引用。

本轮最终新增验证仅针对封存工具与元数据：默认 validation 队列先核对实际源闭集、
接受身份、相关稳定输入及小 metadata 预算，再生成源归档并逐成员核对字节，生成完整
[manifest.json](validation/vm-final/final-v1/manifest.json)。独立审计只读新胶囊，核对
闭集、普通文件/路径/别名、归档字节、D030/D032 分离、构建与 authority 绑定及结果分类。
它不重读外部大档案、不重算历史或 owner 统计，不增加 native、C++、容器或测量运行。
执行是否完成及检查结果由外置 [final-review.json](validation/vm-final/final-review.json)
和 [final-seal.json](validation/vm-final/final-seal.json) 绑定，避免报告/manifest 自身哈希循环；
不能仅以本段描述代替成功回执。

可靠基础证据按完整相关输入身份及其实际 profile 继承。逐项分类为 `new_passed`、
`inherited_same_identity`、`failed`、`prerequisite_unavailable` 或
`not_applicable_with_reason`；已执行但噪声不合格的校准单独保留。无条件新增 C++ 任务为 0。
G4 的本地交付不会把 Windows 原生执行、独立 Linux 性能、默认 D/F 归因、兼容 ASan ABI
或真实业务 consumer 前提标为完成，重开条件见最新 [后续计划](VM_NEXT_STAGE_PLAN.md)。

[共享文档最小建议](validation/vm-final/shared-document-edits.md) 的 README 三处澄清已按
[D024 精确窗口](coordination/vm-followup/readme-window.json) 应用：反射 API 与 C++17
storage 边界、平台验证范围、N7 同步 TCP 参考及测量入口。原有其他修改保留，迁移说明和
`stable_storage.cpp` 未改；仅检查精确差异、链接和文本身份，没有 C++ 重跑。
[窗口交回记录](validation/vm-final/readme-window-return.json) 保存 README postimage，
[父验收](coordination/vm-followup/readme-window-acceptance.json) 已关闭窗口，没有持续写权限。

[全局任务表](../openspec/changes/vm-next-stage-hardening/tasks.md) 已由统筹区分已验收的
诊断基础与消费者/争用应用、已验收网络功能与分阶段网络测量；Windows full
工具链评估完成与原生执行未完成也分别保留。本文沿用各项最新范围验收，不把准备或前提评估
计为运行通过。

历史草稿来源与当前封存状态见 [draft-status.json](validation/vm-final/draft-status.json)。
最终不可变身份以胶囊 manifest 和外置 seal 为准，G4 父验收以统筹记录为准。
