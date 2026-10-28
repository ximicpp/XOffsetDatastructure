# L2 Mac 门禁批次 2026-10-28-ckpt4-af601ea

- head `af601ea0372e17333ff8a6d64cce0a409d815f7a`（CKPT4 head = next_cpp26 af601ea = CKPT4 head (M2 gate; since v3.1.0: L #147 #153, S #149 #148 #155, 0 the rest), build-r3d params）；anchor `9ef5ce75d72a172660a97d9fb4e188da3945f6c6`（v3.1.0 9ef5ce75 (release 3.1.0, library = INT4 head e913d22), build-r3d params）
- CKPT4: mainline checkpoint and M2 gate; anchor v3.1.0; each point built with its own bench/slg; bisection points prebuilt
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `44d81b5afc1928fd3ee272c69f1b582506b904c3db4949049c24347300d7ae77`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 1.0030 [0.9911, 1.0136] | 0.9942 [0.9838, 1.0036] | 1.0014 [0.9891, 1.0124] | 1.0136 | pass | 0.9977 / 1.0023 / 0.9997 | 有效 |
| formal-1（单批，只报） | 0.9975 [0.9788, 1.0142] | 0.9924 [0.9737, 1.0092] | 0.9976 [0.9808, 1.0127] | 1.0142 | pass | 1.0062 / 1.0012 / 1.0011 | 有效 |
| formal-2（单批，只报） | 1.0085 [0.9934, 1.0220] | 0.9961 [0.9863, 1.0048] | 1.0052 [0.9869, 1.0217] | 1.0220 | pending | 0.9894 / 1.0035 / 0.9984 | **无效** |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9438 / 1.0038 / 1.0665 | 0.9411 / 0.9967 / 1.0490 | 0.9412 / 0.9906 / 1.0884 |
| formal-1 | 0.9417 / 0.9992 / 1.0606 | 0.9327 / 1.0010 / 1.0593 | 0.9399 / 0.9913 / 1.0846 |
| formal-2 | 0.9443 / 1.0143 / 1.0816 | 0.9517 / 0.9956 / 1.0377 | 0.9463 / 0.9897 / 1.1101 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0122 [0.9955, 1.0292] | 1.0030 [0.9887, 1.0174] | 0.9942 [0.9817, 1.0069] | 1.0014 [0.9867, 1.0163] | 1.0091 [0.9945, 1.0240] | 1.0021 [0.9913, 1.0131] |
| formal-1 head÷anchor | 1.0173 [0.9875, 1.0479] | 0.9975 [0.9750, 1.0204] | 0.9924 [0.9699, 1.0155] | 0.9976 [0.9773, 1.0183] | 1.0056 [0.9872, 1.0242] | 0.9987 [0.9800, 1.0177] |
| formal-2 head÷anchor | 1.0072 [0.9907, 1.0239] | 1.0085 [0.9903, 1.0271] | 0.9961 [0.9843, 1.0081] | 1.0052 [0.9832, 1.0278] | 1.0127 [0.9892, 1.0368] | 1.0056 [0.9942, 1.0172] |
| 合并 head÷r3 | 0.9520 [0.9388, 0.9654] | 0.9595 [0.9482, 0.9708] | 0.9693 [0.9591, 0.9797] | 0.8660 [0.8516, 0.8806] | 0.9770 [0.9634, 0.9907] | 0.9581 [0.9507, 0.9657] |
| formal-1 head÷r3 | 0.9570 [0.9335, 0.9811] | 0.9543 [0.9375, 0.9715] | 0.9700 [0.9541, 0.9862] | 0.8624 [0.8436, 0.8817] | 0.9787 [0.9606, 0.9972] | 0.9563 [0.9452, 0.9675] |
| formal-2 head÷r3 | 0.9470 [0.9339, 0.9603] | 0.9646 [0.9493, 0.9802] | 0.9686 [0.9551, 0.9824] | 0.8696 [0.8472, 0.8926] | 0.9752 [0.9544, 0.9965] | 0.9600 [0.9496, 0.9705] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9942 | 1.0028 | 是 |
| formal-1 | 0.9924 | 1.0078 | 是 |
| formal-2 | 0.9961 | 1.0041 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.761 / 0.774 通过 | 0.770 / 0.782 通过 |
| cista:mutate | 1.05 | 0.692 / 0.699 通过 | 0.688 / 0.696 通过 |
| cista:emit_after | 0.2 | 0.127 / 0.129 通过 | 0.128 / 0.130 通过 |
| flatbuffers:read_sum | 1.05 | 0.597 / 0.608 通过 | 0.597 / 0.605 通过 |
| flatbuffers:mutate | 1.05 | 0.613 / 0.627 通过 | 0.606 / 0.617 通过 |
| flatbuffers:emit_after | 0.2 | 0.120 / 0.122 通过 | 0.120 / 0.123 通过 |
| capnproto:read_sum | 1.05 | 0.201 / 0.204 通过 | 0.201 / 0.204 通过 |
| capnproto:mutate | 1.05 | 0.239 / 0.242 通过 | 0.237 / 0.240 通过 |
| capnproto:emit_after | 0.2 | 0.328 / 0.334 **不通过** | 0.322 / 0.329 **不通过** |
| cereal:read_sum | 1.05 | 0.821 / 0.834 通过 | 0.834 / 0.847 通过 |
| cereal:mutate | 1.05 | 1.009 / 1.026 通过 | 1.008 / 1.025 通过 |
| cereal:emit_after | 0.2 | 0.166 / 0.168 通过 | 0.168 / 0.171 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 10/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（cleanup） | 无效（cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2057 [0.2002, 0.2112] | 0.5023 [0.4896, 0.5152] | 0.3511 [0.3432, 0.3593] | 0.3927 [0.3821, 0.4036] |
| formal-2 | 0.2072 [0.2027, 0.2119] | 0.5087 [0.4973, 0.5203] | 0.3427 [0.3353, 0.3503] | 0.3945 [0.3856, 0.4036] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0162）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9936 [0.9764, 1.0111] | 是 |
| B | 0.9979 [0.9806, 1.0154] | 是 |
| C | 1.0002 [0.9844, 1.0162] | 是 |
| D | 0.8091 [0.7879, 0.8309] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9936 [0.9764, 1.0111] / 0.9936 [0.9753, 1.0122] | 0.9979 [0.9806, 1.0154] / 0.9979 [0.9830, 1.0130] | 1.0002 [0.9844, 1.0162] / 1.0002 [0.9842, 1.0164] | 0.8091 [0.7879, 0.8309] / 0.8091 [0.7952, 0.8233] | head A 0.991 B 0.989 C 0.981 D 1.051；anchor A 0.992 B 0.974 C 0.979 D 1.019 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9936 [0.9405, 1.0497] | 0.9979 [0.9837, 1.0122] | 1.0002 [0.9943, 1.0061] | 0.8091 [0.7618, 0.8593] |
| 合并 | 0.9936 [0.9405, 1.0497] | 0.9979 [0.9837, 1.0122] | 1.0002 [0.9943, 1.0061] | 0.8091 [0.7618, 0.8593] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.241 / 0.246 (0.261) | 0.099 / 0.102 (0.108) | 0.330 / 0.333 (0.351) |
| B | 0.129 / 0.132 (0.134) | 0.068 / 0.069 (0.070) | 0.174 / 0.177 (0.181) |
| C | 0.105 / 0.107 (0.113) | 0.065 / 0.066 (0.070) | 0.255 / 0.260 (0.278) |
| D | 0.228 / 0.236 (0.251) | 0.345 / 0.354 (0.381) | 0.948 / 0.985 (0.980) |

## 4. 环境

- 开跑条件 formal-1：2026-10-28T07:04:40+00:00 ok=True, quiet_s_required=180.0, waited_s=240.8, load1=1.94, quiet_s=240.8, screen={'WindowServer': 44.7, 'parsecd': 0.0, 'UURemoteServer': 0.0}, remote_active=[], screen_over=['WindowServer'], screen_gated=False
- 开跑条件 formal-2：2026-10-28T07:16:38+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.52, quiet_s=240.9, screen={'WindowServer': 3.7, 'parsecd': 0.0, 'UURemoteServer': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-10-28T07:27:49+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.78, quiet_s=240.9, screen={'WindowServer': 4.2, 'parsecd': 0.0, 'UURemoteServer': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-10-28T07:08:41+00:00 → 2026-10-28T07:16:37+00:00，load [1.94091796875, 3.2236328125, 6.88916015625] → [2.181640625, 2.4169921875, 4.88330078125]，CPU 采样 {'samples': 235, 'other_busy_samples': 65, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 59, 'Claude Helper (Renderer)': 3, 'triald': 3, 'XprotectService': 1, 'com.apple.geod': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.64}
  - 计时期间屏幕采样：0/120 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 44.7, 'parsecd': 0.4, 'UURemoteServer': 0.5}；远程桌面活动（≥5%）0/120 次采样，只记录；WindowServer ≥20% 30/120 次采样，有效
- formal-2：2026-10-28T07:20:39+00:00 → 2026-10-28T07:27:49+00:00，load [1.515625, 1.998046875, 4.0927734375] → [2.21435546875, 2.10107421875, 3.318359375]，CPU 采样 {'samples': 212, 'other_busy_samples': 5, 'compile_samples': 0, 'other_busy_top': {'Claude Helper (Renderer)': 4, 'spindump': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.51}
  - 计时期间屏幕采样：0/108 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.9, 'parsecd': 0.0, 'UURemoteServer': 0.0}；远程桌面活动（≥5%）0/108 次采样，只记录；WindowServer ≥20% 0/108 次采样，有效
- slg 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.2, 'parsecd': 0.0, 'UURemoteServer': 0.0}；远程桌面活动（≥5%）0/15 次采样，只记录；WindowServer ≥20% 0/15 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.083，mutate 0.076，emit_after 0.106；mutate 按副本 0.9911 / 1.0064 / 1.0060
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.096，mutate 0.090，emit_after 0.102；mutate 按副本 0.9922 / 0.9983 / 1.0201
- host macOS-27.2-arm64-arm-64bit；冻结 2026-10-28T07:04:39+00:00
- 二进制 sha256：head `7159b821ff7ae356…`；head_aa `7159b821ff7ae356…`；anchor `934e7624616d9128…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 66ea9df7, anchor 0dd0a31e; bisection points their own)（sha256 `a3a2c95f3b799021db93db5d4966039c95a187f44649488fb01882bbd3e2825c`），slg_head `ee036987e77e161d…`，slg_anch `d97b078c72258a3b…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
