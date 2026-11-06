# L2 Mac 门禁批次 2026-11-06-stage340-1162e37

- head `1162e37bf40fad6074b9e4cc9c7ff109a8b35288`（STAGE340 head = v3.4.0 1162e37b (tag object aa8b38a2; release PR #245; #237 #240 #242 #241 #243 #244 since v3.3.1), build-r3d params）；anchor `ee506d3f54a979ededdd71cdc5cca5857ff38f11`（v3.3.1 ee506d3f (previous release tag), build-r3d params）
- STAGE340: the 3.4.0 post-release stage review (docs/RELEASING.md section 7: L2 rules, report only): v3.4.0 1162e37b vs v3.3.1 ee506d3f; merge tier of TEST_TIERS (core gate + G9R); every point built with bench/slg bd7b674e (#243 merge 8003a5d5: the last bench/slg before #244, whose trust-tier P2 bench needs XReceiver::apply_delta, absent before #244; the xoffset format code is the same in both), with the D3 header xoffset_shared_delta.hpp (unchanged since #242) copied into points before #242; deferred bisection points (#237 4d1ac732, #240 56aa849e, #241 fd0dc58c, #243 8003a5d5); no report-only step
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `3f7227acafc8d009f5beb0ea56a9731cf9ca13f55129b5e7ad4abeec27314cbd`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9978 [0.9909, 1.0040] | 0.9926 [0.9817, 1.0024] | 0.9965 [0.9877, 1.0044] | 1.0044 | pass | 1.0013 / 1.0019 / 1.0034 | 有效 |
| formal-1（单批，只报） | 0.9993 [0.9914, 1.0063] | 0.9964 [0.9877, 1.0041] | 1.0032 [0.9921, 1.0131] | 1.0131 | pass | 0.9998 / 1.0006 / 0.9996 | 有效 |
| formal-2（单批，只报） | 0.9964 [0.9848, 1.0067] | 0.9889 [0.9687, 1.0072] | 0.9899 [0.9762, 1.0021] | 1.0072 | pass | 1.0028 / 1.0032 / 1.0072 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9678 / 0.9994 / 1.0307 | 0.9568 / 0.9964 / 1.0304 | 0.9473 / 0.9981 / 1.0391 |
| formal-1 | 0.9731 / 1.0001 / 1.0287 | 0.9540 / 0.9988 / 1.0343 | 0.9574 / 1.0034 / 1.0503 |
| formal-2 | 0.9450 / 0.9994 / 1.0383 | 0.9583 / 0.9944 / 1.0292 | 0.9436 / 0.9938 / 1.0308 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0077 [0.9997, 1.0158] | 0.9978 [0.9895, 1.0062] | 0.9926 [0.9795, 1.0059] | 0.9965 [0.9860, 1.0072] | 1.0041 [0.9943, 1.0140] | 0.9972 [0.9883, 1.0061] |
| formal-1 head÷anchor | 1.0095 [0.9984, 1.0207] | 0.9993 [0.9897, 1.0089] | 0.9964 [0.9859, 1.0069] | 1.0032 [0.9898, 1.0168] | 1.0030 [0.9956, 1.0104] | 0.9996 [0.9917, 1.0076] |
| formal-2 head÷anchor | 1.0060 [0.9939, 1.0182] | 0.9964 [0.9824, 1.0105] | 0.9889 [0.9645, 1.0139] | 0.9899 [0.9734, 1.0066] | 1.0052 [0.9867, 1.0241] | 0.9947 [0.9785, 1.0112] |
| 合并 head÷r3 | 0.9353 [0.9282, 0.9424] | 0.9570 [0.9477, 0.9664] | 0.9391 [0.9312, 0.9471] | 0.8409 [0.8317, 0.8502] | 0.9006 [0.8937, 0.9076] | 0.9365 [0.9294, 0.9437] |
| formal-1 head÷r3 | 0.9357 [0.9249, 0.9467] | 0.9634 [0.9492, 0.9778] | 0.9422 [0.9299, 0.9546] | 0.8452 [0.8323, 0.8583] | 0.9015 [0.8948, 0.9083] | 0.9407 [0.9301, 0.9514] |
| formal-2 head÷r3 | 0.9348 [0.9253, 0.9444] | 0.9506 [0.9383, 0.9630] | 0.9361 [0.9255, 0.9468] | 0.8367 [0.8232, 0.8503] | 0.8997 [0.8873, 0.9123] | 0.9323 [0.9226, 0.9422] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9926 | 1.0016 | 是 |
| formal-1 | 0.9964 | 1.0034 | 是 |
| formal-2 | 0.9889 | 1.0057 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.802 / 0.813 通过 | 0.783 / 0.817 通过 |
| cista:mutate | 1.05 | 0.678 / 0.685 通过 | 0.674 / 0.687 通过 |
| cista:emit_after | 0.2 | 0.125 / 0.126 通过 | 0.122 / 0.124 通过 |
| flatbuffers:read_sum | 1.05 | 0.612 / 0.620 通过 | 0.617 / 0.627 通过 |
| flatbuffers:mutate | 1.05 | 0.604 / 0.614 通过 | 0.607 / 0.618 通过 |
| flatbuffers:emit_after | 0.2 | 0.118 / 0.119 通过 | 0.116 / 0.118 通过 |
| capnproto:read_sum | 1.05 | 0.196 / 0.197 通过 | 0.193 / 0.196 通过 |
| capnproto:mutate | 1.05 | 0.236 / 0.238 通过 | 0.231 / 0.235 通过 |
| capnproto:emit_after | 0.2 | 0.318 / 0.322 **不通过** | 0.310 / 0.315 **不通过** |
| cereal:read_sum | 1.05 | 0.849 / 0.858 通过 | 0.846 / 0.853 通过 |
| cereal:mutate | 1.05 | 0.994 / 1.007 通过 | 0.991 / 1.000 通过 |
| cereal:emit_after | 0.2 | 0.165 / 0.167 通过 | 0.162 / 0.164 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 10/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 有效 | 无效（retire_modified_state） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.1850 [0.1835, 0.1866] | 0.4503 [0.4460, 0.4546] | 0.3259 [0.3232, 0.3286] | 0.3522 [0.3487, 0.3556] |
| formal-2 | 0.1862 [0.1834, 0.1891] | 0.4501 [0.4411, 0.4594] | 0.3196 [0.3136, 0.3257] | 0.3573 [0.3502, 0.3644] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0099）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9391 [0.9251, 0.9532] | 是 |
| B | 0.9745 [0.9543, 0.9951] | 是 |
| C | 0.9793 [0.9497, 1.0099] | 是 |
| D | 0.9606 [0.9376, 0.9842] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9391 [0.9251, 0.9532] / 0.9391 [0.9278, 0.9505] | 0.9745 [0.9543, 0.9951] / 0.9745 [0.9632, 0.9859] | 0.9793 [0.9497, 1.0099] / 0.9793 [0.9619, 0.9970] | 0.9606 [0.9376, 0.9842] / 0.9606 [0.9411, 0.9805] | head A 0.977 B 0.972 C 0.959 D 1.018；anchor A 0.977 B 0.992 C 0.983 D 1.029 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9391 [0.9275, 0.9508] | 0.9745 [0.9291, 1.0220] | 0.9793 [0.9217, 1.0406] | 0.9606 [0.9178, 1.0054] |
| 合并 | 0.9391 [0.9275, 0.9508] | 0.9745 [0.9291, 1.0220] | 0.9793 [0.9217, 1.0406] | 0.9606 [0.9178, 1.0054] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.210 / 0.214 (0.229) | 0.091 / 0.093 (0.098) | 0.358 / 0.366 (0.386) |
| B | 0.153 / 0.158 (0.158) | 0.076 / 0.078 (0.079) | 0.289 / 0.296 (0.297) |
| C | 0.110 / 0.115 (0.119) | 0.069 / 0.071 (0.074) | 0.349 / 0.363 (0.373) |
| D | 0.217 / 0.222 (0.242) | 0.346 / 0.356 (0.381) | 0.935 / 0.967 (0.969) |

## 4. 环境

- 开跑条件 formal-1：2026-11-06T00:54:43+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.06, quiet_s=241.1, screen={'WindowServer': 1.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-06T01:05:55+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.93, quiet_s=241.1, screen={'WindowServer': 1.6, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-06T01:17:57+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.26, quiet_s=241.1, screen={'WindowServer': 1.2, 'UURemoteServer': 0.6, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-06T00:58:44+00:00 → 2026-11-06T01:05:55+00:00，load [1.05615234375, 1.44873046875, 1.56201171875] → [1.744140625, 1.6806640625, 1.6337890625]，CPU 采样 {'samples': 212, 'other_busy_samples': 4, 'compile_samples': 0, 'other_busy_top': {'Codex (Renderer)': 1, 'mediaanalysisd': 1, 'mds_stores': 1, 'ChatGPT': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.12}
  - 计时期间屏幕采样：0/108 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 5.0, 'UURemoteServer': 0.6, 'parsecd': 0.0}；远程桌面活动（≥5%）0/108 次采样，只记录；WindowServer ≥20% 0/108 次采样，有效
- formal-2：2026-11-06T01:09:56+00:00 → 2026-11-06T01:17:57+00:00，load [1.92578125, 2.0556640625, 1.81005859375] → [3.1552734375, 2.427734375, 2.02978515625]，CPU 采样 {'samples': 237, 'other_busy_samples': 11, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 4, 'Codex (Renderer)': 2, 'duetexpertd': 2, 'WallpaperAerialsExtension': 1, 'com.apple.AmbientDisplayAgent': 1}, 'chrome_busy_mean': 0.3, 'chrome_busy_max': 62.7, 'load1_max': 5.76}
  - 计时期间屏幕采样：0/121 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 41.1, 'UURemoteServer': 0.8, 'parsecd': 1.3}；远程桌面活动（≥5%）0/121 次采样，只记录；WindowServer ≥20% 3/121 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 1.3, 'UURemoteServer': 0.6, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.059，mutate 0.056，emit_after 0.076；mutate 按副本 0.9978 / 1.0033 / 1.0007
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.050，mutate 0.052，emit_after 0.074；mutate 按副本 1.0029 / 1.0049 / 1.0019
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-06T00:54:43+00:00
- 二进制 sha256：head `7c0f22163bcfddbd…`；head_aa `7c0f22163bcfddbd…`；anchor `a394582b515d0ab1…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 bench/slg bd7b674e of the #243 merge for every point (git tree bd7b674e; head, anchor and bisection points; D3 header copied before #242)（sha256 `98731556bf235491e6f15bacae349ced9b36e8ad6972b391cf4475fffcceeb59`），slg_head `4ec422be4f8807e4…`，slg_anch `80982198ffd76e80…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
