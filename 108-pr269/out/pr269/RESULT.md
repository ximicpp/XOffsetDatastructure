# L2 Mac 门禁批次 2026-11-12-pr269-8753965

- head `8753965d49520ea0ca522d1639033f831237c703`（PR269 head = next_cpp26 8753965d (#269 merged: P1 first-storage bump, P2 SLG UL in-place refill), build-r3d params）；anchor `30fb525c262b235a9de2a982942d60c4069808ab`（next_cpp26 30fb525c (v3.6.0 anchor PR #271, first parent of #269), build-r3d params）
- PR269: post-merge Mac review of #269 (3.6 perf: P1 first-storage bump, P2 SLG UL in-place refill), report only: 8753965d vs its first parent 30fb525c (v3.6.0 anchor); merge tier of TEST_TIERS (core gate + G9R); both points built with the head's bench/slg 62533ded; no bisection point (the range is #269 alone); report-only step 5x: SLG demo cases EB/UC/UP/UL send on head, anc and the anchor with its own bench/slg
- **结论：fail**。正式部分 pass（合并 2 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `1e568ae41347ab44fdc5c9db95bef670562a2600549b9fee77d65255a9211492`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9932 [0.9847, 1.0008] | 0.9968 [0.9890, 1.0039] | 0.9961 [0.9845, 1.0065] | 1.0065 | pass | 1.0031 / 1.0026 / 0.9998 | 有效 |
| formal-1（单批，只报） | 0.9962 [0.9848, 1.0065] | 1.0026 [0.9920, 1.0122] | 1.0051 [0.9922, 1.0166] | 1.0166 | pass | 1.0058 / 1.0067 / 0.9975 | 有效 |
| formal-2（单批，只报） | 0.9902 [0.9773, 1.0017] | 0.9911 [0.9795, 1.0014] | 0.9871 [0.9679, 1.0044] | 1.0044 | pass | 1.0004 / 0.9985 / 1.0021 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9395 / 1.0005 / 1.0356 | 0.9508 / 0.9976 / 1.0448 | 0.9259 / 0.9987 / 1.0607 |
| formal-1 | 0.9423 / 1.0027 / 1.0402 | 0.9551 / 1.0001 / 1.0518 | 0.9484 / 1.0081 / 1.0588 |
| formal-2 | 0.9200 / 0.9986 / 1.0307 | 0.9335 / 0.9934 / 1.0394 | 0.9093 / 0.9903 / 1.0696 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9982 [0.9890, 1.0074] | 0.9932 [0.9829, 1.0035] | 0.9968 [0.9874, 1.0064] | 0.9961 [0.9821, 1.0102] | 0.9915 [0.9826, 1.0006] | 0.9942 [0.9858, 1.0027] |
| formal-1 head÷anchor | 0.9982 [0.9865, 1.0101] | 0.9962 [0.9824, 1.0102] | 1.0026 [0.9897, 1.0157] | 1.0051 [0.9895, 1.0209] | 0.9975 [0.9848, 1.0104] | 0.9987 [0.9874, 1.0101] |
| formal-2 head÷anchor | 0.9981 [0.9837, 1.0128] | 0.9902 [0.9746, 1.0060] | 0.9911 [0.9771, 1.0052] | 0.9871 [0.9639, 1.0108] | 0.9856 [0.9728, 0.9986] | 0.9897 [0.9771, 1.0025] |
| 合并 head÷r3 | 0.9466 [0.9384, 0.9548] | 0.9648 [0.9570, 0.9727] | 0.9356 [0.9282, 0.9430] | 0.8442 [0.8344, 0.8543] | 0.9070 [0.8997, 0.9144] | 0.9397 [0.9334, 0.9460] |
| formal-1 head÷r3 | 0.9431 [0.9311, 0.9553] | 0.9649 [0.9534, 0.9766] | 0.9349 [0.9243, 0.9456] | 0.8431 [0.8310, 0.8553] | 0.9051 [0.8958, 0.9144] | 0.9389 [0.9299, 0.9480] |
| formal-2 head÷r3 | 0.9500 [0.9385, 0.9617] | 0.9647 [0.9537, 0.9758] | 0.9362 [0.9256, 0.9470] | 0.8454 [0.8293, 0.8619] | 0.9090 [0.8974, 0.9207] | 0.9405 [0.9314, 0.9497] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9968 | 1.0033 | 是 |
| formal-1 | 1.0026 | 1.0114 | 是 |
| formal-2 | 0.9911 | 1.0006 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.813 / 0.822 通过 | 0.807 / 0.822 通过 |
| cista:mutate | 1.05 | 0.671 / 0.678 通过 | 0.667 / 0.676 通过 |
| cista:emit_after | 0.2 | 0.134 / 0.135 通过 | 0.134 / 0.135 通过 |
| flatbuffers:read_sum | 1.05 | 0.622 / 0.631 通过 | 0.630 / 0.640 通过 |
| flatbuffers:mutate | 1.05 | 0.599 / 0.610 通过 | 0.607 / 0.616 通过 |
| flatbuffers:emit_after | 0.2 | 0.125 / 0.127 通过 | 0.125 / 0.127 通过 |
| capnproto:read_sum | 1.05 | 0.199 / 0.201 通过 | 0.197 / 0.199 通过 |
| capnproto:mutate | 1.05 | 0.232 / 0.235 通过 | 0.230 / 0.233 通过 |
| capnproto:emit_after | 0.2 | 0.304 / 0.308 **不通过** | 0.304 / 0.309 **不通过** |
| cereal:read_sum | 1.05 | 0.847 / 0.859 通过 | 0.854 / 0.862 通过 |
| cereal:mutate | 1.05 | 0.978 / 0.991 通过 | 0.977 / 0.987 通过 |
| cereal:emit_after | 0.2 | 0.174 / 0.177 通过 | 0.175 / 0.177 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（cleanup） | 无效（cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2366 [0.2334, 0.2397] | 0.5755 [0.5661, 0.5849] | 0.3843 [0.3789, 0.3897] | 0.4551 [0.4487, 0.4616] |
| formal-2 | 0.2384 [0.2322, 0.2448] | 0.5771 [0.5608, 0.5940] | 0.3876 [0.3774, 0.3982] | 0.4605 [0.4487, 0.4726] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0342）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 1.0005 [0.9822, 1.0191] | 是 |
| B | 1.0012 [0.9856, 1.0169] | 是 |
| C | 1.0088 [0.9840, 1.0342] | **否** |
| D | 0.9913 [0.9766, 1.0063] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 1.0005 [0.9822, 1.0191] / 1.0005 [0.9825, 1.0188] | 1.0012 [0.9856, 1.0169] / 1.0012 [0.9924, 1.0099] | 1.0088 [0.9840, 1.0342] / 1.0088 [0.9958, 1.0220] | 0.9913 [0.9766, 1.0063] / 0.9913 [0.9764, 1.0064] | head A 0.981 B 0.981 C 0.966 D 1.003；anchor A 0.984 B 0.971 C 0.959 D 1.005 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 1.0005 [0.9857, 1.0155] | 1.0012 [0.9782, 1.0247] | 1.0088 [0.9865, 1.0316] | 0.9913 [0.9805, 1.0023] |
| 合并 | 1.0005 [0.9857, 1.0155] | 1.0012 [0.9782, 1.0247] | 1.0088 [0.9865, 1.0316] | 0.9913 [0.9805, 1.0023] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.216 / 0.219 (0.232) | 0.089 / 0.091 (0.096) | 0.351 / 0.356 (0.370) |
| B | 0.158 / 0.160 (0.162) | 0.075 / 0.076 (0.077) | 0.286 / 0.290 (0.292) |
| C | 0.117 / 0.120 (0.126) | 0.069 / 0.071 (0.074) | 0.353 / 0.362 (0.373) |
| D | 0.250 / 0.254 (0.279) | 0.346 / 0.350 (0.385) | 0.930 / 0.939 (0.973) |

## 4. 环境

- 开跑条件 formal-1：2026-11-12T09:41:41+00:00 ok=True, quiet_s_required=180.0, waited_s=241.3, load1=1.35, quiet_s=241.2, screen={'WindowServer': 1.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-12T09:53:02+00:00 ok=True, quiet_s_required=180.0, waited_s=241.3, load1=1.46, quiet_s=241.2, screen={'WindowServer': 1.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-12T10:05:25+00:00 ok=True, quiet_s_required=180.0, waited_s=241.3, load1=1.25, quiet_s=241.2, screen={'WindowServer': 1.2, 'UURemoteServer': 0.6, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-12T09:45:42+00:00 → 2026-11-12T09:53:02+00:00，load [1.3505859375, 1.57373046875, 1.57275390625] → [2.2265625, 1.97509765625, 1.74169921875]，CPU 采样 {'samples': 216, 'other_busy_samples': 6, 'compile_samples': 0, 'other_busy_top': {'mediaanalysisd': 2, 'Claude Helper (Renderer)': 1, 'UURemote': 1, 'launchd': 1, 'Codex (Aperitif Renderer)': 1}, 'chrome_busy_mean': 0.3, 'chrome_busy_max': 61.7, 'load1_max': 3.23}
  - 计时期间屏幕采样：0/110 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.8, 'UURemoteServer': 1.4, 'parsecd': 0.0}；远程桌面活动（≥5%）0/110 次采样，只记录；WindowServer ≥20% 0/110 次采样，有效
- formal-2：2026-11-12T09:57:03+00:00 → 2026-11-12T10:05:24+00:00，load [1.46484375, 1.685546875, 1.6611328125] → [1.951171875, 2.025390625, 1.85595703125]，CPU 采样 {'samples': 246, 'other_busy_samples': 8, 'compile_samples': 0, 'other_busy_top': {'UURemote': 2, 'duetexpertd': 2, 'mds_stores': 1, 'mediaanalysisd-access': 1, 'dasd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 3.43}
  - 计时期间屏幕采样：0/126 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 5.7, 'UURemoteServer': 1.5, 'parsecd': 1.5}；远程桌面活动（≥5%）0/126 次采样，只记录；WindowServer ≥20% 0/126 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 1.5, 'UURemoteServer': 1.5, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.074，mutate 0.068，emit_after 0.074；mutate 按副本 1.0134 / 1.0113 / 0.9956
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.049，mutate 0.048，emit_after 0.086；mutate 按副本 1.0024 / 0.9993 / 0.9938
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-12T09:41:41+00:00
- 二进制 sha256：head `b03a639b8caf13ee…`；head_aa `b03a639b8caf13ee…`；anchor `d0a9aefb6d3c4aad…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 bench/slg 62533ded of the head for both points (git tree 62533ded; the anchor's own 17b7919b differs only in the UL refill, timed in 5x)（sha256 `1a679d10f2821a13b343cce42e822481e4093674833211593394c46aa2a0a4eb`），slg_head `c86823cf01da92e0…`，slg_anch `6e640125554fc06e…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
