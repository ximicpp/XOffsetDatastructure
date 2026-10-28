# L2 Mac 门禁批次 2026-10-28-ckpt5b-70a07a0

- head `70a07a059cd82417bb0bb0a69ca9dea91c3b21ff`（CKPT5b head = next_cpp26 70a07a05 = CKPT5 head (3.2.0 release checkpoint; since CKPT4 af601ea: S #167 #162 #169 #172 #177, 0 the rest), build-r3d params）；anchor `9ef5ce75d72a172660a97d9fb4e188da3945f6c6`（v3.1.0 9ef5ce75 (release 3.1.0, library = INT4 head e913d22), build-r3d params）
- CKPT5b: L2 rerun of CKPT5 (SLG fail by width 10-28 17:35Z), same head and anchor; judged on all its own processes; bisection points #162 #169 #172 #177, report only
- **结论：fail**。正式部分 pass（合并 2 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `648bd4c28773066aa5b0454e86c135421b8b50ec6b648b6b13df77f7fbe7e581`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9977 [0.9904, 1.0041] | 1.0016 [0.9925, 1.0098] | 0.9967 [0.9860, 1.0063] | 1.0098 | pass | 1.0045 / 1.0017 / 1.0022 | 有效 |
| formal-1（单批，只报） | 0.9934 [0.9830, 1.0026] | 1.0002 [0.9875, 1.0116] | 0.9969 [0.9810, 1.0113] | 1.0116 | pass | 1.0098 / 1.0035 / 0.9986 | 有效 |
| formal-2（单批，只报） | 1.0020 [0.9918, 1.0111] | 1.0030 [0.9895, 1.0151] | 0.9965 [0.9818, 1.0097] | 1.0151 | pass | 0.9993 / 1.0000 / 1.0059 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9628 / 0.9998 / 1.0375 | 0.9620 / 1.0006 / 1.0418 | 0.9515 / 0.9944 / 1.0504 |
| formal-1 | 0.9449 / 0.9947 / 1.0429 | 0.9527 / 0.9993 / 1.0504 | 0.9495 / 0.9948 / 1.0549 |
| formal-2 | 0.9699 / 1.0078 / 1.0354 | 0.9790 / 1.0028 / 1.0323 | 0.9525 / 0.9941 / 1.0456 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9760 [0.9574, 0.9948] | 0.9977 [0.9890, 1.0064] | 1.0016 [0.9906, 1.0127] | 0.9967 [0.9839, 1.0097] | 1.0000 [0.9873, 1.0128] | 0.9972 [0.9889, 1.0056] |
| formal-1 head÷anchor | 0.9745 [0.9504, 0.9992] | 0.9934 [0.9808, 1.0061] | 1.0002 [0.9848, 1.0158] | 0.9969 [0.9776, 1.0166] | 0.9931 [0.9745, 1.0120] | 0.9939 [0.9832, 1.0047] |
| formal-2 head÷anchor | 0.9775 [0.9484, 1.0074] | 1.0020 [0.9897, 1.0144] | 1.0030 [0.9867, 1.0196] | 0.9965 [0.9788, 1.0145] | 1.0069 [0.9893, 1.0247] | 1.0006 [0.9874, 1.0139] |
| 合并 head÷r3 | 0.9321 [0.9247, 0.9396] | 0.9582 [0.9488, 0.9677] | 0.9736 [0.9644, 0.9830] | 0.8612 [0.8510, 0.8715] | 0.9468 [0.9371, 0.9567] | 0.9523 [0.9451, 0.9597] |
| formal-1 head÷r3 | 0.9315 [0.9195, 0.9436] | 0.9497 [0.9333, 0.9664] | 0.9702 [0.9543, 0.9863] | 0.8553 [0.8374, 0.8736] | 0.9331 [0.9175, 0.9490] | 0.9444 [0.9322, 0.9569] |
| formal-2 head÷r3 | 0.9327 [0.9234, 0.9422] | 0.9668 [0.9583, 0.9753] | 0.9771 [0.9671, 0.9873] | 0.8672 [0.8573, 0.8772] | 0.9607 [0.9509, 0.9707] | 0.9603 [0.9532, 0.9675] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 1.0016 | 1.0091 | 是 |
| formal-1 | 1.0002 | 1.0107 | 是 |
| formal-2 | 1.0030 | 1.0141 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.788 / 0.800 通过 | 0.812 / 0.820 通过 |
| cista:mutate | 1.05 | 0.700 / 0.708 通过 | 0.699 / 0.706 通过 |
| cista:emit_after | 0.2 | 0.135 / 0.137 通过 | 0.127 / 0.129 通过 |
| flatbuffers:read_sum | 1.05 | 0.605 / 0.615 通过 | 0.625 / 0.633 通过 |
| flatbuffers:mutate | 1.05 | 0.623 / 0.636 通过 | 0.642 / 0.653 通过 |
| flatbuffers:emit_after | 0.2 | 0.126 / 0.128 通过 | 0.121 / 0.122 通过 |
| capnproto:read_sum | 1.05 | 0.199 / 0.201 通过 | 0.194 / 0.196 通过 |
| capnproto:mutate | 1.05 | 0.241 / 0.244 通过 | 0.242 / 0.244 通过 |
| capnproto:emit_after | 0.2 | 0.316 / 0.323 **不通过** | 0.307 / 0.311 **不通过** |
| cereal:read_sum | 1.05 | 0.834 / 0.845 通过 | 0.845 / 0.855 通过 |
| cereal:mutate | 1.05 | 1.012 / 1.027 通过 | 1.032 / 1.044 通过 |
| cereal:emit_after | 0.2 | 0.176 / 0.179 通过 | 0.168 / 0.170 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（retire_modified_state, cleanup） | 无效（retire_modified_state, cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2420 [0.2376, 0.2464] | 0.5916 [0.5807, 0.6028] | 0.3953 [0.3884, 0.4024] | 0.4636 [0.4546, 0.4727] |
| formal-2 | 0.2057 [0.1992, 0.2124] | 0.5002 [0.4847, 0.5163] | 0.3362 [0.3255, 0.3473] | 0.3928 [0.3795, 0.4066] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0408）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 1.0024 [0.9853, 1.0198] | 是 |
| B | 0.9968 [0.9869, 1.0067] | 是 |
| C | 0.9945 [0.9503, 1.0408] | **否** |
| D | 0.8132 [0.7989, 0.8278] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 1.0024 [0.9853, 1.0198] / 1.0024 [0.9856, 1.0195] | 0.9968 [0.9869, 1.0067] / 0.9968 [0.9884, 1.0052] | 0.9945 [0.9503, 1.0408] / 0.9945 [0.9548, 1.0360] | 0.8132 [0.7989, 0.8278] / 0.8132 [0.7985, 0.8282] | head A 0.996 B 0.993 C 0.988 D 1.013；anchor A 0.977 B 0.989 C 0.976 D 0.973 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 1.0024 [0.9477, 1.0602] | 0.9968 [0.9741, 1.0200] | 0.9945 [0.8929, 1.1077] | 0.8132 [0.7652, 0.8642] |
| 合并 | 1.0024 [0.9477, 1.0602] | 0.9968 [0.9741, 1.0200] | 0.9945 [0.8929, 1.1077] | 0.8132 [0.7652, 0.8642] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.240 / 0.243 (0.257) | 0.098 / 0.101 (0.107) | 0.330 / 0.335 (0.347) |
| B | 0.131 / 0.133 (0.136) | 0.069 / 0.069 (0.071) | 0.178 / 0.180 (0.183) |
| C | 0.102 / 0.104 (0.108) | 0.064 / 0.065 (0.067) | 0.255 / 0.258 (0.269) |
| D | 0.228 / 0.231 (0.253) | 0.350 / 0.356 (0.388) | 0.947 / 0.962 (0.986) |

## 4. 环境

- 开跑条件 formal-1：2026-10-28T18:24:18+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=2.02, quiet_s=241.0, screen={'WindowServer': 3.9, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-10-28T18:36:05+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.6, quiet_s=241.1, screen={'WindowServer': 1.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-10-28T18:47:23+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.34, quiet_s=241.0, screen={'WindowServer': 1.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-10-28T18:28:19+00:00 → 2026-10-28T18:36:05+00:00，load [2.01611328125, 1.97119140625, 1.779296875] → [1.814453125, 1.89599609375, 1.8271484375]，CPU 采样 {'samples': 229, 'other_busy_samples': 14, 'compile_samples': 0, 'other_busy_top': {'spindump': 11, 'XprotectService': 1, 'PerfPowerServices': 1, 'runningboardd': 1, 'top': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.6}
  - 计时期间屏幕采样：0/117 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.3, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/117 次采样，只记录；WindowServer ≥20% 0/117 次采样，有效
- formal-2：2026-10-28T18:40:06+00:00 → 2026-10-28T18:47:23+00:00，load [1.595703125, 1.69482421875, 1.7431640625] → [1.90185546875, 1.91943359375, 1.83154296875]，CPU 采样 {'samples': 214, 'other_busy_samples': 2, 'compile_samples': 0, 'other_busy_top': {'ChatGPT': 1, 'Codex (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.37}
  - 计时期间屏幕采样：0/110 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 2.8, 'UURemoteServer': 0.4, 'parsecd': 0.1}；远程桌面活动（≥5%）0/110 次采样，只记录；WindowServer ≥20% 0/110 次采样，有效
- slg 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 2.7, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/15 次采样，只记录；WindowServer ≥20% 0/15 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.070，mutate 0.066，emit_after 0.092；mutate 按副本 0.9987 / 1.0134 / 0.9985
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.051，mutate 0.055，emit_after 0.061；mutate 按副本 1.0034 / 0.9981 / 0.9984
- host macOS-27.2-arm64-arm-64bit；冻结 2026-10-28T18:24:17+00:00
- 二进制 sha256：head `6eece9b8d14ba644…`；head_aa `6eece9b8d14ba644…`；anchor `934e7624616d9128…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 66ea9df7, anchor 0dd0a31e; bisection points their own)（sha256 `a3a2c95f3b799021db93db5d4966039c95a187f44649488fb01882bbd3e2825c`），slg_head `ff454946684a14b2…`，slg_anch `d97b078c72258a3b…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
