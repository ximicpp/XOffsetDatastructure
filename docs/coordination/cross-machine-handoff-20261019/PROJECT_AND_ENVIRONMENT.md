# 项目、交付源码与新机器环境

本记录只做跨机器交接整理。开发已按用户明确要求暂停；本次没有构建、运行项目、导入项目模块或取得执行租约。不要因打开本交接文档而自动恢复实验。状态入口是 `docs/coordination/receive-completion-20261019/continuation-current.json`、`pause-state-20261019-v1.json` 和 `current-stage-v15.json`。

## 1. 仓库 HEAD 与已验收 C14 不是同一份源码

- 仓库远端：<https://github.com/ximicpp/XOffsetDatastructure.git>。
- 本机根目录：`C:\Users\gzsufanchen\Documents\ChatGPT\xoffsetdatastructure开发`。
- 本次只读 `.git/HEAD` 与对应 ref：分支 `codex/offline-codegen-cpp20-20261018`，提交 `7539433ab96d69c4a07b1db5c0a13786da85cdcd`。没有运行 Git status，不据此声明工作区干净或所有成果已跟踪。
- 根目录 `xoffsetdatastructure.hpp` 仍是历史 F 头：183050 B，SHA256 `9bc8f1f533bee83a9e111e05d67319383775dbca1844ef33b1aa717047640bb2`。
- 当前保留交付是 **C14 / final-v4**，其 main 头为 278262 B，SHA256 `66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3`。14 项生产文件的身份见交付 `result.json.production_files` 与 `acceptance/materialization.json`。
- C14 尚未 commit、push、merge、publish，也没有触发远端 CI。根 `.gitignore` 忽略 `build/`；仅 clone 当前提交，或仅复制 Git 跟踪文件，不能恢复这份交付。

原 `HANDOFF.md` 是 2026-10-16 及更早交接的历史入口，记录的分支、F 头和平台测试范围不能覆盖今天的 C14。应保留它；不要把根源码 reset 成清单，也不要修改历史 manifest 来消除差异。

## 2. 可移交的完整源码与安装包

Windows 交付根目录已经存在：

`C:\Users\gzsufanchen\Documents\ChatGPT\xoffsetdatastructure开发\build\receive-completion-20261019\delivery-v2`

解码审阅源码：`delivery-v2/review-source/xoffsetdatastructure-0.1.0-alpha.1/`。交付记录为 1008 个文件、8038649 B；该目录没有 `.git` 元数据。原实际验证源码位置为 `/root/xoffset-receive-completion/20261019/v1/final-v4/source`，这个 Linux 绝对路径只是本机证据位置，不是新机器路径。

下表路径均相对上述 Windows 交付根目录。**名称同为 alpha.1 的历史包很多，必须同时核对路径和 SHA256。**

| 用途 | 文件 | 字节数 | SHA256 |
| --- | --- | ---: | --- |
| 完整源码，建议作为新机器 C14 工作起点 | `artifacts/source/xoffsetdatastructure-0.1.0-alpha.1.tar.gz` | 1276597 | `a6dd9efe0648156a8b8db3af1a39b1ff3906ca7430875f703edb356edbff13b7` |
| 同一源码的 ZIP 形式 | `artifacts/source/xoffsetdatastructure-0.1.0-alpha.1.zip` | 1939159 | `3ddeb05afa78f566d4673d4ca19933a74f29edb45f1250be30bf69d34b7495e4` |
| full 安装前缀归档 | `artifacts/full/xoffsetdatastructure-0.1.0-alpha.1-full.tar.gz` | 1026010 | `557be6d15f42cabdca4d7e2e7cc70a40c9457220c08f565f9de514cec4c20775` |
| 独立 storage 安装前缀归档 | `artifacts/storage/xoffsetdatastructure-0.1.0-alpha.1-storage.tar.gz` | 14695 | `b1824df305c4b14b34c26850a72e060f49586b4a67b0482298984cecdeebe31d` |

每组旁边有独立 `SHA256SUMS`、manifest；源码组还有 `zip-audit.json`。本次包哈希取自已验收的 delivery result，并与 delivery manifest 的成员记录核对；没有再次读取压缩包正文计算哈希。

交付清单：`delivery-manifest.json`，248180 B，SHA256 `9fded38ae3bae64b573d359c60c79ba4b4d3dedd864bd8c48289f20369bee654`。交付 `result.json` 为 `status=passed`，19110 B，SHA256 `fe67b9de31f90e7f2096b92f227b8586e0f4494c465c7cf9e4059bd2f49e3c61`。清单明确排除自身和 result，不构成自哈希。

交付复制了归档、解码源码和选定验收小记录，没有复制编译器/SDK、巨大原始性能正文或整个 WSL 构建树。NTFS 上的 mode 不是 Linux 执行权限；解包后按目标系统恢复必要权限，不能仅凭 mode 差异改写清单。

## 3. 新机器依赖要求

| 目标 | 源码/编译条件 | 工具与依赖 |
| --- | --- | --- |
| full，包括 C14 可变直接接收 | 64-bit little-endian；支持 P2996 的 Clang 与匹配 libc++；普通 C++26 编译器不足 | CMake 3.30+、Threads；通过真实 reflection compile/link probe；匹配 Boost/TypeLayout 内容锁 |
| 独立 storage | C++17 编译器及目标 OS 的 VM API | 不要求 P2996、Boost 或 TypeLayout；源码 CMake 最低声明 3.13，沿用 storage 验证/包 runner 时按历史交接要求使用 CMake/CTest 3.21+ |
| Python 验证/准备工具 | Python 3.9+ | 新机器先记录实际解释器；不要把解释器能运行等同于 native 验证通过 |

full 接口传播 `-std=c++26 -freflection -fexpansion-statements -stdlib=libc++`，并执行反射编译/链接检查；验收构建使用 CMake 4.2.3。具体诊断、编译数据库可能采用 GNU++26；保持该次实际 profile 的参数记录，不混称为另一套构建。

源码和 full 包已捆绑所需的 851 个第三方头及许可证，可在没有依赖 `.git` 的解包目录中验证。`cmake/XOffsetDependencyLock.cmake` 固定：

- Boost：`da041154c6bac1a4aa98254a7d6819059e8ac0b0`。
- TypeLayout：`9a6de1d73617631a9612ba9f572d6c0844a90b19`。
- 验证以目录内排序路径/内容 SHA256 及许可证为准，不仅比较 revision 名。不得放宽锁或改用系统 Boost 来使配置通过。

如果从仓库另建依赖，按原 HANDOFF 的 LF 固定方式取得精确提交，避免 Windows `core.eol=native` 造成字节变化；不要使用 submodule `--remote`。解包 C14 已含依赖，不需要再把它们替换成 submodule checkout。

storage 是独立产品目标，不能用 storage-only 成功代替 full/C14 的编译或验收。

## 4. 本次真正验证的环境与平台边界

C14 的 full normal、ASan/UBSan、业务、V1、矩阵和消费者验收来自 **Windows 主机上的 WSL2 Ubuntu-22.04 / Linux x86_64** 固定 profile。它不是裸机 Linux、Windows 原生 C14 full 或 macOS C14 通过证明。

实际 profile 小文件：`/root/xoffset-serialization-comparison/20261017/p1-v1/tools/p2996-profile/toolchain.json`，名称 `linux-wsl2-current-complete-20261016`：

- 编译器 `/opt/p2996-toolchain/bin/clang++`；Bloomberg `clang-p2996` commit `9ffb96e3ce362289008e14ad2a79a249f58aa90a`；target `x86_64-unknown-linux-gnu`。
- 编译器记录 SHA256 `c7fa55437d34c43201da37960251d7d3dab1e06fc92273efe6f6d88f6dae7c6e`；实际 profile wrapper SHA256 `b970d582c872afbdc6095c56ad235c5f8cf65245ac53b0a51194d6b46fd27ee8`。
- 匹配 libc++/libc++abi 位于工具链目录。sanitizer resource 是另行准备的兼容 Clang 21 resource；其来源包记录 SHA256 `d6f84c34953b0404ec9648ba7364004a82454f79cc447f67894b9a27317ef6ce`。
- wrapper 使用独立提取的 host runtime 和 `ld-linux-x86-64.so.2`；没有替换系统库。路径在该 profile 下的 `host-runtime/`、`sanitizer-resource/`，不能在新机器直接照抄旧绝对路径或混用系统 sanitizer ABI。

**重要差别：**包内 `toolchains/vm-validation.json` 和 `Dockerfile` 保留了历史 Linux 镜像/compiler commit `a5270822...` 及历史 macOS 配方。它们不是本次 C14 实际 `9ffb96e3...` profile 的同一身份，也不代表已在对应平台执行 C14。新机器应另建真实环境记录，不修改历史 profile 冒充复现。

原 HANDOFF 记录的 Windows 原生 storage 2 项、installed 3 项、subproject 3 项，以及早期 macOS、WSL、TSan 结果，属于各自旧源码/旧 profile。Windows 可见归档复制成功不等于 Windows 原生 full/P2996 测试。本次 `final-acceptance-v6.json` 明确 `Windows_native_C_full_P2996_tested_here=false`。

C14 当前固定环境实际已通过：full normal 73、ASan/UBSan 73；业务 native normal/SAN 各 80、主 V1 84 及相关跨读；production matrix 52 个子进程；标准/共享 runtime 消费者及 clean archive closure。新机器上的完整性检查成功只确认文件身份，不能继承为新平台 native 或性能结果。

## 5. 迁移时不能直接复用的状态

1. 不复用旧 CMake cache、编译数据库中的绝对路径、install prefix、可执行文件、共享 runtime ABI 或 sanitizer 状态。后续获准恢复时，用新目录、新 profile 和新结果记录；保留旧结果。
2. 旧 `/root/...`、`/mnt/c/...`、`/Users/...`、WSL UNC 路径和旧会话/PID/队列仅是证据地址。新机器不能据此控制旧进程、恢复旧队列或推定服务/认证存在；不得删除旧全局锁文件来制造空闲状态。
3. C14 保持冻结。cursor、allocation、bitmap、scoped-specialized 等后续候选的准备、正确性或有限 screen 不能自动并入 C14；当前阶段记录为三个独立 screen 关闭且未提升默认方案。
4. 总体性能目标未完成，新 A/A 资格未建立；旧 Q32 29/30 关闭旧 F64，不能重开旧窗口或补样。历史 process-duration maxima 提取 v2 完成但尚待审阅，不能视为新的测量资格。
5. N7 真实业务入口仍为用户已说明的不可用前提。合成业务/模型化 transport 与生命周期矩阵不等于已完成真实业务接入或某个实际异步网络系统。
6. 不将 ASan/UBSan 成功包装为任意 C++ 类型的普遍生命周期证明；保留 C14 的受审类型域、显式 custom 契约、平台 ABI 和 V1/layout 限制。
7. 当前暂停记录称进程已恢复、租约空闲且无暂停后新实验。用户恢复前，只能继续获授权的交接整理，不自动构建、安装、测量、提交、推送、合并、发布或触发远端 CI。

## 6. 本记录的证据范围

主要读取：原 `HANDOFF.md`；交付 `result.json`/`delivery-manifest.json`；`final-acceptance-v6.json`；`pause-state-20261019-v1.json`；`current-stage-v15.json`；解码 C14 的 CMake/依赖锁/历史 toolchain 配方；实际 WSL profile 小文件。另只读计算了根 main、解码 C14 main、交付 result/manifest 的 SHA256。未扫描或搬运巨大性能正文，没有运行环境探测程序或任何构建/测试。
