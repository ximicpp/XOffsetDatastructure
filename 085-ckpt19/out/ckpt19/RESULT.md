# L2 Mac 门禁批次 2026-11-04-ckpt19-60288a2

- head `60288a20ca1adcb72ca0d829f65cdd65191d30f5`（CKPT19 head = PR #222 60288a20 (payload coloring: VM and slot payload starts vary per buffer, so many buffers do not share TLB sets; one commit on next_cpp26 fe8c0743), build-r3d params）；anchor `fe8c07437224d9462f407cd115eae18a3972a829`（next_cpp26 fe8c0743 (#191 merged), build-r3d params）
- CKPT19: L2 gate of PR #222 (class L, payload coloring in xoffset_slot_storage.hpp and xoffset_virtual_memory.hpp), 60288a20 vs its base next_cpp26 fe8c0743; merge tier of TEST_TIERS (core gate + G9R: storage layout); each point built with its own bench/slg (both 23e86624); no bisection point (one commit on the anchor); report-only step 5x: HFT 64 books (book_c.cpp) and the SLG demo cases
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `8dc61078d4e9664a95eeda501d3ea4a2e6c8191956f984d453f26534d56de4d2`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9976 [0.9903, 1.0041] | 0.9962 [0.9893, 1.0024] | 0.9974 [0.9873, 1.0064] | 1.0064 | pass | 1.0034 / 1.0037 / 1.0059 | 有效 |
| formal-1（单批，只报） | 0.9950 [0.9846, 1.0043] | 0.9962 [0.9871, 1.0043] | 0.9933 [0.9808, 1.0044] | 1.0044 | pass | 1.0060 / 1.0039 / 1.0108 | **无效** |
| formal-2（单批，只报） | 1.0003 [0.9898, 1.0096] | 0.9962 [0.9855, 1.0058] | 1.0015 [0.9854, 1.0160] | 1.0160 | pass | 1.0007 / 1.0035 / 1.0009 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9580 / 0.9993 / 1.0385 | 0.9581 / 0.9962 / 1.0314 | 0.9476 / 0.9924 / 1.0526 |
| formal-1 | 0.9554 / 0.9971 / 1.0336 | 0.9633 / 0.9969 / 1.0278 | 0.9536 / 0.9879 / 1.0476 |
| formal-2 | 0.9582 / 1.0030 / 1.0470 | 0.9515 / 0.9953 / 1.0468 | 0.9395 / 0.9957 / 1.0639 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0018 [0.9948, 1.0089] | 0.9976 [0.9889, 1.0065] | 0.9962 [0.9879, 1.0046] | 0.9974 [0.9853, 1.0096] | 1.0081 [0.9945, 1.0220] | 0.9989 [0.9916, 1.0062] |
| formal-1 head÷anchor | 1.0004 [0.9904, 1.0104] | 0.9950 [0.9824, 1.0077] | 0.9962 [0.9852, 1.0073] | 0.9933 [0.9783, 1.0085] | 0.9938 [0.9761, 1.0117] | 0.9954 [0.9847, 1.0062] |
| formal-2 head÷anchor | 1.0033 [0.9929, 1.0138] | 1.0003 [0.9876, 1.0131] | 0.9962 [0.9832, 1.0093] | 1.0015 [0.9821, 1.0214] | 1.0227 [1.0024, 1.0435] | 1.0024 [0.9923, 1.0127] |
| 合并 head÷r3 | 0.9309 [0.9247, 0.9371] | 0.9440 [0.9337, 0.9543] | 0.9411 [0.9336, 0.9488] | 0.8402 [0.8307, 0.8498] | 0.9041 [0.8919, 0.9165] | 0.9305 [0.9230, 0.9380] |
| formal-1 head÷r3 | 0.9338 [0.9256, 0.9420] | 0.9448 [0.9351, 0.9546] | 0.9420 [0.9324, 0.9518] | 0.8343 [0.8216, 0.8472] | 0.8914 [0.8759, 0.9072] | 0.9293 [0.9215, 0.9373] |
| formal-2 head÷r3 | 0.9279 [0.9185, 0.9375] | 0.9431 [0.9247, 0.9619] | 0.9403 [0.9282, 0.9526] | 0.8461 [0.8317, 0.8608] | 0.9170 [0.8987, 0.9357] | 0.9316 [0.9185, 0.9449] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9962 | 1.0019 | 是 |
| formal-1 | 0.9962 | 1.0036 | 是 |
| formal-2 | 0.9962 | 1.0050 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.773 / 0.782 通过 | 0.763 / 0.772 通过 |
| cista:mutate | 1.05 | 0.676 / 0.684 通过 | 0.672 / 0.678 通过 |
| cista:emit_after | 0.2 | 0.127 / 0.129 通过 | 0.127 / 0.129 通过 |
| flatbuffers:read_sum | 1.05 | 0.613 / 0.620 通过 | 0.613 / 0.627 通过 |
| flatbuffers:mutate | 1.05 | 0.609 / 0.618 通过 | 0.604 / 0.619 通过 |
| flatbuffers:emit_after | 0.2 | 0.119 / 0.121 通过 | 0.119 / 0.121 通过 |
| capnproto:read_sum | 1.05 | 0.196 / 0.198 通过 | 0.202 / 0.204 通过 |
| capnproto:mutate | 1.05 | 0.233 / 0.235 通过 | 0.231 / 0.233 通过 |
| capnproto:emit_after | 0.2 | 0.302 / 0.307 **不通过** | 0.304 / 0.310 **不通过** |
| cereal:read_sum | 1.05 | 0.829 / 0.841 通过 | 0.829 / 0.839 通过 |
| cereal:mutate | 1.05 | 0.981 / 0.992 通过 | 0.979 / 0.991 通过 |
| cereal:emit_after | 0.2 | 0.166 / 0.168 通过 | 0.164 / 0.167 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 有效 | 无效（retire_modified_state） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2046 [0.1982, 0.2112] | 0.4975 [0.4812, 0.5144] | 0.3371 [0.3273, 0.3473] | 0.3934 [0.3804, 0.4069] |
| formal-2 | 0.2128 [0.2057, 0.2202] | 0.5155 [0.4974, 0.5342] | 0.3536 [0.3420, 0.3655] | 0.4075 [0.3935, 0.4220] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0194）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9817 [0.9587, 1.0052] | 是 |
| B | 0.9813 [0.9446, 1.0194] | 是 |
| C | 0.9901 [0.9749, 1.0056] | 是 |
| D | 0.9884 [0.9674, 1.0099] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9817 [0.9587, 1.0052] / 0.9817 [0.9656, 0.9981] | 0.9813 [0.9446, 1.0194] / 0.9813 [0.9528, 1.0106] | 0.9901 [0.9749, 1.0056] / 0.9901 [0.9831, 0.9972] | 0.9884 [0.9674, 1.0099] / 0.9884 [0.9701, 1.0071] | head A 0.967 B 0.974 C 0.981 D 1.010；anchor A 0.989 B 0.990 C 0.971 D 1.022 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9817 [0.9605, 1.0033] | 0.9813 [0.9017, 1.0679] | 0.9901 [0.9791, 1.0013] | 0.9884 [0.9133, 1.0697] |
| 合并 | 0.9817 [0.9605, 1.0033] | 0.9813 [0.9017, 1.0679] | 0.9901 [0.9791, 1.0013] | 0.9884 [0.9133, 1.0697] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.231 / 0.234 (0.249) | 0.095 / 0.099 (0.106) | 0.372 / 0.379 (0.399) |
| B | 0.159 / 0.161 (0.163) | 0.076 / 0.077 (0.079) | 0.289 / 0.294 (0.297) |
| C | 0.118 / 0.119 (0.124) | 0.070 / 0.071 (0.074) | 0.355 / 0.361 (0.374) |
| D | 0.249 / 0.252 (0.278) | 0.348 / 0.355 (0.388) | 0.951 / 0.968 (1.002) |

## 4. 环境

- 开跑条件 formal-1：2026-11-04T14:12:56+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.62, quiet_s=241.1, screen={'WindowServer': 4.5, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-04T14:24:42+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=2.75, quiet_s=241.0, screen={'WindowServer': 41.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=['WindowServer'], screen_gated=False
- 开跑条件 slg：2026-11-04T14:36:35+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.6, quiet_s=241.0, screen={'WindowServer': 42.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=['WindowServer'], screen_gated=False
- formal-1：2026-11-04T14:16:57+00:00 → 2026-11-04T14:24:42+00:00，load [1.6162109375, 1.68310546875, 1.64599609375] → [2.0556640625, 2.14794921875, 1.91357421875]，CPU 采样 {'samples': 228, 'other_busy_samples': 20, 'compile_samples': 0, 'other_busy_top': {'spindump': 16, 'launchd': 2, 'Claude Helper (Renderer)': 1, 'Codex (Renderer)': 1, 'corespotlightd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.98}
  - 计时期间屏幕采样：0/117 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 6.9, 'UURemoteServer': 1.4, 'parsecd': 0.3}；远程桌面活动（≥5%）0/117 次采样，只记录；WindowServer ≥20% 0/117 次采样，有效
- formal-2：2026-11-04T14:28:43+00:00 → 2026-11-04T14:36:35+00:00，load [2.74658203125, 2.216796875, 1.97900390625] → [2.51806640625, 2.59912109375, 2.31103515625]，CPU 采样 {'samples': 232, 'other_busy_samples': 127, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 123, 'Claude Helper (Renderer)': 7, 'contactsd': 1, 'com.apple.AmbientDisplayAgent': 1, 'Codex (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 4.29}
  - 计时期间屏幕采样：0/118 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 45.2, 'UURemoteServer': 1.4, 'parsecd': 0.1}；远程桌面活动（≥5%）0/118 次采样，只记录；WindowServer ≥20% 64/118 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 43.7, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 13/13 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.059，mutate 0.053，emit_after 0.074；mutate 按副本 1.0030 / 1.0057 / 1.0031
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.062，mutate 0.059，emit_after 0.086；mutate 按副本 1.0043 / 1.0090 / 0.9971
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-04T14:12:56+00:00
- 二进制 sha256：head `5c042d729a2a3c69…`；head_aa `5c042d729a2a3c69…`；anchor `242c1888c3c0301a…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 23e86624, anchor 23e86624; bisection points their own)（sha256 `c60c7c18d80c929261e1eb847c940c8265ed98df984e3b8d9e97200a0764cf34`），slg_head `ef82694c1a9c2e94…`，slg_anch `2f74a89b48f0d469…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
