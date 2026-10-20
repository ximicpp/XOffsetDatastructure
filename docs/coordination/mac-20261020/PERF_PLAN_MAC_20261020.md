# XOffset Mac 接续：性能开发方案与计划（2026-10-20，草案 v1）

范围：在 Apple M4 / macOS 26.3 原生环境上，基于 C14（主头 `66818de4…`）与独立候选 R1+M2（主头 `155f0847…`）继续性能开发。本文是设计与计划，不含新的性能成绩；所有时间数字都是旧 WSL 诊断值或从正式比值反推的估算，需在 Mac 上实测确认。

## 1. 现状与差距（来自 F64 正式结果）

正式口径：XOffset 候选 ÷ 同批每个竞品，联合单侧 95% 上界。门槛 read_sum ≤1.05、mutate ≤1.05、独立拥有 emit ≤0.20；reload 保留完整校验，只报告。

| 竞品 | read 上界 | mutate 上界 | emit 上界 | 还需降低（按上界） |
| --- | ---: | ---: | ---: | --- |
| cista | 1.185 | 1.836 | 0.471 | read −11%，mutate −43%，emit −58% |
| FlatBuffers | 1.042 ✓ | 1.604 | 0.420 | mutate −35%，emit −52% |
| Cap'n Proto | 0.405 ✓ | 0.561 ✓ | 0.527 | emit −62% |
| cereal | 1.145 | 1.704 | 0.464 | read −8%，mutate −38%，emit −57% |

## 2. 为什么上一轮只到 3/12

1. 候选按“可证明的重复工作”挑选，没有按差距倒推预算。单项效应 1–15%，而目标要求 35–60%。
2. 没有做分阶段 CPU 剖析和硬件计数器（WSL 无 PMU），竞品侧也没有分解；九个修改阶段只有计数，没有逐段时间。
3. emit 的 98% 是 payload 复制和新页缺页（5.67 ms 中 4352 次 minor fault），而预触页、并行复制等手段被排除，所以 emit 无机制，比值 0.994。
4. 中途只和 C14 比，到 F64 才第一次量化与竞品的差距。

## 3. 三项分析的关键结论

### 3.1 读取
- 五库共用同一查询循环（`views.hpp:36-66`）。XOffset 唯一多出的逐访问工作，是每次 rel32 解析里的 canonical 守卫：一次全局 acquire 读加分支（`xoffsetdatastructure.hpp:371-373`，经 `1335-1351`），平均每条查询约 22 次，READ 阶段约 122 万次。
- acquire 读同时是编译器屏障，阻止 `data()` 冗余解析被合并或外提。
- R1 只是把守卫入口改成内联，read 就降 15.4%，说明这一项主导读取。剩余成本未测（估 0.2–0.5 ns/次，READ 占 3–8%，证据弱）。

### 3.2 修改
- 共约 842 万次解析，分阶段：stage4 327 万、stage8 173 万、stage3 104 万、stage7 100 万、stage6 84 万。
- 约 60.7% 来自标量 resize 增长时逐元素 `default_construct(dst, binding())`（`2655-2701`），每元素 3 次解析，元素数 1,703,936，与 M1 记录一致。这是 M1 所在区域，M1 因筛选超时关闭，按规定不重开。
- 其余：stage3 `push_back` 每次约 7 次解析；stage7 每移动一条 Record 约 70 次（双向 binding、owns 递归、release 重复解析）；stage4/8 是 resize 后紧接 `std::copy`，同一字节写两遍。
- 分配器和 owner 查找方向已有三个候选削减了计数但未证明提速，不作为优先方向。

### 3.3 独立输出（emit）
- 路径：`save_verified` → 88 B 头 + 一次 `resize_and_overwrite`（两次 memcpy），没有校验和、没有遍历，已经接近“新页 + memcpy”的物理下限。
- WSL 分解：新页物化约 70%（touch 4.39 ms），拷贝带宽约 28%（热拷贝 1.62 ms）。对 Cap'n Proto 达到 0.20 需要 emit ≤约 2.2 ms，落在两个物理下限之间。
- **结论：在新匿名页上产出独立拥有字符串的前提下，WSL 上不存在可信的 5× 路径。** THP 估算到 0.21–0.29，仍不达标。
- M4（16 KB 页）缺页次数约降为 1/4，但竞品也会变快，比值估算约 0.3，需实测。
- 唯一可能远低于 0.20 的是 macOS `mach_vm_copy` 写时复制。它在语义上是独立拷贝，但物理页共享、成本推迟到 reload 和后续写；是否算“独立拥有输出”需要你裁定（见第 6 节）。

### 3.4 重载（reload，只报告）
- 对 FlatBuffers 4.58× 的主因不是校验：拷贝 17.8 MB 到新 mmap arena（4352 次缺页，约 5–5.5 ms）和 4.6 MB 字节粒度位图 scratch 的新页（约 1–1.3 ms）占了大头，图校验约 7–9 ms（倒推）。
- 竞品复用的是 retire 刚释放回 malloc 堆的常驻页。

## 4. 新机制候选（均不是 M1/L1/R1/M2 的重复）

| 编号 | 阶段 | 机制 | 移除的工作 | 估计收益 | 风险 | 前置 |
| --- | --- | --- | --- | --- | --- | --- |
| D1 | read/mutate | 诊断消融：canonical 守卫恒返回 nullptr（不交付） | 量出守卫上限 | 信息 | 无（仅诊断） | full 工具链 |
| N1 | read/mutate | 稳态去掉守卫：receive 完成后归一化，或守卫改为对启动时预留只读 VM 窗口的非原子范围比较 | 约 1200 万次 acquire 读及其屏障效应 | READ 3–8%，MUTATE 7–18%（弱） | 改 receive provenance 契约或引入 VM 预留新失败模式 | D1 上限 ≥3%；你的决定 |
| N2 | mutate | 单次公共调用内只解析一次 binding/arena/data，任何分配或回调边界后重新解析 | 约 80–100 万次解析 | MUTATE 2–5% | 中低；保留抛错与首错顺序 | 汇编与计数验证 |
| N3 | mutate | 平凡类型 `push_back`/`insert` 容量内快路径，不为构造求值 `binding()` | stage3 约 104 万→50 万次 | MUTATE 1–3% | 低 | — |
| N4 | mutate | 只比较地址的谓词（contains、same_allocator、alias）改为整数计算，移动赋值内复用解析结果 | stage7 约 100 万→50–60 万次 | MUTATE 1–3% | 保留 active 模式 terminate 语义和 owns 双向递归 | 4 个 canonical 用例 |
| N5 | mutate | “resize 后立即覆盖”合并：已知随后整体覆盖时跳过逐元素默认构造（需公开调用不变，作为库内识别或新内部路径） | stage4/8 的写两遍 | 待诊断 | 与 M1 区域相邻，需先确认不构成 M1 重开 | 你的决定 |
| R-a | reload | 库内默认单槽池回收退休 arena（复用已有 `VmStoragePool`） | reload 4352 次缺页与 mprotect | reload 约 −4 ms | RSS 驻留、旧数据清零、是否算改 demo | — |
| R-c | reload | 位图 scratch 复用或改为 8 字节 cell 加精确旁路表 | 4.6 MB 新页 scratch | reload −1~−2 ms | 首错顺序；cell v1 曾因 touch 上升被否决 | 先过 touch 计数门槛 |
| E-cow | emit | macOS `mach_vm_copy` 写时复制输出 | 新页物化和拷贝 | emit 估 0.05–0.3 ms | 合法性灰区；只适用 Darwin；arena 页内偏移和对齐约束 | 你的裁定 |

收益不相加；每个候选单独筛选，组合另跑全门禁。

## 5. 对目标的诚实判断

- **read**：对 FlatBuffers、Cap'n Proto 已达标。对 cista（−11%）和 cereal（−8%）取决于 D1 测出的守卫剩余成本，只有 N1 有可能补齐。
- **mutate**：N2–N4 合计约 5–10%，加上 N1 也只能乐观地接近 −35%~−43%。没有 N5 一类结构性机制，mutate 对 cista、FlatBuffers、cereal 预计仍不达标。
- **emit**：在新页上产出独立字符串的前提下没有可信 5× 路径。除非你裁定允许 COW，否则 emit 四项预计都不达标，只能在 Mac 上实测并据实报告剩余成本。
- **reload**：R-a、R-c 能把对 FlatBuffers 的比值从约 4.6× 降到约 2.5–3.1×，完整校验之下不可能接近 1×。

## 6. 需要你做的决定

1. **emit 写时复制（E-cow）是否算“独立拥有输出”。** 我建议的判定标准：输出是调用方独占的 `std::string`；双方后续修改互不可见；生命周期独立；页面准备在 emit 计时内；推迟到后续阶段的成本计入成本转移审计。若允许，只在 Darwin 上成立，需要重新验收。
2. **N1 是否允许改变 receive 的 provenance 契约**（receive 完成后归一化），还是只走“VM 预留窗口”路线。
3. **N5 是否视为 M1 重开。** 我倾向视为新机制，但它与 M1 同在标量增长区域，需要你确认。
4. **R-a 库内默认单槽池**是否算改动 demo（适配器调用不变，只改库默认行为）。

在你答复前，我按“不允许 COW、N1 只做 D1 诊断、N5 暂缓、R-a 作为库内默认”推进。

## 7. 计划

### 阶段 P0：Mac 平台验证（进行中，正式 bench 前必须完成）
1. ✓ 交接完整性、证据包；✓ storage 原生 2/2 与安装消费 15 项。
2. 构建 clang-p2996（提交 `d77eff1`）、libc++/libc++abi/compiler-rt，记录新 profile，不改旧固定 profile。
3. 用 `cmake/reflection_probe.cpp` 实测编译、链接、运行能力。
4. C14 与隔离的 R1+M2 候选：full normal、ASan/UBSan、TSan，按本机实际全库存执行；业务 S/M/L 真值、V1 双向、安装消费端、生命周期。
5. 按锁定提交重建四个竞品（cista `015b94e6`、FlatBuffers `7e163021`、Cap'n Proto `373e61ec`、cereal `ebef1e92`），模式不变。

### 阶段 P1：Mac 测量基础设施
1. 新建 Mac 版运行器（新目录），不重放旧 Windows/WSL 进程控制、电源、队列和租约工具。
2. 调度：用 `taskpolicy`/QoS 尽量留在 P 核并记录核类型；交流供电、关低电量模式、`caffeinate`；记录温度状态。不暂停你的其他应用。
3. 唯一 heavy executor：重构建、计时、大审计、归档串行；计时期间其他重工作静默。
4. 分阶段剖析：用 xctrace CPU Counters 采集 M4 硬件计数器，覆盖九个修改阶段和竞品侧；只作归因，不作收益证据。

### 阶段 P2：诊断（不交付）
1. D1 守卫消融：read/mutate 上限。
2. 九个修改阶段逐段时间，以及 stage4/8 写两遍的实际份额。
3. emit 物理下限：M4 上“新 malloc + memcpy 17.8 MB”与四个竞品 emit 的实测对比，确定 0.20 在 Mac 上是否物理可达。
4. reload 分解：拷贝缺页、scratch、图校验各自时间。

### 阶段 P3：实现与定向验证（按顺序）
1. N3 → N2 → N4（mutate，风险低到中，各自单独实现、汇编与计数验证、生命周期 52 例和 canonical 用例）。
2. R-a、R-c（reload）。
3. N1、N5、E-cow 视 D1 结果和你的决定。
4. 每个候选：安全与生命周期论证 → 定向测试 → full normal/SAN 回归 → 一次预注册 72 对 AB/BA 筛选（Mac 新协议、新预算）。

### 阶段 P4：正式验收
- 以新宿主、新源码与工具链身份、新结果目录冻结新的 Q/F 协议和预算（maxC/maxP、child 上限用 Mac 原生 A/A 重新推导）。这是新的独立实验条件，不是旧窗口补样。
- 有依据的候选才组合，组合另跑全门禁后进入正式验收。

## 8. 不变的约束

原 demo、模型、输入/trace、公开调用、九修改阶段、八计时边界；独立拥有 string（不预触页、不把准备移出计时、不用复用缓冲或 view/lease 替代）；完整校验和首错误顺序；每次调用 acquire（N1 若改变需另行论证并经你决定）；V1 精确兼容；四个竞品模式固定；已关闭窗口（M1、L1、R1/M2 72 对、Q32/F64）不补样、不改阈值、不重开；插桩耗时不作为收益证据；Linux/amd64 仿真结果不代表 Mac 原生性能。

## 9. 依据

- 正式结果：`docs/coordination/c14-iteration-20261020-v2/delivery/RESULTS.md`、`RESULTS.json`。
- emit/reload 诊断：`c14-iteration-20261020-v2/core/C_WORK_LEDGER.md`；旧包 `docs/PERFORMANCE_FOUR_PATH_COSTS.md`。
- 读取/修改计数：R1 诊断 `R1_DYNAMIC_RESULT.json`；M2 结果。
- 历史候选：旧包 `docs/coordination/receive-completion-20261019/`、`performance-four-paths-20261018/`、`c14-iteration-20261019-v1/`。
- 五库对比工具：旧包 `experiments/serialization_comparison/`。

## 10. 更新（2026-10-20，Mac 探索性诊断后）

以下数据来自 M4 上的探索性测量（M 规模，随机顺序 21–25 轮中位数，无固定核心、无预注册），只用于定方向，原始数据在 `/Volumes/XOffsetCS/diag/`。

- R1+M2 ÷ 竞品（中位数）：读取 cista 1.07、FB 0.86、capnp 0.31、cereal 1.19；修改 1.89 / 1.57 / 0.67 / 2.72；独立输出 0.22 / 0.20 / 0.59 / 0.28。Mac 上输出对 cista/FB 已接近 0.20，Cap'n Proto（emit 1.9 ms）是主要障碍。
- D1（守卫恒返回 nullptr）：修改 −43%，读取 −15%。D2（relaxed 读）、X1（普通全局 bool + 冷调用）、X2（运行时全局变量做范围判断）对修改都没有改善；X3（编译期常量窗口做范围判断）修改 −38%、读取 −15%，接近 D1。结论：开销来自"每次解析都要重新读一个可能被别名写的变量并分支"，只有编译期常量边界能消除。
- 守卫方案改为 **W-固定地址**：在平台选定的固定虚拟地址保留接收窗口并建整体 byte 数组，热路径用常量范围判断。约定变化 6 项（见线程消息）外加"固定地址被占用时接收不可用"。原型实现中（`/Volumes/XOffsetCS/candidates/w1`）。
- **G1（新页免重复清零）**：扩容只清零以前暴露过的字节。修改 −4.7%，缺页 4042→3492，退休 0.23→0.16 ms。full normal 76/76、TSan 73/73、ASan+UBSan 64/73（失败集与基线平台限制 9 项相同）。
- 去掉守卫后修改阶段剖析：分配/回收/owner 查找/Darwin TLS（`_tlv_get_addr`）约占 46%，列为下一方向（Darwin 上 TLS 比 Linux 贵）。
- 分阶段（去掉守卫后）对 cereal 差距最大的是 stage2 replace_name（1.63 vs 0.53 ms，分配器路径）和 stage7 erase（1.31 vs 0.66 ms，移动协议）。
