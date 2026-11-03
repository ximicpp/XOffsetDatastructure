# L2 Mac 门禁批次 2026-11-03-ckpt8-5433b4e

- head `5433b4ebb7b23f20b62389ffbcfe888564880ecc`（CKPT8 head = class 1 part 1 5433b4eb (member swap, reload zero watermark and zero tail, D6 adaptive regrow step; class L, own PR), build-r3d params）；anchor `c9fbaea2e980f6f708df615181ced209d653dc0f`（next_cpp26 c9fbaea2 (#190; library headers byte-identical to v3.2.1 / 3.2.0), build-r3d params）
- CKPT8: L2 gate of class 1 part 1 (5433b4eb, class L, before its PR merges); anchor c9fbaea2 (library = v3.2.1); own bench/slg per point; one prebuilt bisection point; 5f demo and 5g v3 demo (SLGV3 bench on both libraries) report only; no G9R
- **结论：fail**。正式部分 pass（合并 2 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `39d984d479a9d8fe6c5110bf429f52e9cc35a3a46a0442bdc0852a9a10e22a25`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9958 [0.9869, 1.0038] | 0.9961 [0.9879, 1.0035] | 0.9948 [0.9812, 1.0071] | 1.0071 | pass | 1.0004 / 1.0020 / 1.0020 | 有效 |
| formal-1（单批，只报） | 0.9934 [0.9822, 1.0034] | 0.9952 [0.9847, 1.0045] | 0.9914 [0.9785, 1.0030] | 1.0045 | pass | 0.9995 / 1.0027 / 1.0039 | 有效 |
| formal-2（单批，只报） | 0.9982 [0.9839, 1.0111] | 0.9971 [0.9841, 1.0088] | 0.9982 [0.9738, 1.0204] | 1.0204 | pending | 1.0013 / 1.0013 / 1.0001 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9434 / 0.9942 / 1.0356 | 0.9479 / 0.9946 / 1.0437 | 0.9295 / 0.9913 / 1.0516 |
| formal-1 | 0.9421 / 0.9992 / 1.0289 | 0.9455 / 0.9959 / 1.0438 | 0.9393 / 0.9898 / 1.0418 |
| formal-2 | 0.9452 / 0.9917 / 1.0535 | 0.9516 / 0.9939 / 1.0454 | 0.9146 / 0.9942 / 1.0681 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0061 [0.9884, 1.0242] | 0.9958 [0.9850, 1.0067] | 0.9961 [0.9863, 1.0061] | 0.9948 [0.9784, 1.0115] | 0.9939 [0.9831, 1.0047] | 0.9965 [0.9861, 1.0070] |
| formal-1 head÷anchor | 1.0013 [0.9923, 1.0103] | 0.9934 [0.9799, 1.0071] | 0.9952 [0.9825, 1.0079] | 0.9914 [0.9758, 1.0073] | 0.9890 [0.9783, 0.9998] | 0.9936 [0.9827, 1.0046] |
| formal-2 head÷anchor | 1.0110 [0.9760, 1.0472] | 0.9982 [0.9809, 1.0159] | 0.9971 [0.9815, 1.0131] | 0.9982 [0.9687, 1.0286] | 0.9988 [0.9797, 1.0182] | 0.9994 [0.9811, 1.0180] |
| 合并 head÷r3 | 0.9350 [0.9191, 0.9513] | 0.9470 [0.9378, 0.9564] | 0.9745 [0.9655, 0.9836] | 0.8571 [0.8433, 0.8712] | 0.9728 [0.9629, 0.9829] | 0.9510 [0.9418, 0.9602] |
| formal-1 head÷r3 | 0.9323 [0.9247, 0.9399] | 0.9462 [0.9350, 0.9576] | 0.9753 [0.9636, 0.9872] | 0.8631 [0.8504, 0.8761] | 0.9850 [0.9759, 0.9942] | 0.9521 [0.9427, 0.9616] |
| formal-2 head÷r3 | 0.9378 [0.9063, 0.9705] | 0.9479 [0.9326, 0.9634] | 0.9737 [0.9596, 0.9880] | 0.8511 [0.8264, 0.8767] | 0.9608 [0.9439, 0.9780] | 0.9498 [0.9336, 0.9662] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9961 | 1.0029 | 是 |
| formal-1 | 0.9952 | 1.0037 | 是 |
| formal-2 | 0.9971 | 1.0078 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.789 / 0.798 通过 | 0.795 / 0.806 通过 |
| cista:mutate | 1.05 | 0.698 / 0.706 通过 | 0.693 / 0.702 通过 |
| cista:emit_after | 0.2 | 0.127 / 0.129 通过 | 0.134 / 0.138 通过 |
| flatbuffers:read_sum | 1.05 | 0.626 / 0.637 通过 | 0.628 / 0.639 通过 |
| flatbuffers:mutate | 1.05 | 0.624 / 0.636 通过 | 0.618 / 0.629 通过 |
| flatbuffers:emit_after | 0.2 | 0.120 / 0.121 通过 | 0.126 / 0.129 通过 |
| capnproto:read_sum | 1.05 | 0.206 / 0.208 通过 | 0.210 / 0.213 通过 |
| capnproto:mutate | 1.05 | 0.242 / 0.244 通过 | 0.240 / 0.243 通过 |
| capnproto:emit_after | 0.2 | 0.335 / 0.339 **不通过** | 0.311 / 0.318 **不通过** |
| cereal:read_sum | 1.05 | 0.849 / 0.860 通过 | 0.855 / 0.868 通过 |
| cereal:mutate | 1.05 | 1.018 / 1.030 通过 | 1.013 / 1.029 通过 |
| cereal:emit_after | 0.2 | 0.166 / 0.167 通过 | 0.174 / 0.178 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 10/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 有效 | 无效（retire_modified_state, cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2071 [0.2055, 0.2088] | 0.5008 [0.4957, 0.5059] | 0.3674 [0.3639, 0.3710] | 0.3927 [0.3891, 0.3962] |
| formal-2 | 0.2475 [0.2426, 0.2525] | 0.6030 [0.5896, 0.6168] | 0.4063 [0.3984, 0.4143] | 0.4766 [0.4665, 0.4870] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0444）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9884 [0.9694, 1.0077] | 是 |
| B | 1.0049 [0.9938, 1.0161] | 是 |
| C | 1.0048 [0.9666, 1.0444] | **否** |
| D | 0.9988 [0.9827, 1.0151] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9884 [0.9694, 1.0077] / 0.9884 [0.9722, 1.0048] | 1.0049 [0.9938, 1.0161] / 1.0049 [0.9934, 1.0165] | 1.0048 [0.9666, 1.0444] / 1.0048 [0.9791, 1.0310] | 0.9988 [0.9827, 1.0151] / 0.9988 [0.9819, 1.0159] | head A 0.986 B 0.983 C 0.933 D 1.005；anchor A 0.975 B 1.013 C 0.994 D 1.009 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9884 [0.9756, 1.0013] | 1.0049 [0.9649, 1.0466] | 1.0048 [0.9223, 1.0946] | 0.9988 [0.9568, 1.0425] |
| 合并 | 0.9884 [0.9756, 1.0013] | 1.0049 [0.9649, 1.0466] | 1.0048 [0.9223, 1.0946] | 0.9988 [0.9568, 1.0425] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.239 / 0.243 (0.261) | 0.096 / 0.098 (0.105) | 0.326 / 0.333 (0.353) |
| B | 0.131 / 0.132 (0.135) | 0.069 / 0.069 (0.071) | 0.176 / 0.178 (0.183) |
| C | 0.107 / 0.112 (0.118) | 0.067 / 0.070 (0.075) | 0.267 / 0.279 (0.297) |
| D | 0.222 / 0.226 (0.252) | 0.344 / 0.350 (0.387) | 0.934 / 0.957 (0.980) |

## 4. 环境

- 开跑条件 formal-1：2026-11-03T02:15:10+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=2.27, quiet_s=241.0, screen={'WindowServer': 43.7, 'UURemoteServer': 15.8, 'parsecd': 0.0}, remote_active=['UURemoteServer'], screen_over=['WindowServer'], screen_gated=False
- 开跑条件 formal-2：2026-11-03T02:26:33+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.13, quiet_s=241.1, screen={'WindowServer': 1.7, 'UURemoteServer': 8.3, 'parsecd': 0.0}, remote_active=['UURemoteServer'], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-03T02:37:31+00:00 ok=True, quiet_s_required=180.0, waited_s=240.9, load1=2.55, quiet_s=240.9, screen={'WindowServer': 42.4, 'UURemoteServer': 18.3, 'parsecd': 0.0}, remote_active=['UURemoteServer'], screen_over=['WindowServer'], screen_gated=False
- formal-1：2026-11-03T02:19:11+00:00 → 2026-11-03T02:26:33+00:00，load [2.27294921875, 2.1923828125, 2.16748046875] → [1.99169921875, 1.92333984375, 2.0283203125]，CPU 采样 {'samples': 217, 'other_busy_samples': 26, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 19, 'duetexpertd': 2, 'Codex (Renderer)': 2, 'launchd': 2, 'deleted': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.62}
  - 计时期间屏幕采样：0/111 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 46.6, 'UURemoteServer': 16.5, 'parsecd': 0.1}；远程桌面活动（≥5%）111/111 次采样，只记录；WindowServer ≥20% 10/111 次采样，有效
- formal-2：2026-11-03T02:30:34+00:00 → 2026-11-03T02:37:31+00:00，load [1.13427734375, 1.458984375, 1.7880859375] → [2.87646484375, 2.17919921875, 1.9931640625]，CPU 采样 {'samples': 205, 'other_busy_samples': 13, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 8, 'duetexpertd': 2, 'runningboardd': 1, 'PerfPowerServices': 1, 'WallpaperAerialsExtension': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 3.22}
  - 计时期间屏幕采样：0/105 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 40.9, 'UURemoteServer': 32.6, 'parsecd': 0.0}；远程桌面活动（≥5%）105/105 次采样，只记录；WindowServer ≥20% 8/105 次采样，有效
- slg 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 45.3, 'UURemoteServer': 18.9, 'parsecd': 0.0}；远程桌面活动（≥5%）15/15 次采样，只记录；WindowServer ≥20% 3/15 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.067，mutate 0.082，emit_after 0.072；mutate 按副本 1.0042 / 1.0084 / 0.9956
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.082，mutate 0.076，emit_after 0.134；mutate 按副本 1.0075 / 0.9993 / 0.9972
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-03T02:15:10+00:00
- 二进制 sha256：head `15a0bede15dcde1f…`；head_aa `15a0bede15dcde1f…`；anchor `6eece9b8d14ba644…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree fd03f498, anchor 4180ba9b; bisection points their own)（sha256 `1e1d15a85ab338290e98853c316dad17ba4b79603975535a2965e20211c884d6`），slg_head `c3662b5c2d701388…`，slg_anch `19978229bfc1997e…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
