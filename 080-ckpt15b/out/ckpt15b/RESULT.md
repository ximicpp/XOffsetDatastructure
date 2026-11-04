# L2 Mac 门禁批次 2026-11-04-ckpt15b-daf56e8

- head `daf56e89cd4c4ec8a8d838e52846864e24bf82bd`（CKPT15b head = PR #213 daf56e89 (keep a reloaded buffer's free table instead of recycling it, 4a54734d, merged with next_cpp26 49ce3c68), build-r3d params）；anchor `49ce3c68fa60ff9b531f1fa482fc4fb4b8e65479`（next_cpp26 49ce3c68 (#212; mainline merged into PR #213), build-r3d params）
- CKPT15b: L2 gate of PR #213 (class L, keep a reloaded buffer's free table) as merged with the current next_cpp26 (daf56e89 vs 49ce3c68, merge result vs mainline as GATE_D4), merge tier of TEST_TIERS (core gate + G9R); anchor 49ce3c68; each point built with its own bench/slg; one deferred bisection point 4a54734d (report only)
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `63fc121f0cf53dba93aa05daa2a38910f189433856e7e6667c0905ec3e4f2a88`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9973 [0.9873, 1.0062] | 0.9943 [0.9855, 1.0021] | 1.0003 [0.9871, 1.0122] | 1.0122 | pass | 1.0013 / 1.0054 / 1.0032 | 有效 |
| formal-1（单批，只报） | 1.0015 [0.9863, 1.0152] | 0.9940 [0.9814, 1.0053] | 0.9991 [0.9811, 1.0153] | 1.0153 | pass | 0.9986 / 1.0053 / 1.0097 | 有效 |
| formal-2（单批，只报） | 0.9930 [0.9799, 1.0047] | 0.9945 [0.9820, 1.0057] | 1.0015 [0.9816, 1.0195] | 1.0195 | pass | 1.0041 / 1.0055 / 0.9968 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9419 / 1.0005 / 1.0481 | 0.9489 / 0.9958 / 1.0504 | 0.9198 / 1.0006 / 1.0947 |
| formal-1 | 0.9351 / 1.0064 / 1.0608 | 0.9501 / 0.9961 / 1.0509 | 0.9304 / 0.9962 / 1.0987 |
| formal-2 | 0.9438 / 0.9952 / 1.0415 | 0.9379 / 0.9955 / 1.0442 | 0.9146 / 1.0080 / 1.0811 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9944 [0.9840, 1.0050] | 0.9973 [0.9853, 1.0093] | 0.9943 [0.9838, 1.0049] | 1.0003 [0.9844, 1.0164] | 1.0191 [1.0053, 1.0331] | 0.9997 [0.9901, 1.0093] |
| formal-1 head÷anchor | 0.9918 [0.9757, 1.0080] | 1.0015 [0.9831, 1.0203] | 0.9940 [0.9788, 1.0095] | 0.9991 [0.9774, 1.0213] | 1.0242 [1.0010, 1.0479] | 1.0021 [0.9873, 1.0170] |
| formal-2 head÷anchor | 0.9971 [0.9831, 1.0113] | 0.9930 [0.9772, 1.0091] | 0.9945 [0.9794, 1.0099] | 1.0015 [0.9775, 1.0261] | 1.0140 [0.9984, 1.0299] | 0.9973 [0.9847, 1.0100] |
| 合并 head÷r3 | 0.9323 [0.9250, 0.9397] | 0.9473 [0.9369, 0.9577] | 0.9394 [0.9297, 0.9492] | 0.8341 [0.8232, 0.8451] | 0.9149 [0.9003, 0.9297] | 0.9328 [0.9242, 0.9414] |
| formal-1 head÷r3 | 0.9279 [0.9166, 0.9394] | 0.9454 [0.9305, 0.9605] | 0.9378 [0.9239, 0.9519] | 0.8280 [0.8138, 0.8424] | 0.9195 [0.9006, 0.9388] | 0.9316 [0.9202, 0.9432] |
| formal-2 head÷r3 | 0.9368 [0.9274, 0.9463] | 0.9492 [0.9341, 0.9644] | 0.9410 [0.9270, 0.9553] | 0.8402 [0.8234, 0.8574] | 0.9103 [0.8875, 0.9337] | 0.9340 [0.9208, 0.9473] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9943 | 1.0015 | 是 |
| formal-1 | 0.9940 | 1.0044 | 是 |
| formal-2 | 0.9945 | 1.0048 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.770 / 0.782 通过 | 0.773 / 0.786 通过 |
| cista:mutate | 1.05 | 0.668 / 0.677 通过 | 0.671 / 0.679 通过 |
| cista:emit_after | 0.2 | 0.128 / 0.131 通过 | 0.130 / 0.132 通过 |
| flatbuffers:read_sum | 1.05 | 0.615 / 0.626 通过 | 0.609 / 0.620 通过 |
| flatbuffers:mutate | 1.05 | 0.598 / 0.609 通过 | 0.601 / 0.612 通过 |
| flatbuffers:emit_after | 0.2 | 0.121 / 0.123 通过 | 0.121 / 0.123 通过 |
| capnproto:read_sum | 1.05 | 0.203 / 0.206 通过 | 0.199 / 0.201 通过 |
| capnproto:mutate | 1.05 | 0.231 / 0.233 通过 | 0.229 / 0.231 通过 |
| capnproto:emit_after | 0.2 | 0.303 / 0.308 **不通过** | 0.302 / 0.308 **不通过** |
| cereal:read_sum | 1.05 | 0.836 / 0.848 通过 | 0.833 / 0.845 通过 |
| cereal:mutate | 1.05 | 0.983 / 0.994 通过 | 0.985 / 0.996 通过 |
| cereal:emit_after | 0.2 | 0.167 / 0.170 通过 | 0.169 / 0.172 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（retire_modified_state） | 无效（cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2251 [0.2162, 0.2344] | 0.5495 [0.5278, 0.5720] | 0.3702 [0.3559, 0.3850] | 0.4344 [0.4176, 0.4519] |
| formal-2 | 0.2223 [0.2162, 0.2285] | 0.5410 [0.5264, 0.5560] | 0.3629 [0.3519, 0.3741] | 0.4277 [0.4160, 0.4397] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；2 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0134）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9914 [0.9787, 1.0043] | 是 |
| B | 1.0014 [0.9902, 1.0128] | 是 |
| C | 1.0004 [0.9876, 1.0134] | 是 |
| D | 1.0028 [0.9883, 1.0176] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9909 [0.9712, 1.0110] / 0.9909 [0.9709, 1.0113] | 1.0029 [0.9863, 1.0199] / 1.0029 [0.9928, 1.0132] | 1.0057 [0.9890, 1.0228] / 1.0057 [0.9937, 1.0179] | 1.0061 [0.9827, 1.0301] / 1.0061 [0.9836, 1.0291] | head A 1.003 B 0.986 C 0.973 D 1.002；anchor A 1.000 B 0.986 C 0.977 D 1.002 |
| slg-2 | 0.9920 [0.9714, 1.0130] / 0.9920 [0.9803, 1.0038] | 0.9999 [0.9800, 1.0202] / 0.9999 [0.9922, 1.0076] | 0.9951 [0.9713, 1.0195] / 0.9951 [0.9815, 1.0089] | 0.9996 [0.9734, 1.0264] / 0.9996 [0.9743, 1.0255] | head A 0.972 B 0.968 C 0.981 D 0.990；anchor A 0.973 B 0.974 C 0.965 D 1.037 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9909 [0.9599, 1.0229] | 1.0029 [0.9786, 1.0279] | 1.0057 [0.9783, 1.0340] | 1.0061 [0.9484, 1.0673] |
| slg-2 | 0.9920 [0.9668, 1.0178] | 0.9999 [0.9802, 1.0199] | 0.9951 [0.9600, 1.0315] | 0.9996 [0.9466, 1.0555] |
| 合并 | 0.9914 [0.9836, 0.9993] | 1.0014 [0.9948, 1.0081] | 1.0004 [0.9879, 1.0131] | 1.0028 [0.9865, 1.0194] |

进程数 head 12、anchor 12。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.232 / 0.234 (0.251) | 0.096 / 0.098 (0.106) | 0.379 / 0.382 (0.403) |
| B | 0.162 / 0.164 (0.167) | 0.077 / 0.078 (0.080) | 0.294 / 0.297 (0.299) |
| C | 0.119 / 0.120 (0.128) | 0.070 / 0.071 (0.076) | 0.357 / 0.361 (0.382) |
| D | 0.247 / 0.249 (0.284) | 0.355 / 0.359 (0.401) | 0.946 / 0.968 (1.022) |

## 4. 环境

- 开跑条件 formal-1：2026-11-04T07:55:36+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.95, quiet_s=241.1, screen={'WindowServer': 5.0, 'UURemoteServer': 2.8, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-04T08:06:47+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=2.2, quiet_s=241.1, screen={'WindowServer': 4.6, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-04T08:18:06+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.41, quiet_s=241.2, screen={'WindowServer': 4.5, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg-2：2026-11-04T08:22:54+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.32, quiet_s=241.1, screen={'WindowServer': 3.0, 'UURemoteServer': 0.1, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-04T07:59:37+00:00 → 2026-11-04T08:06:47+00:00，load [1.95263671875, 2.0029296875, 2.1337890625] → [2.47265625, 2.27978515625, 2.21435546875]，CPU 采样 {'samples': 211, 'other_busy_samples': 7, 'compile_samples': 0, 'other_busy_top': {'Claude Helper (Renderer)': 2, 'duetexpertd': 1, 'deleted': 1, 'Codex (Renderer)': 1, 'runningboardd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.8}
  - 计时期间屏幕采样：0/108 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 7.7, 'UURemoteServer': 2.8, 'parsecd': 0.0}；远程桌面活动（≥5%）0/108 次采样，只记录；WindowServer ≥20% 0/108 次采样，有效
- formal-2：2026-11-04T08:10:49+00:00 → 2026-11-04T08:18:06+00:00，load [2.1953125, 2.07421875, 2.12158203125] → [1.849609375, 1.89697265625, 2.00048828125]，CPU 采样 {'samples': 215, 'other_busy_samples': 3, 'compile_samples': 0, 'other_busy_top': {'Codex (Renderer)': 2, 'transparencyd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.47}
  - 计时期间屏幕采样：0/110 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 6.1, 'UURemoteServer': 2.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/110 次采样，只记录；WindowServer ≥20% 0/110 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.5, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- slg-2 计时期间屏幕采样：0/12 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.8, 'UURemoteServer': 1.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/12 次采样，只记录；WindowServer ≥20% 0/12 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.083，mutate 0.063，emit_after 0.107；mutate 按副本 1.0071 / 1.0071 / 1.0016
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.082，mutate 0.058，emit_after 0.097；mutate 按副本 1.0056 / 1.0038 / 1.0073
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-04T07:55:36+00:00
- 二进制 sha256：head `b749a0f4d98e9678…`；head_aa `b749a0f4d98e9678…`；anchor `b325298ef9e33efe…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 55ba4e84, anchor 55ba4e84; bisection points their own)（sha256 `efa09929732a456b5b8a9169ee03fbb3574f975bc54a60a8e4db024bf9c544e3`），slg_head `6d66e7e3dafdb293…`，slg_anch `4d8a2b8c6d246dc6…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
