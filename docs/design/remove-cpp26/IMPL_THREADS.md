# 移除 C++26 依赖：三个实现线程的指令

2026-10-26 · 设计线程「设计移除 C++26 依赖的方案」写给协调者 · 三个线程都用 high

依据：susu 10-26 16:36Z 回复“全按推荐”（cmsg_01W8h2niLCspyDhp6DxMomYu7uM5WN3n996qrKREsUjMtJ），D1–D6 都选推荐项。设计文档：`/mnt/project-files/remove-cpp26-20261026/DESIGN_REMOVE_CPP26_20261026.md`（第 4、7、9 节）；Claude Doc https://claude.ai/artifact/9sJMHH8pFW9JQN6oepaECD 。

## 0. 依赖顺序（总览）

```
现在 ──┬─ A p0→p1→p2→p3→p4→p5（串行，叠在一起的分支）
       ├─ B b1 CMake 标准选项、b2 local_validate 新列、b3 常量求值步数（不依赖 A）
       └─ C 摸环境（不编库）
A p4 出来 ── B 用 A 的分支试 C++20 整库（不含 <meta>、不含 TypeLayout）
A p5 出来 ── B b4 生成测试 RECORD 行、b5 wire 语料、b6 Ir、b7 编译时间
B 的 C++20 列在云端全绿 ── C 跑各平台
INT3 合入 next_cpp26 ── 按 A p0…p5 → B → C 的顺序合（每合一个，在新的 next_cpp26 上重做证据）
A、B 合完 ── 协调者请产品定位线程做阶段 5（K0/K1/K7/K8、第 2 节“限制”、README、USER_GUIDE、CLAUDE.md）
```

## 1. 三个线程共用的约束（每个线程的指令里都要带上）

- **不影响现有开发**：INT3（性能优化的下一个集成批次，含 #87、#93）合入 next_cpp26 之前，不往 next_cpp26 合任何东西（D4）。PR 可以先以草稿打开。不动在途 PR 和它们的分支（#87、#93、#40、#44、#47、#56 等），冲突只在自己的分支上解。INT3 是否已合入，以 `git log origin/next_cpp26` 里 #87/#93 的合并提交为准，拿不准就问协调者。
- **自动执行**（susu 16:54Z，cmsg_01W8h2niLCspyDhp6DxMomYu9pZUUpknta2bAxHCSdRBpY：“所有会话都按推荐来进行，不需要我确认，自动执行，直到完成会话目的。”）：设计层面的选择直接按推荐项定，写进 PR 正文或报告，不停下来等确认。只有必须 susu 本人动手的事（例如在 Mac 上装 Xcode、登录 Apple ID）才去问。
- **合并授权**：susu 16:36Z 的“全按推荐”就是 D4 的批准，即 INT3 后分批合。权限检查拦合并时，附上这条消息；还拦就告诉协调者是哪个线程，不要换路绕过。
- **0 类判据（MERGE_RULES_V2，sha256 `62d178d46830c3ecc3625689adbebc04a2626616664d1b29f22e4c1c89f55dfc`，第 0 类第 16 行）**：在云端 GCC 16 和 P2996 d77eff1 两种编译器下，正式 harness、bench/slg、bench/reloc 每个函数的反汇编逐条相同（比较时忽略地址和符号名），PR 里要附两种编译器的比对脚本输出。.text 里函数的顺序和大小也相同，直接算 0 类；只是摆放（顺序或大小）变了，仍算 0 类，但要在 PR 正文里写明，并记进性能优化的 ledger，作为下一次阶段复核的单独二分点（起因：INT1 里 map.insert 指令完全相同，只是对齐变了，Mac 上就慢了 6–8%）。Mac 在下一次阶段复核时顺带核对二进制。纯 Linux 的 CMake、测试、工具改动按原有 0 类（Apple 头文件文本逐字节不变）。
- **验证**：没有 GitHub CI，用 `tools/local_validate.sh`（GCC 16 全量 ctest、ASan/UBSan、fuzz、差分、smoke、Ir、P2996 ctest 和签名、P2996 Ir）。P2996 全量 ctest 必须全过（当前 98/98）。云端 P2996 工具链在 `/mnt/project-files/toolchains/`。dockerd 要手动起（`nohup dockerd &`），GCC 16 镜像 digest 在 `local_validate.sh` 里。
- **wire 不变**：不升 wire 版本；`test_v2_golden`、`test_cross_compiler_wire` 的语料逐字节不变；`tools/sigs` 签名基线不变；schema 哈希值不变。
- **性能优先**：任何构建模式的性能退化都要量化报告，不能静默降级；性能宣称以 C++23 构建为准（D1）。
- **其他硬约束**：不保留 V1 兼容；公开 API 可以改（`XOFFSET_ORDINARY_RECORDS` 在 C++26 下保留）；不能要求用户加 `-fconstexpr-steps`；不写 `f<[:x:]>()`；TypeLayout 头文件是 CRLF，按字节改；Mac 队列只放决定合并的批次（susu 04:23Z 精简规则），这三个线程不排计时批次。
- **可复用的探针**：`/mnt/project-files/remove-cpp26-20261026/probes/`（分支 `claude/remove-cpp26-design-20261026` 的 `docs/design/remove-cpp26/probes/` 同一份）。`xo_describe.hpp` 是字段表后端的起点，`cmp_asm2.py` 是反汇编比对脚本的起点，`probe_dump.cpp` 是 describe_dump 的起点，`run_probes.sh` 一键复现。
- **交付文件**放在 `/mnt/project-files/remove-cpp26-20261026/<线程子目录>/`；设计层面的选择按推荐项直接定（见下条“自动执行”）。

---

## 线程 A：「重构 schema 接口」（阶段 1、2）

**目标**：库里所有反射查询改走一层 schema 字段接口，接口下挂反射和字段表两个后端；C++26 构建的机器码不变。

**分支**：从 next_cpp26 当前 tip（`7620bc3`）起，6 个叠在一起的 PR 分支 `claude/cpp20-schema-p0-20261026` … `claude/cpp20-schema-p5-20261026`，每个基于前一个。INT3 合入后整体 rebase 到新的 next_cpp26。

**PR 拆分**（按顺序）：
- **p0 比对工具**：把 `probes/cmp_asm2.py` 做成仓库工具（例如 `tools/perf_gate/asm_equal.py`）：输入两组构建，按函数 demangle 后比较反汇编，忽略地址、符号名和调用目标的名字，另外单独报告 .text 里函数的顺序和大小是否相同。在 GCC 16 和 P2996 下各自跑 next_cpp26 对自己，结果必须是“全部相同、摆放相同”（自检）。后面每个 PR 都用它出 0 类证据。
- **p1 接口层**：新增 `xoffset_schema.hpp`（`member_count_v`、`member_t<T, I>` 的 type/offset/get、`base_count_v`、`base_t`、`is_described_v`、`enum_range`、`implicit_lifetime_aggregate`、`only_implicit_special_members`）和 `xoffset_schema_reflect.hpp`；`xoffset_core_lifetime.hpp`（7 个函数）和 `xoffset_snapshot_index.hpp`（2 个）改走接口。
- **p2**：`xoffsetdatastructure.hpp` 的 28 个函数改走接口（函数清单见设计文档附录 A）。
- **p3**：`receive_raw.hpp`（12 个）和 `xoffset_verified_paths.hpp`（7 个）改走接口。合完 p3 后，反射后端文件之外不再出现 `std::meta`、`^^`、`[:`。
- **p4 准入和 TypeLayout**（D3）：K0 准入改由 XOffset 按字段接口自己算，规则与 TypeLayout `is_byte_copy_safe_v` 相同；`FixedString`、`to_fixed_string` 搬进 XOffset；核心头文件不再 include TypeLayout。在 C++26 构建里加一个交叉测试：对 `tools/signature_type_list.hpp` 里的全部类型，XOffset 的准入结论与 TypeLayout 一致。TypeLayout 留给 `tools/sigs`、`check_compat` 这些 C++26 签名工具。
- **p5 字段表后端**：`xoffset_schema_describe.hpp`，提供 `XOFFSET_RECORD(T, 字段...)` 和 `XOFFSET_RECORD_DERIVED(T, (基类...), 字段...)`；完整性证明（偏移严格递增且不重叠，加聚合体初始化计数，见设计 3.1.1；只查 sizeof 不够）；库自带描述（`XKeyValue<K, V>`、`std::array<E, N>`、库内部记录）；`XOFFSET_SCHEMA_BACKEND=auto|describe`；C++26 下写了 RECORD 的类型仍用反射，只加静态断言要求两者一致；C++20 下的 enum 规则（要求固定底层类型，报错给出改法）和隐式生存期承诺（有 `std::is_implicit_lifetime` 或 `__builtin_is_implicit_lifetime` 就用）；负例编译测试（探针 NEG=1..5，加上“没有固定底层类型的 enum”）；`tools/describe_dump`（在 GCC 16 或 P2996 上用反射印出可达记录的 RECORD 行）。

**每个 PR 的验收**：
1. `tools/local_validate.sh` 全过，包括 P2996 全量 ctest 和签名不变。
2. 0 类证据：p0 的工具在 GCC 16 和 P2996 下比较 PR 前后的正式 harness、bench/slg、bench/reloc，每个函数反汇编逐条相同。比对输出贴进 PR。摆放变了的，写进 PR 正文和 ledger，作为二分点。
3. wire 语料、`tools/sigs` 基线、schema 哈希都不变。

**p5 额外的验收**：
- M26D（同一编译器、C++26、强制 `describe`）与 M26 的反汇编逐条相同，GCC 16 和 P2996 都要，三个基准程序都要。
- M26D 下全量 ctest 通过。
- 负例都按预期编译失败，报错指出类型和原因。
- `describe_dump` 对 SLG 的输出与提交的 13 行逐字相同。

**完成标准**：p0–p5 在 INT3 之后按顺序合入 next_cpp26，每个都附证据；摆放变化的二分点已记进 ledger。如果 p0–p5 都做完了 INT3 还没合，就把 PR 停在草稿、证据就绪的状态，订阅 PR，等 INT3。

**并行协作**：B 会在你的分支上试 C++20 构建。B 报来的问题，比如常量求值步数超限要改成流式哈希，由你在 A 的 PR 里改。

---

## 线程 B：「搭建 C++20 构建与测试矩阵」（阶段 3）

**目标**：库在 C++20 和 C++23 下用 GCC、Clang 整库构建，全量测试通过，wire 与 C++26 构建逐字节相同，并量化性能和编译时间。

**分支**：`claude/cpp20-build-20261026`，先从 next_cpp26（`7620bc3`）起做 b1–b3。需要 A 的代码时 rebase 到 A 的 p4、p5 分支上验证。PR 分支是 `claude/cpp20-build-pN-20261026`。

**现在就能做（不依赖 A）**：
- **b1 CMake**：加 `XOFFSET_CXX_STANDARD`，取值 20、23、26。本仓库默认仍是 26，现有门禁不变。20 和 23 用字段表后端：不加 `-freflection`，不包含 `<meta>`，核心目标不加 TypeLayout 的 include 路径。只有 C++26 才建的目标：`tools/sigs`、`check_compat`、TypeLayout 交叉测试、零样板相关测试。接口目标在 MSVC 下自动加 `/Zc:preprocessor`。
- **b2 local_validate 新列**：`gcc13-c++20`（容器自带 GCC 13.3）、`clang18-c++20`（容器自带 Clang 18.1）、`gcc16-c++20`、`gcc16-c++23`、`p2996-c++20`。每列跑全量 ctest；gcc13 和 clang18 加 ASan/UBSan；至少一列跑差分 fuzz。各列并行跑。缺环境的列报“没有运行”，不能静默跳过。
- **b3 常量求值步数**：用探针在 GCC 12、GCC 13、Clang 16、Clang 18（docker 镜像）的默认步数上限下，算 SLG 最大的 BattleMsg、PlayerState 的签名和哈希（签名约 1.6 KB）。超限就请 A 把签名哈希改成流式 FNV-1a，不拼接字符串，哈希值不变，也不加编译参数。结论定下编译器下限（设计初定 GCC 12+、Clang 16+）。

**A 的 p5 出来后**：
- **b4 测试树**：用 `describe_dump` 为 tests、bench、examples 生成 RECORD 行，只给真正用到的记录，不手写。`tests/test_receive_raw.cpp` 里没有固定底层类型的 enum（Nonfixed、Negative、Zero、EmptyEnum、Wide、UnsignedWide）测的正是反射求取值范围，这些用例只在 C++26 下跑；C++20 下改为负例编译测试，确认报错清楚。golden 和跨编译器语料的类型一个字节都不改。如果发现有语料类型在 C++20 下描述不了，停下来报给协调者，不要改类型。
- **b5 wire**：每列写出的 golden 和跨编译器语料与 M26（GCC 16）参照逐字节相同，每列都能 `load_verified` 其他每一列的语料，即把 `test_cross_compiler_wire` 扩成矩阵。
- **b6 性能**：M23D（GCC 16，C++23，字段表）对 M26 跑云端 Ir 门禁，按 S 类阈值（合计 ≤1.003，占比 ≥5% 的阶段 ≤1.02）。GCC 和 P2996 都要不出警告；云端 P2996 Ir 还没就绪的话，先报 GCC 的结果，并注明 P2996 未跑。M20D：输出路径的 `resize()` 清零单独计量，写成一个数字进文档。
- **b7 编译时间**：SLG 和正式 harness 在 M26、M26D、M23D、M20D 下的编译时间表，只记录，不设门禁。

**完成标准**：
- 五列全量 ctest 通过；gcc13 和 clang18 的 ASan/UBSan 通过；差分 fuzz 通过。
- wire 语料逐字节相同，并且互相能载入。
- M23D 的 Ir 不出警告；M20D 的差异已量化。
- 编译器下限和编译时间写进 `/mnt/project-files/remove-cpp26-20261026/build-matrix/` 的报告。
- 这些 PR 按 0 类在 INT3 之后、A 对应的 PR 合入之后合入；如果 INT3 还没合，就停在草稿、证据就绪。

---

## 线程 C：「验证原生客户端工具链」（阶段 4）

**目标**：落实 D5（客户端改用各平台原生编译器）：在 Apple clang、Android NDK、MinGW GCC 上用 C++20 构建并验证 wire 一致；更新 `docs/CLIENT_TARGETS.md`。MSVC 按 D6 不做，标“未验证”。

**分支**：`claude/native-toolchains-20261026`，放在 B 的分支上面。改动以工具链文件、CMake 预设和 `docs/CLIENT_TARGETS.md` 为主。

**依赖**：B 的 C++20 列在云端全绿之后才跑各平台。在那之前只做环境准备。

**现在就做（环境准备，不编库）**：
- 查 Mac 上的 Apple clang 版本（Command Line Tools）、有没有 Xcode 和 iOS SDK（`xcode-select -p`、`xcrun --sdk iphonesimulator --show-sdk-path`），以及剩余磁盘。10-21 的结论是没有 Xcode，10-26 磁盘只剩约 15 GB。
- 在云端准备 Android NDK（r27 LTS 或更新）和 MinGW-w64 GCC（能用 16 最好，至少 13）的 docker 环境。网络被拦时按环境文档处理。

**各目标与验收**（每个目标都要写清实际跑了什么：ctest、只编译，还是被阻塞以及原因）：
1. **macOS / Apple clang**（Mac M4，原生）：C++20 和 C++23 构建，跑全量 ctest。不计时，不需要静音门。
   - 查清 `std::pmr` 在 Apple 系统 libc++ 上的最低部署版本：用 `-mmacosx-version-min` 从低往高试编译和链接。
   - 用 Linux x86-64 的参照语料核对 wire，要逐字节相同、互相能载入。签名要与 `tools/sigs` 基线一致。
   - 走 Mac 队列（`tools/perf_gate/mac_queue.sh add-batch`）时排成不计时的批次，放在决定合并的门禁批次后面，不能插进计时步骤之间。排之前请协调者和性能优化确认位置。给 Mac 的文件写清路径和完整 sha256。
2. **iOS 模拟器**：需要 Xcode 和 iOS SDK。如果 Mac 上没有，装 Xcode 是 susu 的操作（体积大，要 Apple ID，还要先腾磁盘）。这一步只能 susu 本人做：经协调者请 susu 装一次（附推荐和需要腾出的磁盘），不要因此停下其他目标。装好之后：构建，用 `xcrun simctl spawn` 跑 ctest，核对 wire 语料。
3. **Android arm64-v8a**（NDK Clang）：在云端交叉编译 C++20。
   - 能跑就跑：静态链接后用 qemu-aarch64 用户态跑，或在有 KVM 时用模拟器；都不行就只编译和链接，并写明“未运行”。
   - 记录 NDK libc++ 的 `std::pmr` 是否可用。
4. **Windows x64**（MinGW-w64 GCC）：在云端交叉编译 C++20；有 wine 就跑 ctest，没有就只编译，并写明。
5. **MSVC**：按 D6 不做，在 `CLIENT_TARGETS.md` 里标“未验证”，写明 `/Zc:preprocessor` 和常量求值步数这两个已知风险。

**`std::pmr` 的处理**：如果某个平台不可用，或部署版本门槛过高，就出一个建议（把 `std::pmr` 接口改成可选，或者规定最低部署版本），交给协调者和 A 定。这是库头文件改动，可能不再属于 0 类，不要自己直接改。

**完成标准**：
- `docs/CLIENT_TARGETS.md` 改成按目标逐行列出：编译器、标准、实际跑了什么、结果。Android 和 iOS 的“不可行”改成实测状态。
- 所有实际运行过的目标，wire 语料都逐字节相同。
- `std::pmr` 的结论和建议写进 `/mnt/project-files/remove-cpp26-20261026/native/` 的报告。
- 还剩的阻塞写清由谁来做，比如装 Xcode。
- PR（文档和工具链文件，0 类）在 INT3 之后、B 合入之后合入；INT3 还没合时，停在草稿、证据就绪。
- 把 `CLIENT_TARGETS.md` 的事实交给协调者，转给产品定位线程做阶段 5。
