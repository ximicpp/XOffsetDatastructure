# L2 Mac 门禁批次 2026-10-28-ckpt5-70a07a0

- head `70a07a059cd82417bb0bb0a69ca9dea91c3b21ff`（CKPT5 head = next_cpp26 70a07a05 = CKPT5 head (3.2.0 release checkpoint; since CKPT5 af601ea: S #167 #162 #169 #172 #177, 0 the rest), build-r3d params）；anchor `9ef5ce75d72a172660a97d9fb4e188da3945f6c6`（v3.1.0 9ef5ce75 (release 3.1.0, library = INT4 head e913d22), build-r3d params）
- CKPT5: 3.2.0 release checkpoint; anchor v3.1.0; CKPT5 head af601ea is bisection point 1 and the report-only GATED comparison; each point built with its own bench/slg; bisection points prebuilt
- **结论：fail**。正式部分 pass（合并 2 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `ed253cf45b9ee21ab8ca83aa90349e740a0202b8188f97bb71a819646e3729d8`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 1.0024 [0.9922, 1.0115] | 1.0051 [0.9964, 1.0129] | 1.0007 [0.9878, 1.0122] | 1.0129 | pass | 1.0073 / 1.0025 / 0.9978 | 有效 |
| formal-1（单批，只报） | 1.0026 [0.9871, 1.0165] | 1.0005 [0.9876, 1.0121] | 1.0000 [0.9831, 1.0153] | 1.0165 | pass | 1.0093 / 1.0068 / 1.0031 | 有效 |
| formal-2（单批，只报） | 1.0022 [0.9885, 1.0144] | 1.0097 [0.9977, 1.0204] | 1.0014 [0.9815, 1.0192] | 1.0204 | pending | 1.0053 / 0.9982 / 0.9926 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9413 / 1.0021 / 1.0684 | 0.9524 / 1.0029 / 1.0645 | 0.9233 / 1.0039 / 1.0780 |
| formal-1 | 0.9406 / 1.0036 / 1.0644 | 0.9476 / 1.0001 / 1.0648 | 0.9175 / 1.0068 / 1.0732 |
| formal-2 | 0.9415 / 0.9960 / 1.0687 | 0.9602 / 1.0053 / 1.0649 | 0.9243 / 1.0012 / 1.0924 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9439 [0.9094, 0.9796] | 1.0024 [0.9902, 1.0148] | 1.0051 [0.9946, 1.0157] | 1.0007 [0.9853, 1.0164] | 1.0107 [0.9973, 1.0243] | 0.9990 [0.9909, 1.0072] |
| formal-1 head÷anchor | 0.9917 [0.9786, 1.0050] | 1.0026 [0.9839, 1.0217] | 1.0005 [0.9850, 1.0163] | 1.0000 [0.9796, 1.0209] | 1.0048 [0.9858, 1.0243] | 1.0016 [0.9903, 1.0131] |
| formal-2 head÷anchor | 0.8983 [0.8374, 0.9637] | 1.0022 [0.9857, 1.0190] | 1.0097 [0.9952, 1.0243] | 1.0014 [0.9774, 1.0259] | 1.0167 [0.9973, 1.0364] | 0.9965 [0.9845, 1.0086] |
| 合并 head÷r3 | 0.9238 [0.9153, 0.9324] | 0.9477 [0.9352, 0.9604] | 0.9671 [0.9554, 0.9789] | 0.8654 [0.8513, 0.8797] | 0.9617 [0.9489, 0.9747] | 0.9477 [0.9396, 0.9558] |
| formal-1 head÷r3 | 0.9258 [0.9139, 0.9379] | 0.9456 [0.9279, 0.9636] | 0.9621 [0.9471, 0.9775] | 0.8542 [0.8347, 0.8741] | 0.9465 [0.9291, 0.9642] | 0.9431 [0.9327, 0.9535] |
| formal-2 head÷r3 | 0.9219 [0.9093, 0.9346] | 0.9498 [0.9314, 0.9686] | 0.9721 [0.9538, 0.9906] | 0.8768 [0.8564, 0.8976] | 0.9772 [0.9595, 0.9953] | 0.9523 [0.9397, 0.9651] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 1.0051 | 1.0123 | 是 |
| formal-1 | 1.0005 | 1.0111 | 是 |
| formal-2 | 1.0097 | 1.0195 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.769 / 0.781 通过 | 0.764 / 0.774 通过 |
| cista:mutate | 1.05 | 0.692 / 0.701 通过 | 0.693 / 0.702 通过 |
| cista:emit_after | 0.2 | 0.132 / 0.134 通过 | 0.131 / 0.133 通过 |
| flatbuffers:read_sum | 1.05 | 0.587 / 0.603 通过 | 0.585 / 0.595 通过 |
| flatbuffers:mutate | 1.05 | 0.605 / 0.621 通过 | 0.602 / 0.618 通过 |
| flatbuffers:emit_after | 0.2 | 0.122 / 0.125 通过 | 0.123 / 0.125 通过 |
| capnproto:read_sum | 1.05 | 0.201 / 0.204 通过 | 0.200 / 0.202 通过 |
| capnproto:mutate | 1.05 | 0.239 / 0.241 通过 | 0.240 / 0.243 通过 |
| capnproto:emit_after | 0.2 | 0.320 / 0.326 **不通过** | 0.340 / 0.345 **不通过** |
| cereal:read_sum | 1.05 | 0.817 / 0.834 通过 | 0.815 / 0.830 通过 |
| cereal:mutate | 1.05 | 0.995 / 1.019 通过 | 1.006 / 1.025 通过 |
| cereal:emit_after | 0.2 | 0.172 / 0.175 通过 | 0.171 / 0.175 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（cleanup） | 无效（cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2215 [0.2140, 0.2291] | 0.5432 [0.5236, 0.5635] | 0.3629 [0.3502, 0.3761] | 0.4214 [0.4071, 0.4363] |
| formal-2 | 0.2041 [0.2017, 0.2065] | 0.5014 [0.4952, 0.5076] | 0.3586 [0.3545, 0.3628] | 0.3862 [0.3814, 0.3910] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0557）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9984 [0.9802, 1.0169] | 是 |
| B | 1.0041 [0.9715, 1.0378] | **否** |
| C | 1.0138 [0.9736, 1.0557] | **否** |
| D | 0.8075 [0.7930, 0.8222] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9984 [0.9802, 1.0169] / 0.9984 [0.9821, 1.0149] | 1.0041 [0.9715, 1.0378] / 1.0041 [0.9761, 1.0329] | 1.0138 [0.9736, 1.0557] / 1.0138 [0.9761, 1.0530] | 0.8075 [0.7930, 0.8222] / 0.8075 [0.7926, 0.8226] | head A 0.979 B 0.994 C 1.068 D 1.014；anchor A 0.987 B 0.996 C 0.991 D 0.999 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9984 [0.9342, 1.0670] | 1.0041 [0.9309, 1.0830] | 1.0138 [0.8764, 1.1727] | 0.8075 [0.7724, 0.8442] |
| 合并 | 0.9984 [0.9342, 1.0670] | 1.0041 [0.9309, 1.0830] | 1.0138 [0.8764, 1.1727] | 0.8075 [0.7724, 0.8442] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.240 / 0.245 (0.259) | 0.099 / 0.101 (0.107) | 0.329 / 0.336 (0.351) |
| B | 0.130 / 0.135 (0.133) | 0.068 / 0.070 (0.070) | 0.175 / 0.182 (0.179) |
| C | 0.106 / 0.111 (0.109) | 0.065 / 0.068 (0.067) | 0.254 / 0.267 (0.265) |
| D | 0.229 / 0.233 (0.254) | 0.345 / 0.353 (0.389) | 0.938 / 0.969 (0.980) |

## 4. 环境

- 开跑条件 formal-1：2026-10-28T16:44:12+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.79, quiet_s=241.0, screen={'WindowServer': 4.0, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-10-28T16:55:48+00:00 ok=True, quiet_s_required=180.0, waited_s=441.1, load1=2.97, quiet_s=441.0, screen={'WindowServer': 4.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-10-28T17:10:03+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.63, quiet_s=241.0, screen={'WindowServer': 4.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-10-28T16:48:13+00:00 → 2026-10-28T16:55:48+00:00，load [1.7880859375, 2.091796875, 2.3349609375] → [3.71240234375, 2.67822265625, 2.4814453125]，CPU 采样 {'samples': 224, 'other_busy_samples': 4, 'compile_samples': 0, 'other_busy_top': {'XprotectService': 1, 'corespotlightd': 1, 'mediaanalysisd': 1, 'Claude Helper (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 4.42}
  - 计时期间屏幕采样：0/114 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.2, 'UURemoteServer': 0.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/114 次采样，只记录；WindowServer ≥20% 0/114 次采样，有效
- formal-2：2026-10-28T17:03:09+00:00 → 2026-10-28T17:10:03+00:00，load [2.97119140625, 3.158203125, 2.74951171875] → [1.9755859375, 2.2470703125, 2.4228515625]，CPU 采样 {'samples': 204, 'other_busy_samples': 7, 'compile_samples': 0, 'other_busy_top': {'Claude Helper (Renderer)': 3, 'XProtectRemediatorAdload': 2, 'corespotlightd': 1, 'Codex (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.97}
  - 计时期间屏幕采样：0/104 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/104 次采样，只记录；WindowServer ≥20% 0/104 次采样，有效
- slg 计时期间屏幕采样：0/16 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/16 次采样，只记录；WindowServer ≥20% 0/16 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.080，mutate 0.079，emit_after 0.117；mutate 按副本 1.0100 / 1.0171 / 0.9934
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.090，mutate 0.096，emit_after 0.101；mutate 按副本 0.9840 / 1.0142 / 0.9965
- host macOS-27.2-arm64-arm-64bit；冻结 2026-10-28T16:44:12+00:00
- 二进制 sha256：head `6eece9b8d14ba644…`；head_aa `6eece9b8d14ba644…`；anchor `934e7624616d9128…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 66ea9df7, anchor 0dd0a31e; bisection points their own)（sha256 `a3a2c95f3b799021db93db5d4966039c95a187f44649488fb01882bbd3e2825c`），slg_head `ff454946684a14b2…`，slg_anch `d97b078c72258a3b…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
