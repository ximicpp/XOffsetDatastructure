# C14 以正式性能目标为验收的实施计划 v2

执行授权：2026-10-20，用户明确要求实施本计划；当前为 Default 模式。这个文件替代本轮调度计划，不改写 v1 历史。当前默认源码仍为 C14/final-v4；本计划完成不等于性能目标完成。

## 当前执行结果（2026-10-20）

A、B、C及R1自身完整功能门禁已通过。R1唯一72对已完成144次，完整审计和两项独立复核通过：读取合计相对同批C14的点估计0.842878847、联合上界0.854663272；修改辅助点估计0.764263918。等待只读封口后登记保留，不代表对竞品达标。M2自身normal73/SAN73通过，业务、消费者和生命周期余项执行中；默认源码尚未集成候选。输出仍缺少已证明的五倍实现路径，Q32/F64尚未准入。以下已有证据段落保留计划建立时的历史状态；实时状态见STATE.json。

## 基线、职责与目标

- 唯一源码工作树：`C:/Users/gzsufanchen/.codex/worktrees/cross-machine-handoff-20261019/xoffsetdatastructure开发`；HEAD `1904b9c71230026ac6f2d58f919a9ab76447d264`；baseline main SHA256 `66818de49b18fe85ab8049c9f9ad299bbebbb214eb3fceee0a665fe65f4275c3`。身份按154成员/14生产文件绑定，不仅核对main。
- 原 Documents 工程根为历史F，只读历史工具和证据。基线、冻结源码、v1请求/窗口、原wire和manifest保持不变；新候选、构建、安装、工具与结果进入v2独立目录。
- 正式分子为最终XOffset候选，分母为同批每个固定竞品：`read_sum <=1.05`、`mutate <=1.05`、原demo完整独立owning `emit <=0.20`，使用约定联合单侧置信上界。reload完整校验并分别报告，不新增门槛。旧V5均值不能当C14差距。
- 模型、固定输入/trace、原public调用、九修改阶段、八计时阶段、完整校验/首错误、异常和生命周期、V1不变。生产容量复用、冻结/共享、直接接收成绩不能替代owning输出。
- 核心会话唯一写候选生产源和测试；验证会话唯一执行重负载；统筹拥有根PLAN/TASKS/STATE和准入审查。临时agent仅处理明确授权的窄范围。
- 所有重负载沿用原全局`run_job.py`和`Documents/.../build/vm-coordination/queue.lock`，不自建并行锁。当前lease必须重新核对。计时期间其他agent停止扫描、编译和归档；用户进程恢复先于离线审计。

## 已有证据与关闭决定

- v1/M1：`validation/M1_FINAL_RESULT.json`记录完整normal73/SAN73、业务80+80、V1/消费端/生命周期通过；24576次fresh resize、1703936次逐元素binding被bulk替代。唯一screen为119 attempted、118 validated、child119超过20s、25未执行；summary=null，性能未证明。旧窗口永久关闭，不补样、删点、前缀收益、换名或通过无关组合重验；不晋升默认。
- L1上下文长缓存维持拒绝：PMR/new_handler重入可使mapping失效；保留OwnerRecord不等于保留存储。拒绝不宣称所有更小局部优化不可能。
- R1独立`r1-canonical-hotcold-v1`（main `b4c61b2d286d6d4ffdfbd44af3ff7151b1640dd07f8b9a2c9bfcfa01a6a9c23f`）代码生成已通过，native尚未执行。原query的静态helper调用/前置push和候选内联ledger分支只证明机制机会，不是动态频度或耗时。
- M2机会已由现有真实driver汇编确认：非空Attr owning vector、containment未命中路径最多4次解析。控制asm50761/50785/50795/50804，R1 asm52237/52272/52291/52304。不再次编译控制寻找同一事实。
- 旧warm16每次17830872B、一次string分配请求和完整复制，零fault且无clock；不建立首次物化预算。已有四路径成本账本先核对适用身份再复用，不重复完整调查。

## A–E实施顺序

### A 身份和状态同步

验证者有限核对当前baseline154/14、既有P2996/libc++/SAN profile、实际依赖、lease与磁盘。相同身份控制功能证据明确引用，不能为路径变化重跑全平台。Windows原生full没有通过结论。交付A_RESULT及当前可复用/待补清单。

### B R1定向正确性

沿用冻结R1，不追加生产patch。四现有程序`test_canonical_zero_target`、`test_canonical_cold_no_callback`、`test_canonical_registry_v2`、`test_owner_identity`分别normal/SAN；加一个同步混合owner/线程交接用例normal/SAN，共10程序执行。覆盖active count零/非零/归零、普通与真实C owner、冷热TLS、边界、移动/退休/复用以及既有OOM、replaceable-delete/new-handler。各payload独占或同步，不并发退役同一mapping，不缩小支持域。核心提交精确源码/测试/命令闭包；验证者审查后可自动执行本有限批次，不逐步等用户确认。

R1仍逐调用acquire，不跨调用缓存零状态；cold body、anchor优先、zero target、TLS退出和无冷驱逐回调规则保持。active成本按真实内联闭包检查，不预设必然增加一次调用。

### C 一组机制和首次输出诊断

在B通过且完整工具/请求独立准入后执行。固定C14先、R1后，各一个原run进程；同输入/trace/profile/hook，控制同时承担首次owning输出观察。不是validate预先导出暖化；八阶段和事后全量truth顺序保持。计数/时钟只作诊断，不作AB速度、置信区间或0.20验收。

- 最多两次诊断编译链接，各120秒；两个实际diag native各20秒；独立任务总600秒。body上限128MiB、日志1MiB；构建及审计另投影，沿用原RSS/磁盘/backing门禁，不删除历史腾空间。
- ≤64固定边界事件即时写到外部sidecar/pipe并由监护进程保留，覆盖main、input read/decode/hash、t0..t8、post-audit load/export/hash及产物write前后。不能只存在child内存或正常退出才落盘。
- emit内仅固定无分配POD记录，离开emit后flush细节；禁止日志string/ostream/new、预留/触页输出。记录规范化、header准备、string准备至callback、header/payload复制、返回等嵌套工作；不把准备区间称纯malloc，也不把复制fault另加一次。
- wrapper记录launch前/返回、完整进程wall/utime/stime/minor+major faults、退出/kill和最后区段。wait4/Popen单一reaper；事件通道有界且不阻塞child；退出/超时后不留子进程。
- 工具先测试正常退出、无事件挂起、已发事件后超时kill、截断/EPIPE/通道失败、单reaper、deadline及资源清理等真实错误，不造完整框架或追完整符号。
- 任一身份/真值/预算/超时失败，停止剩余运行并封存；全部通过、未复现或仍未知均收束，不追加第三次，不追认M1原超时原因。诊断失败不阻止其他独立正确性工作。正式裸-O3性能二进制不含诊断hook。

### D R1与M2独立完整门禁和唯一筛选

R1机制命中与定向正确性通过后执行它自己的完整normal/SAN、S/M/L业务真值、V1双向、安装/子项目/跨TU/既有共享runtime消费者、OOM与生命周期门禁，不能借M1结果。

M2可并行证明并在独立C14候选中实现：非string owning vector仅保存当前节点一次data，原capacity containment、miss后读取size、`data ? data+size : nullptr`、原双向完整递归和次序保持。不得空指针算术、用capacity作终点、缓存owns predicate、跨新callback保留pointer或延长有效期。检查真实canonical/reflection闭包；当前无冷TLS驱逐/内部last-owner回调事实可复用，不把L1的PMR边界泛化。候选不混入R1/M1，基线不改。新候选机器码应确认4→1；基线asm不重编。定向normal/SAN、一次控制/候选固定M机制及完整门禁依新请求绑定。若真实命中为零、机器码未减少工作或安全证明不成立，关闭M2，不消费screen。

每个获准候选仅一次72配对/144 child、AB/BA各36。R1主`read_sum`，M2主`mutate`；保留八阶段、read_sum、total和内存释放成本。原四主目标/36辅助族和df71不重置、不回收：primary `t(1-.05/4,71)`，auxiliary双侧 `t(1-.05/(2*36),71)`。新窗口按固定公开seed事前生成平衡随机pair次序并冻结完整schedule；不改旧M1工具和窗口。AB/BA不证明独立，不删点、不事后去趋势/换单位或检验。主上界<1且全部工程门禁通过才能晋升；辅助区间跨1不能称无退化，辅助下界>1为确认回退。

晋升还必须没有任何辅助时间区间下界>1的已确认回退，并通过全部冻结成本转移门禁。合法零阶段值依v1最终worker政策保留，受影响的log对比标记undefined并禁止晋升；不加epsilon、不删pair。缺失、负值和非有限值按原无效数据政策关闭。

screen沿用child20秒、命令600秒、audit3600秒、原RSS/allocated空间门禁。实际完整进程/产物投影和host准入必须在freeze前闭合；投影不满足不启动，不把native.total当process wall。screen freeze必须由统筹独立准入，不能仅凭本计划立即开窗。失败即关闭，不补样、不计算成功前缀性能。

### E 组合、Q32与条件F64

只组合已获独立收益依据的候选。R1/M2节省部分可能重叠，不相加预测；组合闭包完成相关完整门禁后才冻结正式资格。没有晋升候选不伪造组合，不为填状态而运行巨大C14基线Q/F。

沿用原六角色、竞品版本/模式、输入/API和正式调度：Q32共4656 child，30项阶段A/A全部区间在[1/1.05,1.05]内才通过；F64共9264 child，仅同一闭包及合格条件执行。12项主比较为4竞品×read_sum/mutate/owning emit的联合单侧95%上界；reload/load/retire/cleanup/total和内存另列。

原Q2700s、F5400s、各audit3600s、child20s、非child在线余量300s保持。实际六角色`S=sum(maxC+maxP)`投影分别满足`388*S+300<=2700`、`772*S+300<=5400`，另保留当前child/seal余量；历史max不保证未来尾部。资格失败不启动F。原正式统计、资源和恢复约束全部保留，需统筹新freeze准入。

## 收束和继续迭代

安全反例、无机制、统计未建立、工具/资源失败分别记录；一项失败只关闭该候选/窗口，不停止其他独立工作。输出若没有可消除工作，交付17.83MB完整物化的剩余成本，不用可选生产接口或旧V5成绩替代。

每阶段交付实际身份、命令/退出状态、真值、已减少工作、成本范围、通过/拒绝/前提不足及下一动作。正式未达则如实报告逐竞品结果与剩余限制；下一轮必须有实质新机制依据，不改名重试。只有正式目标证据成立才标记performance_goal_complete。

本轮不自动commit/push/merge/Release/publish/远端CI，不触发Superpowers。阶段间自动推进；需要人类介入的具体权限/凭据/不可逆操作才询问，不做进度确认。
