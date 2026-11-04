# L2 Mac 门禁批次 2026-11-03-ckpt12-33c3478

- head `33c34789df8164d8940e3e1bb4a026f371fec931`（CKPT12 head = class L integration claude/int-save-root-20261103 33c34789 (#207: verified save no longer shrinks + save_verified_into one-pass fast path; #208: root<T>() cache, two words 16-byte aligned), build-r3d params）；anchor `0bf0aa1f426486ea48645ba4c6ac80c55f86beba`（next_cpp26 0bf0aa1f (#206; the integration base), build-r3d params）
- CKPT12: L2 gate of the class L integration of #207 and #208 (33c34789), merge tier of TEST_TIERS (core gate, no G9R); anchor 0bf0aa1f; each point built with its own bench/slg; one deferred bisection point 177d2703 (#207 only)
- **结论：fail**。正式部分 pass（合并 2 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `e1c16f15bf4a0033cf8a156ad22ca812c9173c780080dd2531f02eafc707442e`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9997 [0.9911, 1.0074] | 0.9938 [0.9864, 1.0004] | 1.0027 [0.9891, 1.0149] | 1.0149 | pass | 1.0040 / 1.0036 / 0.9947 | 有效 |
| formal-1（单批，只报） | 0.9963 [0.9834, 1.0078] | 0.9915 [0.9817, 1.0002] | 0.9980 [0.9795, 1.0147] | 1.0147 | pass | 1.0091 / 1.0094 / 0.9919 | 有效 |
| formal-2（单批，只报） | 1.0031 [0.9913, 1.0137] | 0.9961 [0.9847, 1.0063] | 1.0075 [0.9871, 1.0259] | 1.0259 | pending | 0.9990 / 0.9978 / 0.9976 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9522 / 1.0007 / 1.0595 | 0.9542 / 0.9916 / 1.0323 | 0.9310 / 0.9990 / 1.0845 |
| formal-1 | 0.9433 / 1.0001 / 1.0506 | 0.9559 / 0.9920 / 1.0296 | 0.9373 / 0.9938 / 1.1094 |
| formal-2 | 0.9604 / 1.0007 / 1.0627 | 0.9475 / 0.9908 / 1.0398 | 0.9274 / 1.0128 / 1.0801 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9992 [0.9906, 1.0079] | 0.9997 [0.9893, 1.0102] | 0.9938 [0.9849, 1.0028] | 1.0027 [0.9864, 1.0193] | 1.0105 [0.9945, 1.0267] | 1.0000 [0.9916, 1.0084] |
| formal-1 head÷anchor | 0.9962 [0.9834, 1.0091] | 0.9963 [0.9807, 1.0121] | 0.9915 [0.9797, 1.0034] | 0.9980 [0.9757, 1.0208] | 1.0178 [0.9961, 1.0399] | 0.9984 [0.9861, 1.0108] |
| formal-2 head÷anchor | 1.0023 [0.9904, 1.0143] | 1.0031 [0.9889, 1.0175] | 0.9961 [0.9824, 1.0101] | 1.0075 [0.9829, 1.0327] | 1.0033 [0.9793, 1.0278] | 1.0015 [0.9896, 1.0136] |
| 合并 head÷r3 | 0.9325 [0.9254, 0.9397] | 0.9444 [0.9333, 0.9557] | 0.9406 [0.9325, 0.9488] | 0.8426 [0.8300, 0.8553] | 0.9058 [0.8921, 0.9197] | 0.9310 [0.9227, 0.9393] |
| formal-1 head÷r3 | 0.9312 [0.9210, 0.9416] | 0.9413 [0.9256, 0.9573] | 0.9398 [0.9290, 0.9508] | 0.8462 [0.8283, 0.8644] | 0.9036 [0.8831, 0.9246] | 0.9290 [0.9174, 0.9407] |
| formal-2 head÷r3 | 0.9337 [0.9235, 0.9441] | 0.9476 [0.9312, 0.9643] | 0.9414 [0.9289, 0.9541] | 0.8389 [0.8208, 0.8575] | 0.9079 [0.8891, 0.9272] | 0.9329 [0.9207, 0.9454] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9938 | 0.9999 | 是 |
| formal-1 | 0.9915 | 0.9995 | 是 |
| formal-2 | 0.9961 | 1.0055 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.773 / 0.782 通过 | 0.780 / 0.789 通过 |
| cista:mutate | 1.05 | 0.672 / 0.679 通过 | 0.672 / 0.680 通过 |
| cista:emit_after | 0.2 | 0.134 / 0.137 通过 | 0.134 / 0.136 通过 |
| flatbuffers:read_sum | 1.05 | 0.614 / 0.624 通过 | 0.619 / 0.631 通过 |
| flatbuffers:mutate | 1.05 | 0.593 / 0.604 通过 | 0.603 / 0.617 通过 |
| flatbuffers:emit_after | 0.2 | 0.125 / 0.127 通过 | 0.124 / 0.127 通过 |
| capnproto:read_sum | 1.05 | 0.203 / 0.205 通过 | 0.205 / 0.207 通过 |
| capnproto:mutate | 1.05 | 0.230 / 0.232 通过 | 0.230 / 0.233 通过 |
| capnproto:emit_after | 0.2 | 0.307 / 0.313 **不通过** | 0.306 / 0.312 **不通过** |
| cereal:read_sum | 1.05 | 0.829 / 0.842 通过 | 0.840 / 0.852 通过 |
| cereal:mutate | 1.05 | 0.962 / 0.979 通过 | 0.980 / 0.992 通过 |
| cereal:emit_after | 0.2 | 0.172 / 0.175 通过 | 0.172 / 0.175 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 10/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（cleanup） | 无效（cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2419 [0.2369, 0.2470] | 0.5890 [0.5770, 0.6013] | 0.3917 [0.3837, 0.3998] | 0.4651 [0.4548, 0.4756] |
| formal-2 | 0.2433 [0.2398, 0.2469] | 0.5946 [0.5850, 0.6045] | 0.3965 [0.3903, 0.4027] | 0.4686 [0.4616, 0.4756] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0307）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9913 [0.9773, 1.0056] | 是 |
| B | 0.9779 [0.9500, 1.0065] | 是 |
| C | 0.9917 [0.9541, 1.0307] | **否** |
| D | 0.9972 [0.9730, 1.0220] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9913 [0.9773, 1.0056] / 0.9913 [0.9790, 1.0038] | 0.9779 [0.9500, 1.0065] / 0.9779 [0.9510, 1.0055] | 0.9917 [0.9541, 1.0307] / 0.9917 [0.9538, 1.0310] | 0.9972 [0.9730, 1.0220] / 0.9972 [0.9722, 1.0228] | head A 0.978 B 0.992 C 0.968 D 1.011；anchor A 0.996 B 1.041 C 1.050 D 0.982 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9913 [0.9536, 1.0305] | 0.9779 [0.9131, 1.0472] | 0.9917 [0.8880, 1.1075] | 0.9972 [0.9123, 1.0900] |
| 合并 | 0.9913 [0.9536, 1.0305] | 0.9779 [0.9131, 1.0472] | 0.9917 [0.8880, 1.1075] | 0.9972 [0.9123, 1.0900] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.232 / 0.236 (0.249) | 0.095 / 0.097 (0.103) | 0.373 / 0.379 (0.396) |
| B | 0.157 / 0.160 (0.162) | 0.075 / 0.076 (0.078) | 0.285 / 0.289 (0.292) |
| C | 0.119 / 0.123 (0.130) | 0.071 / 0.072 (0.077) | 0.355 / 0.363 (0.387) |
| D | 0.246 / 0.252 (0.281) | 0.354 / 0.361 (0.402) | 0.936 / 0.976 (1.006) |

## 4. 环境

- 开跑条件 formal-1：2026-11-03T23:24:17+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=2.56, quiet_s=241.0, screen={'WindowServer': 5.3, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-03T23:36:23+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=2.6, quiet_s=241.0, screen={'WindowServer': 5.3, 'UURemoteServer': 0.3, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-03T23:47:21+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=2.25, quiet_s=241.1, screen={'WindowServer': 5.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-03T23:28:18+00:00 → 2026-11-03T23:36:23+00:00，load [2.556640625, 2.587890625, 2.31884765625] → [2.943359375, 2.75048828125, 2.51904296875]，CPU 采样 {'samples': 239, 'other_busy_samples': 3, 'compile_samples': 0, 'other_busy_top': {'wechatwebdevtools Helper (Renderer)': 1, 'PerfPowerServices': 1, 'Claude Helper (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 3.38}
  - 计时期间屏幕采样：0/122 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 6.8, 'UURemoteServer': 2.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/122 次采样，只记录；WindowServer ≥20% 0/122 次采样，有效
- formal-2：2026-11-03T23:40:24+00:00 → 2026-11-03T23:47:21+00:00，load [2.59521484375, 2.43115234375, 2.4130859375] → [2.337890625, 2.45166015625, 2.4287109375]，CPU 采样 {'samples': 205, 'other_busy_samples': 0, 'compile_samples': 0, 'other_busy_top': {}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 3.13}
  - 计时期间屏幕采样：0/105 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 6.6, 'UURemoteServer': 0.6, 'parsecd': 0.0}；远程桌面活动（≥5%）0/105 次采样，只记录；WindowServer ≥20% 0/105 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 5.4, 'UURemoteServer': 0.6, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.080，mutate 0.062，emit_after 0.100；mutate 按副本 1.0168 / 0.9987 / 1.0127
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.070，mutate 0.065，emit_after 0.124；mutate 按副本 1.0027 / 0.9895 / 1.0012
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-03T23:24:16+00:00
- 二进制 sha256：head `b325298ef9e33efe…`；head_aa `b325298ef9e33efe…`；anchor `50b4b4a99cd5764f…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree cd85a32f, anchor cd85a32f; bisection points their own)（sha256 `b635d0d8acd6fe534355e75152597f79fdfd05a69091f7a2a229a22fc5e15acb`），slg_head `9ca48b2a31f0d21c…`，slg_anch `747a0945d546c065…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
