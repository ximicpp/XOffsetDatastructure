# L2 Mac 门禁批次 2026-11-12-stage360-372c117

- head `372c117e2a116d523796f0365a0ceb720ba010cc`（STAGE360 head = v3.6.0 372c117e (tag object cd84c0f3; release PR #270; 16 merges since v3.5.0), build-r3d params）；anchor `0cbc130decd722fc1baaa4c96882c1085e5b20a3`（v3.5.0 0cbc130d (previous release tag), build-r3d params）
- STAGE360: the 3.6.0 post-release stage review (docs/RELEASING.md section 7: L2 rules, report only): v3.6.0 372c117e vs v3.5.0 0cbc130d; merge tier of TEST_TIERS (core gate + G9R); every point built with bench/slg 53cccd53 (v3.5.0: the head's bench/slg needs xsync XRecordDelta::member_count/update of #260/#263, absent in v3.5.0); sc_xoffset from each point's own bench/formal (the meta-r3 adapter does not compile after #254); deferred bisection points (#255 9e636300, #262 1f9a8e92, #260 ce658e8e, #264 9e173149, #265 aad6cd86, #256 cf9b63e5, #266 bb537d0d); known: Mac A.req_encode +6.3% from #266, accepted; no report-only step
- **结论：pending**。正式部分 pending（合并 3 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `31ec7386dd452282dcee2ed63271f506b14c0dd6b67747834dee5e035de0c7b1`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 3 批（判定）** | 0.9987 [0.9924, 1.0044] | 0.9924 [0.9860, 0.9982] | 1.0105 [1.0020, 1.0180] | 1.0180 | pending | 1.0008 / 1.0023 / 1.0002 | 有效 |
| formal-1（单批，只报） | 1.0088 [0.9942, 1.0219] | 1.0036 [0.9903, 1.0154] | 1.0161 [1.0011, 1.0297] | 1.0297 | pending | 0.9906 / 0.9907 / 0.9963 | 有效 |
| formal-2（单批，只报） | 0.9881 [0.9798, 0.9956] | 0.9883 [0.9783, 0.9972] | 1.0099 [0.9962, 1.0222] | 1.0222 | pending | 1.0069 / 1.0091 / 0.9995 | 有效 |
| formal-3（单批，只报） | 0.9993 [0.9905, 1.0072] | 0.9856 [0.9758, 0.9943] | 1.0054 [0.9896, 1.0195] | 1.0195 | pass | 1.0050 / 1.0073 / 1.0049 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

**两批不一致，未合并判定（待定，先诊断再加跑一批）：** formal-1 vs formal-2 read_sum: 1.0088 vs 0.9881

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9605 / 0.9968 / 1.0338 | 0.9503 / 0.9909 / 1.0313 | 0.9539 / 1.0089 / 1.0746 |
| formal-1 | 0.9708 / 1.0059 / 1.0452 | 0.9644 / 0.9997 / 1.0350 | 0.9644 / 1.0109 / 1.0821 |
| formal-2 | 0.9494 / 0.9901 / 1.0223 | 0.9474 / 0.9832 / 1.0279 | 0.9526 / 1.0092 / 1.0762 |
| formal-3 | 0.9653 / 0.9929 / 1.0347 | 0.9426 / 0.9867 / 1.0299 | 0.9489 / 1.0058 / 1.0692 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9961 [0.9902, 1.0020] | 0.9987 [0.9911, 1.0064] | 0.9924 [0.9848, 1.0002] | 1.0105 [1.0003, 1.0207] | 1.0109 [1.0030, 1.0188] | 0.9994 [0.9931, 1.0058] |
| formal-1 head÷anchor | 0.9975 [0.9861, 1.0090] | 1.0088 [0.9911, 1.0268] | 1.0036 [0.9876, 1.0198] | 1.0161 [0.9979, 1.0347] | 1.0118 [0.9995, 1.0242] | 1.0076 [0.9933, 1.0222] |
| formal-2 head÷anchor | 0.9993 [0.9908, 1.0077] | 0.9881 [0.9781, 0.9983] | 0.9883 [0.9762, 1.0004] | 1.0099 [0.9934, 1.0268] | 1.0134 [0.9999, 1.0269] | 0.9936 [0.9848, 1.0026] |
| formal-3 head÷anchor | 0.9915 [0.9803, 1.0029] | 0.9993 [0.9886, 1.0101] | 0.9856 [0.9738, 0.9975] | 1.0054 [0.9864, 1.0247] | 1.0075 [0.9915, 1.0237] | 0.9971 [0.9877, 1.0066] |
| 合并 head÷r3 | 0.9410 [0.9362, 0.9457] | 0.9640 [0.9561, 0.9720] | 0.9293 [0.9220, 0.9366] | 0.8344 [0.8261, 0.8427] | 0.9129 [0.9055, 0.9203] | 0.9379 [0.9317, 0.9443] |
| formal-1 head÷r3 | 0.9446 [0.9355, 0.9538] | 0.9670 [0.9495, 0.9847] | 0.9370 [0.9221, 0.9520] | 0.8411 [0.8238, 0.8588] | 0.9131 [0.9015, 0.9248] | 0.9421 [0.9286, 0.9559] |
| formal-2 head÷r3 | 0.9382 [0.9307, 0.9459] | 0.9589 [0.9463, 0.9718] | 0.9244 [0.9122, 0.9367] | 0.8329 [0.8197, 0.8463] | 0.9143 [0.9012, 0.9276] | 0.9340 [0.9238, 0.9444] |
| formal-3 head÷r3 | 0.9401 [0.9316, 0.9486] | 0.9661 [0.9549, 0.9775] | 0.9265 [0.9154, 0.9377] | 0.8291 [0.8160, 0.8423] | 0.9112 [0.8967, 0.9260] | 0.9376 [0.9286, 0.9467] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9924 | 0.9977 | 是 |
| formal-1 | 1.0036 | 1.0144 | 是 |
| formal-2 | 0.9883 | 0.9964 | 是 |
| formal-3 | 0.9856 | 0.9936 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 | formal-3 |
| --- | --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.816 / 0.833 通过 | 0.810 / 0.820 通过 | 0.810 / 0.822 通过 |
| cista:mutate | 1.05 | 0.674 / 0.682 通过 | 0.667 / 0.674 通过 | 0.665 / 0.672 通过 |
| cista:emit_after | 0.2 | 0.134 / 0.136 通过 | 0.134 / 0.135 通过 | 0.130 / 0.132 通过 |
| flatbuffers:read_sum | 1.05 | 0.630 / 0.641 通过 | 0.616 / 0.626 通过 | 0.622 / 0.631 通过 |
| flatbuffers:mutate | 1.05 | 0.607 / 0.617 通过 | 0.593 / 0.605 通过 | 0.600 / 0.610 通过 |
| flatbuffers:emit_after | 0.2 | 0.125 / 0.127 通过 | 0.125 / 0.127 通过 | 0.123 / 0.124 通过 |
| capnproto:read_sum | 1.05 | 0.201 / 0.204 通过 | 0.198 / 0.199 通过 | 0.198 / 0.199 通过 |
| capnproto:mutate | 1.05 | 0.233 / 0.237 通过 | 0.230 / 0.232 通过 | 0.231 / 0.233 通过 |
| capnproto:emit_after | 0.2 | 0.305 / 0.310 **不通过** | 0.303 / 0.308 **不通过** | 0.299 / 0.303 **不通过** |
| cereal:read_sum | 1.05 | 0.864 / 0.878 通过 | 0.845 / 0.855 通过 | 0.847 / 0.861 通过 |
| cereal:mutate | 1.05 | 0.988 / 1.002 通过 | 0.973 / 0.983 通过 | 0.968 / 0.985 通过 |
| cereal:emit_after | 0.2 | 0.175 / 0.178 通过 | 0.174 / 0.176 通过 | 0.171 / 0.173 通过 |
| 通过数 head | | 11/12 | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 9/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（retire_modified_state, cleanup） | 有效 | 无效（cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2447 [0.2412, 0.2481] | 0.5946 [0.5849, 0.6044] | 0.3981 [0.3928, 0.4034] | 0.4750 [0.4681, 0.4819] |
| formal-2 | 0.2441 [0.2404, 0.2479] | 0.5985 [0.5899, 0.6072] | 0.3958 [0.3896, 0.4021] | 0.4727 [0.4654, 0.4800] |
| formal-3 | 0.2335 [0.2289, 0.2382] | 0.5703 [0.5585, 0.5824] | 0.3816 [0.3742, 0.3892] | 0.4497 [0.4407, 0.4589] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；2 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0115）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9909 [0.9762, 1.0059] | 是 |
| B | 0.9807 [0.9707, 0.9908] | 是 |
| C | 0.9992 [0.9870, 1.0115] | 是 |
| D | 0.9978 [0.9836, 1.0121] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9922 [0.9683, 1.0167] / 0.9922 [0.9776, 1.0070] | 0.9839 [0.9684, 0.9996] / 0.9839 [0.9755, 0.9923] | 1.0022 [0.9790, 1.0260] / 1.0022 [0.9877, 1.0169] | 1.0013 [0.9778, 1.0253] / 1.0013 [0.9769, 1.0262] | head A 0.981 B 0.979 C 0.971 D 1.028；anchor A 0.967 B 0.978 C 0.970 D 0.992 |
| slg-2 | 0.9897 [0.9699, 1.0098] / 0.9897 [0.9688, 1.0110] | 0.9776 [0.9635, 0.9918] / 0.9776 [0.9635, 0.9919] | 0.9962 [0.9816, 1.0110] / 0.9962 [0.9889, 1.0036] | 0.9943 [0.9711, 1.0180] / 0.9943 [0.9702, 1.0189] | head A 1.004 B 0.986 C 0.976 D 1.037；anchor A 0.993 B 0.993 C 0.981 D 0.993 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9922 [0.9629, 1.0224] | 0.9839 [0.9541, 1.0146] | 1.0022 [0.9348, 1.0745] | 1.0013 [0.9278, 1.0805] |
| slg-2 | 0.9897 [0.9117, 1.0742] | 0.9776 [0.9392, 1.0175] | 0.9962 [0.9939, 0.9985] | 0.9943 [0.8901, 1.1106] |
| 合并 | 0.9909 [0.9743, 1.0079] | 0.9807 [0.9699, 0.9917] | 0.9992 [0.9850, 1.0136] | 0.9978 [0.9717, 1.0246] |

进程数 head 12、anchor 12。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.220 / 0.221 (0.233) | 0.088 / 0.090 (0.096) | 0.353 / 0.357 (0.371) |
| B | 0.159 / 0.160 (0.162) | 0.075 / 0.076 (0.079) | 0.288 / 0.290 (0.296) |
| C | 0.118 / 0.119 (0.127) | 0.069 / 0.070 (0.074) | 0.350 / 0.353 (0.374) |
| D | 0.249 / 0.251 (0.274) | 0.346 / 0.350 (0.378) | 0.923 / 0.936 (0.966) |

## 4. 环境

- 开跑条件 formal-1：2026-11-12T08:28:34+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.32, quiet_s=241.1, screen={'WindowServer': 1.3, 'UURemoteServer': 0.5, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-12T08:40:27+00:00 ok=True, quiet_s_required=180.0, waited_s=241.3, load1=1.69, quiet_s=241.3, screen={'WindowServer': 1.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-12T08:51:53+00:00 ok=True, quiet_s_required=180.0, waited_s=241.3, load1=1.41, quiet_s=241.2, screen={'WindowServer': 1.2, 'UURemoteServer': 0.1, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg-2：2026-11-12T08:56:40+00:00 ok=True, quiet_s_required=180.0, waited_s=241.3, load1=1.21, quiet_s=241.2, screen={'WindowServer': 1.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-3：2026-11-12T11:33:23+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.11, quiet_s=241.1, screen={'WindowServer': 1.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-12T08:32:36+00:00 → 2026-11-12T08:40:27+00:00，load [1.31640625, 1.59130859375, 1.5361328125] → [1.6826171875, 1.73291015625, 1.63623046875]，CPU 采样 {'samples': 232, 'other_busy_samples': 7, 'compile_samples': 0, 'other_busy_top': {'UURemote': 2, 'Codex (Aperitif Renderer)': 2, 'modelcatalogd': 1, 'stickersd': 1, 'sysmond': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.11}
  - 计时期间屏幕采样：0/118 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.9, 'UURemoteServer': 1.3, 'parsecd': 0.0}；远程桌面活动（≥5%）0/118 次采样，只记录；WindowServer ≥20% 0/118 次采样，有效
- formal-2：2026-11-12T08:44:28+00:00 → 2026-11-12T08:51:53+00:00，load [1.6884765625, 1.59619140625, 1.58984375] → [1.80517578125, 1.6767578125, 1.61865234375]，CPU 采样 {'samples': 219, 'other_busy_samples': 11, 'compile_samples': 0, 'other_busy_top': {'Codex (Aperitif Renderer)': 3, 'mediaanalysisd': 3, 'UURemote': 2, 'launchd': 2, 'mds_stores': 1}, 'chrome_busy_mean': 0.4, 'chrome_busy_max': 53.0, 'load1_max': 2.08}
  - 计时期间屏幕采样：0/112 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.9, 'UURemoteServer': 0.9, 'parsecd': 0.0}；远程桌面活动（≥5%）0/112 次采样，只记录；WindowServer ≥20% 0/112 次采样，有效
- formal-3：2026-11-12T11:37:25+00:00 → 2026-11-12T11:44:39+00:00，load [1.1064453125, 1.59130859375, 1.70556640625] → [1.7041015625, 1.8212890625, 1.78173828125]，CPU 采样 {'samples': 214, 'other_busy_samples': 6, 'compile_samples': 0, 'other_busy_top': {'Codex (Aperitif Renderer)': 2, 'launchd': 1, 'UURemote': 1, 'mds_stores': 1, 'mediaanalysisd-access': 1}, 'chrome_busy_mean': 0.7, 'chrome_busy_max': 80.3, 'load1_max': 2.38}
  - 计时期间屏幕采样：0/109 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.8, 'UURemoteServer': 1.8, 'parsecd': 0.0}；远程桌面活动（≥5%）0/109 次采样，只记录；WindowServer ≥20% 0/109 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 1.3, 'UURemoteServer': 0.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- slg-2 计时期间屏幕采样：0/12 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 2.3, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/12 次采样，只记录；WindowServer ≥20% 0/12 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.094，mutate 0.078，emit_after 0.089；mutate 按副本 0.9791 / 0.9921 / 1.0010
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.058，mutate 0.060，emit_after 0.086；mutate 按副本 1.0025 / 1.0129 / 1.0118
- formal-3 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.063，mutate 0.055，emit_after 0.077；mutate 按副本 1.0039 / 1.0065 / 1.0116
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-12T08:28:34+00:00
- 二进制 sha256：head `d0a9aefb6d3c4aad…`；head_aa `d0a9aefb6d3c4aad…`；anchor `74d0abb9b509a923…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 bench/slg 53cccd53 of v3.5.0 for every point (git tree 53cccd53; head, anchor and bisection points)（sha256 `98d4ef285b493b3a6c36eb4cda8b6a9f42655f4920f942dc778487c194ba5f83`），slg_head `0dbadb915455be41…`，slg_anch `4a64fea7c86651d0…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
