# XOffsetDatastructure：2026-10-19 跨机器接续入口

本入口优先于根目录旧 `HANDOFF.md` 中的当前状态；旧文件保留为历史。状态截点为北京时间 **2026-10-19 20:01:57**。本次仅整理交接，开发继续保持用户要求的暂停。

## 1. 先明确当前状态

- **整体性能目标未完成。** 新的五库正式比较尚未执行；新 A/A 资格尚未建立。
- **默认保留 C14 / final-v4。** 已验收的生产代码是 14 个文件的冻结闭包，不能用 Git HEAD 或项目根目录的旧生产头代替。
- 功能、完整加载校验、V1、业务真值、生命周期、安装/共享 runtime 消费者和干净发行包已在旧机器固定 Linux/WSL profile 下通过。Windows 可见源码包也已交付。
- 四个内部性能窗口已关闭：早期 Cursor，以及后续 allocation witness、bitmap 修订、scoped-specialized；均未获得晋升所需的耗时证据，**没有合入默认 C14**。不能将早期“allocation 尚未计时”的段落当成当前状态。
- 暂停前已完成历史 Q 的完整进程时长最大值提取 v2；该结果尚待后续技术复核。下一步是评估新的测量资格条件，不是重跑已关闭候选。
- 暂停时共享租约空闲、子代理静默，曾临时暂停的游戏客户端和 TGitCache 已恢复。未提交、推送、合并、正式发布或触发远端 CI。

最新状态依据：`docs/coordination/receive-completion-20261019/pause-state-20261019-v1.json`、`current-stage-v15.json` 及各候选 decision。早期 `plan.json`/`ACCEPTANCE_20261019.md` 的某些进度段落已被这些后续记录取代。

## 2. 项目与源码身份：不要只 clone 旧 HEAD

| 项目 | 已核实信息 |
|---|---|
| 仓库 | https://github.com/ximicpp/XOffsetDatastructure |
| 旧机当前分支名 | `codex/offline-codegen-cpp20-20261018`；分支名不代表 C14 已提交 |
| 旧机 Git HEAD | `7539433ab96d69c4a07b1db5c0a13786da85cdcd`，2026-10-16 的历史交接提交 |
| 旧机 Windows 工程根 | `C:\Users\gzsufanchen\Documents\ChatGPT\xoffsetdatastructure开发` |
| 对应 WSL 路径 | `/mnt/c/Users/gzsufanchen/Documents/ChatGPT/xoffsetdatastructure开发` |
| C14 验收源码（旧机 native） | `/root/xoffset-receive-completion/20261019/v1/final-v4/source` |
| C14 可查看源码（工程相对路径） | `build/receive-completion-20261019/delivery-v2/review-source/xoffsetdatastructure-0.1.0-alpha.1/` |
| 当前主开发会话 | `接续 XOffsetDatastructure 开发`，ID `01a1436d-dd29-71b3-be72-f2878f9f7c3e` |
| 统筹/分析会话 | ID `01a14d62-bc8e-7580-a12b-368196e6e08a` |

旧工作区存在大量未提交/ignored 成果；只 clone 远端会遗漏 C14、本轮实验和当前进度。根目录主头仍是历史 F，**不能直接当作最新源码构建**。当前本地 tracked 修改另行保存为上下文补丁；它也不能盲目套到 C14 上。

新机优先从本次迁移包内的 C14 source ZIP/tar 解压到一个新工作目录，以该解压目录作为库源码。保留冻结原件，后续开发使用新的副本和身份；不要将历史根源码覆盖进来，不要 reset 既有用户修改。

关键身份：

| 内容 | SHA-256 |
|---|---|
| C14 主头 `xoffsetdatastructure.hpp` | `66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3` |
| C14 VM 头 `xoffset_virtual_memory.hpp` | `90105e4c172da26a5b23cbb10021a18613b458e92b795beee8ec9d7faed72991` |
| source ZIP | `3ddeb05afa78f566d4673d4ca19933a74f29edb45f1250be30bf69d34b7495e4` |
| source tar.gz | `a6dd9efe0648156a8b8db3af1a39b1ff3906ca7430875f703edb356edbff13b7` |

source 包位置：`build/receive-completion-20261019/delivery-v2/artifacts/source/`。完整 14 文件身份见同级 `delivery-v2/result.json` 的 `production_files`。

## 3. 核心功能与已验收范围

XOffset 使用 arena 内相对偏移和可修改变长容器，提供 owning V1 快照、完整加载校验、固定最大容量内稳定的 arena 基址。arena/root 稳定不表示容器修改后所有旧元素引用仍有效；同 owner 并发修改需要同步。

本轮新增/完成的生产接口包括可复用容量的独立保存、不可变快照与完成 lease、库拥有 VM 存储的直接接收 token、稳定批删除/冻结所有权证据索引，以及显式独占验证 scratch。`finish_verified` 完整校验成功后才发布可修改 typed owner；失败、短读、取消、OOM、移动、重入及最终完成寿命均有既定回归。索引不是业务 ID 查询索引，直接接收不以 copy-load 冒充。

| 验收（旧机固定 profile） | 结果 |
|---|---|
| C14 全库 normal / ASan+UBSan | 各 73/73 |
| callback / zero-target 新回归 | 包含在新闭包；相关 normal2 / SAN2 通过 |
| 原业务功能 | normal / SAN 各 80 个实际子进程 |
| V1 | 主交叉消费 84 项，附加 8 项，AB/C 双向消费通过 |
| 生命周期与独立池 | 52/52 实际子进程 |
| 安装、subproject、可选共享 runtime | 通过，含 C 消费者 |
| 干净包 | 新解压源码 normal / SAN 各 73 项，头文件先包含和消费者通过 |

旧归档外层 metadata 检查失败已保留；后续 archive closure v3 与 profile-reuse guard 补完并验收，不改写原失败。

这不是新机器平台验证，也不是 Windows 原生 full/C14 验证。当前 C14 **仍需要 P2996 反射工具链及匹配 libc++**；普通 C++26 编译器不能替代。独立 storage 支持 C++17。离线 C++20 生成器是独立研究线，尚未替换默认 C14。

## 4. 原性能目标与不可更换的口径

正式耗时比是 **XOffset ÷ 同批每一个竞品**，四个竞品固定为 cista、FlatBuffers、Cap'n Proto、cereal：

- 读取合计（两轮 query 之和）：联合置信上界 ≤ 1.05。
- 修改：联合置信上界 ≤ 1.05。
- 旧 demo 的完整、独立拥有序列化输出：联合置信上界 ≤ 0.20。
- 重载：保留完整校验，分别报告对四竞品数据；没有擅自增加比例门槛。

原 demo、模型、输入/trace、公开调用、九个修改阶段及八个计时阶段保持。容器容量复用、冻结两段视图、共享 lease 或生产接口专项结果不能替代旧 owning emit。完整校验和 V1 兼容不能为跑分削弱。不得隐去缓存维护、构建/释放、scratch 或内存成本转移。

| 已关闭候选 | 主目标点估计 | 联合单侧上界 | 决定 |
|---|---:|---:|---|
| Cursor / mutate | 1.003023 | 1.027490 | 不晋升 |
| Allocation witness / mutate | 0.998607 | 1.025749 | 不晋升 |
| Bitmap 修订 / load | 1.034153 | 1.073866 | 不晋升 |
| Bitmap 修订 / reload | 1.014966 | 1.038445 | 不晋升 |
| Scoped-specialized / mutate | 1.025618 | 1.062446 | 不晋升 |

这些是独立候选相对各自 XOffset 控制版的内部筛选，**不是竞品倍率**。每个内部窗口 72 对/144 个实际子进程、AB/BA 各 36 对，完整原始产物已有旧机离线审计记录；新机缺少大 body 时只能引用该记录，不能声称已经重新完整审计。

旧 Q32 共 4656 个进程，30 项 A/A 仅 29 项建立等价，cereal mutate 区间越过原界限，故旧 F64 未执行。旧 Q/F 和上述候选窗口均已关闭：不追加样本、不删离群值、不挑快值、不改阈值、不转移取消预算，不以重命名重跑同一已拒方案。区间跨 1 是不确定，不是已证明等价或退化。

## 5. 恢复后的具体顺序

当前仍暂停。新机用户明确发送“继续”或使用下方启动提示词后，才在新机恢复开发；不得因此恢复旧机器任务。

1. **迁移核对**：运行本次包的只读 `verify_handoff.py`，核对清单、C14 source 包和 14 文件身份。识别哪些原始大档未迁移。该检查不是平台测试。
2. **新机能力与功能复验**：记录 OS/架构、原生/仿真、CPU、磁盘、工具链/SDK/libc++/sanitizer、CMake 与依赖。使用全新 build/install/output，不复用旧 CMake cache 或二进制；复验适用的 storage/full/package/业务门禁。依赖及环境准备由代理推进，只有真实缺少权限/凭据等才请求介入。
3. **复核进程时长提取 v2**：移交包带有实际 `new-AA-oldQ-process-maxima-v2/result.json` 和 dispatcher 结果。指标是完整进程区间，不是 native 内 total；原 v1 的错误预算核对及失败保留。历史 max 不是新机耗时保证。
4. **形成独立新 A/A 设计**：现有 `new-AA-qualification-design-v1` 是条件性草案，早期前提/预算 max 说明已部分过时；不能直接运行。需明确测量对象是保留默认 C14 还是新的实质候选，修订适用前提、冻结角色/fixture/顺序/单位/阈值/预算/控制恢复/存储与审计闭包。旧角色标签不能冒充新的 C14 身份。没有满足资格时继续可独立推进的实现与归因，不能将资格失败当整个开发停止条件。
5. **资格合格后才正式比较**：新机是新的测量条件，不会抹去旧失败；只有相应独立资格全部通过，才进入新预登记的有限五库正式窗口。如出现真正新的优化方向，先证明实际工作减少、正确性和成本，再进入独立筛选；不能重试旧否决窗口以碰运气。

不要从“逻辑 pin 消除 16383 次”“store 数减少”“测试全部通过”推导毫秒收益。读取、修改、独立输出仍无本轮正式达标证据。整体完成日期尚无法可靠承诺。

## 6. 路径映射和迁移边界

全部旧机器绝对路径仅供定位，不在新机原样执行：

- Windows 根：`C:\Users\gzsufanchen\Documents\ChatGPT\xoffsetdatastructure开发`。
- 近期 native 工作区：`/root/xoffset-receive-completion/20261019/v1`。
- 上一四路径工作区：`/root/xoffset-four-paths/20261018/v1`。
- 早期五库准备/工具：`/root/xoffset-serialization-comparison/20261017`。
- 近期脚本：工程 `build/receive-completion-20261019/`；其 `common.py` 与旧 runner 存在多层绝对路径依赖，**不能在新机直接执行**。
- 业务实验源码：`experiments/serialization_comparison/`；维护工具：`tools/serialization_comparison/`；协调源码：`experiments/vm_coordination/`。新机需重新冻结依赖/工具链/二进制与输入闭包。
- 最新状态与决定：`docs/coordination/receive-completion-20261019/`。

本次迁移包镜像保留上述关键工程相对路径，并包含 C14 source/full/storage 归档、精选旧机小结果/审计摘要、benchmark 源码与固定 M 输入、下一阶段设计/只读工具。**不包含完整 WSL 文件系统、SDK/compiler/runtime、全部竞争库源码/二进制、完整历史窗口 body 或全部旧 receipts**。是否随包以 `TRANSFER_MANIFEST.json` 为准；缺档据实登记，不能伪造、回写旧 hash 或宣称完整复现。

新机如使用 Git，可 clone 仓库保留历史，但必须单独导入已核对 C14 快照与本次接续资料；新分支默认用 `codex/` 前缀。本次没有代用户提交或推送，远端不包含本次未提交成果。

## 7. 并行研究线

- 离线代码生成：`docs/codegen-production-assessment-20261018/`。存在早期有限 C++20 原型与后续 `research-design-v3` 评估，不能将目录初始 README 的“未实现”直接当所有后续状态，也不能将原型当已证明可替代 C14。仍需完整操作元数据、V1/生命周期/普通工具链/平台证明，运行时提速未证明；不要回写旧 profile 或混合证据。
- 多档 VM 槽位池：`docs/vm-slot-pool-assessment-20261018/`。用户决定先文档化、后续再安排；F01–F07 尚未启动，不自动恢复原型或测量。要区分 VA 预留、Windows commit、accessible、RSS 与 idle-retained；分档不能自动消除页尾浪费。
- 真实 N7 网络业务整合：用户已确认暂无真实业务入口，保持开放；不要反复索要相同前提，也不能把内存/回环生命周期模型写成真实网络延迟。

## 8. 新机会话启动提示词（可直接复制）

> 请从本次迁移包的 HANDOFF_20261019.md（或 START_HERE.md）开始接续 XOffsetDatastructure。我明确要求在这台新机器恢复开发；旧机器仍保持暂停。先读 docs/coordination/cross-machine-handoff-20261019/ 的三份补充说明，执行只读 verify_handoff.py，确认实际 C14 源码和未迁移原始档案。不要只 clone 旧 HEAD 或构建根目录历史 F。自行完成适用的新机依赖/环境配置和功能复验，保持完整校验、V1 兼容、原 demo 与独立拥有输出语义。先复核已提取的旧 Q 进程时长 max，评估保留默认 C14 与受控新宿主的独立 A/A 方案；旧 Q/F 和已关闭候选不重跑、不补样。性能验收使用 XOffset÷各竞品：读取合计/修改上界≤1.05，独立输出上界≤0.20，重载完整校验并另报。继续已授权且可推进的工作，不在阶段间等待“继续”；真实阻塞应说明具体原因。保持历史档案和冻结源码，所有新工作使用新目录/身份。Superpowers 仅本轮明确点名时使用；不自动提交、推送、合并、发布或触发远端CI。旧机器PID/句柄/租约/一次性恢复脚本不在新机复用。

补充详见 `docs/coordination/cross-machine-handoff-20261019/PROJECT_AND_ENVIRONMENT.md`、`PERFORMANCE_STATUS.md`、`TRANSFER_CHECKLIST.md`。本入口与迁移完整性验证均不新增任何平台通过或性能达标结论。
