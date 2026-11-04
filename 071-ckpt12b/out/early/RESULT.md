# L2 Mac 门禁批次 2026-11-04-ckpt12b-33c3478

- head `33c34789df8164d8940e3e1bb4a026f371fec931`（CKPT12b head = class L integration claude/int-save-root-20261103 33c34789 (#207: verified save no longer shrinks + save_verified_into one-pass fast path; #208: root<T>() cache, two words 16-byte aligned), build-r3d params）；anchor `0bf0aa1f426486ea48645ba4c6ac80c55f86beba`（next_cpp26 0bf0aa1f (#206; the integration base), build-r3d params）
- CKPT12b: L2 gate of the class L integration of #207 and #208 (33c34789), merge tier of TEST_TIERS (core gate, no G9R); anchor 0bf0aa1f; each point built with its own bench/slg; one deferred bisection point 177d2703 (#207 only)
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `1de6c9d93117562ae534ebc007e1dac81c5d3a9e0006dcf18dc3eef50cb78763`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9887 [0.9776, 0.9986] | 0.9937 [0.9805, 1.0056] | 0.9968 [0.9815, 1.0106] | 1.0106 | pass | 1.0023 / 0.9988 / 0.9960 | 有效 |
| formal-1（单批，只报） | 0.9861 [0.9742, 0.9967] | 0.9891 [0.9790, 0.9981] | 0.9916 [0.9730, 1.0084] | 1.0084 | pass | 1.0037 / 1.0040 / 0.9979 | 有效 |
| formal-2（单批，只报） | 0.9912 [0.9722, 1.0084] | 0.9984 [0.9735, 1.0210] | 1.0020 [0.9771, 1.0246] | 1.0246 | pending | 1.0010 / 0.9936 / 0.9941 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9285 / 0.9875 / 1.0359 | 0.9485 / 0.9962 / 1.0362 | 0.9134 / 0.9871 / 1.0916 |
| formal-1 | 0.9340 / 0.9840 / 1.0331 | 0.9497 / 0.9935 / 1.0288 | 0.9211 / 0.9808 / 1.0777 |
| formal-2 | 0.9195 / 0.9893 / 1.0561 | 0.9343 / 0.9982 / 1.0512 | 0.8988 / 1.0016 / 1.1065 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0016 [0.9919, 1.0114] | 0.9887 [0.9754, 1.0021] | 0.9937 [0.9778, 1.0099] | 0.9968 [0.9784, 1.0156] | 0.9958 [0.9809, 1.0111] | 0.9919 [0.9803, 1.0036] |
| formal-1 head÷anchor | 1.0041 [0.9915, 1.0168] | 0.9861 [0.9717, 1.0006] | 0.9891 [0.9769, 1.0014] | 0.9916 [0.9691, 1.0146] | 0.9904 [0.9737, 1.0073] | 0.9888 [0.9782, 0.9995] |
| formal-2 head÷anchor | 0.9991 [0.9838, 1.0147] | 0.9912 [0.9683, 1.0148] | 0.9984 [0.9683, 1.0294] | 1.0020 [0.9720, 1.0330] | 1.0014 [0.9758, 1.0276] | 0.9950 [0.9738, 1.0166] |
| 合并 head÷r3 | 0.9373 [0.9280, 0.9466] | 0.9429 [0.9300, 0.9559] | 0.9466 [0.9324, 0.9610] | 0.8409 [0.8264, 0.8556] | 0.8959 [0.8829, 0.9091] | 0.9305 [0.9195, 0.9416] |
| formal-1 head÷r3 | 0.9394 [0.9285, 0.9505] | 0.9441 [0.9293, 0.9590] | 0.9366 [0.9249, 0.9485] | 0.8381 [0.8200, 0.8567] | 0.8796 [0.8657, 0.8937] | 0.9257 [0.9149, 0.9367] |
| formal-2 head÷r3 | 0.9351 [0.9198, 0.9506] | 0.9417 [0.9199, 0.9639] | 0.9568 [0.9307, 0.9836] | 0.8436 [0.8204, 0.8675] | 0.9125 [0.8914, 0.9342] | 0.9353 [0.9157, 0.9553] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9937 | 1.0047 | 是 |
| formal-1 | 0.9891 | 0.9974 | 是 |
| formal-2 | 0.9984 | 1.0191 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.779 / 0.792 通过 | 0.769 / 0.784 通过 |
| cista:mutate | 1.05 | 0.671 / 0.678 通过 | 0.686 / 0.702 通过 |
| cista:emit_after | 0.2 | 0.132 / 0.134 通过 | 0.134 / 0.137 通过 |
| flatbuffers:read_sum | 1.05 | 0.619 / 0.630 通过 | 0.612 / 0.627 通过 |
| flatbuffers:mutate | 1.05 | 0.605 / 0.615 通过 | 0.602 / 0.620 通过 |
| flatbuffers:emit_after | 0.2 | 0.123 / 0.126 通过 | 0.124 / 0.127 通过 |
| capnproto:read_sum | 1.05 | 0.200 / 0.203 通过 | 0.212 / 0.216 通过 |
| capnproto:mutate | 1.05 | 0.228 / 0.230 通过 | 0.236 / 0.242 通过 |
| capnproto:emit_after | 0.2 | 0.304 / 0.310 **不通过** | 0.310 / 0.319 **不通过** |
| cereal:read_sum | 1.05 | 0.834 / 0.846 通过 | 0.830 / 0.848 通过 |
| cereal:mutate | 1.05 | 0.979 / 0.989 通过 | 0.988 / 1.016 通过 |
| cereal:emit_after | 0.2 | 0.170 / 0.173 通过 | 0.172 / 0.176 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（retire_modified_state, cleanup） | 无效（retire_modified_state, cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2313 [0.2259, 0.2369] | 0.5606 [0.5457, 0.5759] | 0.3779 [0.3709, 0.3851] | 0.4446 [0.4334, 0.4561] |
| formal-2 | 0.2473 [0.2396, 0.2554] | 0.6040 [0.5847, 0.6240] | 0.4042 [0.3939, 0.4149] | 0.4735 [0.4579, 0.4897] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；2 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0139）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9971 [0.9836, 1.0108] | 是 |
| B | 0.9817 [0.9682, 0.9953] | 是 |
| C | 0.9900 [0.9666, 1.0139] | 是 |
| D | 0.9914 [0.9810, 1.0019] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9896 [0.9678, 1.0119] / 0.9896 [0.9742, 1.0052] | 0.9815 [0.9691, 0.9942] / 0.9815 [0.9683, 0.9950] | 1.0021 [0.9815, 1.0231] / 1.0021 [0.9815, 1.0231] | 0.9909 [0.9748, 1.0072] / 0.9909 [0.9741, 1.0080] | head A 0.976 B 1.016 C 0.999 D 1.012；anchor A 0.967 B 1.007 C 0.994 D 1.004 |
| slg-2 | 1.0046 [0.9832, 1.0266] / 1.0046 [0.9862, 1.0234] | 0.9818 [0.9529, 1.0115] / 0.9818 [0.9520, 1.0124] | 0.9780 [0.9285, 1.0301] / 0.9780 [0.9287, 1.0298] | 0.9920 [0.9715, 1.0128] / 0.9920 [0.9728, 1.0114] | head A 0.968 B 0.961 C 0.983 D 0.992；anchor A 0.983 B 1.032 C 1.051 D 1.009 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9896 [0.9584, 1.0218] | 0.9815 [0.9486, 1.0156] | 1.0021 [0.9721, 1.0330] | 0.9909 [0.9462, 1.0377] |
| slg-2 | 1.0046 [0.9707, 1.0397] | 0.9818 [0.8814, 1.0935] | 0.9780 [0.9007, 1.0619] | 0.9920 [0.9346, 1.0528] |
| 合并 | 0.9971 [0.9815, 1.0129] | 0.9817 [0.9605, 1.0032] | 0.9900 [0.9638, 1.0168] | 0.9914 [0.9771, 1.0059] |

进程数 head 12、anchor 12。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.235 / 0.238 (0.254) | 0.097 / 0.099 (0.105) | 0.376 / 0.380 (0.404) |
| B | 0.159 / 0.161 (0.162) | 0.076 / 0.077 (0.078) | 0.287 / 0.291 (0.292) |
| C | 0.123 / 0.125 (0.132) | 0.072 / 0.073 (0.077) | 0.362 / 0.368 (0.385) |
| D | 0.243 / 0.247 (0.275) | 0.350 / 0.353 (0.388) | 0.937 / 0.952 (0.977) |

## 4. 环境

- 开跑条件 formal-1：2026-11-04T00:26:17+00:00 ok=True, quiet_s_required=180.0, waited_s=246.1, load1=2.9, quiet_s=246.0, screen={'WindowServer': 2.4, 'UURemoteServer': 1.1, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-04T00:38:31+00:00 ok=True, quiet_s_required=180.0, waited_s=281.0, load1=2.35, quiet_s=281.0, screen={'WindowServer': 40.7, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=['WindowServer'], screen_gated=False
- 开跑条件 slg：2026-11-04T00:50:17+00:00 ok=True, quiet_s_required=180.0, waited_s=240.9, load1=2.22, quiet_s=240.9, screen={'WindowServer': 42.5, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=['WindowServer'], screen_gated=False
- 开跑条件 slg-2：2026-11-04T00:55:05+00:00 ok=True, quiet_s_required=180.0, waited_s=240.9, load1=1.6, quiet_s=240.9, screen={'WindowServer': 41.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=['WindowServer'], screen_gated=False
- formal-1：2026-11-04T00:30:23+00:00 → 2026-11-04T00:38:31+00:00，load [2.90478515625, 2.2685546875, 2.22265625] → [2.21533203125, 2.08056640625, 2.11767578125]，CPU 采样 {'samples': 240, 'other_busy_samples': 6, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 2, 'PerfPowerServices': 1, 'Codex (Renderer)': 1, 'opendirectoryd': 1, 'WallpaperAerialsExtension': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.92}
  - 计时期间屏幕采样：0/122 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 30.3, 'UURemoteServer': 1.7, 'parsecd': 0.0}；远程桌面活动（≥5%）0/122 次采样，只记录；WindowServer ≥20% 7/122 次采样，有效
- formal-2：2026-11-04T00:43:12+00:00 → 2026-11-04T00:50:17+00:00，load [2.34716796875, 3.3330078125, 2.82666015625] → [3.12353515625, 3.0224609375, 2.84228515625]，CPU 采样 {'samples': 210, 'other_busy_samples': 207, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 203, 'Claude Helper (Renderer)': 6, 'spotlightknowledged.updater': 2, 'duetexpertd': 2, 'Codex (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 3.24}
  - 计时期间屏幕采样：0/107 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 45.6, 'UURemoteServer': 0.4, 'parsecd': 0.0}；远程桌面活动（≥5%）0/107 次采样，只记录；WindowServer ≥20% 104/107 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 44.4, 'UURemoteServer': 0.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 13/13 次采样，有效
- slg-2 计时期间屏幕采样：0/12 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 43.9, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/12 次采样，只记录；WindowServer ≥20% 12/12 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.067，mutate 0.066，emit_after 0.091；mutate 按副本 0.9976 / 1.0076 / 1.0068
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.117，mutate 0.133，emit_after 0.140；mutate 按副本 0.9890 / 0.9829 / 1.0091
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-04T00:26:17+00:00
- 二进制 sha256：head `b325298ef9e33efe…`；head_aa `b325298ef9e33efe…`；anchor `50b4b4a99cd5764f…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree cd85a32f, anchor cd85a32f; bisection points their own)（sha256 `b635d0d8acd6fe534355e75152597f79fdfd05a69091f7a2a229a22fc5e15acb`），slg_head `9ca48b2a31f0d21c…`，slg_anch `747a0945d546c065…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
