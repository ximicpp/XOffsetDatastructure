# 验证执行入口与阶段回执

唯一入口：`tools/c14_iteration_20261019/validation/candidate_job.py`，在 Ubuntu-22.04/root 中运行。`check --request <绝对JSON路径>` 只做输入准入；`run --request <绝对JSON路径>` 使用旧根的真实 `experiments/vm_coordination/run_job.py` 默认共享锁。没有新的锁、任意 shell 命令、计时或暂停进程入口。

S0 已完成，只读复用现有固定 WSL profile。新工作树154成员和14生产文件吻合；旧构建源仅 SOURCE_MANIFEST 元数据与最终 ZIP 不同。依赖11个 include 树和3份许可证内容锁通过。工作树的两个 submodule 为空；新构建快照从已验证 final-v4/source 复制固定 vendor，CMake 再次校验原锁。

每个 S2 定向请求包含：

- `schema_version:1`、`stage:"S2_targeted"`、`candidate:"M1|L1|R1|M2"`、新 `job_id`、`mode:"normal|sanitized"`。
- `baseline_main_sha256`、完整14文件 `production_files`（每项 bytes/sha256）。
- `overlays` 映射目标相对文件名到 `{path:绝对候选路径, identity:{bytes,sha256}}`；基线154成员自动补足，额外测试/候选CMake也必须列入。候选来源限本轮C14树或本轮WSL core目录。
- 精确 `targets` 和 `tests`；不得用退出0代替实际 inventory/JUnit 名单。测试断言保持项目 `-UNDEBUG`，独立 bare 代码生成另行登记，不能混用。
- `runner_identity`、`entry_identity` 和 `tool_files`（固定run_job/CMake/CTest/libc++ wrapper/Ninja）；均使用 bytes/sha256。固定compiler/runtime及sanitizer resource另按S0身份再核对。
- 实际 C 盘 `backing_free_bytes` 与15分钟内的 `backing_observed_utc`。WSL VHDX 在C盘，WSL逻辑空闲不代表宿主容量。定向构建保留8GiB余量，正式capture预算仍未准入。

可参照 `validation/entry-check-v2/valid-preflight.json` 的结构；这是工具自检fixture，不是核心候选请求，不得执行它。核心请求提交后由唯一验证执行者核对与绑定，不由核心自行编译。

每个执行目录固定 `/root/xoffset-c14-iteration/20261019/v1/validation/<job_id>/`，源快照、build、stdout/stderr、JUnit和receipt均新建。租约独立保存在同级 `<job_id>-lease/`，run_job原生request/result/events/manifest保持原格式。C14文档目录收到 `<job_id>-dispatch.json`，含真实argv/退出状态和原始回执位置。run_job历史 `source_headers` 属于旧根F，仅作runner上下文；新候选的可信源码绑定以request、14文件、快照、compile_commands及阶段receipt为准。

阶段receipt至少包含：阶段/候选/实际profile、请求及工具身份、完整输入清单、14生产文件、当前真实租约run_id、所有实际argv/退出状态、开始/结束时间、真实inventory及JUnit名单、通过/拒绝原因、性能样本数。通过只覆盖该定向请求；不授予业务/V1、筛选或正式资格。失败保留目录，先诊断；源码修复后用新请求身份和新目录补受影响门禁。

S1/S2机制/业务任务使用本轮另行冻结的具体worker和现有同一run_job，不能塞进此定向入口。每次登记明确固定输入/trace、观察次数、bare `-O3 -DNDEBUG`、真实工具和输出身份。筛选72对、Q32/F64尚无准入，不能由本工具启动。

当前下一请求：核心M1候选overlay、专用反例测试和明确S2定向normal/SAN名单；同时提交一次固定M机制/bare代码生成所需的具体源和输入。S1固定16次 owning 输出仍为待执行诊断，无速度结论。
