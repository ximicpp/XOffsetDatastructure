# L2 Mac 门禁批次 2026-10-26-int3fr-4e6944a

- head `4e6944a8e73f4d4ca3d4a6f939acdf3b2c0437b5`（FR = e46cf38 + #93 + p2 fused regrow (4e6944a), build-r3d params）；anchor `e46cf38190e119d27b6e3043d518d7c135bfefe8`（next_cpp26 e46cf38 (#104 merged), build-r3d params）
- FR alone on e46cf38 (#93 assign_range fused regrow + p2 reserve relocation fused, flat insert out-of-line reserve when full): xoffsetdatastructure.hpp +95/-34; bench/ unchanged; M wire identical
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `f2e0d5cf898cf94a8c9b86ce9ae9e3f71d2c87009207280f0b8fe0036471e571`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 1.0034 [0.9931, 1.0126] | 1.0013 [0.9895, 1.0119] | 1.0040 [0.9903, 1.0163] | 1.0163 | pass | 0.9999 / 1.0016 / 0.9952 | 有效 |
| formal-1（单批，只报） | 1.0031 [0.9914, 1.0135] | 0.9958 [0.9804, 1.0097] | 1.0106 [0.9902, 1.0290] | 1.0290 | pending | 1.0086 / 1.0010 / 0.9806 | **无效** |
| formal-2（单批，只报） | 1.0037 [0.9863, 1.0193] | 1.0068 [0.9886, 1.0233] | 0.9974 [0.9787, 1.0143] | 1.0233 | pending | 0.9913 / 1.0023 / 1.0101 | **无效** |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9495 / 1.0021 / 1.0558 | 0.9343 / 0.9984 / 1.0752 | 0.9246 / 1.0093 / 1.0946 |
| formal-1 | 0.9585 / 1.0052 / 1.0549 | 0.9330 / 0.9934 / 1.0694 | 0.9245 / 1.0162 / 1.1021 |
| formal-2 | 0.9467 / 0.9993 / 1.0618 | 0.9360 / 1.0010 / 1.0767 | 0.9270 / 1.0088 / 1.0751 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0202 [1.0082, 1.0324] | 1.0034 [0.9910, 1.0159] | 1.0013 [0.9871, 1.0158] | 1.0040 [0.9875, 1.0207] | 1.0047 [0.9875, 1.0221] | 1.0040 [0.9944, 1.0138] |
| formal-1 head÷anchor | 1.0156 [1.0038, 1.0274] | 1.0031 [0.9890, 1.0174] | 0.9958 [0.9771, 1.0149] | 1.0106 [0.9860, 1.0359] | 1.0062 [0.9829, 1.0300] | 1.0030 [0.9929, 1.0132] |
| formal-2 head÷anchor | 1.0249 [1.0034, 1.0469] | 1.0037 [0.9827, 1.0251] | 1.0068 [0.9848, 1.0294] | 0.9974 [0.9749, 1.0205] | 1.0032 [0.9772, 1.0298] | 1.0051 [0.9882, 1.0224] |
| 合并 head÷r3 | 0.5085 [0.5029, 0.5142] | 0.9581 [0.9476, 0.9687] | 0.9839 [0.9705, 0.9974] | 0.9968 [0.9796, 1.0143] | 0.5479 [0.5381, 0.5579] | 0.8236 [0.8164, 0.8309] |
| formal-1 head÷r3 | 0.5053 [0.4999, 0.5108] | 0.9579 [0.9463, 0.9696] | 0.9784 [0.9609, 0.9961] | 0.9996 [0.9723, 1.0277] | 0.5463 [0.5328, 0.5601] | 0.8221 [0.8149, 0.8294] |
| formal-2 head÷r3 | 0.5117 [0.5017, 0.5219] | 0.9583 [0.9402, 0.9766] | 0.9894 [0.9687, 1.0106] | 0.9940 [0.9721, 1.0165] | 0.5495 [0.5349, 0.5646] | 0.8252 [0.8124, 0.8382] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 1.0013 | 1.0111 | 是 |
| formal-1 | 0.9958 | 1.0086 | 是 |
| formal-2 | 1.0068 | 1.0219 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.810 / 0.822 通过 | 0.813 / 0.828 通过 |
| cista:mutate | 1.05 | 0.707 / 0.717 通过 | 0.718 / 0.731 通过 |
| cista:emit_after | 0.2 | 0.157 / 0.161 通过 | 0.156 / 0.159 通过 |
| flatbuffers:read_sum | 1.05 | 0.632 / 0.642 通过 | 0.631 / 0.644 通过 |
| flatbuffers:mutate | 1.05 | 0.618 / 0.630 通过 | 0.617 / 0.635 通过 |
| flatbuffers:emit_after | 0.2 | 0.145 / 0.148 通过 | 0.145 / 0.148 通过 |
| capnproto:read_sum | 1.05 | 0.215 / 0.217 通过 | 0.217 / 0.220 通过 |
| capnproto:mutate | 1.05 | 0.251 / 0.254 通过 | 0.253 / 0.258 通过 |
| capnproto:emit_after | 0.2 | 0.373 / 0.385 **不通过** | 0.373 / 0.380 **不通过** |
| cereal:read_sum | 1.05 | 0.869 / 0.883 通过 | 0.876 / 0.891 通过 |
| cereal:mutate | 1.05 | 1.022 / 1.039 通过 | 1.041 / 1.062 **不通过** |
| cereal:emit_after | 0.2 | 0.204 / 0.208 **不通过** | 0.202 / 0.206 **不通过** |
| 通过数 head | | 10/12 | 9/12 |
| 通过数 anchor（同批） | | 10/12 | 9/12 |
| 通过数 r3（同批） | | 9/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（retire_modified_state, cleanup） | 无效（retire_modified_state, cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2695 [0.2588, 0.2807] | 0.6061 [0.5825, 0.6307] | 0.4377 [0.4220, 0.4540] | 0.4935 [0.4755, 0.5122] |
| formal-2 | 0.2757 [0.2671, 0.2846] | 0.6197 [0.6010, 0.6389] | 0.4522 [0.4383, 0.4666] | 0.5014 [0.4853, 0.5179] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；2 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0150）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 1.0031 [0.9914, 1.0150] | 是 |
| B | 0.9962 [0.9877, 1.0048] | 是 |
| C | 0.9934 [0.9769, 1.0102] | 是 |
| D | 1.0067 [0.9977, 1.0157] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 1.0006 [0.9801, 1.0215] / 1.0006 [0.9816, 1.0200] | 0.9961 [0.9808, 1.0115] / 0.9961 [0.9800, 1.0124] | 0.9991 [0.9824, 1.0160] / 0.9991 [0.9834, 1.0150] | 1.0085 [0.9965, 1.0205] / 1.0085 [0.9975, 1.0196] | head A 0.982 B 1.000 C 0.984 D 1.007；anchor A 0.994 B 0.991 C 0.998 D 1.005 |
| slg-2 | 1.0057 [0.9874, 1.0242] / 1.0057 [0.9883, 1.0233] | 0.9964 [0.9848, 1.0081] / 0.9964 [0.9858, 1.0070] | 0.9877 [0.9525, 1.0243] / 0.9877 [0.9614, 1.0148] | 1.0049 [0.9870, 1.0232] / 1.0049 [0.9906, 1.0194] | head A 0.987 B 0.997 C 0.992 D 1.019；anchor A 0.984 B 0.985 C 0.949 D 1.007 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 1.0006 [0.9616, 1.0412] | 0.9961 [0.9715, 1.0213] | 0.9991 [0.9884, 1.0099] | 1.0085 [0.9954, 1.0217] |
| slg-2 | 1.0057 [0.9554, 1.0586] | 0.9964 [0.9433, 1.0524] | 0.9877 [0.8963, 1.0885] | 1.0049 [0.9436, 1.0702] |
| 合并 | 1.0031 [0.9900, 1.0164] | 0.9962 [0.9847, 1.0078] | 0.9934 [0.9726, 1.0146] | 1.0067 [0.9940, 1.0196] |

进程数 head 12、anchor 12。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.257 / 0.260 (0.279) | 0.095 / 0.096 (0.105) | 0.336 / 0.339 (0.364) |
| B | 0.140 / 0.141 (0.147) | 0.072 / 0.073 (0.075) | 0.182 / 0.183 (0.190) |
| C | 0.108 / 0.109 (0.117) | 0.064 / 0.064 (0.069) | 0.244 / 0.246 (0.265) |
| D | 0.339 / 0.343 (0.380) | 0.517 / 0.520 (0.577) | 1.350 / 1.377 (1.395) |

## 4. 环境

- 开跑条件 formal-1：2026-10-26T21:51:38+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.83, quiet_s=241.0, screen={'WindowServer': 3.6, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-10-26T22:04:33+00:00 ok=True, quiet_s_required=180.0, waited_s=240.7, load1=2.92, quiet_s=240.7, screen={'WindowServer': 4.5, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-10-26T22:15:47+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.8, quiet_s=241.0, screen={'WindowServer': 3.5, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg-2：2026-10-26T22:20:45+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.7, quiet_s=241.0, screen={'WindowServer': 3.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-10-26T21:55:39+00:00 → 2026-10-26T22:04:33+00:00，load [1.8330078125, 2.00439453125, 2.041015625] → [3.04443359375, 2.48876953125, 2.2900390625]，CPU 采样 {'samples': 263, 'other_busy_samples': 4, 'compile_samples': 0, 'other_busy_top': {'Claude Helper (Renderer)': 2, 'duetexpertd': 2, 'XprotectService': 1, 'searchpartyd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 4.67}
  - 计时期间屏幕采样：0/134 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 5.0, 'UURemoteServer': 0.6, 'parsecd': 0.0}；远程桌面活动（≥5%）0/134 次采样，只记录；WindowServer ≥20% 0/134 次采样，有效
- formal-2：2026-10-26T22:08:34+00:00 → 2026-10-26T22:15:47+00:00，load [2.92333984375, 2.42578125, 2.279296875] → [2.7578125, 2.642578125, 2.45947265625]，CPU 采样 {'samples': 214, 'other_busy_samples': 0, 'compile_samples': 0, 'other_busy_top': {}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 3.57}
  - 计时期间屏幕采样：0/109 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.5, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/109 次采样，只记录；WindowServer ≥20% 0/109 次采样，有效
- slg 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/15 次采样，只记录；WindowServer ≥20% 0/15 次采样，有效
- slg-2 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/15 次采样，只记录；WindowServer ≥20% 0/15 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.085，mutate 0.074，emit_after 0.113；mutate 按副本 0.9963 / 1.0193 / 0.9877
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.103，mutate 0.103，emit_after 0.129；mutate 按副本 1.0069 / 1.0129 / 0.9872
- host macOS-26.3-arm64-arm-64bit；冻结 2026-10-26T21:51:38+00:00
- 二进制 sha256：head `72706ca20922de14…`；head_aa `72706ca20922de14…`；anchor `2ddce61e21b9f3b0…`；r3 `6d4a33ba1f8547dc…`；cista `13bf74996c1723ce…`；flatbuffers `684f7be86bd7b628…`；capnproto `5a7625c96f0a3e7b…`；cereal `c897ba0d1a11a0e8…`
- SLG 树 bench/slg of e46cf38 (v3 tree, git tree ffd88a16, unchanged since f8eebf4), used for every point（sha256 `2a010b04d53b4049727c4061b9ac750f26ff55bff2bb56da78a3f65c1c91081e`），slg_head `2c3d8443d430d379…`，slg_anch `f801d7abd33f1685…`
- 准备检查：gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0；gate_sml_fr exit 0；slg_verify_reuse_fr exit 0；slg_verify_default_fr exit 0
