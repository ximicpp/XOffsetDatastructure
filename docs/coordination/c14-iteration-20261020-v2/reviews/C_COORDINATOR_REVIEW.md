# C 阶段协调器源码窄审

日期：2026-10-20（Asia/Shanghai）。审查者：`runtime_observer_review`。

读取 `tools/c14_iteration_20261020_v2/validation/diag/phase_c_job.py` 的稳定快照：**34727B，SHA256 `2b47de043f08be8eb32420843bf3ee045aca02d274d95c498e6c08624d7cb26f`**，mtime UTC `2026-10-19T17:37:07.3365209Z`。最初两次读取身份相同；交付期间作者的后续revision在文末另列。本次仅静态读取协调器及其引用的少量身份/原runner资料；编译、native、dummy、性能样本均0，不重复先前wrapper/producer源码审查。

**结论：协调执行主链的源码审查通过；实际 freeze/启动仍需补齐下述外层launcher绑定，然后核对同一freeze的资源、源码及准入。** 本文件不批准尚未提供的freeze，不证明诊断或性能已通过。

## 1. 已成立的监督与执行边界

- 原共享RUNNER、LOCK与W3 `diagnostic` 600秒队列直接绑定；`leased`核对准确Python/-B/coordinator/argv、cwd、run_id、supervisor PID、实际held lock和session。没有再次调用run_job，也不试图在原租约中获得嵌套租约。
- `ENTRY_NS`在模块早期建立，统一deadline为ENTRY+600秒；外层真实runner另有600秒硬监督。身份检查、编译、native、角色audit及封存都在同一队列中，不把角色间工作移出预算。
- 只按`['c14','r1']`执行。prepare虽绑定m2源码，但明确不生成其执行角色。每角色单次`compile_one`，随后单次`D.supervise`；没有warmup、retry、validate、extra emit或动态补run循环。前一个角色失败即退出for，保留已使用的唯一输出prefix。
- 编译deadline在fork前为120秒；selector在fork前创建，注册/日志打开均在清理保护内。wait4非阻塞轮询，超时按重验owned PID终止，主child单次回收，后代另列；清理最多至deadline+2秒。迟回收、残留或编译非零都失败。
- native调用已将统一`absolute_job_deadline_ns`传入接受过的wrapper，保持20秒；调用前有110.5秒的native/audit/seal余量复核。每角色前232.5秒启动预算检查，compile本身另检后续余量。预算不足只停止，不缩短任务或放宽cap。
- 编译、native与角色audit顺序串行。最终仍要求原runner实际cleanup为空和complete，内部`RESULT.status=passed`不能替代外层结果。600秒外kill若发生在最终收据前，保留现存prefix及queue事实，不能新建替代attempt。

`role_audit_seconds=60`为结束后观测门限，不是独立60秒杀进程计时器；它仍处于统一600秒硬监督内。此处不建议增加audit进程层，最终报告应保持该区别。

## 2. 最终freeze必须补齐的一处接入差异

**C1：实际外层launcher/cleanup工具闭包尚不完整。** 102行仅加入`Path(sys.executable).resolve()`；121行link scope与117行旧profile筛选没有收录外层runner实际依赖的`ps`。只读原RUNNER可见其105行通过`subprocess.run(['ps', ...])`从继承PATH解析工具，367行又通过同一`sys.executable`启动supervisor。因此只冻结RUNNER源码与解析后的Python实体，尚未证明最终请求所用Python路径、PATH/ps解析在freeze和执行间一致。

最小闭合方式可以由最终freeze/dispatch完成：

- 绑定请求中的实际Python路径及其解析实体，执行前后核对；可以将`collect_file`作用于absolute的`sys.executable`而非先resolve，从而同时记录requested/resolved路径。需要区分“磁盘上旧Python路径未变”与“当前实际使用的interpreter就是该身份”。
- 冻结外层实际PATH及`ps`解析路径、二进制和必要runtime闭包，确保原runner真正使用同一解析。不需要修改原runner、增加另一层launcher或新建租约。

当前draft `runtime_files`有381项，但没有ps/libproc/Python成员；旧profile被筛选的tool-runtime/taskset-host项也没有ps，不能从其他名称的runtime清单推断本项已经覆盖。此项仅针对原runner实际cleanup调用及请求Python路径，用原fixed_files机制补实际resolve/PATH记录即可；不要求扩展成全OS工具链闭包，也不改旧runner、不新增native/dummy或新框架。实际cleanup三项为空仍须执行后核验，固定路径身份本身不能代替清理结果；该接入项不阻塞独立功能工作。

## 3. 实际source/input/build/runtime与失败封存

- prepare核对三角色材料化身份；每角色`files`实际160项，含154成员和14生产文件，不是只绑定主头。实际compile argv替换为final materialization目录，控制/候选分别输出新二进制；-O3/-DNDEBUG且排除-UNDEBUG/SAN。
- 固定wire、fixed-M trace/fixture、真实adapter label及原run flags均冻结。compiler launcher、clang实体、头文件include roots、GNU11 CRT/linker/libs、配置runtime、observer/audit工具纳入fixed_files；执行前、每角色及末尾重查。-MD实际依赖必须存在于冻结集合，且必须包含最终driver/event_channel/core POD hook。
- 真实二进制是在冻结source与编译命令下本轮生成，其SHA、compile receipt、依赖清单及source/hook身份与native-attempt绑定；native之后再核对。最终freeze仍须独立验证这些路径确为实际生效闭包，而非仅接受`passed`布尔。
- 资源准入要求guest/backing各覆盖4GiB峰值加2GiB余量；编译/native具有file/AS限制，输出在各角色及末尾核对allocated。4GiB是工程准入/观测上限，不是协调器对所有临时文件提供的内核级aggregate磁盘quota；不能将单文件RLIMIT_FSIZE称为全卷总量硬配额。
- child失败由已接受wrapper保留raw和收据；编译失败保留attempt/log/receipt。角色错误停止后续角色；`attempted_roles`、`compile_attempts`、`native_attempts`和completed区分保留，无前缀替代或收益计算。
- 没有宿主暂停、进程恢复或电源配置变更。若实际dispatch另引入此类动作，需有独立当前授权/恢复证据；不能借历史PID回执。该协调器只负责自己的owned child/descendant与原runner最终清理。
- 最末写入失败或外层超时时可能没有`RESULT.json`，应以原队列失败事实和已经落盘prefix封存，不把缺少内部summary解释成可重跑。最后seal前600秒检查也不能替代完整外层队列回执。

## 4. 观测口径

- `audit_role`要求raw和parsed同一精确36事件顺序、seq、同域monotonic/因果界限及完整终结；不是仅检查labels集合。原始stderr必须空，stdout仅允许三份明确的诊断JSON，不能将其输出开销解释成业务工作。
- 原生八阶段、首次emit九个nested samples、九修改阶段truth replay、完整wait4进程资源分别保留。native receipt使用child返回时刻，明确不把随后Python审核/封存加入child业务时间。
- emit申请/复制字节、sample时间和CPU/fault单调、POD与完整stdout一致均核对。完整wait4故障数据、replay资源和原native t0..t8字段没有改名混用；峰值RSS可能包含replay/observer，limitations已明确。
- t4后POD/JSON flush在邻近诊断区段计费；nested区间不得相加，复制中的缺页不能独立重复计费。额外truth replay不产生第二次owning emit。
- 所有产物明确`diagnostic_only`、`timing_samples=0`、`performance_admitted=false`，不输出R1加速比或置信结论。最后边界只能缩小未观测区间，不能反推历史M1超时根因。

最终freeze提供后，只需核对C1和精确闭包/门禁/资源身份；不应为了本次协调器审查再执行额外C++或诊断样本。

交付时作者并发更新为 **35052B，SHA256 `2286b3e2be4b4bc43fdea2fe71965ef7263103a0c7c3e3acc26f1055742c3824`**。已窄读新增normal36准入和实际evidence路径/身份核对，以及末尾1MiB收据余量检查；它们加强准入，不改变执行角色/次数/监督路径。C1在该版仍未闭合，最终freeze应使用其真正稳定版身份，不能引用最初2b47...快照冒称未变。

## C1 与 interface36 准入字段的接入复审

**C1 的源码接入已闭合。** 本次开始/结束均为 **38059B，SHA256 `9d7b3585cdc05a2c3444b931702da27bad9c914944478fcf0b61a878f86e60bb`**，不是中间通知的5d642d9c...。只审本节差分；其他新增机制门禁由其专项审查负责，最终freeze仍须绑定实际稳定SHA。

- prepare:102–105读取实际继承PATH，要求其能解析绝对ps路径，将`/usr/bin/python3`词法路径、实际解析Python实体及ps路径交给原`collect_file`。后者同时保存lexical/resolved文件身份。freeze记录`inherited_PATH`、`runner_ps_path`和`runner_python_lexical_path`。
- run:370在任何角色启动前要求继承PATH和`shutil.which('ps')`与freeze相同，再以既有`verify_files`检查路径指向的实际bytes。外层runner及coordinator继承同一环境，原runner代码未改；最终dispatch需按冻结的`/usr/bin/python3`请求路径桥接。这个小范围接入不扩成全OS工具链，也不增native/dummy。
- 外层实际queue的cleanup三项仍必须独立核验；这些静态身份字段只排除路径漂移，不证明实际清理完成。

此前中间版要求`normal36_passed`会暗示额外36标签dummy已执行；当前run:371–374已改为 **`interface36_scope_accepted`**，evidence项为`interface36_acceptance`，并绑定真正的`C_OBSERVER_SCOPE_ACCEPTANCE.json`身份。prepare:111–115实际比较旧/new interface：旧34标签、当前36标签，删除新增truth.replay.before/after后顺序相同，其他所有字段相同；与component acceptance中的明确data-review桥接一致。

这项修改要求的是准确的源码/data接受记录，不把旧34标签dummy追认为36标签运行，也不要求再补一轮dummy。最终admission应使用该字段名及具体evidence身份；若继续使用旧`normal36_passed`模板，工具会在任何编译/native之前拒绝，需修正准入JSON而非重跑实验。

本复审0编译、0native、0dummy、0性能样本；待最终freeze只核实际身份桥接和外层队列事实，无新增工具或实验要求。
