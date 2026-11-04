# L2 Mac 门禁批次 2026-11-04-ckpt16e-d438627

- head `d438627ad0a39523e9c7190a57b8376cb1d97d28`（CKPT16E head = PR #191 d438627a (delta sync d5401486 merged with next_cpp26 1c528d67, #213 included; library = fe0dd726), build-r3d params）；anchor `1c528d67739bc94d91205c9dd2bf4bf1260253bf`（next_cpp26 1c528d67 (#213 merged), build-r3d params）
- CKPT16E: L2 gate of PR #191 (class L, delta sync) as merged with next_cpp26 1c528d67 (#213 in), d438627a vs 1c528d67, merge result vs mainline; rerun after CKPT16C failed on SLG width alone (UU on); merge tier of TEST_TIERS (core gate + G9R); each point built with its own bench/slg (both 23e86624); one deferred bisection point 298d718f (#191 on the mainline before #213; report only)
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `ba280f3bb21fe112072ac06c02328b8c89bdbec7c79ac847708b2b10e16d82d1`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9988 [0.9891, 1.0074] | 1.0004 [0.9918, 1.0080] | 0.9987 [0.9875, 1.0087] | 1.0087 | pass | 0.9951 / 0.9992 / 0.9998 | 有效 |
| formal-1（单批，只报） | 0.9987 [0.9847, 1.0112] | 0.9964 [0.9865, 1.0052] | 1.0032 [0.9870, 1.0177] | 1.0177 | pass | 0.9936 / 0.9997 / 0.9931 | 有效 |
| formal-2（单批，只报） | 0.9989 [0.9851, 1.0113] | 1.0044 [0.9902, 1.0171] | 0.9942 [0.9784, 1.0084] | 1.0171 | pass | 0.9965 / 0.9987 / 1.0067 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9520 / 1.0001 / 1.0516 | 0.9611 / 0.9995 / 1.0526 | 0.9424 / 0.9983 / 1.0460 |
| formal-1 | 0.9483 / 0.9997 / 1.0419 | 0.9625 / 0.9946 / 1.0352 | 0.9413 / 0.9988 / 1.0521 |
| formal-2 | 0.9546 / 1.0001 / 1.0535 | 0.9549 / 1.0030 / 1.0584 | 0.9464 / 0.9970 / 1.0424 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9992 [0.9850, 1.0136] | 0.9988 [0.9872, 1.0106] | 1.0004 [0.9901, 1.0107] | 0.9987 [0.9852, 1.0123] | 0.9925 [0.9784, 1.0069] | 0.9985 [0.9882, 1.0089] |
| formal-1 head÷anchor | 1.0033 [0.9872, 1.0196] | 0.9987 [0.9818, 1.0158] | 0.9964 [0.9845, 1.0084] | 1.0032 [0.9836, 1.0231] | 0.9972 [0.9728, 1.0222] | 0.9988 [0.9837, 1.0141] |
| formal-2 head÷anchor | 0.9951 [0.9711, 1.0197] | 0.9989 [0.9823, 1.0159] | 1.0044 [0.9872, 1.0218] | 0.9942 [0.9751, 1.0137] | 0.9879 [0.9726, 1.0034] | 0.9982 [0.9836, 1.0131] |
| 合并 head÷r3 | 0.9293 [0.9206, 0.9380] | 0.9438 [0.9316, 0.9562] | 0.9376 [0.9242, 0.9512] | 0.8382 [0.8267, 0.8498] | 0.9049 [0.8920, 0.9180] | 0.9297 [0.9187, 0.9409] |
| formal-1 head÷r3 | 0.9310 [0.9157, 0.9466] | 0.9443 [0.9271, 0.9619] | 0.9377 [0.9230, 0.9527] | 0.8450 [0.8267, 0.8637] | 0.9089 [0.8871, 0.9312] | 0.9312 [0.9159, 0.9467] |
| formal-2 head÷r3 | 0.9275 [0.9187, 0.9364] | 0.9434 [0.9253, 0.9618] | 0.9376 [0.9146, 0.9612] | 0.8314 [0.8171, 0.8460] | 0.9009 [0.8860, 0.9161] | 0.9283 [0.9119, 0.9450] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 1.0004 | 1.0074 | 是 |
| formal-1 | 0.9964 | 1.0044 | 是 |
| formal-2 | 1.0044 | 1.0161 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.775 / 0.790 通过 | 0.773 / 0.784 通过 |
| cista:mutate | 1.05 | 0.672 / 0.680 通过 | 0.673 / 0.685 通过 |
| cista:emit_after | 0.2 | 0.127 / 0.129 通过 | 0.125 / 0.127 通过 |
| flatbuffers:read_sum | 1.05 | 0.614 / 0.626 通过 | 0.610 / 0.621 通过 |
| flatbuffers:mutate | 1.05 | 0.604 / 0.614 通过 | 0.601 / 0.612 通过 |
| flatbuffers:emit_after | 0.2 | 0.119 / 0.121 通过 | 0.118 / 0.120 通过 |
| capnproto:read_sum | 1.05 | 0.197 / 0.200 通过 | 0.201 / 0.204 通过 |
| capnproto:mutate | 1.05 | 0.231 / 0.233 通过 | 0.232 / 0.236 通过 |
| capnproto:emit_after | 0.2 | 0.312 / 0.318 **不通过** | 0.297 / 0.304 **不通过** |
| cereal:read_sum | 1.05 | 0.835 / 0.848 通过 | 0.834 / 0.850 通过 |
| cereal:mutate | 1.05 | 0.983 / 0.995 通过 | 0.988 / 1.004 通过 |
| cereal:emit_after | 0.2 | 0.165 / 0.167 通过 | 0.164 / 0.167 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 10/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（retire_modified_state） | 无效（retire_modified_state, cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2003 [0.1939, 0.2069] | 0.4838 [0.4676, 0.5006] | 0.3329 [0.3232, 0.3430] | 0.3825 [0.3694, 0.3960] |
| formal-2 | 0.2061 [0.1992, 0.2134] | 0.5005 [0.4829, 0.5188] | 0.3383 [0.3263, 0.3508] | 0.3925 [0.3759, 0.4097] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0192）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9989 [0.9790, 1.0192] | 是 |
| B | 1.0013 [0.9870, 1.0159] | 是 |
| C | 0.9982 [0.9852, 1.0113] | 是 |
| D | 0.9984 [0.9741, 1.0232] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9989 [0.9790, 1.0192] / 0.9989 [0.9789, 1.0193] | 1.0013 [0.9870, 1.0159] / 1.0013 [0.9900, 1.0128] | 0.9982 [0.9852, 1.0113] / 0.9982 [0.9845, 1.0120] | 0.9984 [0.9741, 1.0232] / 0.9984 [0.9783, 1.0189] | head A 0.980 B 0.989 C 1.016 D 1.041；anchor A 0.996 B 0.995 C 1.009 D 1.021 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9989 [0.9891, 1.0088] | 1.0013 [0.9839, 1.0190] | 0.9982 [0.9856, 1.0108] | 0.9984 [0.9629, 1.0351] |
| 合并 | 0.9989 [0.9891, 1.0088] | 1.0013 [0.9839, 1.0190] | 0.9982 [0.9856, 1.0108] | 0.9984 [0.9629, 1.0351] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.233 / 0.237 (0.256) | 0.096 / 0.100 (0.108) | 0.381 / 0.387 (0.415) |
| B | 0.161 / 0.162 (0.168) | 0.077 / 0.078 (0.083) | 0.294 / 0.301 (0.317) |
| C | 0.117 / 0.119 (0.125) | 0.070 / 0.071 (0.073) | 0.351 / 0.357 (0.363) |
| D | 0.248 / 0.259 (0.287) | 0.352 / 0.362 (0.390) | 0.941 / 0.964 (0.987) |

## 4. 环境

- 开跑条件 formal-1：2026-11-04T11:22:07+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=2.05, quiet_s=241.1, screen={'WindowServer': 4.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-04T11:34:14+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.87, quiet_s=241.1, screen={'WindowServer': 4.3, 'UURemoteServer': 0.7, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-04T11:45:17+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.49, quiet_s=241.0, screen={'WindowServer': 4.8, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-04T11:26:08+00:00 → 2026-11-04T11:34:14+00:00，load [2.0517578125, 1.814453125, 1.8076171875] → [2.57275390625, 2.30126953125, 2.04248046875]，CPU 采样 {'samples': 238, 'other_busy_samples': 10, 'compile_samples': 0, 'other_busy_top': {'launchd': 2, 'Claude Helper (Renderer)': 2, 'contactsd': 2, 'sysmond': 1, 'tccd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.84}
  - 计时期间屏幕采样：0/122 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 5.7, 'UURemoteServer': 1.9, 'parsecd': 0.0}；远程桌面活动（≥5%）0/122 次采样，只记录；WindowServer ≥20% 0/122 次采样，有效
- formal-2：2026-11-04T11:38:15+00:00 → 2026-11-04T11:45:17+00:00，load [1.87158203125, 2.14892578125, 2.0419921875] → [2.681640625, 2.27490234375, 2.099609375]，CPU 采样 {'samples': 207, 'other_busy_samples': 53, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 51, 'Claude Helper (Renderer)': 4, 'WallpaperAerialsExtension': 1, 'duetexpertd': 1, 'spotlightknowledged': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.78}
  - 计时期间屏幕采样：0/106 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 46.0, 'UURemoteServer': 2.2, 'parsecd': 0.0}；远程桌面活动（≥5%）0/106 次采样，只记录；WindowServer ≥20% 33/106 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.9, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.078，mutate 0.051，emit_after 0.092；mutate 按副本 1.0025 / 0.9960 / 1.0007
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.063，mutate 0.068，emit_after 0.084；mutate 按副本 1.0056 / 1.0031 / 0.9875
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-04T11:22:07+00:00
- 二进制 sha256：head `242c1888c3c0301a…`；head_aa `242c1888c3c0301a…`；anchor `b749a0f4d98e9678…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 23e86624, anchor 23e86624; bisection points their own)（sha256 `c60c7c18d80c929261e1eb847c940c8265ed98df984e3b8d9e97200a0764cf34`），slg_head `2f74a89b48f0d469…`，slg_anch `9e89e86bb227e857…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
