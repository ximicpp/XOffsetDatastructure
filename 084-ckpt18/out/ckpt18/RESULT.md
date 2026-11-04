# L2 Mac 门禁批次 2026-11-04-ckpt18-c7f2429

- head `c7f24296d4a59ab51a647c8672f4a36a6a488aec`（CKPT18 head = PR #219 c7f24296 (save path: an empty kept free table writes nothing, a non-empty one walks only nonempty_words; 18d18c25 merged with next_cpp26 1c528d67), build-r3d params）；anchor `1c528d67739bc94d91205c9dd2bf4bf1260253bf`（next_cpp26 1c528d67 (#213 merged), build-r3d params）
- CKPT18: L2 gate of PR #219 (class L, save path skips an empty kept free table; follow-up of the #213 A.res_encode +3.4%) as merged with next_cpp26 (c7f24296 vs 1c528d67, merge result vs mainline), merge tier of TEST_TIERS (core gate, no G9R: save path only); anchor 1c528d67; each point built with its own bench/slg; one deferred bisection point 18d18c25 (#219 before the merge; report only)
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `ec80e80e9d99f7975fa909cbb4b9c515cacd82dde134f964149b8a31af5c4113`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 1.0000 [0.9883, 1.0106] | 0.9932 [0.9831, 1.0021] | 0.9963 [0.9853, 1.0061] | 1.0106 | pass | 0.9964 / 0.9982 / 1.0024 | 有效 |
| formal-1（单批，只报） | 1.0054 [0.9925, 1.0169] | 0.9966 [0.9801, 1.0114] | 0.9973 [0.9802, 1.0127] | 1.0169 | pass | 0.9966 / 0.9996 / 1.0033 | 有效 |
| formal-2（单批，只报） | 0.9947 [0.9748, 1.0127] | 0.9897 [0.9779, 1.0003] | 0.9952 [0.9810, 1.0080] | 1.0127 | pass | 0.9961 / 0.9968 / 1.0015 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9609 / 1.0023 / 1.0544 | 0.9564 / 0.9954 / 1.0358 | 0.9358 / 0.9982 / 1.0646 |
| formal-1 | 0.9660 / 1.0056 / 1.0533 | 0.9692 / 0.9999 / 1.0387 | 0.9500 / 0.9966 / 1.0674 |
| formal-2 | 0.9514 / 0.9974 / 1.0631 | 0.9544 / 0.9906 / 1.0344 | 0.9294 / 0.9993 / 1.0619 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9941 [0.9843, 1.0039] | 1.0000 [0.9859, 1.0144] | 0.9932 [0.9811, 1.0054] | 0.9963 [0.9831, 1.0096] | 0.9909 [0.9753, 1.0068] | 0.9961 [0.9842, 1.0082] |
| formal-1 head÷anchor | 0.9948 [0.9837, 1.0061] | 1.0054 [0.9899, 1.0212] | 0.9966 [0.9767, 1.0169] | 0.9973 [0.9767, 1.0184] | 0.9931 [0.9720, 1.0146] | 0.9999 [0.9847, 1.0153] |
| formal-2 head÷anchor | 0.9933 [0.9768, 1.0101] | 0.9947 [0.9707, 1.0193] | 0.9897 [0.9754, 1.0043] | 0.9952 [0.9780, 1.0128] | 0.9888 [0.9651, 1.0131] | 0.9923 [0.9735, 1.0115] |
| 合并 head÷r3 | 0.9261 [0.9183, 0.9340] | 0.9510 [0.9387, 0.9634] | 0.9396 [0.9301, 0.9493] | 0.8399 [0.8307, 0.8493] | 0.9116 [0.8970, 0.9264] | 0.9342 [0.9242, 0.9444] |
| formal-1 head÷r3 | 0.9242 [0.9115, 0.9370] | 0.9501 [0.9293, 0.9714] | 0.9385 [0.9246, 0.9527] | 0.8370 [0.8229, 0.8512] | 0.8984 [0.8740, 0.9234] | 0.9310 [0.9139, 0.9483] |
| formal-2 head÷r3 | 0.9281 [0.9185, 0.9377] | 0.9518 [0.9379, 0.9659] | 0.9407 [0.9271, 0.9545] | 0.8429 [0.8304, 0.8555] | 0.9250 [0.9098, 0.9404] | 0.9375 [0.9262, 0.9490] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9932 | 1.0014 | 是 |
| formal-1 | 0.9966 | 1.0102 | 是 |
| formal-2 | 0.9897 | 0.9995 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.780 / 0.791 通过 | 0.770 / 0.784 通过 |
| cista:mutate | 1.05 | 0.674 / 0.680 通过 | 0.673 / 0.683 通过 |
| cista:emit_after | 0.2 | 0.130 / 0.132 通过 | 0.125 / 0.127 通过 |
| flatbuffers:read_sum | 1.05 | 0.624 / 0.632 通过 | 0.615 / 0.626 通过 |
| flatbuffers:mutate | 1.05 | 0.606 / 0.615 通过 | 0.605 / 0.617 通过 |
| flatbuffers:emit_after | 0.2 | 0.121 / 0.123 通过 | 0.118 / 0.119 通过 |
| capnproto:read_sum | 1.05 | 0.200 / 0.202 通过 | 0.198 / 0.201 通过 |
| capnproto:mutate | 1.05 | 0.230 / 0.232 通过 | 0.231 / 0.234 通过 |
| capnproto:emit_after | 0.2 | 0.300 / 0.306 **不通过** | 0.306 / 0.311 **不通过** |
| cereal:read_sum | 1.05 | 0.833 / 0.845 通过 | 0.833 / 0.845 通过 |
| cereal:mutate | 1.05 | 0.973 / 0.984 通过 | 0.985 / 0.998 通过 |
| cereal:emit_after | 0.2 | 0.168 / 0.170 通过 | 0.164 / 0.166 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（retire_modified_state, cleanup） | 无效（cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2231 [0.2167, 0.2296] | 0.5448 [0.5293, 0.5609] | 0.3628 [0.3526, 0.3732] | 0.4293 [0.4170, 0.4421] |
| formal-2 | 0.1970 [0.1926, 0.2015] | 0.4808 [0.4708, 0.4911] | 0.3207 [0.3137, 0.3280] | 0.3779 [0.3700, 0.3861] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；2 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0158）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 1.0038 [0.9962, 1.0115] | 是 |
| B | 0.9992 [0.9896, 1.0089] | 是 |
| C | 1.0026 [0.9896, 1.0158] | 是 |
| D | 0.9967 [0.9854, 1.0082] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 1.0050 [0.9964, 1.0137] / 1.0050 [0.9976, 1.0125] | 1.0012 [0.9814, 1.0214] / 1.0012 [0.9867, 1.0159] | 0.9994 [0.9829, 1.0161] / 0.9994 [0.9868, 1.0121] | 0.9977 [0.9828, 1.0128] / 0.9977 [0.9846, 1.0109] | head A 0.990 B 0.976 C 0.979 D 1.011；anchor A 0.992 B 1.002 C 1.001 D 1.003 |
| slg-2 | 1.0026 [0.9866, 1.0188] / 1.0026 [0.9885, 1.0168] | 0.9972 [0.9837, 1.0109] / 0.9972 [0.9908, 1.0036] | 1.0059 [0.9796, 1.0329] / 1.0059 [0.9816, 1.0307] | 0.9958 [0.9730, 1.0192] / 0.9958 [0.9772, 1.0148] | head A 0.977 B 0.976 C 0.978 D 1.022；anchor A 1.001 B 0.988 C 0.994 D 1.008 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 1.0050 [0.9955, 1.0147] | 1.0012 [0.9548, 1.0499] | 0.9994 [0.9725, 1.0270] | 0.9977 [0.9964, 0.9989] |
| slg-2 | 1.0026 [0.9564, 1.0510] | 0.9972 [0.9776, 1.0172] | 1.0059 [0.9490, 1.0661] | 0.9958 [0.9698, 1.0226] |
| 合并 | 1.0038 [0.9943, 1.0134] | 0.9992 [0.9888, 1.0097] | 1.0026 [0.9891, 1.0163] | 0.9967 [0.9914, 1.0021] |

进程数 head 12、anchor 12。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.238 / 0.240 (0.256) | 0.097 / 0.098 (0.105) | 0.383 / 0.386 (0.403) |
| B | 0.161 / 0.164 (0.167) | 0.078 / 0.078 (0.080) | 0.296 / 0.299 (0.301) |
| C | 0.119 / 0.120 (0.124) | 0.070 / 0.071 (0.073) | 0.356 / 0.361 (0.370) |
| D | 0.248 / 0.251 (0.281) | 0.350 / 0.354 (0.396) | 0.934 / 0.947 (0.984) |

## 4. 环境

- 开跑条件 formal-1：2026-11-04T12:31:45+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.59, quiet_s=241.1, screen={'WindowServer': 5.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-04T12:44:23+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.62, quiet_s=241.1, screen={'WindowServer': 4.7, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-04T12:55:40+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.57, quiet_s=241.1, screen={'WindowServer': 4.8, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg-2：2026-11-04T13:00:27+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.38, quiet_s=241.2, screen={'WindowServer': 4.7, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-04T12:35:46+00:00 → 2026-11-04T12:44:23+00:00，load [1.59033203125, 1.81103515625, 1.953125] → [1.9091796875, 1.8779296875, 1.91845703125]，CPU 采样 {'samples': 253, 'other_busy_samples': 12, 'compile_samples': 0, 'other_busy_top': {'Claude Helper (Renderer)': 6, 'corespotlightd': 2, 'Codex (Renderer)': 1, 'dasd': 1, 'ChatGPT': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.4}
  - 计时期间屏幕采样：0/129 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 6.9, 'UURemoteServer': 2.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/129 次采样，只记录；WindowServer ≥20% 0/129 次采样，有效
- formal-2：2026-11-04T12:48:24+00:00 → 2026-11-04T12:55:39+00:00，load [1.625, 1.779296875, 1.8583984375] → [1.9111328125, 1.94873046875, 1.91748046875]，CPU 采样 {'samples': 214, 'other_busy_samples': 7, 'compile_samples': 0, 'other_busy_top': {'Claude Helper (Renderer)': 5, 'frauddefensed': 1, 'Codex (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.37}
  - 计时期间屏幕采样：0/109 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 6.5, 'UURemoteServer': 1.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/109 次采样，只记录；WindowServer ≥20% 0/109 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 7.4, 'UURemoteServer': 0.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- slg-2 计时期间屏幕采样：0/12 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 5.9, 'UURemoteServer': 0.3, 'parsecd': 0.0}；远程桌面活动（≥5%）0/12 次采样，只记录；WindowServer ≥20% 0/12 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.069，mutate 0.053，emit_after 0.088；mutate 按副本 1.0002 / 1.0046 / 0.9941
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.072，mutate 0.066，emit_after 0.080；mutate 按副本 1.0023 / 0.9904 / 0.9976
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-04T12:31:45+00:00
- 二进制 sha256：head `48faaf0af3f96a2d…`；head_aa `48faaf0af3f96a2d…`；anchor `b749a0f4d98e9678…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 23e86624, anchor 23e86624; bisection points their own)（sha256 `c60c7c18d80c929261e1eb847c940c8265ed98df984e3b8d9e97200a0764cf34`），slg_head `ee9c7dd63beab06c…`，slg_anch `9e89e86bb227e857…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
