# G4 适用验证矩阵：准备清单

状态：准备审查；**未下发 G4，未执行本清单中的构建、测试或容器任务**。
日期：2026-10-15。此文件不是最终交付报告。既有可靠结果按完整相关输入身份评估，
相同身份的证据不为 G4 名称重跑。逐轴判断见 `identity-mapping.json` / `identity-notes.md`。

当前建议的无条件基础 C++ 新任务为 **0**。可继承相关闭包不变的原生 full 48（含签名
3 项）、wire 504、原生 full/storage package 各 installed 3 + subproject 3，保留原来的
profile 和来源链限制。旧 TSan/storage/sanitizer 入口差异与 Linux package 外层 image
字段缺口先补既有证据或交统筹裁决，不自动转成新跑。

只有明确需要“当前普通 Linux 48”主张时，缺的 allocator 两项普通配置才是条件增量；
原普通 46 加 sanitizer 2 不能混算。新增 W1 网络源码 sanitizer 是另一项条件范围，已知
Apple ASan ABI 失败不重复。Docker 只保留设计，不为元数据缺口先造执行器。

## 冻结与启动条件

由统筹确认采用决策、W1/W2/W3 冻结身份及 G4 范围后，才创建新的 G4 归档。
HEAD 不是当前 F；必须逐文件选取并记录 SHA-256。最终 selected surface 包括：

- 两个生产头、CMake/cmake/工具/测试/消费者/示例及实际参与验证的依赖与许可证。
- 被采用的 W1/W2/W3 工具、生成规则和生成头、contract、预登记、审计器、决策依据。
- 本机 runner、其验证记录、平台探测器及结果；若另行接受 Docker adapter，也固定它的实现与验收。
- 各角色当时的 handoff/状态文件的不可变副本；活跃状态文件随后允许更新。
- 实际命令、工具链/SDK/runtime/输入数据身份、生成源码和二进制身份、精确 case 集及原始结果。
- 每个全局 OpenSpec 条目的交付/待项对应关系。真实消费者、原生平台及噪声前提缺失仍开放。

`inventory.json` 保存本次只读展开的当前预期 case 数组及输入哈希，`inventory-notes.md`
说明来源。它们是候选清单；G4 必须从冻结源码生成实际 CMake/CTest inventory 并逐项比较，
不能只比较总数。W1/W2/W3 当前尚在工作时，其列表只代表该时点的契约快照。

所有本机重型步骤由已接受的 `experiments/vm_coordination/run_job.py` 使用默认
`build/vm-coordination/queue.lock` 执行。并行编译最多 2；不嵌套同锁，不自定义锁。
正式计时由 W3 单独安排，本清单不重启候选筛选或用功能验证产生性能结论。

## 路径与精确入口约定

下列大写名称是统筹在 G4 清单中必须绑定的**绝对路径参数**，不是自动发现的输入。
每条命令使用新的 receipt 和新的 output。例中命令全部为准备模板，尚未执行。

| 参数 | 绑定要求 |
| --- | --- |
| `SELECTED_ARCHIVE` / `SELECTED_MANIFEST` | G4 新冻结源码包及逐文件 sha256/bytes 清单；与 Git HEAD 分开 |
| `VALIDATOR` / `PACKAGE_VALIDATOR` | G4 冻结工具副本中的 `tools/validate_vm.py` / `tools/validate_vm_package.py`，均绑定哈希 |
| `PACKAGE_SOURCE` | 从该包准备的独立消费输入，加入已核对的依赖；不修改冻结归档 |
| `DEPS` | 冻结 Boost/TypeLayout 外部依赖根及许可证；提交和内容清单均匹配 |
| `COMPILER` | `/usr/local/bin/clang++` 的已核对 P2996 实体；不得换为系统 Apple Clang |
| `SDK` | `/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk`；当前默认是 27.0 |
| `LIBRARY` | `/usr/local/lib` 的匹配 libc++/libc++abi；传入配置并核对实际链接 |
| `TSAN_RUNTIME` | `/Library/Developer/CommandLineTools/usr/lib/clang/21/lib/darwin` |
| `MAC_RESOURCE` | 含 builtin include 和匹配 ASan/UBSan runtime 的完整 resource 输入，身份/可用性仍待确认 |
| `WIRE_BUNDLE` | `docs/validation/2026-10-15-vm-followup/wire-inputs-and-versions.tar.gz` 的绝对路径与固定哈希 |
| `RECEIPT` | `build/vm-final/<new-run>/receipts/<matrix-id>`，必须不存在 |
| `OUT` | `build/vm-final/<new-run>/results/<matrix-id>`，必须不存在 |

Receipt 与 validator output 分开：validator 的 staged source/安装审计可能包含有意的依赖
symlink，而共享 runner 的收据只接受普通文件/目录。runner 的递归清单覆盖 receipt 内部；
OUT 必须由 validator 清单、CTest/JUnit、最终归档清单和独立审计共同覆盖。不能把 receipt
通过当作 OUT 的语义通过，也不能在 receipt 封存后继续向其中写入结果。

本机常规、storage、ASan/UBSan 和 TSan 的外层形式相同：

```text
python3 experiments/vm_coordination/run_job.py run --owner W4 --kind validation --out RECEIPT --timeout-seconds APPROVED_TIMEOUT -- python3 VALIDATOR native --source-archive SELECTED_ARCHIVE --source-manifest SELECTED_MANIFEST --deps DEPS --compiler COMPILER --sdk SDK --library-dir LIBRARY --jobs 2 --out OUT [MATRIX_OPTIONS]
```

`MATRIX_OPTIONS` 只取下表列出的 argv，不通过 shell 拼接。storage 可不传 `--deps`；
该层不能依赖完整反射库才成功。`APPROVED_TIMEOUT` 与磁盘/内存预算在 G4 派发时固定，
本次不预先占用资源。单个正式计时窗口不与本清单的验证步骤混跑。

## 适用矩阵与判定

下表入口只是确需补跑时的候选。先比较完整相关源码/工具、工具链/SDK/runtime、flags、
依赖、case 集、实际二进制来源和证据可信性。结果分为 `inherit_same_identity`、
`delta_requires_run`、`identity_evidence_missing`；非参与输入的差异须说明，不强行算同身份。
多个历史运行覆盖的 case 可以按身份逐项归并，但不能改写为一次新的完整运行。

| ID | G4 后入口/附加参数 | 预期检查与范围 | 新跑或继承 |
| --- | --- | --- | --- |
| `mac-storage` | 上述 `native --suite storage --sanitizer none` | 精确两项 storage CTest，OS 页/池层 | 比较既有 storage 或 full 中对应二进制/实际输入；同身份继承 |
| `mac-full` | 上述 `native --suite full --sanitizer none` | 当前完整 runtime/scenario/tooling 数组；configure 检查单列 | 优先比较 StageIII 的当前 48 项；仅实际差异/证据不足需补充 |
| `mac-asan-ubsan` | 当前无合格前提；仅保留未来候选参数 | validator SANITIZED 子集；Apple 不宣称 LSan | W1 已实证 Apple clang21 runtime 与 P2996 ABI 不兼容（v8 / apple_clang_2100）；保留缺口，不重复同失败 |
| `mac-tsan` | 上述 `native --suite full --sanitizer thread --runtime-dir TSAN_RUNTIME` | THREADED target 展开及子进程 case | 比较既有 15 项完整相关输入和二进制来源；同身份继承 |
| `mac-wire` | 下方独立 wire 入口 | preserved frame + 跨历史版本两轮 mutation 的精确操作集 | 比较既有逐操作源码、输入帧与二进制；差异部分才补 |
| `mac-package-storage` | 下方 package 入口，`--suite storage` | installed/subproject 各 3 项 consumer、多 TU；完整组件预期拒绝另计 | 比较完整 producer/export/dependency/consumer/tool 输入；同身份继承 |
| `mac-package-full` | 同上，`--suite full` | installed/subproject 各 3 项 consumer、多 TU、依赖和 relocation 审计 | 同上；不因 G4 名称重跑 |
| `signature` | 已包含在 `mac-full` 的 tooling case | 当前目标再生成、matrix、compat 三个具体 CTest；其他目标 baseline 是静态检查 | 可随同身份 full 结果继承；不重复累计或伪称跨平台执行 |
| `validation-python` | inventory 中列明的 unittest 文件和固定 case ID | 采用的 runner/package/bootstrap/Windows 准备工具负控；receipt/平台离线审计 | 已验收同身份工具结果继承；工具实际变更才补适用项 |
| `new-network` | W1 最终冻结 CLI / smoke / 审计入口 | 精确 case 集、语义与字节、同步 TCP、模拟生命周期分别记录 | 最终构建身份下的新功能证据；可继承相同冻结对象已验收结果 |
| `new-observability` | W2 最终冻结 CLI / event audit | 事件含义/归属/负控、生成头与二进制身份 | 相同冻结对象可明确继承；有变更则重跑适用 case |
| `measurement-audit` | W3 最终离线审计 CLI | 原始 case 集、配对/哈希/语义/噪声与候选决策 | 审计冻结证据；不重新跑到噪声偶然通过 |
| `linux-emulated-full` | **当前无可执行批准入口**；见 Docker 设计提案 | 同一 full case 集、linux/amd64 P2996 + CMake/runtime 输入 | 先比历史覆盖和真实差异；确需补充且 adapter 被接受后才跑 |
| `linux-emulated-sanitizer` | 同上，另需自包含 compiler resource tree | 对应 sanitizer case 数组，Linux leak detection 条件另记 | 先按 case 比历史 26 项及 StageIII 2 项；不能伪称新 28 项运行 |
| `linux-native` | 可达设备、工具链、执行协调另行确认 | OS/full 编译运行与原生性能分别验收 | 已有历史 storage 不冒充本次；当前设备前提未满足 |
| `windows-storage-package` | 既有 `run-windows.ps1 -OutDirectory NEW_RESULT` | 两项 storage + installed/subproject 各 3，精确名称见 inventory | 包字节核验可继承；Windows 原生编译运行仍缺前提 |
| `windows-full-p2996` | 目前无已验证工具链入口 | 反射、依赖、完整 CTest、安装消费须独立建立 | prerequisite_unavailable，不由 storage 包替代 |

Wire 的完整 argv（外层仍使用同一 runner、独立 RECEIPT）：

```text
python3 VALIDATOR wire --source-archive SELECTED_ARCHIVE --source-manifest SELECTED_MANIFEST --deps DEPS --compiler COMPILER --sdk SDK --library-dir LIBRARY --wire-bundle WIRE_BUNDLE --wire-variants v0,v1,d --out OUT
```

Package 的完整 argv（外层同上）：

```text
python3 PACKAGE_VALIDATOR --source-dir PACKAGE_SOURCE --out OUT --suite storage --compiler COMPILER --sdk SDK --library-dir LIBRARY --jobs 2
python3 PACKAGE_VALIDATOR --source-dir PACKAGE_SOURCE --out OUT --suite full --compiler COMPILER --sdk SDK --library-dir LIBRARY --jobs 2
```

`PACKAGE_SOURCE`、工具本身和工具使用的所有源码必须与 G4 manifest 绑定。不能仅把当前
工作区路径交给 package runner 而忽略冻结状态。配置阶段的 reflection 正例、polymorphic
正例/预期拒绝、storage-only 缺 full component 等不计作 CTest runtime 数量。

W1/W2 的执行器可能用所在源码根证明默认队列归属，不能假设把工具移到任意归档目录后
构建入口仍等价。G4 使用其最终契约支持的冻结入口；如必须调用规范位置的工具，则
逐项验证其代码与冻结副本的前后哈希，并保持该阶段唯一写入约束。离线审计的可移动性
与构建入口的资源证明分别核验。

N3 build/collect 的现有资源证明只接受 W2/W3，因此其具体模板以 W2 为执行者；需要
补跑时由统筹派回对应角色。W4 可审核和继承其记录，不通过伪填 owner 绕过限制。

Windows 现有包是第二阶段的 121 文件 selected surface。实际使用前须比较 storage、
CMake/package、consumer 和验证工具的全部输入与 G4 选择，不能只比双头。若相关
输入改变，须在 W4 新目录准备明确身份的新包并复核；不能把旧包原生结果映射到不同
最终打包面。当前原生 Windows 前提仍未满足，也没有授权下载/安装工具链。

## 历史证据的继承边界

N4/N5/N6 已拒绝候选、历史文件筛选与其原始 A/A、原 Windows 包准备测试、既有平台全量
记录均只读保留。它们解释历史决策和兼容性输入，不更名为 G4 新跑。F 双头未变也不能
自动把旧测试清单当作当前清单，尤其后加测试、工具、contract 和新构建身份必须明确。

同一完整相关输入的可靠验证应继承，不为了最终 gate 再执行。若只有证据字段缺失，先
从已保存的源清单、构建命令、CMake/CTest/JUnit 和二进制/归档补出身份；无法补证才
说明最小重跑，不能把所有旧结果一并作废。Docker adapter 是否值得实施，取决于身份
映射后是否确有 Linux 补跑需求；当前仅接受为受限设计储备，不实施。

历史没有保存的被测二进制 SHA、生成输出字节或完整 SDK 字节指纹不能补造。映射应
明确已有 source/runner/command/JUnit 来源链及 profile 边界，再由统筹决定继承；
只读归档核验不是对不存在的可执行文件重新验证，也不是一轮平台测试。

最终报告逐项列 `new_passed`、`inherited_same_identity`、`failed`、
`prerequisite_unavailable` 或 `not_applicable_with_reason`，每项链接具体原始记录。
不能用 aggregate 通过覆盖未运行的平台；不能将 Linux 仿真变成原生性能，或把回环参考
与模型测试变成实际业务 consumer。没有可靠 A/A/代表性输入时，N3/N7 的对应条目保持开放。

## 封存与独立审查

每项要求命令返回码、非空 inventory、case 集精确一致、JUnit 无 skip/重复/缺项、
源码/依赖/工具链/二进制哈希一致；负例必须有指定原因和正例控制。签名基线不得为了
通过而更新。封存后从新位置离线审计并记录不重执行工作负载；全局任务清单和支持矩阵
与真实结果一致。共享 README/CMake/迁移文档仍需统筹单独给出唯一写入窗口。
