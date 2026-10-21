# XOffset：GitHub 跨机器接续入口

> **2026-10-20 更新：** 默认实现现为 **W9a**（提交 `14b57637d6fa57f8521138a0997fb3c8736db4f1`，经 PR #2 合并，`next_cpp26` = `9e3602f`）。
> 下文以 C14 为默认的描述均为历史记录。下文引用的证据路径（`handoff/`、`docs/coordination/`、`tools/c14_*` 等）已移出工作树，
> 保存在 tag `archive/codex-mac-continuation-20261020`，恢复方法见 [docs/ARCHIVED_EVIDENCE.md](docs/ARCHIVED_EVIDENCE.md)。

仓库：<https://github.com/ximicpp/XOffsetDatastructure>

交接分支：`codex/cross-machine-handoff-20261019`。使用用户启动提示词给定的提交确认 `git rev-parse HEAD`，不要误用默认分支。

## 本分支已有最新 C14

根目录已从已验收 source ZIP 逐字节展开全部 **154个非 external 成员**，含 CMake、tests、tools 和14个生产文件。默认版本为 **C14 / final-v4**；C14是14文件闭包标签，不是C++14。完整库仍依赖支持P2996的Clang与匹配libc++，普通C++26编译器不足；独立storage为C++17目标。

精确依赖gitlinks保留：Boost `da041154c6bac1a4aa98254a7d6819059e8ac0b0`，TypeLayout `9a6de1d73617631a9612ba9f572d6c0844a90b19`。相同vendor文件树也保存在冻结迁移包内。本分支额外保留历史文档；根 `SOURCE_MANIFEST.json` 描述发行包闭包，不枚举整个Git工作区，新增材料化记录是 `handoff/github-20261019/C14_MATERIALIZATION.json`。

旧HEAD `7539433ab96d69c4a07b1db5c0a13786da85cdcd` 是历史父基点。**现在克隆本指定分支可取得C14和交接资料**，无需另行从旧机器复制文件。用户已授权这次交接提交和推送；未合并主分支、未创建Release、未运行远端CI，旧机开发仍暂停。

## 新机会话先自动恢复资料

从仓库根运行（Windows可用 `py -3 -I -B` 或实际Python）：

```sh
python3 -I -B tools/restore_handoff_20261019.py
```

脚本只用Python3.9+标准库：检查已提交ZIP固定SHA，解压到新目录 `handoff/continuation-20261019-v1/`，执行其中只读 `verify_handoff.py`，核对2031个移交文件、1008个发行源码成员、14个生产文件，再核对根目录154个C14成员和两项gitlinks。不联网、不安装、不编译、不执行C++或性能实验、不操作进程。

已完整解压目录可只读复验；部分解压目录不会被自动覆盖/删除，可另选新的 `--out`。解压不覆盖根源码或submodules。此检查不是新机平台验证。

| 内容 | 仓库内/恢复目录内路径 |
|---|---|
| 当前库源码 | 仓库根目录 |
| 完整迁移包及SHA | `handoff/XOffset-continuation-20261019-v1.zip` 与 `.zip.sha256` |
| 完整旧机接续总览 | 恢复目录的 `START_HERE.md` |
| 项目环境、性能、转移边界 | 恢复目录的 `docs/coordination/cross-machine-handoff-20261019/` 三份说明 |
| 最新暂停/阶段/性能决定 | 恢复目录的 `docs/coordination/receive-completion-20261019/` |
| 已提取旧Q进程时长 | 恢复目录的 `portable-evidence/oldQ-process-maxima-v2.json` |
| 提取dispatcher结果 | 恢复目录的 `portable-evidence/maxima-dispatch-v2.json` |
| 固定M输入、历史XOffset wire | 恢复目录的 `portable-inputs/` |
| 五库实验/维护工具 | 恢复目录的 `experiments/serialization_comparison/`、`tools/serialization_comparison/` |
| 新A/A草案/只读工具 | 恢复目录的 `build/receive-completion-20261019/new-AA-*` |

ZIP共46,380,763字节，SHA256为 `313e41f3890042c44cf93a14ddfb6b833d7112d35b23b07c8c5030e5ae149deb`。C14主头SHA256为 `66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3`。

**冻结包内部“根还是历史F、远端尚无C14”属于提交前旧机状态，不适用于此分支。** 其 `review-source/...`、`/root/...`、`/mnt/c/...`、`C:\Users\gzsufanchen\...`、会话ID/PID/队列/租约仅作来源引用，不能在新机原样使用。保留旧包和manifest不变。

包不含编译器、SDK/runtime、全部竞品依赖及完整历史窗口正文/4656份原始Q回执。缺档标记为“引用，未随包转移”，不能把摘要或旧审计passed当成新机完整重审。

## 新机配置与复验

代理自行准备实际可用工具、完整CMake3.30+、P2996/匹配libc++和sanitizer；记录OS/架构、原生/仿真、CPU、磁盘及ABI。历史Docker/profile不等于旧机C14实际profile，详见项目环境说明。全新build/install/output，不复用旧cache、二进制或install前缀。

若使用Git依赖，固定LF及精确提交，禁止 `--remote` 或系统Boost替换：

```sh
git -c core.autocrlf=false -c core.eol=lf submodule update --init -- external/boost external/typelayout
git -c core.autocrlf=false -c core.eol=lf -C external/boost submodule update --init -- libs/container libs/core libs/config libs/throw_exception libs/static_assert libs/container_hash libs/type_traits libs/winapi libs/intrusive libs/predef
```

以 `cmake/XOffsetDependencyLock.cmake` 内容锁为准，不放宽锁或修改历史profile来强行通过。storage-only通过不能替代full/C14。

## 当前进度与下一阶段

暂停截点为北京时间2026-10-19 20:01:57。旧机用户进程已恢复、共享租约空闲。C14在固定WSL2/Linux x86_64 profile通过normal73/SAN73、业务各80、主V1 84及双向/附加消费、生命周期52、安装/runtime和干净包验收。没有新机或Windows原生C14 full通过结论。

默认C14未加入后续窄优化。已封存内部筛选是相对各自XOffset控制，均不晋升：Cursor modify点估计1.003023/U1.027490；allocation modify 0.998607/U1.025749；bitmap修订load 1.034153/U1.073866、reload 1.014966/U1.038445；scoped-specialized modify 1.025618/U1.062446。各窗口72对/144进程，无补样、删点或重测。早期cell-v1被机制否决，与后来的bitmap修订不同。

整体性能目标未完成，新A/A尚未建立，新五库正式比较为0。旧Q32仅29/30项建立等价，旧F64未执行。禁止重开旧Q/F、关闭的候选窗口、补样、择快或转移取消预算。

正式验收是 **XOffset÷同批每个竞品**：读取合计和修改联合上界≤1.05；原demo完整独立拥有输出≤0.20；重载保持完整校验并分别报告，没有额外比例门槛。四竞品固定cista、FlatBuffers、Cap'n Proto、cereal。模型、输入/trace、原公开调用、九修改阶段、八计时阶段和V1保持；生产共享/冻结接口成绩不能替代原owning输出。

新用户明确要求继续后：

1. 完成迁移核对和新机适用功能/依赖验证。
2. 复核已提取的完整进程时长max，不用native.total冒充它，也不以历史max保证新机耗时。
3. 为保留默认C14或真正新候选及明确新宿主条件，独立修订/冻结A/A方案。现有设计v1早期前提已部分过时，不能直接启动。新C14必须有实际身份，不拿旧角色标签替代。
4. 新资格全部通过后才进入新的有限五库正式比较。资格失败不等于全部开发停止，继续可独立推进的实现与归因，但不重试关闭窗口碰运气。

离线C++20生成器仍是独立原型/研究，未替换C14；多档VM槽位池按用户决定暂缓实现；真实N7业务入口已确认暂无，保持开放，不重复索要相同前提。

用户在新机会话要求继续，仅恢复新机开发，旧机仍暂停。此次授权只覆盖本交接分支提交/推送，不自动授权后续merge主分支、force push、Release或远端CI。Superpowers仅当前用户明确点名时使用。历史证据与冻结交付保留，新工作用新目录/身份；失败先诊断，不静默降级。可推进的常规工作持续执行，阶段间不等“继续”；确需权限/凭据/外部动作才说明具体阻塞。不复用旧PID/句柄/一次性恢复脚本或旧租约。正式测量期间唯一执行者，避免并行编译/归档/扫描。
