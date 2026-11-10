# L2 Mac 门禁批次 2026-11-10-stage350-0cbc130

- head `0cbc130decd722fc1baaa4c96882c1085e5b20a3`（STAGE350 head = v3.5.0 0cbc130d (tag object 07163a2a; release PR #252; #248 #249 #247 #251 since v3.4.0), build-r3d params）；anchor `1162e37bf40fad6074b9e4cc9c7ff109a8b35288`（v3.4.0 1162e37b (previous release tag), build-r3d params）
- STAGE350: the 3.5.0 post-release stage review (docs/RELEASING.md section 7: L2 rules, report only): v3.5.0 0cbc130d vs v3.4.0 1162e37b; merge tier of TEST_TIERS (core gate + G9R); every point built with bench/slg 07f5a956 (v3.4.0: v3.5.0's bench/slg needs XReceiver::view of #247 and xsync::XSyncCursor of #249, absent in v3.4.0; the xoffset full-check format code is the same in both); deferred bisection points (#248 38a741f3, #249 21dc9708, #247 f0557545, #251 46e0b4dc); no report-only step
- **结论：invalid**。正式部分 invalid（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `1b6788b202dc55f399fe5c25e5fd399d165d2d0b91a6b1d5ff45d2685a951ce6`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9985 [0.9924, 1.0040] | 0.9937 [0.9868, 0.9999] | 0.9985 [0.9894, 1.0067] | 1.0067 | invalid | 1.0011 / 1.0042 / 1.0106 | **无效** |
| formal-1（单批，只报） | 0.9972 [0.9885, 1.0050] | 0.9958 [0.9858, 1.0048] | 1.0022 [0.9880, 1.0149] | 1.0149 | pass | 1.0069 / 1.0098 / 1.0110 | **无效** |
| formal-2（单批，只报） | 0.9999 [0.9909, 1.0079] | 0.9917 [0.9819, 1.0004] | 0.9948 [0.9830, 1.0054] | 1.0079 | pass | 0.9953 / 0.9987 / 1.0102 | **无效** |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9655 / 0.9990 / 1.0318 | 0.9508 / 0.9928 / 1.0349 | 0.9506 / 0.9971 / 1.0481 |
| formal-1 | 0.9653 / 1.0006 / 1.0280 | 0.9534 / 0.9942 / 1.0390 | 0.9463 / 1.0068 / 1.0549 |
| formal-2 | 0.9654 / 0.9979 / 1.0388 | 0.9485 / 0.9904 / 1.0335 | 0.9493 / 0.9907 / 1.0342 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0083 [1.0015, 1.0152] | 0.9985 [0.9911, 1.0060] | 0.9937 [0.9854, 1.0021] | 0.9985 [0.9875, 1.0096] | 1.0032 [0.9957, 1.0107] | 0.9986 [0.9923, 1.0049] |
| formal-1 head÷anchor | 1.0069 [0.9974, 1.0166] | 0.9972 [0.9866, 1.0079] | 0.9958 [0.9837, 1.0082] | 1.0022 [0.9851, 1.0196] | 0.9996 [0.9881, 1.0112] | 0.9978 [0.9887, 1.0070] |
| formal-2 head÷anchor | 1.0098 [0.9996, 1.0200] | 0.9999 [0.9891, 1.0108] | 0.9917 [0.9798, 1.0036] | 0.9948 [0.9805, 1.0093] | 1.0068 [0.9971, 1.0166] | 0.9994 [0.9904, 1.0085] |
| 合并 head÷r3 | 0.9380 [0.9318, 0.9443] | 0.9604 [0.9526, 0.9682] | 0.9341 [0.9266, 0.9417] | 0.8238 [0.8141, 0.8336] | 0.8994 [0.8924, 0.9064] | 0.9350 [0.9290, 0.9411] |
| formal-1 head÷r3 | 0.9372 [0.9277, 0.9468] | 0.9566 [0.9455, 0.9678] | 0.9345 [0.9231, 0.9461] | 0.8266 [0.8122, 0.8413] | 0.8956 [0.8846, 0.9067] | 0.9329 [0.9243, 0.9415] |
| formal-2 head÷r3 | 0.9389 [0.9304, 0.9474] | 0.9642 [0.9531, 0.9755] | 0.9337 [0.9234, 0.9440] | 0.8210 [0.8075, 0.8348] | 0.9032 [0.8944, 0.9121] | 0.9372 [0.9284, 0.9460] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9937 | 0.9994 | 是 |
| formal-1 | 0.9958 | 1.0041 | 是 |
| formal-2 | 0.9917 | 0.9997 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.817 / 0.827 通过 | 0.810 / 0.822 通过 |
| cista:mutate | 1.05 | 0.675 / 0.682 通过 | 0.675 / 0.682 通过 |
| cista:emit_after | 0.2 | 0.131 / 0.133 通过 | 0.128 / 0.130 通过 |
| flatbuffers:read_sum | 1.05 | 0.625 / 0.634 通过 | 0.620 / 0.629 通过 |
| flatbuffers:mutate | 1.05 | 0.605 / 0.614 通过 | 0.602 / 0.612 通过 |
| flatbuffers:emit_after | 0.2 | 0.122 / 0.123 通过 | 0.121 / 0.122 通过 |
| capnproto:read_sum | 1.05 | 0.201 / 0.202 通过 | 0.198 / 0.200 通过 |
| capnproto:mutate | 1.05 | 0.235 / 0.237 通过 | 0.234 / 0.236 通过 |
| capnproto:emit_after | 0.2 | 0.298 / 0.302 **不通过** | 0.300 / 0.305 **不通过** |
| cereal:read_sum | 1.05 | 0.847 / 0.857 通过 | 0.847 / 0.857 通过 |
| cereal:mutate | 1.05 | 0.975 / 0.985 通过 | 0.979 / 0.991 通过 |
| cereal:emit_after | 0.2 | 0.170 / 0.172 通过 | 0.167 / 0.169 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（retire_modified_state, cleanup） | 有效 |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2417 [0.2393, 0.2441] | 0.5897 [0.5830, 0.5966] | 0.3892 [0.3826, 0.3960] | 0.4664 [0.4619, 0.4710] |
| formal-2 | 0.2179 [0.2100, 0.2261] | 0.5353 [0.5156, 0.5557] | 0.3542 [0.3412, 0.3678] | 0.4197 [0.4039, 0.4361] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0080）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9871 [0.9697, 1.0049] | 是 |
| B | 0.9982 [0.9885, 1.0080] | 是 |
| C | 0.9902 [0.9768, 1.0038] | 是 |
| D | 0.9931 [0.9741, 1.0125] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9871 [0.9697, 1.0049] / 0.9871 [0.9684, 1.0062] | 0.9982 [0.9885, 1.0080] / 0.9982 [0.9886, 1.0079] | 0.9902 [0.9768, 1.0038] / 0.9902 [0.9780, 1.0026] | 0.9931 [0.9741, 1.0125] / 0.9931 [0.9727, 1.0139] | head A 0.986 B 1.003 C 1.004 D 0.989；anchor A 0.994 B 0.995 C 0.998 D 1.024 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9871 [0.9593, 1.0158] | 0.9982 [0.9734, 1.0235] | 0.9902 [0.9657, 1.0153] | 0.9931 [0.9620, 1.0252] |
| 合并 | 0.9871 [0.9593, 1.0158] | 0.9982 [0.9734, 1.0235] | 0.9902 [0.9657, 1.0153] | 0.9931 [0.9620, 1.0252] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.220 / 0.222 (0.235) | 0.088 / 0.090 (0.094) | 0.353 / 0.360 (0.371) |
| B | 0.162 / 0.163 (0.164) | 0.076 / 0.077 (0.077) | 0.292 / 0.295 (0.292) |
| C | 0.117 / 0.118 (0.120) | 0.069 / 0.070 (0.070) | 0.348 / 0.355 (0.353) |
| D | 0.251 / 0.253 (0.285) | 0.346 / 0.350 (0.393) | 0.893 / 0.933 (1.001) |

## 4. 环境

- 开跑条件 formal-1：2026-11-10T19:47:53+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.28, quiet_s=241.1, screen={'WindowServer': 1.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-10T19:59:32+00:00 ok=True, quiet_s_required=180.0, waited_s=241.3, load1=1.29, quiet_s=241.2, screen={'WindowServer': 1.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-10T20:10:33+00:00 ok=True, quiet_s_required=180.0, waited_s=241.3, load1=1.19, quiet_s=241.2, screen={'WindowServer': 1.3, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-10T19:51:54+00:00 → 2026-11-10T19:59:32+00:00，load [1.27734375, 1.55078125, 1.65283203125] → [1.83349609375, 1.73046875, 1.67822265625]，CPU 采样 {'samples': 225, 'other_busy_samples': 7, 'compile_samples': 0, 'other_busy_top': {'UURemote': 2, 'mediaanalysisd': 2, 'cloudd': 1, 'mds_stores': 1, 'mediaanalysisd-access': 1}, 'chrome_busy_mean': 0.4, 'chrome_busy_max': 88.3, 'load1_max': 2.29}
  - 计时期间屏幕采样：0/115 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.9, 'UURemoteServer': 0.9, 'parsecd': 0.0}；远程桌面活动（≥5%）0/115 次采样，只记录；WindowServer ≥20% 0/115 次采样，有效
- formal-2：2026-11-10T20:03:33+00:00 → 2026-11-10T20:10:33+00:00，load [1.29296875, 1.50830078125, 1.58740234375] → [2.072265625, 1.9267578125, 1.75]，CPU 采样 {'samples': 206, 'other_busy_samples': 4, 'compile_samples': 0, 'other_busy_top': {'launchd': 2, 'Codex (Aperitif Renderer)': 1, 'mds_stores': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.58}
  - 计时期间屏幕采样：0/105 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.1, 'UURemoteServer': 2.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/105 次采样，只记录；WindowServer ≥20% 0/105 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 1.3, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.055，mutate 0.083，emit_after 0.091；mutate 按副本 1.0148 / 1.0019 / 1.0128
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.050，mutate 0.049，emit_after 0.071；mutate 按副本 0.9929 / 1.0012 / 1.0020
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-10T19:47:53+00:00
- 二进制 sha256：head `74d0abb9b509a923…`；head_aa `74d0abb9b509a923…`；anchor `7456e92e4fcfe4ee…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 bench/slg 07f5a956 of v3.4.0 for every point (git tree 07f5a956; head, anchor and bisection points)（sha256 `303ae31b697c5135c73a8b472da7198bb48bc4fd44767ba430912c387df6a869`），slg_head `80c0900444a65343…`，slg_anch `170c0802fe330ba7…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
