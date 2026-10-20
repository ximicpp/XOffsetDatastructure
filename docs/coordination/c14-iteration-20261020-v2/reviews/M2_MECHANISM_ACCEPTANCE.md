# M2 实际机制有限验收

日期：2026-10-20（Asia/Shanghai）。审查者：`runtime_observer_review`。

**机制阶段通过：原业务路径命中，并实际减少重复data/canonical解析。** 本结论不是耗时收益，也不直接批准screen或竞品比较。精确机器记录见同名JSON。

冻结 `001ddddf...`、准入 `0d61cead...` 与实际输出引用一致。原共享W3 diagnostic队列 `bf6d7ffe921f4646a9efe6785d0fd89c` 的六metadata文件实际SHA通过，准确请求与冻结相同；队列complete/exit0、14.889786028秒≤300秒、cleanup三项全空。

实际只有一次编译、一次诊断native。编译5.371518837秒≤120秒，native2.278170359秒≤20秒，均准确argv、单次回收且无残留；没有新控制编译/运行或retry。6492成员的before/after记录与冻结完全相同。候选AUDIT `fd6fea08...` 与原C14控制AUDIT `7b834986...` 当前pin吻合，完整九阶段truth及query通过；原完整body/oracle审计复用，本次未重新hash大body。

修改阶段保留owns=320465、vectors=154575、capacity_hits=0、nontrivial_misses=28652、nonempty_misses=16131及完整counts守恒。实际变化为：

| 工作 | 控制 | M2 | 减少 |
|---|---:|---:|---:|
| data解析 | 228010 | 154575 | 73435 |
| canonical调用 | 8421400 | 8373007 | 48393 |

减少量分别等于`2×28652+16131`和`3×16131`。变化只发生在第7修改阶段；primary、mutate和mutation_7是同一工作在嵌套范围中的观察，不能重复相加。其余阶段计数没有减少；两角色active canonical调用均0。

静态source/safety和实际bare codegen证明按已有root接受及`core/M2_SCREEN_GATE_BINDING.json`的精确范围复用：当前节点局部data复用、capacity覆盖先检查、按live size遍历、原递归/短路顺序及所有权检查保留。没有把一般ADL递归说成无用户回调，也不把静态4→1调用机会直接换算成实际时间。

原checker输出为6101B/`e10f8749...`，最终`M2_DYNAMIC_RESULT.json`为7221B/`44746860...`，其result与原输出完全相同。checker源码7389B/`7315074f...`与冻结相同；封口工具8541B/`bbc0434e...`记录没有新增checker或native运行。checker自己的`bare_codegen_checked=false`和`execution_admission_checked=false`仅表示该组件不承担这些门禁，不将其改写为已执行；本验收另行绑定已有源码证明和实际准入/队列。

本审查0编译、0项目native、0新queue、0checker执行、0大bodyhash。当前只接受机制命中；hook时间不进入速度结论。候选自己的完整功能/工程闭包、唯一72对及正式逐竞品目标继续按既定阶段独立验收。
