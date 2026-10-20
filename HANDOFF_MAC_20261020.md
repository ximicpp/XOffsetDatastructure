# Mac 新工程接续：2026-10-20

仓库：<https://github.com/ximicpp/XOffsetDatastructure.git>。本次交接分支为 `codex/mac-continuation-20261020`；以用户随启动提示词给定的实际推送提交为准。若用户只给出分支，先记录远端该分支实际 tip 和本地 `git rev-parse HEAD`，核对一致，再记录本次接续身份。不要误用默认分支或把历史基线提交当作本次交接提交。

用户只需在 Mac 上新建工程/会话并粘贴 [PROMPT_MAC_20261020.md](PROMPT_MAC_20261020.md)。代理自行获取源码、恢复资料、准备依赖和工具链、验证平台并继续实施。不要要求用户手工搬文件或运行命令。常规可逆工作持续推进；仅在确需凭据、系统权限或外部操作且代理无法自行解决时说明具体阻塞。

## 当前状态与精确源码

本轮 R1、M2、组合、Q32 与 F64 的有限实施计划已经完成，**正式性能目标尚未全部达到，F64 仅 3/12 项通过**。默认实现保留 C14；R1 + M2 以独立候选交付，未晋升默认源码。最新结果优先于旧交接中“新比较为 0、F64 未执行”等历史状态。

先阅读以下文件：

1. [当前迭代指针](docs/coordination/CURRENT_C14_ITERATION.json)、[状态](docs/coordination/c14-iteration-20261020-v2/STATE.json)、[任务](docs/coordination/c14-iteration-20261020-v2/TASKS.json)。
2. [最终性能结果](docs/coordination/c14-iteration-20261020-v2/delivery/RESULTS.md)及同目录 `RESULTS.json`。
3. [代码说明](docs/coordination/c14-iteration-20261020-v2/delivery/SOURCE_CHANGE.md)、[组合补丁](docs/coordination/c14-iteration-20261020-v2/delivery/COMBINED.patch)和 `IMPLEMENTATION_ACCEPTANCE.json`。
4. 需要了解证明和旧验收时，再读本轮 `core/`、`reviews/`、`validation/` 中的相关材料，保持各证据的实际作用域。

| 源码身份 | SHA-256 |
| --- | --- |
| 默认 C14 主头 `xoffsetdatastructure.hpp` | `66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3` |
| R1 + M2 候选主头 | `155f0847dff72047ac580ae8d3ead6927e52ff84eb5c9062ea2d4999980cd501` |
| 独立候选 `COMBINED.patch`，2520 B | `02273bdee63a43031183a370ad19f18b3af5e1910e483711c0e27573e3dc8ccb` |

候选相对精确 C14 的 154 个非 external 源码成员只改变主头；14 个生产文件的其余部分不变。C14 是源码闭包标签，不是 C++14。历史基线提交为 `1904b9c71230026ac6f2d58f919a9ab76447d264`，也不是本次推送提交。

R1 保留每次 acquire 检查，将可内联 inactive 入口与原 active helper 拆开，不新增跨调用缓存。M2 仅复用当前非 string owning-vector 节点的一次 data 解析，保留 capacity 检查先于 size、双向 owns、递归和早期返回顺序。完整证明及已接受范围见代码说明；不要把局部解析 4→1 外推为整个修改减少 75%。

## 资料恢复与完整性

先用实际 Python 3.9+，从仓库根执行新交接检查：

```sh
python3 -I -B tools/verify_mac_handoff_20261020.py
```

它以 [handoff/mac-20261020/TRANSFER_MANIFEST.json](handoff/mac-20261020/TRANSFER_MANIFEST.json) 核对当前 C14 的 154 成员/14 生产文件、本次交接的 docs/tools 和候选/证据包身份。以实际 scope 清单为准；这个清单不声称枚举所有 Git 历史或任意未跟踪文件。它不安装、不编译、不启动 C++/性能实验，也不操作进程。

若需候选工作副本，可使用 `--materialize-candidate <全新目录>`，生成精确 154 成员的隔离候选，只将主头替换为上表候选身份。目标必须全新；不要覆盖根 C14、既有工作区或把用户修改 reset 成旧 manifest。检查目标主头与成员清单后再开发。手工应用补丁也必须先核对基线、执行 check，再核对完整目标身份，不能重复应用。

原 20261019 ZIP、SHA 和恢复工具原样保留，需要旧 portable-inputs、固定 M fixture、V1/历史资料或旧五库工具时执行：

```sh
python3 -I -B tools/restore_handoff_20261019.py
```

它只恢复到自己的新目录，不覆盖根 C14。已完整恢复目录可复验；部分恢复目录保留现场，另选新输出目录，不自动删除。详见 [原 GitHub 交接](HANDOFF_GITHUB_20261019.md)。旧包内旧机状态、旧入口和未执行状态不能覆盖本文件及本轮最终结果。

本次 Q32/F64 证据索引为 [BUNDLE.json](handoff/mac-20261020/evidence/BUNDLE.json)，使用说明为 [evidence/README.md](handoff/mac-20261020/evidence/README.md)。4 个 ZIP 合计 **126,548,323 B（约127 MB）**，内容寻址去重后包含 28,022 个 CAS blob / 282,577,279 B，代表 98,418 个逻辑文件 / 485,631,080,466 B。按默认命令只读验证：

```sh
python3 -I -B tools/verify_c14_evidence_bundle_20261020.py
```

包保留 actual run、frozen、原离线 audit 队列及其 scope 中列出的资料。先读索引/scope，再验证 unique blob 和引用；**不要自动将约 485 GB 的逻辑 alias 全量展开**。物理 ZIP、解压后的 unique blob、逻辑别名量、实际生成 wire 字节与内存是不同指标，不互相替代。

编译器、SDK/runtime、旧机二进制及 run 范围之外的历史大型 raw 不因此全部转交。以实际包 scope 和转移清单逐项确认；只有来源引用的文件标为“引用，未随包转移”，不补造。Git 内 docs 的绝对路径和 passed 摘要不等于其引用的每个原始文件都已转交，也不等于新机重审通过。

**迁移完整性通过不等于 Mac 平台、full 库、sanitizer、兼容性或性能测试通过。**

## Mac 工具链和文件系统先决条件

记录实际 OS/版本、CPU/架构、原生还是 Rosetta/容器、磁盘和内存、工程与输出文件系统、Python、CMake/CTest、编译器、SDK、libc++、sanitizer runtime 及 ABI。优先复用可验证的现有工具；缺失项由代理在项目独立环境中准备，不修改无关项目或降低系统安全设置。

- Python 3.9+；维护验证入口需 CMake/CTest 3.21+，当前 full 目标需 CMake 3.30+。
- 独立 storage 是 C++17，不依赖 P2996；full 仍需支持 P2996 的 Clang、匹配 libc++/SDK 及实际可用的 runtime。普通 AppleClang 或仅声称支持 C++26 的编译器不满足 full 要求。
- full 实际使用 `-std=c++26 -freflection -fexpansion-statements -stdlib=libc++`。以 `cmake/reflection_probe.cpp` 的真实编译/链接和实际运行能力为准，不能只看版本字符串。
- `toolchains/vm-validation.json` 和 `tools/check_vm_toolchain.py` 包含旧固定 profile。不要改其旧哈希/提交/SDK 字段来伪装新机通过。为新工具链记录新 profile；显式选择适用 SDK/libc++/sanitizer，不盲用旧 `/Users/...` 路径或历史 SDK 名称。
- 本轮 Windows 已真实遇到根文件 `VERSION` 被项目根 `-I` 搜索当作 libc++ `<version>` 的失败，旧失败保留。Mac 常见 APFS 同样大小写不敏感：先探测实际工程/构建卷，优先使用新建的大小写敏感工作区保存精确源码快照；或提出并验证新的 include 方案。不要删改历史 `VERSION` 掩盖问题，也不要把编译失败当性能失败。
- Apple Silicon 上仿真的 Linux/amd64 容器只证明其实际环境内的功能情况，不能称为 Mac 原生或 ARM 原生性能。

固定依赖 gitlinks：Boost `da041154c6bac1a4aa98254a7d6819059e8ac0b0`；TypeLayout `9a6de1d73617631a9612ba9f572d6c0844a90b19`。代理按需执行：

```sh
git -c core.autocrlf=false -c core.eol=lf submodule update --init -- external/boost external/typelayout
git -c core.autocrlf=false -c core.eol=lf -C external/boost submodule update --init -- libs/container libs/core libs/config libs/throw_exception libs/static_assert libs/container_hash libs/type_traits libs/winapi libs/intrusive libs/predef
```

以 `cmake/XOffsetDependencyLock.cmake` 的 include 树/许可证内容锁为准，不使用 `--remote`、系统 Boost 替代或放宽锁。认证受阻时可在新隔离检出中用公开 HTTPS 取得相同提交，不向仓库写凭据。

## 新机先做平台验证，再做新机制

首次回复简短报告已经恢复的状态、实际环境缺口和并行分工，随后立即执行，不停在计划等待确认。

1. 核对本次 Git/文件身份、证据边界和依赖；在新构建、安装、结果目录记录实际环境。
2. 使用维护入口 `tools/validate_vm.py native --suite storage`，显式传入真实 compiler/source/out；再用 `tools/validate_vm_package.py --suite storage` 做对应安装和消费验证。storage 通过仅覆盖 OS 层。
3. 可并行准备 full P2996/SDK/runtime。满足能力后完成当前源码及隔离候选的适用完整 normal/SAN、业务 S/M/L 真值、V1 双向、安装/消费端和生命周期验证，按实际库存记录。不复用旧 cache、二进制、安装前缀或输出目录。参见 [验证入口](docs/VM_VALIDATION_RUNNER.md)及 [打包说明](docs/VM_PACKAGING.md)。
4. **维护入口 full SAN 默认选择有限子集，不能冒充本轮历史 73/73。** 要宣称完整 SAN，必须以新机实际完整库存、实际编译/运行/回执证明；ABI 或 runtime 失败据实保留。历史 normal73/SAN73、业务80+80、V1 及消费结果均限原 WSL profile，不能转借。
5. 基于最新差距，先建立新安全结构性机制、真实可消除工作和成本预算，再做局部证明、定向验证、完整回归与一次预注册筛选。读取需针对真实查找/扫描；修改需针对实际所有权/解析/迁移工作。输出先说明能减少哪些字节、分配/触页/复制成本；没有依据就记录未解决，不能承诺五倍。
6. Mac 适用原生平台验证完成前不启动正式性能窗口。之后如需新实验，以新宿主、新源码和工具链身份、新结果目录及明确冻结预算设计新的有限协议。主会话负责设计/验收，子 agent 做独立安全、源码/机器码和统计审查；重构建/测量/大审计/归档由唯一 heavy executor 协调，计时期间其他重工作静默。

正式目标不变：候选除以同批每个固定竞品，读取合计/修改的约定联合上界各 ≤1.05，原 demo 完整独立拥有输出 ≤0.20；重载保留完整校验、分别报告，不增设比例门槛。当前 F64 对四竞品仅通过 3/12。读取/修改相对 C14 的收益不能代替逐竞品验收，输出约两倍优势不能称为五倍达标。

原 demo、模型、输入/trace、公开调用、九修改阶段、八计时边界、独立拥有 string、完整校验及首错误顺序、V1 与竞品模式继续保留。可选复用缓冲、冻结 view/lease、共享发送接口不能替代旧独立输出成绩。复制期间的 faults 已在复制区段内，不能另做 touch 后再相加；插桩耗时不作为正式收益证据。

历史 M1 为“功能通过、筛选超时、性能未证明”，L1 有生命周期反例被否决；R1/M2 的唯一 72 对和本轮 Q/F 已结束。均不补样、不删除失败、不改阈值、不重开旧窗口。新平台可以形成新、独立的实验条件，但不能作为给旧窗口追加有利样本的理由。无新安全机制依据时继续分析和实现准备，不盲跑基准。

## 旧机工具与授权边界

所有 Windows/WSL 绝对路径、旧会话 ID、PID/句柄、token、queue/lease、电源 GUID、profile 和 once admission 都是历史身份引用，不能按字面在 Mac 使用。尤其 `q32_host_v1`、`formal_host_v1` 和 screen host 中的 PowerShell/Windows 进程暂停、电源和恢复工具不能重放；旧两进程暂停授权不授权在 Mac 随意暂停其他应用。不要恢复旧队列或旧 `ATTEMPT_CLAIMED`，也不把旧 REVIEW/HOST_REVIEW/GLOBAL_QUIET 当作新机执行许可。

本轮 `tools/c14_iteration_20261020_v2/` 多个 prepare/bind/stage/execute 工具固定了旧绝对根、旧 source snapshot、profile 或一次性请求。它们用于保留机制和证据，不能批量运行来“恢复状态”。新平台适配在新目录实施，先审查实际闭包和接口，保留原文件及失败证据。

用户已授权本次交接的 commit/push。该授权不自动覆盖新机会话未来的 merge、force push、Release、远端 CI 或其他发布行为。用户在 Mac 要求继续开发，只恢复新机工作；旧机不启动新的开发/性能窗口。Superpowers 默认不加载，仅在当前用户请求明确点名时使用。
