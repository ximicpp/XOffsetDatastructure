# 性能状态与跨机器接续边界

状态截点：2026-10-19 12:01:57 UTC（北京时间20:01:57）。**开发已按用户要求暂停，本文件仅整理交接，不授权恢复构建、实验、计时或旧队列。**

以 `receive-completion-20261019/pause-state-20261019-v1.json`、`current-stage-v15.json` 和各候选最新 decision/result 为准。`ACCEPTANCE_20261019.md` 中 C14 验收与早期研究说明仍可参考，但其中“尚未检查allocation机器码/尚未计时”等早期段落已经被后来的 decision/result 取代，不能当作当前待办。

## 当前默认交付及未完成目标

保留已经冻结的 C14/final-v4 交付，14个生产文件。主头 SHA256：

`66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3`

旧机器 Windows 可见等字节源码位于项目的
`build/receive-completion-20261019/delivery-v2/review-source/xoffsetdatastructure-0.1.0-alpha.1/`；源码 ZIP SHA256：

`3ddeb05afa78f566d4673d4ca19933a74f29edb45f1250be30bf69d34b7495e4`

这份交付已取得旧机器固定 Linux/WSL P2996/libc++ 配置下的 normal73/SAN73、业务/V1、生命周期与包消费验收。它不等于新机器平台测试，也不等于 Windows 原生 full/C、所有自定义类型的通用 ISO 证明或真实 N7 网络整合。用户确认暂无真实业务入口，N7仍开放。

性能目标尚未完成；`AA_status=not_established`，新的五库正式比较次数为0。原目标保持：read和mutate相对每个竞品耗时倍率≤1.05，旧demo独立拥有输出倍率≤0.20，reload保持完整校验。容量复用、冻结lease或独立生产模型不能替代旧 owning emit 来完成该目标。

## 已封存的内部筛选

以下都是 Candidate/Control 的配对几何耗时比；低于1只表示点估计较小。它们是独立候选相对自身控制的内部筛选，**不是相对四竞品的验收或排名**，也不自动代表 C14 整体实现。每个窗口实际72对/144子进程，AB/BA各36对，无删点、补样、替换或重测；完整原始产物离线审计均有旧机器通过记录。本次交接只读取小型分析/审计结果，没有重读全部大body。

主目标使用预先固定的单侧联合族95%上界（df71，critical t=2.289930854179051）。原规则要求上界<1；未执行候选的名额/alpha不回收。

| 候选与主目标 | 比值点估计 | 单侧联合上界 | 已封存决定 |
|---|---:|---:|---|
| cursor dominance / mutate（早期独立窗口） | 1.0030229611616686 | 1.0274895908232469 | 不晋升 |
| allocation witness / mutate | 0.998606591160608 | 1.0257485832238138 | 不晋升，不重测 |
| bitmap修订 / load | 1.034152838288296 | 1.0738664187046698 | 不晋升，两项主目标均未通过 |
| bitmap修订 / reload | 1.014965651773676 | 1.0384454897957944 | 同上，不重测 |
| scoped-specialized / mutate | 1.0256183714177964 | 1.062446358156947 | 不晋升，不重测 |

其中后续三项窄优化均已实际执行，而不是只做注册或准备。它们的 `screen.internal_direction_evidence` 与 `eligible_for_followup_not_promoted` 均为false。三个后续窗口合计432个实际计时子进程；早期cursor窗口另144个。不要把这些独立窗口合并成一个大样本或挑选其中较快阶段。

总量属于预先固定36项辅助比较族的双侧95%区间（critical t=3.3279351330195457）：

| 候选 | total点估计 | 辅助联合区间 |
|---|---:|---|
| cursor dominance | 0.9920379133483952 | [0.9691667429532159, 1.0154488158784714] |
| allocation witness | 0.9950153818757574 | [0.9703475340769796, 1.0203103273829894] |
| bitmap修订 | 1.0115782876497912 | [0.9841486708663433, 1.0397724066869733] |
| scoped-specialized | 1.0166806934545995 | [0.9839195854737195, 1.0505326326497175] |

四项均未触发已确认非目标/总量回退否决；这不证明没有成本转移，也不证明等价。主目标未通过不能被较小的total点估计覆盖。不得把区间跨1解释成“确定没有收益”“确定退化”，也不得把差异事后全部归因于噪声、缓存或某条指令。

各候选的真实机制与局限：

* Cursor：旧M实际1245次跨请求拒绝区间跳过，节点访问75572→67846，维护比较1055342→1046983；节点减少全部来自第8修改阶段。动态申请53248、owner入口98304及复制量未减少，新增见证存储/分支有成本。计数减少没有变成已确认耗时改善。
* Allocation：机制诊断实际fast commit16383、fallback1；identity-pin入口16384→1、Core通用入口16385→2，动态请求53248、owner入口98304、host申请调用2060均不变。后续bare代码检查确见原子引用操作绕过，同时存在outlined helper及额外guard成本；因此“尚未检查机器码/尚未计时”已经过时。最终72对仍未证明mutate提速，未合入C14。
* Bitmap修订：源store事件load504207→269267、reload576636→338740，但实际load/reload耗时主目标均未确认改善。它与早先被机制否决的cell bitmap v1不是同一个候选，不能混用结果。
* Scoped-specialized：实际hot owner查询98304→94208（减少4096），NoBorrow代码/栈帧证据保留；最终耗时仍未通过，不晋升。

其他已关闭研究也不得恢复原窗口：cell bitmap v1虽normal/SAN各13项通过且旧M全cell覆盖，但数据word-touch在load367487→404114、reload403719→686176，按机制门槛取消，实际计时0，预算不转移。旧四路径8字节内部对齐与CheckedRange各自唯一72对未证明自改善；共享owned/free控制区的配额/OOM回退缺口、透明mutable locator的祖先拥有关系反例仍有效。见 `docs/PERFORMANCE_FOUR_PATH_SELECTION.md`，不要以重命名方式重试同一已否决方案。

## 历史A/A及进程时长提取的最新状态

旧Q32：4656个实际进程=2328 primary+2328 conditioner；32个macrocycle，24个primary warmup保留但不用于推断，primary推断行2304。30项阶段A/A仅29项建立等价；同一cereal二进制的chronological first/second mutate比值1.005199，联合区间[0.954965,1.058075]超出允许区间[0.952381,1.05]。这说明此次精度不足以建立要求的等价，不能推断cereal真实退化。旧F64实际0，旧Q/F已关闭。

`AA-runtime-maxima-v2` 已在暂停前完成：5个纯工具测试通过，提取状态 `passed_metadata_extraction`，新native执行0、新测量样本0。它逐个读取旧4656份成功进程receipt并匹配封存调度/manifest，提取的指标是
`finished_monotonic_ns - started_monotonic_ns`，**是完整进程区间，不是native.total或某个phase**。

包含全部388个C及388个P观测/角色（含各4个warmup）的历史最大秒数：

| 旧角色标签 | conditioner最大秒 | primary最大秒 |
|---|---:|---:|
| old_xoffset | 0.433105700 | 0.425475918 |
| new_xoffset | 0.421542173 | 0.421037871 |
| cista | 0.372833111 | 0.381292848 |
| flatbuffers | 0.419277980 | 0.423875311 |
| capnproto | 0.522507271 | 0.522050879 |
| cereal | 0.420542075 | 0.372945779 |

旧Q整体队列墙钟为2137.8921854050004秒。上述最大值仅是旧配置的预算设计输入，不是新机器时长保证。旧 `new_xoffset` 标签不是将来C14候选的替代名称；新候选需要自身实际固定M capture边界，不能挑一个历史X标签来当将来预算。

提取结果的 `planning_only` 写明324×五个选定角色(maxC+maxP)+300秒只是待研究公式，`future_budget_fit_computed=false`、`future_duration_guaranteed=false`、`new_AA_qualified=false`。**尚未冻结或执行新A/A资格方案，也不能据此承诺新窗口完成时刻。**

原提取v1因错误地用wire-only预算比较cereal而失败；正确原公式是max(wire,canonical,queries,1MiB)，cereal的canonical恰好多8字节。v2只修正元数据预算核对并输出新目录，原失败和旧Q正文未改。它没有重读native JSON、wire/canonical/query正文、SDK/runtime或body pool；旧完整body审计只是引用，不是此次重做。

结果定位及身份（旧机器引用）：

* `/root/xoffset-receive-completion/20261019/v1/AA-runtime-maxima-dispatch-v2/result.json`：passed，pure_tests5，native_executions0，new_measurement_samples0。
* `/root/xoffset-receive-completion/20261019/v1/new-AA-oldQ-process-maxima-v2/result.json`：3376551字节，SHA256 `642a17384f0a8472027acdbf4c7303db8f66f50266a6cdca9b01d6f3c323e7ab`。该文件含大量receipt来源索引；本交接仅保留上面的小型摘要，除非实际bundle清单另行列入完整文件。

## 恢复后下一阶段

暂停记录显示无挂起进程，之前临时暂停的进程已恢复、全局租约空闲、全局锁文件保留，暂停后未开始新构建/实验。旧机器PID、进程句柄、电源恢复状态和租约不能当新机器的现状或直接沿用。

在用户明确恢复工作后：先按实际交接清单核对默认C14与小型证据身份，确认新机器工具链/依赖/平台；交接完整性检查不等于平台测试。随后审阅上述进程时长提取和测量约束，为保留默认实现及明确的受控宿主条件**另行事前设计独立A/A资格方案**。只有新资格所有要求通过，才考虑新的单窗口五库正式比较。

不得重开旧Q32/F64或任何已关闭候选窗口，不追加旧样本、不删离群值、不择快值、不放宽界值、不回收取消候选的样本/alpha预算。不得把新平台或新宿主条件当作重写旧结论的理由。若新方案需要新的候选/条件/预算，应独立登记并保持原证据不变；此交接文件不授予执行权限。

## 可迁移证据索引与旧路径限制

以下为仓库相对定位键；是否随包必须以实际bundle manifest为准：

* `docs/coordination/receive-completion-20261019/pause-state-20261019-v1.json`
* `docs/coordination/receive-completion-20261019/current-stage-v15.json`
* 同目录 `cursor-window-v3-final-acceptance.json`、`allocation-screen-decision-v1.json`、`bitmap-effect-decision-v1.json`、`scoped-specialized-effect-decision-v1.json`
* 同目录 `AA-runtime-maxima-v2-derivation.json`、`AA-runtime-maxima-v2-receipt.json`
* `build/receive-completion-20261019/allocation-screen-result-v1/`、`bitmap-effect-results-v1/`、`scoped-specialized-effect-results-v1/` 内的小型analysis/audit/acceptance等；对应analysis SHA256依次为 `9808b76cf63a273012cb74db35eeb2d75694f18a70a8af6732c01e8d9deefa5e`、`3ff3ec8c180dd1529e52274dfe194305d99086376a1bdfb73b9f4f79ec480350`、`273fea38e609eca944f06066b689f760a25558f5ec3ddf1ac80dc30b7a352faa`。

所有 `/root/...`、`/mnt/c/Users/gzsufanchen/...` 和 `C:\Users\gzsufanchen\...` 均是**旧机器历史位置**，不能作为新机可直接执行的路径。不得恢复旧runner/队列或照旧PID操作进程。原始wire、canonical/query正文、数GiB窗口body、旧SDK/二进制和完整receipt集合若未列入bundle，只能标作“引用，未随包转移”；哈希与旧审计passed不能替代缺失原件，不能宣称新机已完整重审或已复现旧平台性能。

本文件只读取现有小型decision/analysis/audit及历史receipt提取元数据并整理摘要；没有运行项目、构建/native测试、取得租约、新建性能样本或修改历史证据。未提交、推送、合并、发布或触发远端CI。
