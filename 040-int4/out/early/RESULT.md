# L2 Mac 门禁批次 2026-10-27-int4-e913d22

- head `e913d220d1f42bf90883396e63286651ba50a352`（INT4 head = claude/wire-v21 e913d22 = 3.1 RC head (V2.1 R4 #133 + R7 #134 + docs #135, D fast path #139, G9 L10/L11 #140, R4 follow-ups #138), build-r3d params）；anchor `e383aef51d6983a753d86f6840308c01c45b5a2e`（v3.0.0 e383aef (release 3.0.0, library = CKPT3 head 30af216), build-r3d params）
- INT4: GATE_V21 + D fast path + G9 L10/L11 in one L2 (merge rules v2: L PRs on one integration branch, one batch); anchor v3.0.0; bisection points deferred
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `04fd04b5f0b4c86b03b74ae62e5c49c29edcc2b37f35498f081a15e55e5eb005`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9502 [0.9398, 0.9595] | 0.9698 [0.9575, 0.9808] | 0.8689 [0.8584, 0.8783] | 0.9808 | pass | 0.9973 / 1.0001 / 1.0056 | 有效 |
| formal-1（单批，只报） | 0.9519 [0.9428, 0.9600] | 0.9773 [0.9671, 0.9863] | 0.8761 [0.8633, 0.8876] | 0.9863 | pass | 1.0004 / 1.0006 / 1.0081 | 有效 |
| formal-2（单批，只报） | 0.9486 [0.9296, 0.9657] | 0.9624 [0.9401, 0.9826] | 0.8617 [0.8450, 0.8767] | 0.9826 | pass | 0.9942 / 0.9996 / 1.0032 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9082 / 0.9557 / 1.0020 | 0.9282 / 0.9726 / 1.0196 | 0.8206 / 0.8695 / 0.9264 |
| formal-1 | 0.9128 / 0.9522 / 1.0003 | 0.9362 / 0.9747 / 1.0175 | 0.8237 / 0.8778 / 0.9273 |
| formal-2 | 0.9053 / 0.9604 / 1.0025 | 0.9223 / 0.9712 / 1.0264 | 0.8113 / 0.8669 / 0.9237 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9953 [0.9479, 1.0450] | 0.9502 [0.9378, 0.9629] | 0.9698 [0.9551, 0.9848] | 0.8689 [0.8562, 0.8817] | 0.9604 [0.9429, 0.9783] | 0.9571 [0.9469, 0.9674] |
| formal-1 head÷anchor | 0.9518 [0.9187, 0.9861] | 0.9519 [0.9409, 0.9630] | 0.9773 [0.9650, 0.9897] | 0.8761 [0.8607, 0.8918] | 0.9699 [0.9558, 0.9842] | 0.9558 [0.9478, 0.9638] |
| formal-2 head÷anchor | 1.0408 [0.9510, 1.1390] | 0.9486 [0.9257, 0.9720] | 0.9624 [0.9355, 0.9901] | 0.8617 [0.8415, 0.8823] | 0.9510 [0.9189, 0.9844] | 0.9585 [0.9393, 0.9780] |
| 合并 head÷r3 | 0.4189 [0.3993, 0.4394] | 0.9099 [0.9020, 0.9178] | 0.9470 [0.9384, 0.9557] | 0.8578 [0.8472, 0.8685] | 0.4421 [0.4379, 0.4463] | 0.7603 [0.7542, 0.7664] |
| formal-1 head÷r3 | 0.4010 [0.3871, 0.4154] | 0.9095 [0.8986, 0.9206] | 0.9468 [0.9334, 0.9605] | 0.8583 [0.8420, 0.8749] | 0.4436 [0.4387, 0.4485] | 0.7573 [0.7504, 0.7643] |
| formal-2 head÷r3 | 0.4376 [0.4006, 0.4781] | 0.9102 [0.8984, 0.9222] | 0.9472 [0.9360, 0.9585] | 0.8573 [0.8430, 0.8717] | 0.4406 [0.4338, 0.4476] | 0.7633 [0.7531, 0.7737] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9698 | 0.9799 | 是 |
| formal-1 | 0.9773 | 0.9856 | 是 |
| formal-2 | 0.9624 | 0.9810 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.782 / 0.792 通过 | 0.786 / 0.795 通过 |
| cista:mutate | 1.05 | 0.683 / 0.690 通过 | 0.686 / 0.693 通过 |
| cista:emit_after | 0.2 | 0.127 / 0.129 通过 | 0.129 / 0.130 通过 |
| flatbuffers:read_sum | 1.05 | 0.602 / 0.610 通过 | 0.604 / 0.613 通过 |
| flatbuffers:mutate | 1.05 | 0.601 / 0.612 通过 | 0.599 / 0.608 通过 |
| flatbuffers:emit_after | 0.2 | 0.120 / 0.121 通过 | 0.120 / 0.122 通过 |
| capnproto:read_sum | 1.05 | 0.199 / 0.201 通过 | 0.200 / 0.203 通过 |
| capnproto:mutate | 1.05 | 0.244 / 0.246 通过 | 0.244 / 0.246 通过 |
| capnproto:emit_after | 0.2 | 0.331 / 0.336 **不通过** | 0.325 / 0.331 **不通过** |
| cereal:read_sum | 1.05 | 0.831 / 0.841 通过 | 0.843 / 0.853 通过 |
| cereal:mutate | 1.05 | 0.985 / 0.999 通过 | 0.987 / 1.002 通过 |
| cereal:emit_after | 0.2 | 0.165 / 0.168 通过 | 0.168 / 0.170 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 10/12 |
| 通过数 r3（同批） | | 10/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（cleanup） | 无效（load, cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.1996 [0.1978, 0.2015] | 0.4467 [0.4424, 0.4511] | 0.3550 [0.3514, 0.3585] | 0.3607 [0.3574, 0.3641] |
| formal-2 | 0.2001 [0.1971, 0.2031] | 0.4503 [0.4426, 0.4581] | 0.3452 [0.3392, 0.3513] | 0.3638 [0.3580, 0.3698] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0052）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9913 [0.9776, 1.0052] | 是 |
| B | 0.9513 [0.9416, 0.9612] | 是 |
| C | 0.9503 [0.9418, 0.9588] | 是 |
| D | 0.8455 [0.8318, 0.8594] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9913 [0.9776, 1.0052] / 0.9913 [0.9775, 1.0052] | 0.9513 [0.9416, 0.9612] / 0.9513 [0.9430, 0.9598] | 0.9503 [0.9418, 0.9588] / 0.9503 [0.9417, 0.9589] | 0.8455 [0.8318, 0.8594] / 0.8455 [0.8321, 0.8591] | head A 0.987 B 0.980 C 0.991 D 1.020；anchor A 0.999 B 1.004 C 1.005 D 1.008 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9913 [0.9762, 1.0066] | 0.9513 [0.9211, 0.9825] | 0.9503 [0.9308, 0.9702] | 0.8455 [0.8311, 0.8601] |
| 合并 | 0.9913 [0.9762, 1.0066] | 0.9513 [0.9211, 0.9825] | 0.9503 [0.9308, 0.9702] | 0.8455 [0.8311, 0.8601] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.240 / 0.243 (0.256) | 0.088 / 0.090 (0.095) | 0.312 / 0.318 (0.332) |
| B | 0.126 / 0.127 (0.130) | 0.065 / 0.065 (0.067) | 0.161 / 0.164 (0.169) |
| C | 0.101 / 0.102 (0.105) | 0.060 / 0.061 (0.063) | 0.227 / 0.232 (0.241) |
| D | 0.284 / 0.290 (0.310) | 0.433 / 0.443 (0.476) | 1.117 / 1.156 (1.158) |

## 4. 环境

- 开跑条件 formal-1：2026-10-27T10:30:41+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.76, quiet_s=241.0, screen={'WindowServer': 3.0, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-10-27T10:41:55+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.87, quiet_s=241.0, screen={'WindowServer': 3.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-10-27T10:54:11+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.74, quiet_s=240.9, screen={'WindowServer': 3.0, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-10-27T10:34:42+00:00 → 2026-10-27T10:41:55+00:00，load [1.755859375, 1.83203125, 1.62158203125] → [2.275390625, 2.1240234375, 1.84130859375]，CPU 采样 {'samples': 213, 'other_busy_samples': 5, 'compile_samples': 0, 'other_busy_top': {'XprotectService': 3, 'runningboardd': 1, 'PerfPowerServices': 1, 'Claude Helper (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.57}
  - 计时期间屏幕采样：0/109 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.3, 'UURemoteServer': 0.5, 'parsecd': 0.0}；远程桌面活动（≥5%）0/109 次采样，只记录；WindowServer ≥20% 0/109 次采样，有效
- formal-2：2026-10-27T10:45:56+00:00 → 2026-10-27T10:54:11+00:00，load [1.87255859375, 2.1025390625, 1.89599609375] → [2.26123046875, 2.27783203125, 2.06982421875]，CPU 采样 {'samples': 243, 'other_busy_samples': 8, 'compile_samples': 0, 'other_busy_top': {'XprotectService': 5, 'Codex (Renderer)': 1, 'searchpartyd': 1, 'launchd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.88}
  - 计时期间屏幕采样：0/124 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.9, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/124 次采样，只记录；WindowServer ≥20% 0/124 次采样，有效
- slg 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.3, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/15 次采样，只记录；WindowServer ≥20% 0/15 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.064，mutate 0.063，emit_after 0.092；mutate 按副本 0.9922 / 1.0097 / 0.9999
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.064，mutate 0.062，emit_after 0.100；mutate 按副本 1.0023 / 0.9922 / 1.0042
- host macOS-26.3-arm64-arm-64bit；冻结 2026-10-27T10:30:41+00:00
- 二进制 sha256：head `a643ae2d45967052…`；head_aa `a643ae2d45967052…`；anchor `328d8872af63ecee…`；r3 `6d4a33ba1f8547dc…`；cista `13bf74996c1723ce…`；flatbuffers `684f7be86bd7b628…`；capnproto `5a7625c96f0a3e7b…`；cereal `c897ba0d1a11a0e8…`
- SLG 树 bench/slg of the head (git tree 0dd0a31e: v3 tree plus the 2.1 direct-receive offset), used for every point（sha256 `60b47b07a90ecba6ecde53c6f3403cf8d2b99f965c2acef8b2112007c9942b69`），slg_head `671c18b27b019d73…`，slg_anch `8de2ed6446045d50…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
