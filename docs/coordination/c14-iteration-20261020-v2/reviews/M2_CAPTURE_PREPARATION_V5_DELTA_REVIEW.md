# M2 capture v5 最小差分审查

日期：2026-10-20（Asia/Shanghai）；审查者 `v2_stats_review`。只读v4→v5差分及已指定小metadata/hash，不执行Python源码、queue、编译、native、原review重审或新实验。

**结论：实际v5准备源码通过差分审查。exact request/config及执行准入仍待root核定。首次定位尚无gate binder v2，本次不把旧binder的缺失断言视为已修复。**

## 实际版本与失败保留

- `tools/c14_iteration_20261020_v2/validation/screen/m2/prepare_capture_v5.py`：17518 bytes，SHA256 **`9356a43e5feda46cbc0dc9f20ea8ce21e9902faf11ebcc7ac48f8b47d35c22b7`**。这是本次审查实际版本；早期未执行草稿hash `3be228…` 不代表本版本。
- v4源码仍为SHA `3b922974dbc6b0b97b57a157a4520107e13e3868733caff0d2ee78eaa44b0f79`。`M2_CAPTURE_PREPARE_FAILURE_v4.json` 实际SHA `d736f93a4a7cdcbed16feeefddf6160ab21dcccc49a24f47d9a3dd614e17495a`，保留 `KeyError:candidate_exemption` 及0新output/compile/native/queue事实，没有覆写原parent CONTROL review。

## 已通过的差分

忽略CRLF/LF表示差异后，执行语义差分仅以下内容：

1. 新metadata文件names改为v5，保持独占新建；没有变更capture namespace、两角色、20秒child/120秒functional outer、0compile/重试/推断样本或原screen统计政策。
2. 移除对parent CONTROL不存在的顶层 `candidate_exemption` 索引。改为绑定原parent review精确SHA `812a151321ba3cb317747f91395ed6919bd4dd91ffc2e8b6c996a24ebc5ad0cf`，检查kind/status、153+14一致、308个实际member身份、空mismatches、compiled14 digest一致，以及完整“仅历史control packaging manifest例外、无candidate/code/header/dependency/workload例外”scope。
3. 再绑定原executor CONTROL事实文件SHA `1ac89b1d75c524b3165396f3fb6346cb2e9f2b9db48850acbcd670cf3ed0bdc8`，检查其实际存在的 `candidate_exemption=false`、same kind/status、153+14一致、actual/expected manifest与parent相同。对provenance和两manifest执行原有checked引用核对，未拓宽candidate豁免。
4. 新PRECONDITIONS只补executor_fact/historical_control_profile精确来源；仍引用原parent review。原桥接full/mechanism检查、bare profile/源/输入、原共享租约和后续精确request准入流程均不变。

本次未改执行worker；此前M2 worker `9dc7c7808c3cb8e5fc9c77ed5ed4cf34e6f51b82a5ca490c6751ba17a7b07aaf` 的统计、资源capture和完整truth审查继续按原身份适用。v5是修复metadata schema来源的派生，不是重开性能窗口或要求重跑门禁。

## Gate binder v2 的精确待补范围

本次首次文件定位只存在 `tools/c14_iteration_20261020_v2/validation/screen/bind_gates_m2.py` 原版。该版mechanism断言有动态命中事实，但缺少原screen freeze本就要求的 `static_actual_codegen_passed=true` 和 `unhooked_screen_binary=true`。

因此binder v2尚未在本次审查范围内通过；后续只需对其新增两项实际codegen/bare二进制证据接入做差分复核，不应修改worker、统计目标/族或要求新增编译/native。此pending只涉及后续完整gate/freeze接入，不否决本次v5 source readiness，也不要求阻塞独立有限资源capture准备。

root可基于实际当前v5身份生成新exact request，再核具体参数/hash决定capture准入。本文件不批准capture或screen执行，不复用旧失败记录代替成功，也不要求额外框架、采样或重复审查。
