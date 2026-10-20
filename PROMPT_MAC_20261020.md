# 可直接复制到 Mac 新会话的启动提示词

请在这台 Mac 上以新的工程和会话接续 XOffsetDatastructure 开发，并使用子 agent 分担独立审查/实施。请直接推进可执行工作，不停在计划等待“继续”，也不要要求我手工搬文件或运行命令。不要加载 Superpowers，除非我在当前请求明确点名。

仓库是 `https://github.com/ximicpp/XOffsetDatastructure.git`，交接分支是 `codex/mac-continuation-20261020`。如我另外提供了实际推送提交 SHA，以该提交为准；否则先取得该分支真实 tip 并记录 Git/文件身份。不要使用默认分支或把历史基线 `1904b9c71230026ac6f2d58f919a9ab76447d264` 当作最新交接。已有检出先读状态并保护我的修改，新建工程则由你自行 clone 到合适的新目录；不要 reset 或覆盖既有工作。

从 `AGENTS.md`、`HANDOFF.md` 和 **`HANDOFF_MAC_20261020.md`** 开始。新 Mac 交接覆盖旧文档内过时的进展；旧 ZIP、manifest 和历史记录保留不改。再读 `docs/coordination/CURRENT_C14_ITERATION.json`、`c14-iteration-20261020-v2/STATE.json`、`TASKS.json`、`delivery/RESULTS.md`/`RESULTS.json`、`SOURCE_CHANGE.md` 及必要证明。

先运行 `python3 -I -B tools/verify_mac_handoff_20261020.py`，按 `handoff/mac-20261020/TRANSFER_MANIFEST.json` 和证据 scope 核对转交内容。Q/F 内容寻址去重包的确切索引为 [handoff/mac-20261020/evidence/BUNDLE.json](handoff/mac-20261020/evidence/BUNDLE.json)，说明见 [evidence/README.md](handoff/mac-20261020/evidence/README.md)。4ZIP合计126,548,323 B（约127 MB），代表98,418个逻辑文件/485,631,080,466 B；28,022个unique CAS blob合计282,577,279 B。运行默认命令 `python3 -I -B tools/verify_c14_evidence_bundle_20261020.py` 验证unique blobs/引用，不要自动展开约485 GB逻辑alias。需要旧 portable-inputs、固定 M fixture、历史 V1 和五库资料时，运行原样保留的 `python3 -I -B tools/restore_handoff_20261019.py`，恢复到新目录。完整性通过不能当作 Mac 平台/性能通过；只有引用未随包转移的 raw 明确标注，不补造。

当前默认源码是 C14，主头 SHA-256 为 `66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3`。R1+M2 已通过原 WSL 功能/正式测量，但只作独立候选，未晋升默认实现。`delivery/COMBINED.patch` SHA 为 `02273bdee63a43031183a370ad19f18b3af5e1910e483711c0e27573e3dc8ccb`；候选主头应为 `155f0847dff72047ac580ae8d3ead6927e52ff84eb5c9062ea2d4999980cd501`。用新 checker 的 `--materialize-candidate <全新目录>` 恢复隔离的 154 成员候选，核对完整身份；根 C14 和历史 manifest 保留。

最新有限实施计划已完成，Q32、F64 各 30/30 A/A 通过，F64 的 9,264 次及完整审计通过；**正式性能目标仅 3/12 通过，整体目标未完成**。相对 C14 读取约快 15.4%、修改约快 23.7%，不能替代竞品验收，也不能把 R1/M2 收益相加。正式目标是 XOffset÷同批每个 cista、FlatBuffers、Cap’n Proto、cereal：读取和修改约定联合上界各≤1.05，原 demo 独立拥有输出≤0.20；重载保留完整校验并报告，不设额外门槛。独立输出仍物化约17.83 MB，尚无已证明的五倍实现路径。历史 M1 超时、L1 生命周期反例、R1/M2 唯一72对及本轮Q/F均已关闭，不能补样、重开、改阈值或择快。

请先记录这台 Mac 的实际 OS、CPU/架构、原生/Rosetta/容器、资源和文件系统，再准备真实工具链/依赖并完成适用平台验证。Python3.9+，维护验证入口需 CMake/CTest3.21+，full需CMake3.30+。storage仅C++17；full仍要求实际支持P2996的Clang、匹配libc++/SDK/sanitizer ABI，普通AppleClang或仅标称C++26不够。不要修改旧固定 profile 来伪装通过。固定Boost提交为 `da041154c6bac1a4aa98254a7d6819059e8ac0b0`，TypeLayout为 `9a6de1d73617631a9612ba9f572d6c0844a90b19`，内容按 `cmake/XOffsetDependencyLock.cmake` 核对；不使用 `--remote`、系统Boost替代或放宽锁。

特别检查大小写不敏感 APFS：原 Windows 已真实遇到根 `VERSION` 被 `-I` 项目根误作 libc++ `<version>` 的编译错误。优先在新的大小写敏感工作区做精确源码快照，或者设计并验证明确的新 include 解决办法；不要删改历史VERSION来掩盖。full以 `cmake/reflection_probe.cpp` 的真实编译/链接/运行能力为准，不复用旧cache/二进制/install目录。先独立推进storage和package，同时准备full；storage通过不能代替full。维护入口full SAN默认仅有限子集，不能宣称完成历史73/73，完整SAN必须按本机实际全库存执行和审计。Linux/amd64仿真功能结果不能冒充Mac/ARM原生性能。

旧Windows/WSL绝对路径、PID/句柄/token、PowerShell暂停和电源助手、queue/lease、once admission、REVIEW/HOST_REVIEW/GLOBAL_QUIET及ATTEMPT_CLAIMED都只作历史引用，不在Mac重放。`tools/c14_iteration_20261020_v2/` 多个工具硬编码旧环境，先审查闭包再在新目录适配，不能批量执行恢复。旧两进程暂停许可不等于允许暂停Mac其他应用。

请基于当前代码和实测差距继续系统分析、设计和实现新的安全结构性优化。先量化可消除的真实读取/修改/输出/重载工作，做安全和生命周期证明、定向验证，再完整回归及新的有限预注册筛选；有依据才组合和正式验收。原demo、输入/trace、公开调用、九修改阶段、八计时边界、独立拥有string、完整校验/首错误顺序和V1、竞品模式均保持。不可用复用buffer/view/lease接口成绩替代原独立输出，不可预触页或把准备成本移出计时。没有可信五倍机制时据实说明剩余成本并继续有依据的方向，不承诺目标已能达成。

Mac适用原生功能验证完成前不启动正式bench。之后如有需要，以新宿主、新源码/工具链身份、新目录及明确预算冻结新的实验方案；这不是旧窗口补样。主会话负责设计和验收，子agent做独立安全、源码/机器码和统计审查；重构建、计时、大审计和归档由唯一heavy executor协调，计时期间其他重工作静默。

首次回复用简短状态说明已恢复什么、真实环境缺口和分工，随后立即执行可推进步骤；失败先诊断，不盲目重试、不静默降级、不重复已证明的历史工作。只有确需我提供凭据/权限/外部动作且你无法自行解决时才说明具体阻塞。本次已授权的提交推送是交接行为；新的merge、force push、Release、远端CI或其他发布行为不自动授权。旧机不恢复新的开发/性能窗口。
