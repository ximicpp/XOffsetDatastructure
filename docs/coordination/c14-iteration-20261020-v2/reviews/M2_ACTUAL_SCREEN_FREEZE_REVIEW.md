# M2 实际唯一screen freeze接入窄审

日期：2026-10-20（Asia/Shanghai）；审查者 `v2_stats_review`。复用此前M2设计/worker/profile/v5准备审查，只核新freeze元数据、实际gate引用及投影算术；没有执行Python源码、编译、native、queue、host控制或大body hash。

**结论：接入通过。实际host/global quiet与最终执行准入由root另核；本文件不单独批准计时或Q/F。完成本文件后立即idle。**

## 冻结身份与统计次序

`validation/screen/M2/frozen-v1/freeze.json`：**1827701 bytes，SHA256 `d74af43ad891b87873b7bdb1bf4ce9bc0fd79eca4fcb4474b58c1c7af386fab2`**，schema `m2-screen-freeze-v2`。

- 完整144项schedule逐项与已审M2 design children对应，ID唯一，角色/ordinal/pair/order/symbol一致；72对、AB36/BA36，每对同一C14 control和独立M2 candidate各一次。
- 主mutate、原主族4/辅助族36、df71、t=`2.289930854179058`/`3.327935133019535`，warmup/retry/replacement均0。child20秒/window600秒/audit3600秒及原seal规则不变。
- design仍是SHA `9b6737a6a0d4a1962c3183e4da995fb60ef1d9c0e15f2f158b63bd49bbf31151`；原八阶段、derived先求和、零值undefined禁止晋升、辅助下界>1否决等继续按已审M2 worker执行，没有新增统计族或复用旧样本。

## 实际候选、输入与十一gate

config identity为605727 bytes、SHA `4833240e84162073a61ea636cf984c425d32c863e19ba3ba5e9a39e8c837c0cf`。freeze的candidate main为 `3e347612c1a62299bb910c68dd6b1411663f3165e18e298a51e7ba2e062c201f`；profile/binary/production14 digest与已审M2实际裸profile observation相同，candidate identity仍 `4a62016c53b103964d4a0b14ce56e3b717b59b54b011efbd52c021b7114b944c`。两角色effective argv均含 `-O3 -DNDEBUG`，无UNDEBUG/SAN/diag/forced include；M2 INPUT_POLICY实际引用核对通过，同一已批准staged输入和独立owning/full truth语义保持。本次不重复全runtime或所有实际source文件hash。

`gate_admission`明确引用root新 `reviews/M2_SCREEN_COMPLETE_GATES_ADMISSION.json`：4738 bytes、SHA `bfce36241fce5611ed361c92ece363f83a1a819a0deaf92ad175b217bc80ac44`，status passed。其十一map引用与freeze逐项path/bytes/SHA一致：codegen、mechanism、targeted、full_normal、full_san、business_SML、v1_bidirectional、consumers、lifecycle、safety、engineering。

逐gate已核实际receipt字节身份、kind/gate/status、same candidate，以及source154/production14与freeze runtime一致；独立review envelope为passed，record实际bytes/SHA与其status通过；所有required_assertions均由实际assertions满足。mechanism已包含原freeze要求的 `actual_nonempty_path_observed=true`、`static_actual_codegen_passed=true`、`unhooked_screen_binary=true`。这些是root直接生成的新实际typed gate，本次没有执行或借用此前未执行binder，原binder pending不再是这条实际接入链的阻断。

## capture隔离与资源投影

引用root `reviews/M2_RESOURCE_CAPTURE_ACCEPTANCE.json`，SHA `389339a3f7b1349a0fce37e16b6d10024c88a3271650adf189034bfd0d68bf45`；其passed same-candidate、capture_manifest SHA `116c8d10525a2183a98799797c1ec3b6b2470580ef0326cde4f411f0699946e3`与freeze一致。本次复用root已审2个resource child/0compile、原共享queue及清理结论，不重hash业务body，也不从capture两角色时间作收益推断。

独立用freeze元数据重算：

- 时间：`144*(max(online_child_audit_seconds，已含完整process) + 0.25秒预留) + 60秒nonchild + 1.275457023秒staging` = **176.08999206299993秒**，在600秒扣除30秒seal后预算内。
- 空间：144份页取整wire、canonical/query oracle、7个临时槽、64MiB metadata、retained capture及staging allocated合计 **2915424609 bytes**，与freeze完全一致且低于4GiB；原双卷各2GiB reserve保留。历史实测和投影不保证未来无长尾，仍依每child/全窗实际停止规则。

resource capture路径与唯一M2 screen输出路径独立，capture值不进入144项推断；没有滚动补样、重开M1或把R1结果当M2独立证据。正式收益仍须完整唯一窗口、恢复、离线audit和独立72对重算后由root判定。

以上相对路径以 `docs/coordination/c14-iteration-20261020-v2/` 为根；本次只审新freeze接入，不新增执行条件，不复算已完成R1。
