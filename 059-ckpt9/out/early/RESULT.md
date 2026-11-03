# L2 Mac 门禁批次 2026-11-03-ckpt9-50f96b0

- head `50f96b0db482d40909d6c2e00adadb58df12e329`（CKPT9 head = class 1 integration 50f96b0d (c4925fa7 + 6594edaa test fix + 50f96b0d emission speed-up; contains 5433b4eb; class L, pre-merge), build-r3d params）；anchor `c9fbaea2e980f6f708df615181ced209d653dc0f`（next_cpp26 c9fbaea2 (#190; library headers byte-identical to v3.2.1 / 3.2.0), build-r3d params）
- CKPT9: L2 gate of the class 1 integration second head (50f96b0d, class L, before merge); anchor c9fbaea2 (library = v3.2.1); own bench/slg per point; one prebuilt bisection point (5433b4eb = CKPT8 head); 5f demo and 5g v3 demo (bench d36510eb, new write-back + SLG_A_FULL=1, head / 5433b4eb / anchor libraries) report only; no G9R
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `a8b6ccc20cd7824906a5193876effa45acbeef10942eee02be0d497de75994cf`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9978 [0.9907, 1.0041] | 0.9978 [0.9907, 1.0041] | 0.9781 [0.9688, 0.9863] | 1.0041 | pass | 1.0027 / 1.0046 / 1.0039 | 有效 |
| formal-1（单批，只报） | 0.9960 [0.9853, 1.0056] | 0.9964 [0.9860, 1.0058] | 0.9789 [0.9657, 0.9907] | 1.0058 | pass | 1.0029 / 1.0042 / 0.9998 | 有效 |
| formal-2（单批，只报） | 0.9995 [0.9900, 1.0079] | 0.9991 [0.9894, 1.0079] | 0.9772 [0.9639, 0.9891] | 1.0079 | pass | 1.0025 / 1.0050 / 1.0079 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9608 / 0.9988 / 1.0395 | 0.9593 / 0.9979 / 1.0439 | 0.9321 / 0.9773 / 1.0289 |
| formal-1 | 0.9558 / 0.9970 / 1.0462 | 0.9599 / 0.9941 / 1.0456 | 0.9324 / 0.9798 / 1.0366 |
| formal-2 | 0.9618 / 1.0001 / 1.0386 | 0.9565 / 1.0005 / 1.0423 | 0.9319 / 0.9754 / 1.0294 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0005 [0.9940, 1.0069] | 0.9978 [0.9893, 1.0063] | 0.9978 [0.9893, 1.0063] | 0.9781 [0.9670, 0.9893] | 0.9901 [0.9827, 0.9975] | 0.9960 [0.9892, 1.0030] |
| formal-1 head÷anchor | 1.0046 [0.9956, 1.0136] | 0.9960 [0.9831, 1.0092] | 0.9964 [0.9838, 1.0093] | 0.9789 [0.9630, 0.9951] | 0.9892 [0.9803, 0.9983] | 0.9951 [0.9849, 1.0055] |
| formal-2 head÷anchor | 0.9964 [0.9870, 1.0058] | 0.9995 [0.9880, 1.0111] | 0.9991 [0.9873, 1.0111] | 0.9772 [0.9612, 0.9935] | 0.9909 [0.9788, 1.0032] | 0.9969 [0.9874, 1.0066] |
| 合并 head÷r3 | 0.9293 [0.9233, 0.9354] | 0.9521 [0.9430, 0.9612] | 0.9697 [0.9618, 0.9778] | 0.8290 [0.8194, 0.8387] | 0.9395 [0.9319, 0.9472] | 0.9455 [0.9387, 0.9524] |
| formal-1 head÷r3 | 0.9310 [0.9214, 0.9407] | 0.9467 [0.9335, 0.9602] | 0.9688 [0.9568, 0.9810] | 0.8374 [0.8237, 0.8514] | 0.9496 [0.9396, 0.9596] | 0.9448 [0.9349, 0.9548] |
| formal-2 head÷r3 | 0.9277 [0.9199, 0.9355] | 0.9574 [0.9448, 0.9702] | 0.9707 [0.9598, 0.9817] | 0.8207 [0.8075, 0.8341] | 0.9296 [0.9189, 0.9404] | 0.9463 [0.9364, 0.9562] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9978 | 1.0036 | 是 |
| formal-1 | 0.9964 | 1.0050 | 是 |
| formal-2 | 0.9991 | 1.0072 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.786 / 0.795 通过 | 0.799 / 0.810 通过 |
| cista:mutate | 1.05 | 0.693 / 0.701 通过 | 0.696 / 0.702 通过 |
| cista:emit_after | 0.2 | 0.125 / 0.127 通过 | 0.128 / 0.130 通过 |
| flatbuffers:read_sum | 1.05 | 0.610 / 0.620 通过 | 0.615 / 0.625 通过 |
| flatbuffers:mutate | 1.05 | 0.625 / 0.635 通过 | 0.626 / 0.638 通过 |
| flatbuffers:emit_after | 0.2 | 0.118 / 0.120 通过 | 0.121 / 0.122 通过 |
| capnproto:read_sum | 1.05 | 0.195 / 0.197 通过 | 0.197 / 0.199 通过 |
| capnproto:mutate | 1.05 | 0.238 / 0.240 通过 | 0.239 / 0.241 通过 |
| capnproto:emit_after | 0.2 | 0.312 / 0.317 **不通过** | 0.299 / 0.303 **不通过** |
| cereal:read_sum | 1.05 | 0.842 / 0.852 通过 | 0.845 / 0.857 通过 |
| cereal:mutate | 1.05 | 1.023 / 1.035 通过 | 1.028 / 1.042 通过 |
| cereal:emit_after | 0.2 | 0.164 / 0.166 通过 | 0.169 / 0.170 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 10/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（retire_modified_state, cleanup） | 无效（cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2143 [0.2069, 0.2220] | 0.5197 [0.5008, 0.5393] | 0.3695 [0.3608, 0.3783] | 0.4085 [0.3933, 0.4243] |
| formal-2 | 0.2334 [0.2291, 0.2378] | 0.5702 [0.5592, 0.5815] | 0.3845 [0.3775, 0.3916] | 0.4491 [0.4408, 0.4577] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0096）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9875 [0.9659, 1.0096] | 是 |
| B | 0.9601 [0.9343, 0.9865] | 是 |
| C | 0.9814 [0.9616, 1.0017] | 是 |
| D | 1.0046 [0.9908, 1.0185] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9875 [0.9659, 1.0096] / 0.9875 [0.9687, 1.0066] | 0.9601 [0.9343, 0.9865] / 0.9601 [0.9416, 0.9789] | 0.9814 [0.9616, 1.0017] / 0.9814 [0.9670, 0.9961] | 1.0046 [0.9908, 1.0185] / 1.0046 [0.9899, 1.0195] | head A 0.977 B 0.981 C 0.971 D 1.009；anchor A 0.985 B 0.985 C 0.979 D 0.998 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9875 [0.9797, 0.9953] | 0.9601 [0.9046, 1.0190] | 0.9814 [0.9640, 0.9992] | 1.0046 [0.9658, 1.0449] |
| 合并 | 0.9875 [0.9797, 0.9953] | 0.9601 [0.9046, 1.0190] | 0.9814 [0.9640, 0.9992] | 1.0046 [0.9658, 1.0449] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.234 / 0.237 (0.252) | 0.097 / 0.100 (0.107) | 0.328 / 0.336 (0.355) |
| B | 0.127 / 0.131 (0.131) | 0.067 / 0.069 (0.069) | 0.173 / 0.178 (0.180) |
| C | 0.102 / 0.104 (0.111) | 0.063 / 0.064 (0.069) | 0.252 / 0.259 (0.276) |
| D | 0.226 / 0.229 (0.254) | 0.352 / 0.359 (0.392) | 0.934 / 0.951 (0.985) |

## 4. 环境

- 开跑条件 formal-1：2026-11-03T04:52:42+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.75, quiet_s=241.0, screen={'WindowServer': 4.3, 'UURemoteServer': 0.7, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-03T05:04:28+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.75, quiet_s=241.0, screen={'WindowServer': 4.0, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-03T05:15:26+00:00 ok=True, quiet_s_required=180.0, waited_s=241.3, load1=1.34, quiet_s=241.2, screen={'WindowServer': 1.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-03T04:56:43+00:00 → 2026-11-03T05:04:28+00:00，load [1.7548828125, 1.85546875, 2.2099609375] → [2.23388671875, 2.04052734375, 2.14111328125]，CPU 采样 {'samples': 229, 'other_busy_samples': 9, 'compile_samples': 0, 'other_busy_top': {'fairplaydeviceidentityd': 2, 'Codex (Renderer)': 1, 'sysmond': 1, 'PerfPowerServices': 1, 'appstoreagent': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 3.1}
  - 计时期间屏幕采样：0/117 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 6.9, 'UURemoteServer': 1.8, 'parsecd': 0.4}；远程桌面活动（≥5%）0/117 次采样，只记录；WindowServer ≥20% 0/117 次采样，有效
- formal-2：2026-11-03T05:08:29+00:00 → 2026-11-03T05:15:26+00:00，load [1.74951171875, 1.71142578125, 1.95263671875] → [1.8671875, 1.8251953125, 1.9111328125]，CPU 采样 {'samples': 205, 'other_busy_samples': 4, 'compile_samples': 0, 'other_busy_top': {'Claude Helper (Renderer)': 1, 'CarbonComponentScannerXPC': 1, 'logd_helper': 1, 'mobileassetd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.39}
  - 计时期间屏幕采样：0/105 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 6.1, 'UURemoteServer': 12.7, 'parsecd': 0.0}；远程桌面活动（≥5%）1/105 次采样，只记录；WindowServer ≥20% 0/105 次采样，有效
- slg 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 1.3, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/15 次采样，只记录；WindowServer ≥20% 0/15 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.056，mutate 0.056，emit_after 0.073；mutate 按副本 1.0082 / 1.0019 / 1.0026
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.062，mutate 0.057，emit_after 0.088；mutate 按副本 1.0079 / 1.0070 / 1.0003
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-03T04:52:42+00:00
- 二进制 sha256：head `fdbefb076ebfd01f…`；head_aa `fdbefb076ebfd01f…`；anchor `6eece9b8d14ba644…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree f24c253a, anchor 4180ba9b; bisection points their own)（sha256 `f4de6283f6a04ddff84619363f7a3a21917bced75e6bc9a81fc6dd77f8eb98ad`），slg_head `e1330ebd2309f937…`，slg_anch `19978229bfc1997e…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
