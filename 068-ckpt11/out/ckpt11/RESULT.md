# L2 Mac 门禁批次 2026-11-03-ckpt11-827943c

- head `827943cfb9725aeb54b2ff93ac99ff1eaf9351e2`（CKPT11 head = PR #191 827943cf (delta sync, class L, fixed: free table keeps the pre-generation field offsets, redundant bump_gen dropped; merged with next_cpp26 b6e06788), build-r3d params）；anchor `b6e06788102838dc1364a22aba146cc23d70a030`（next_cpp26 b6e06788 (#204; PR #191 base), build-r3d params）
- CKPT11: L2 gate of PR #191 (class L, delta sync) at 827943cf after the CKPT10 fail, merge tier of TEST_TIERS (core gate + G9R); anchor b6e06788; each point built with its own bench/slg; one deferred bisection point 0bbbaa07 (the pre-fix head)
- **结论：fail**。正式部分 pass（合并 2 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `9ad86d10c20d162c69c01de0c77e0e1c7f1649b318c9109b0595c886145aae31`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9997 [0.9900, 1.0084] | 0.9972 [0.9890, 1.0045] | 1.0062 [0.9962, 1.0152] | 1.0152 | pass | 0.9979 / 0.9979 / 0.9915 | 有效 |
| formal-1（单批，只报） | 0.9997 [0.9870, 1.0110] | 1.0005 [0.9889, 1.0110] | 1.0114 [0.9980, 1.0233] | 1.0233 | pending | 1.0004 / 0.9971 / 0.9915 | 有效 |
| formal-2（单批，只报） | 0.9997 [0.9846, 1.0133] | 0.9939 [0.9821, 1.0044] | 1.0011 [0.9858, 1.0148] | 1.0148 | pass | 0.9954 / 0.9988 / 0.9916 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9475 / 1.0001 / 1.0487 | 0.9485 / 0.9953 / 1.0455 | 0.9536 / 1.0069 / 1.0681 |
| formal-1 | 0.9499 / 1.0001 / 1.0471 | 0.9595 / 0.9963 / 1.0556 | 0.9563 / 1.0157 / 1.0681 |
| formal-2 | 0.9441 / 0.9993 / 1.0534 | 0.9440 / 0.9944 / 1.0471 | 0.9432 / 1.0016 / 1.0713 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0011 [0.9944, 1.0078] | 0.9997 [0.9880, 1.0115] | 0.9972 [0.9873, 1.0071] | 1.0062 [0.9942, 1.0184] | 0.9982 [0.9883, 1.0082] | 0.9993 [0.9901, 1.0086] |
| formal-1 head÷anchor | 1.0064 [0.9957, 1.0171] | 0.9997 [0.9844, 1.0152] | 1.0005 [0.9864, 1.0149] | 1.0114 [0.9953, 1.0277] | 0.9944 [0.9819, 1.0070] | 1.0003 [0.9881, 1.0126] |
| formal-2 head÷anchor | 0.9958 [0.9877, 1.0040] | 0.9997 [0.9815, 1.0183] | 0.9939 [0.9796, 1.0083] | 1.0011 [0.9827, 1.0198] | 1.0021 [0.9863, 1.0181] | 0.9983 [0.9842, 1.0127] |
| 合并 head÷r3 | 0.9326 [0.9266, 0.9386] | 0.9545 [0.9442, 0.9650] | 0.9502 [0.9413, 0.9593] | 0.8520 [0.8417, 0.8623] | 0.9550 [0.9454, 0.9648] | 0.9458 [0.9378, 0.9540] |
| formal-1 head÷r3 | 0.9360 [0.9271, 0.9449] | 0.9554 [0.9421, 0.9688] | 0.9503 [0.9380, 0.9627] | 0.8527 [0.8385, 0.8672] | 0.9518 [0.9390, 0.9648] | 0.9462 [0.9358, 0.9567] |
| formal-2 head÷r3 | 0.9292 [0.9210, 0.9374] | 0.9537 [0.9373, 0.9704] | 0.9502 [0.9366, 0.9640] | 0.8512 [0.8359, 0.8668] | 0.9583 [0.9434, 0.9735] | 0.9455 [0.9328, 0.9584] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9972 | 1.0039 | 是 |
| formal-1 | 1.0005 | 1.0101 | 是 |
| formal-2 | 0.9939 | 1.0035 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.771 / 0.783 通过 | 0.772 / 0.785 通过 |
| cista:mutate | 1.05 | 0.677 / 0.685 通过 | 0.673 / 0.682 通过 |
| cista:emit_after | 0.2 | 0.123 / 0.124 通过 | 0.123 / 0.125 通过 |
| flatbuffers:read_sum | 1.05 | 0.620 / 0.631 通过 | 0.612 / 0.623 通过 |
| flatbuffers:mutate | 1.05 | 0.612 / 0.623 通过 | 0.605 / 0.617 通过 |
| flatbuffers:emit_after | 0.2 | 0.116 / 0.117 通过 | 0.117 / 0.118 通过 |
| capnproto:read_sum | 1.05 | 0.198 / 0.200 通过 | 0.199 / 0.202 通过 |
| capnproto:mutate | 1.05 | 0.233 / 0.235 通过 | 0.234 / 0.236 通过 |
| capnproto:emit_after | 0.2 | 0.323 / 0.327 **不通过** | 0.324 / 0.329 **不通过** |
| cereal:read_sum | 1.05 | 0.820 / 0.838 通过 | 0.838 / 0.851 通过 |
| cereal:mutate | 1.05 | 0.977 / 0.998 通过 | 0.992 / 1.006 通过 |
| cereal:emit_after | 0.2 | 0.159 / 0.162 通过 | 0.162 / 0.164 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 10/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 有效 | 有效 |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.1951 [0.1934, 0.1968] | 0.4722 [0.4667, 0.4777] | 0.3487 [0.3456, 0.3519] | 0.3685 [0.3650, 0.3720] |
| formal-2 | 0.1976 [0.1955, 0.1996] | 0.4787 [0.4732, 0.4842] | 0.3532 [0.3493, 0.3570] | 0.3763 [0.3723, 0.3803] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0527）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 1.0294 [1.0066, 1.0527] | **否** |
| B | 1.0090 [0.9961, 1.0221] | **否** |
| C | 1.0098 [0.9909, 1.0292] | **否** |
| D | 1.0133 [0.9961, 1.0309] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 1.0294 [1.0066, 1.0527] / 1.0294 [1.0128, 1.0463] | 1.0090 [0.9961, 1.0221] / 1.0090 [1.0018, 1.0163] | 1.0098 [0.9909, 1.0292] / 1.0098 [0.9964, 1.0235] | 1.0133 [0.9961, 1.0309] / 1.0133 [0.9963, 1.0306] | head A 0.992 B 0.985 C 0.993 D 1.005；anchor A 0.957 B 0.985 C 0.976 D 1.006 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 1.0294 [0.9967, 1.0631] | 1.0090 [1.0059, 1.0122] | 1.0098 [0.9487, 1.0749] | 1.0133 [0.9655, 1.0635] |
| 合并 | 1.0294 [0.9967, 1.0631] | 1.0090 [1.0059, 1.0122] | 1.0098 [0.9487, 1.0749] | 1.0133 [0.9655, 1.0635] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.240 / 0.243 (0.255) | 0.098 / 0.101 (0.105) | 0.387 / 0.393 (0.403) |
| B | 0.167 / 0.169 (0.172) | 0.079 / 0.080 (0.082) | 0.302 / 0.306 (0.310) |
| C | 0.119 / 0.120 (0.123) | 0.071 / 0.071 (0.074) | 0.358 / 0.360 (0.369) |
| D | 0.256 / 0.259 (0.290) | 0.362 / 0.365 (0.408) | 0.971 / 0.994 (1.043) |

## 4. 环境

- 开跑条件 formal-1：2026-11-03T18:17:09+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.04, quiet_s=241.1, screen={'WindowServer': 4.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-03T18:29:15+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.55, quiet_s=241.1, screen={'WindowServer': 4.2, 'UURemoteServer': 0.9, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-03T18:40:10+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=2.02, quiet_s=241.1, screen={'WindowServer': 5.5, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-03T18:21:10+00:00 → 2026-11-03T18:29:15+00:00，load [1.03759765625, 1.52587890625, 1.64501953125] → [1.95361328125, 1.828125, 1.7529296875]，CPU 采样 {'samples': 238, 'other_busy_samples': 4, 'compile_samples': 0, 'other_busy_top': {'mds_stores': 1, 'Claude Helper (Renderer)': 1, 'deleted': 1, 'ChatGPT': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.34}
  - 计时期间屏幕采样：0/122 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 6.6, 'UURemoteServer': 2.1, 'parsecd': 0.1}；远程桌面活动（≥5%）0/122 次采样，只记录；WindowServer ≥20% 0/122 次采样，有效
- formal-2：2026-11-03T18:33:16+00:00 → 2026-11-03T18:40:10+00:00，load [1.55419921875, 1.63427734375, 1.669921875] → [2.13720703125, 1.98583984375, 1.83935546875]，CPU 采样 {'samples': 204, 'other_busy_samples': 4, 'compile_samples': 0, 'other_busy_top': {'Codex (Renderer)': 1, 'modelcatalogd': 1, 'stickersd': 1, 'mds_stores': 1, 'launchd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.69}
  - 计时期间屏幕采样：0/104 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 6.0, 'UURemoteServer': 1.8, 'parsecd': 0.0}；远程桌面活动（≥5%）0/104 次采样，只记录；WindowServer ≥20% 0/104 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 5.5, 'UURemoteServer': 0.3, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.068，mutate 0.065，emit_after 0.085；mutate 按副本 0.9920 / 0.9997 / 0.9995
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.075，mutate 0.068，emit_after 0.087；mutate 按副本 1.0006 / 1.0035 / 0.9923
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-03T18:17:09+00:00
- 二进制 sha256：head `ae4cb971c361a49f…`；head_aa `ae4cb971c361a49f…`；anchor `d8fd30e4cf7e03af…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree cd85a32f, anchor cd85a32f; bisection points their own)（sha256 `b635d0d8acd6fe534355e75152597f79fdfd05a69091f7a2a229a22fc5e15acb`），slg_head `d03d592c293e248a…`，slg_anch `09abcb48c09ce8be…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
