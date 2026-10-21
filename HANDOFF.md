# 当前接续入口：2026-10-20 Mac 新工程交接

> **2026-10-20 更新：** 默认实现现为 **W9a**（提交 `14b57637d6fa57f8521138a0997fb3c8736db4f1`，经 PR #2 合并，`next_cpp26` = `9e3602f`）。
> 下文以 C14 为默认的描述均为历史记录。下文引用的证据路径（`handoff/`、`docs/coordination/`、`tools/c14_*` 等）已移出工作树，
> 保存在 tag `archive/codex-mac-continuation-20261020`，恢复方法见 [docs/ARCHIVED_EVIDENCE.md](docs/ARCHIVED_EVIDENCE.md)。

**先读 [HANDOFF_MAC_20261020.md](HANDOFF_MAC_20261020.md)。** 仓库与本次交接分支、完整性检查、候选恢复、去重证据包及 Mac 平台验证入口均在新说明中；可直接复制的启动提示词见 [PROMPT_MAC_20261020.md](PROMPT_MAC_20261020.md)。

最新 R1/M2 与组合的有限实施及 Q32/F64 已完成，正式主目标仅 3/12 通过；默认仍为 C14，独立候选未晋升。下文旧交接状态仅保留历史，不能覆盖新说明。迁移完整性不是 Mac 平台测试，不重放旧 Windows/WSL 工具、租约或一次性准入。

---

# 当前接续入口：2026-10-19 GitHub 交接分支

**先读 [HANDOFF_GITHUB_20261019.md](HANDOFF_GITHUB_20261019.md)。** 本分支根目录已展开经过验收的 C14/final-v4，完整迁移 ZIP、SHA256 和恢复校验工具也已提交，新机不需要用户另行搬文件。

下文保留旧交接历史；其中旧根目录、旧 HEAD、旧平台状态不能覆盖新的 GitHub 接续说明。旧入口另原样保存于 `docs/handoff-history/HANDOFF_20261016_before_github_transfer.md`。本次仅提交转交，旧机开发仍暂停；迁移完整性通过不等于新平台或性能验收通过。

# 历史：XOffset 新机器接续入口

此分支供 Codex 自动接续开发。用户只需在新机器添加工程并输入启动提示词；源码获取、
依赖准备、环境配置、验证和后续实施由 Codex 完成，不要求用户手工搬文件或执行命令。

## 当前源码和交付范围

- 仓库：<https://github.com/ximicpp/XOffsetDatastructure>
- 交接分支：`codex/vm-handoff-20261016`
- 原开发分支：`next_cpp26`；Git 基点：`07d649909d7f43ca0d11b85accb6d9dab991e284`。
- 原工作区有大量未提交成果。此分支已将已验收的 245 个源码成员展开到项目根目录，
  无需再次解包覆盖源码；原 Git 基点本身不代表 F。
- 原冻结胶囊完整保留于 `docs/validation/vm-final/final-v1/`。其中的元数据是 cut
  时点快照；项目根目录的计划、交接及 D034 文件使用后来已验收的版本。
- 本次增加跨机器说明、完整性检查及少量接续输入，没有改变生产头或采用新的优化。

F 生产头 SHA-256：

| 文件 | SHA-256 |
| --- | --- |
| `xoffsetdatastructure.hpp` | `9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2` |
| `xoffset_virtual_memory.hpp` | `22f181b52c11ce442a5576ffc0dc35af1828f77deb4424f387d0acfa658239ad` |

`handoff/materialization.json` 记录新增文件的来源类别；`handoff/manifest.json` 固定
此次交接的普通文件身份（自身除外）及依赖 gitlinks。首次接续先执行：

```sh
python3 tools/verify_vm_handoff.py
```

Windows 可使用本机实际可用的 `python` 或 `py -3`。工具只读、不联网、不编译。
完成后合法修改源码会导致初始交接校验不一致，应解释差异并记录新身份，不能 reset
或回写历史清单来消除不一致。`.gitattributes` 保持主仓库检出字节，防止换行转换。

这是可继续开发的源码交接，并非完整离线构建环境或全部历史原始证据包。未包含：
依赖源码树、编译器、SDK/runtime、旧机器二进制、完整 `build/` 及大型测量/wire 档案。
历史说明中的 `/Users/...`、旧会话 ID、SSH 别名和队列状态仅是当时记录；不代表新机
可访问的地址、当前有效授权或正在运行的任务。不要尝试恢复旧队列或执行旧批准。
缺失的历史原始文件据实标注为“引用，未随本次转移”，不补造、不宣称已完整重审。

为使后续任务可以开始，另保留以下小型输入：

- `docs/perf/2026-10-15-vm-stage1/` 的 D/F 源码快照、身份、摘要及一份原始 fixture。
- `docs/validation/2026-10-15-vm-followup/wire-inputs-and-versions.tar.gz` 及对应说明。
- Windows 准备/验证脚本及其旧 Stage II 源码测试 fixture。旧包的 121 文件与当前
  245 文件源码集合分别记录，旧包不能冒充当前 Windows 验证。

某些历史实验工具自测还引用未转移的旧 `build/` fixture 或二进制身份。例如网络测量
绑定测试引用 `build/vm-network/timing-v2` 和旧冻结 measurement 工具；approval 补证
测试引用旧 proposal。不能直接执行全目录测试后把缺失历史 fixture 算作生产库故障。
先看该测试的输入闭包；能从胶囊只读 metadata 恢复到独立 fixture 的再恢复，其余明确
记录限制。新平台功能和新实验使用当前源码、新构建身份及新输出目录。

## 先恢复方案和状态

依次阅读：

1. `docs/VM_FINAL_DELIVERY.md`
2. `docs/coordination/vm-followup/G4-final-acceptance.json`（D034 最终父验收）
3. `docs/VM_FOLLOWUP_ORCHESTRATION.md`，尤其“本轮之后的启动条件”
4. `docs/VM_NEXT_STAGE_PLAN.md`
5. `openspec/changes/vm-next-stage-hardening/tasks.md`
6. `docs/STABLE_VM_STORAGE_MIGRATION.md`

核心改造及 G0–G4 本地研究交付已完成，保留 F；整体路线图尚未完成。旧状态中的外部
前提阻塞属于旧机器情境，迁移后应重新评估实际前提，不机械沿用阻塞，也不自动关闭。
不要因冻结包内 handoff 尚写“待父验收”而重复 D034 已完成的工作。

已经完成的 allocator、Automatic、池/回收、网络参考、诊断及固定请求量 owner
研究按原范围保留。六个 allocator 候选已拒绝，Automatic 窄缓存转移候选有明确反例。
三个 I/O 方向目前是“前提未满足，保留 F”，没有实施，也未被实验性拒绝。

既有默认 F/F 与网络池对照有噪声资格限制，不能宣称加速、等价或回退。owner 多线程
比较仍不确定，不能单独归因于 registry 或锁。网络实现为同步 TCP 回环参考和独立
异步生命周期模型，并非生产异步 transport；新网络 sanitizer 曾因 ABI 不匹配链接
失败，运行阶段未执行。新机是新的可能前提，不是自动通过证明。

## 自动配置环境与依赖

先记录 OS/架构、CPU、原生或仿真、资源、实际编译器、SDK、libc++、sanitizer runtime、
Python 和 CMake。优先复用可验证的现有工具，缺失组件优先装到项目独立环境。能够由
Codex 完成的下载、安装和配置不交给用户；确需系统权限/凭据时只询问具体缺口，并继续
独立工作。不要更改无关项目或降低系统安全设置。

最低入口：Python 3.9+；storage 验证 CMake/CTest 3.21+；完整库 CMake 3.30+。
独立 storage 需要 C++17 编译器；完整库需要支持 P2996 的 Clang 和匹配 libc++。
普通 C++26 编译器不能替代。运行库 ABI 单独检查，不能绕过 sanitizer 版本检查。

固定依赖 gitlinks：

- Boost：`da041154c6bac1a4aa98254a7d6819059e8ac0b0`
- TypeLayout：`9a6de1d73617631a9612ba9f572d6c0844a90b19`

以下为 Codex 执行的依赖准备入口，无需用户操作。子模块不继承主仓库换行属性，检出时
禁用 autocrlf；不要使用 `--remote` 或按 `.gitmodules` 的分支提示升级。

```sh
git -c core.autocrlf=false submodule update --init -- external/boost external/typelayout
git -c core.autocrlf=false -C external/boost submodule update --init -- libs/container libs/core libs/config libs/throw_exception libs/static_assert libs/container_hash libs/type_traits libs/winapi libs/intrusive libs/predef
```

随后以 `cmake/XOffsetDependencyLock.cmake` 的 include 树和许可证内容锁为准。不要修改
锁来掩盖缺失或不匹配。GitHub 认证或 SSH 不可用时可在新隔离检出中使用公开 HTTPS
地址获取相同提交；不把凭据写进工程。

阅读 `docs/VM_VALIDATION_RUNNER.md`、`docs/VM_PACKAGING.md` 和
`docs/VM_PLATFORM_FOLLOWUP.md` 后选择适用入口：

- Linux/macOS storage：`tools/validate_vm.py native --suite storage`，显式指定当前
  编译器、源码和全新输出目录；安装消费用 `tools/validate_vm_package.py --suite storage`。
- Windows：自动发现 MSVC x64 developer environment 和 SDK，使用当前根源码的 CMake
  `-DXOFFSET_STORAGE_ONLY=ON` 构建/CTest，再运行 storage package 验证。精确记录
  storage 2 项、installed 3 项、subproject 3 项及 JUnit；不能只统计任意退出 0。
  `validate_vm.py native` 不是 MSVC 入口。`experiments/vm_windows/prepare.py` 固定了
  旧 Stage II 身份，不要修改其常量来伪装当前源码。
- 完整库：P2996 能力和依赖验证后使用 full 入口，记录真实 profile。现有
  `check_vm_toolchain.py` 用于检查旧固定 profile，不是通用新机器探测器；新工具链
  应有新记录，不修改旧 profile 来强行通过。
- 固定 Linux 容器可提供相应功能验证；镜像内旧 CMake 不够时复用
  `tools/prepare_vm_cmake.py`。跨架构仿真不用于原生性能结论。

每次使用新的构建和结果目录，不复用旧 CMake cache、安装前缀、编译器路径或二进制。
迁移完整性检查通过不等于 Windows/Linux/full/sanitizer 已通过。

## 后续阶段与实施边界

**阶段 A：接续和平台验证。** 完成文件、依赖及能力核对，再完成本机适用的
storage/package、full 和网络 sanitizer 验证。Windows 完整 P2996 与 storage 支持
分别评估。基础库 sanitizer 的历史结果不能充当新网络代码通过。

**阶段 B：N3 复现和归因。** 新环境中先冻结真实 D/F 源码身份、相同工作量、编译参数、
指标、顺序、样本预算及噪声标准。历史 D 见已附源码快照，不等于旧 Git HEAD。
`experiments/vm_network_measurement/prepare_default.py` 可准备并审计这些源码，
不自动执行测量。先做有界 A/A，达到目标量级检出能力后进行 D/F；稳定差异复现后
才实施单变量归因。保留失败/不确定结果，不追加样本或重试直到通过。

**阶段 C：N7 条件整合。** 兼容 sanitizer 与其他准备可独立推进。生产候选仍需
协议/库、schema、消息尺寸及分布、并发、完成/取消语义、资源和延迟目标。信息不足
时提出必要问题，继续其他任务。条件具备后先建立代表性消费者及等工作量 F 池化加载
对照，再评估 owner 复用、直接读新 staging owner、减少输出复制。逐项明确失败恢复、
取消、异步真实完成和缓冲寿命，依据证据决定原型及采用，缺少依据时保留 F。

**阶段 D：验收。** 独立 review 实际变更，运行相关必要测试，更新任务清单、支持矩阵、
迁移说明和证据索引。区分完成、失败、结果不确定、前提未满足；不把整体目标虚报完成。

持续保持：固定最大容量内 arena 基址稳定、超出硬上限失败、默认 Explicit、可选
Automatic、默认不启用池、显式回收及池预算、现有指针/span/handle/move/swap/epoch/
异常契约、V1 wire 和完整加载校验。arena 基址稳定不代表容器元素永不移动，同一
owner 的并发修改需要同步。不要无依据更改默认策略或增加隐式搬迁/fallback。

## 统筹方式

主会话负责设计、任务拆分和验收；将独立实施工作交给子代理，明确写入边界。共享
生产头/CMake 由单一负责人修改。独立分析和准备可并行，正式性能测量只允许一个
执行者、一个窗口，避免与编译、重测试、大规模归档争抢资源。

新设计和预算由本轮统筹明确记录后推进，不复用旧一次性批准，也不因为历史批准过期
而向用户反复询问已授权的常规操作。先评估现有执行协调工具对新 OS 的适用性，不假设
它已支持 Windows、远程或容器。

此分支的公开交接发布已获用户授权；该授权不等于未来开发可自动提交、推送、合并、
发布或触发远端 CI。未来这些操作仍需明确授权。Superpowers 只在用户当前消息明确
调用时使用。

首次回复简要给出迁移检查、环境能力、待办优先级和并行分工，然后立即执行可推进的
阶段；不要停留在计划。用户不承担手工环境操作。
