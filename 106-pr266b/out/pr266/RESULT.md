# L2 Mac 门禁批次 2026-11-11-pr266-bb537d0

- head `bb537d0db6a7d629f10165e9cc68aadc745b5171`（PR266 head = next_cpp26 bb537d0d (#266 merged: #265 free-table fix + root<T>() cache refill, redo of #223), build-r3d params）；anchor `cf9b63e584247ca6c61f524591557d468062bb48`（next_cpp26 cf9b63e5 (#256 merged, first parent of #266), build-r3d params）
- PR266: post-merge Mac review of #266 (L class: #265 free-table fix for SLG M D3 + root<T>() cache refill, redo of #223), report only: bb537d0d vs its first parent cf9b63e5; merge tier of TEST_TIERS (core gate + G9R); both points built with bench/slg 4e62a93c; no bisection point (the range is #266 alone); report-only step 5x: HFT 64 books (book_c.cpp, access root and ptr) and the SLG M D3 check
- **结论：fail**。正式部分 pass（合并 2 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `87874b710813323ed9cd8793c1658924240a9222fce6c0bf5008f62c2df779f4`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9995 [0.9930, 1.0052] | 1.0050 [0.9985, 1.0108] | 1.0035 [0.9931, 1.0128] | 1.0128 | pass | 1.0008 / 1.0008 / 1.0009 | 有效 |
| formal-1（单批，只报） | 1.0012 [0.9923, 1.0091] | 1.0095 [0.9999, 1.0179] | 1.0075 [0.9979, 1.0162] | 1.0179 | pass | 1.0020 / 0.9984 / 1.0045 | 有效 |
| formal-2（单批，只报） | 0.9978 [0.9882, 1.0063] | 1.0005 [0.9915, 1.0085] | 0.9995 [0.9809, 1.0162] | 1.0162 | pass | 0.9995 / 1.0032 / 0.9972 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9697 / 0.9988 / 1.0281 | 0.9703 / 1.0030 / 1.0406 | 0.9533 / 1.0016 / 1.0441 |
| formal-1 | 0.9611 / 0.9990 / 1.0347 | 0.9658 / 1.0109 / 1.0549 | 0.9684 / 1.0051 / 1.0486 |
| formal-2 | 0.9721 / 0.9976 / 1.0176 | 0.9726 / 0.9997 / 1.0253 | 0.9432 / 0.9920 / 1.0438 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9991 [0.9913, 1.0071] | 0.9995 [0.9917, 1.0072] | 1.0050 [0.9971, 1.0128] | 1.0035 [0.9910, 1.0161] | 1.0006 [0.9932, 1.0082] | 1.0014 [0.9950, 1.0079] |
| formal-1 head÷anchor | 0.9998 [0.9861, 1.0136] | 1.0012 [0.9904, 1.0120] | 1.0095 [0.9980, 1.0211] | 1.0075 [0.9959, 1.0194] | 0.9982 [0.9877, 1.0087] | 1.0032 [0.9944, 1.0120] |
| formal-2 head÷anchor | 0.9985 [0.9901, 1.0070] | 0.9978 [0.9862, 1.0094] | 1.0005 [0.9896, 1.0114] | 0.9995 [0.9770, 1.0224] | 1.0031 [0.9921, 1.0143] | 0.9996 [0.9899, 1.0095] |
| 合并 head÷r3 | 0.9405 [0.9343, 0.9468] | 0.9544 [0.9457, 0.9633] | 0.9277 [0.9190, 0.9364] | 0.8348 [0.8234, 0.8463] | 0.9070 [0.8999, 0.9142] | 0.9326 [0.9258, 0.9394] |
| formal-1 head÷r3 | 0.9445 [0.9354, 0.9537] | 0.9598 [0.9481, 0.9716] | 0.9326 [0.9217, 0.9436] | 0.8304 [0.8186, 0.8424] | 0.9082 [0.8991, 0.9175] | 0.9363 [0.9277, 0.9451] |
| formal-2 head÷r3 | 0.9366 [0.9278, 0.9454] | 0.9491 [0.9359, 0.9625] | 0.9228 [0.9090, 0.9367] | 0.8392 [0.8193, 0.8596] | 0.9058 [0.8945, 0.9173] | 0.9288 [0.9181, 0.9395] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 1.0050 | 1.0103 | 是 |
| formal-1 | 1.0095 | 1.0172 | 是 |
| formal-2 | 1.0005 | 1.0078 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.806 / 0.820 通过 | 0.806 / 0.818 通过 |
| cista:mutate | 1.05 | 0.669 / 0.678 通过 | 0.667 / 0.674 通过 |
| cista:emit_after | 0.2 | 0.132 / 0.133 通过 | 0.133 / 0.136 通过 |
| flatbuffers:read_sum | 1.05 | 0.618 / 0.627 通过 | 0.619 / 0.629 通过 |
| flatbuffers:mutate | 1.05 | 0.601 / 0.612 通过 | 0.602 / 0.610 通过 |
| flatbuffers:emit_after | 0.2 | 0.124 / 0.125 通过 | 0.124 / 0.127 通过 |
| capnproto:read_sum | 1.05 | 0.197 / 0.199 通过 | 0.195 / 0.197 通过 |
| capnproto:mutate | 1.05 | 0.233 / 0.236 通过 | 0.230 / 0.232 通过 |
| capnproto:emit_after | 0.2 | 0.303 / 0.307 **不通过** | 0.305 / 0.312 **不通过** |
| cereal:read_sum | 1.05 | 0.841 / 0.858 通过 | 0.839 / 0.849 通过 |
| cereal:mutate | 1.05 | 0.977 / 0.989 通过 | 0.970 / 0.980 通过 |
| cereal:emit_after | 0.2 | 0.173 / 0.174 通过 | 0.174 / 0.177 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（cleanup） | 无效（cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2295 [0.2218, 0.2374] | 0.5626 [0.5435, 0.5823] | 0.3749 [0.3630, 0.3872] | 0.4447 [0.4295, 0.4604] |
| formal-2 | 0.2311 [0.2230, 0.2394] | 0.5616 [0.5411, 0.5829] | 0.3758 [0.3627, 0.3895] | 0.4462 [0.4300, 0.4631] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0330）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 1.0111 [0.9897, 1.0330] | **否** |
| B | 0.9988 [0.9886, 1.0091] | 是 |
| C | 1.0026 [0.9856, 1.0199] | 是 |
| D | 1.0131 [0.9918, 1.0349] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 1.0111 [0.9897, 1.0330] / 1.0111 [0.9899, 1.0327] | 0.9988 [0.9886, 1.0091] / 0.9988 [0.9897, 1.0080] | 1.0026 [0.9856, 1.0199] / 1.0026 [0.9879, 1.0175] | 1.0131 [0.9918, 1.0349] / 1.0131 [0.9948, 1.0318] | head A 1.000 B 0.996 C 0.999 D 1.006；anchor A 1.019 B 0.989 C 0.982 D 1.000 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 1.0111 [0.9613, 1.0634] | 0.9988 [0.9823, 1.0155] | 1.0026 [0.9678, 1.0387] | 1.0131 [0.9746, 1.0532] |
| 合并 | 1.0111 [0.9613, 1.0634] | 0.9988 [0.9823, 1.0155] | 1.0026 [0.9678, 1.0387] | 1.0131 [0.9746, 1.0532] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.219 / 0.221 (0.231) | 0.089 / 0.092 (0.095) | 0.354 / 0.356 (0.370) |
| B | 0.159 / 0.160 (0.162) | 0.075 / 0.076 (0.077) | 0.288 / 0.292 (0.294) |
| C | 0.118 / 0.119 (0.123) | 0.069 / 0.070 (0.072) | 0.351 / 0.356 (0.363) |
| D | 0.251 / 0.254 (0.284) | 0.343 / 0.347 (0.385) | 0.942 / 0.963 (0.989) |

## 4. 环境

- 开跑条件 formal-1：2026-11-11T15:48:26+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.64, quiet_s=241.1, screen={'WindowServer': 1.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-11T15:59:44+00:00 ok=True, quiet_s_required=180.0, waited_s=241.3, load1=1.05, quiet_s=241.2, screen={'WindowServer': 1.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-11T16:12:11+00:00 ok=True, quiet_s_required=180.0, waited_s=241.3, load1=1.14, quiet_s=241.2, screen={'WindowServer': 1.7, 'UURemoteServer': 0.1, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-11T15:52:27+00:00 → 2026-11-11T15:59:44+00:00，load [1.64013671875, 1.89794921875, 1.8603515625] → [1.8095703125, 1.8359375, 1.828125]，CPU 采样 {'samples': 215, 'other_busy_samples': 6, 'compile_samples': 0, 'other_busy_top': {'launchd': 2, 'mds_stores': 1, 'mediaanalysisd': 1, 'opendirectoryd': 1, 'corespotlightd': 1}, 'chrome_busy_mean': 0.2, 'chrome_busy_max': 40.9, 'load1_max': 2.01}
  - 计时期间屏幕采样：0/110 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.8, 'UURemoteServer': 0.8, 'parsecd': 0.0}；远程桌面活动（≥5%）0/110 次采样，只记录；WindowServer ≥20% 0/110 次采样，有效
- formal-2：2026-11-11T16:03:45+00:00 → 2026-11-11T16:12:11+00:00，load [1.05126953125, 1.494140625, 1.68408203125] → [1.96630859375, 1.77783203125, 1.744140625]，CPU 采样 {'samples': 249, 'other_busy_samples': 7, 'compile_samples': 0, 'other_busy_top': {'Codex (Aperitif Renderer)': 2, 'launchd': 2, 'sysmond': 1, 'frauddefensed': 1, 'UURemote': 1}, 'chrome_busy_mean': 0.8, 'chrome_busy_max': 78.2, 'load1_max': 2.11}
  - 计时期间屏幕采样：0/127 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.0, 'UURemoteServer': 1.6, 'parsecd': 0.1}；远程桌面活动（≥5%）0/127 次采样，只记录；WindowServer ≥20% 0/127 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 1.7, 'UURemoteServer': 0.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.062，mutate 0.054，emit_after 0.081；mutate 按副本 0.9914 / 1.0047 / 0.9991
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.049，mutate 0.045，emit_after 0.079；mutate 按副本 1.0043 / 1.0039 / 1.0016
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-11T15:48:25+00:00
- 二进制 sha256：head `5ac31c765ae421ae…`；head_aa `5ac31c765ae421ae…`；anchor `15362c6eddddb9d3…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 bench/slg 4e62a93c of the head for both points (git tree 4e62a93c; the same tree at the anchor)（sha256 `c48fb21ac7881209bd3e20f51b1e2eea44ce24793877415a9c4504bc84b7ac2a`），slg_head `b8a69e173ed2d056…`，slg_anch `982f45189b576dea…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
