# L2 Mac 门禁批次 2026-10-26-int3p4m-235dcda

- head `235dcda2e4276047245f47ee2430073087f2caa6`（P4m = e46cf38 + validation one-shot claim (235dcda, PR #113), build-r3d params）；anchor `e46cf38190e119d27b6e3043d518d7c135bfefe8`（next_cpp26 e46cf38 (#104 merged), build-r3d params）
- P4m alone on e46cf38 (PR #113): xoffsetdatastructure.hpp +24/-4 (validation claims the arena once), tests/test_wire_validation.cpp; bench/ unchanged; M wire identical
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `3a2b1825c3e981f3be18e1be64393efec82a0fd2731fe2dadb8ca07000710a6e`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 1.0002 [0.9877, 1.0115] | 0.9918 [0.9824, 1.0002] | 0.9995 [0.9859, 1.0117] | 1.0117 | pass | 0.9935 / 1.0091 / 0.9952 | 有效 |
| formal-1（单批，只报） | 0.9965 [0.9809, 1.0104] | 0.9908 [0.9789, 1.0014] | 1.0008 [0.9794, 1.0200] | 1.0200 | pending | 0.9927 / 1.0075 / 0.9905 | 有效 |
| formal-2（单批，只报） | 1.0040 [0.9840, 1.0221] | 0.9929 [0.9779, 1.0063] | 0.9982 [0.9808, 1.0138] | 1.0221 | pending | 0.9943 / 1.0107 / 0.9999 | **无效** |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9431 / 0.9974 / 1.0734 | 0.9379 / 0.9904 / 1.0435 | 0.9236 / 1.0009 / 1.0851 |
| formal-1 | 0.9402 / 0.9938 / 1.0749 | 0.9469 / 0.9887 / 1.0453 | 0.9158 / 1.0077 / 1.0967 |
| formal-2 | 0.9494 / 1.0116 / 1.0664 | 0.9295 / 1.0013 / 1.0430 | 0.9275 / 0.9954 / 1.0756 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.8547 [0.8438, 0.8659] | 1.0002 [0.9852, 1.0155] | 0.9918 [0.9805, 1.0032] | 0.9995 [0.9832, 1.0160] | 0.9353 [0.9191, 0.9519] | 0.9791 [0.9684, 0.9898] |
| formal-1 head÷anchor | 0.8549 [0.8447, 0.8652] | 0.9965 [0.9777, 1.0156] | 0.9908 [0.9764, 1.0053] | 1.0008 [0.9750, 1.0272] | 0.9352 [0.9154, 0.9554] | 0.9774 [0.9658, 0.9892] |
| formal-2 head÷anchor | 0.8546 [0.8347, 0.8750] | 1.0040 [0.9799, 1.0288] | 0.9929 [0.9748, 1.0112] | 0.9982 [0.9772, 1.0196] | 0.9354 [0.9089, 0.9628] | 0.9807 [0.9624, 0.9993] |
| 合并 head÷r3 | 0.4252 [0.4216, 0.4288] | 0.9559 [0.9450, 0.9670] | 0.9794 [0.9687, 0.9902] | 0.9893 [0.9742, 1.0046] | 0.4899 [0.4797, 0.5002] | 0.8002 [0.7943, 0.8062] |
| formal-1 head÷r3 | 0.4233 [0.4185, 0.4283] | 0.9528 [0.9354, 0.9706] | 0.9825 [0.9681, 0.9971] | 0.9885 [0.9654, 1.0122] | 0.4757 [0.4621, 0.4897] | 0.7977 [0.7888, 0.8066] |
| formal-2 head÷r3 | 0.4271 [0.4217, 0.4325] | 0.9590 [0.9453, 0.9730] | 0.9763 [0.9600, 0.9928] | 0.9900 [0.9698, 1.0107] | 0.5044 [0.4909, 0.5183] | 0.8028 [0.7947, 0.8109] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9918 | 0.9995 | 是 |
| formal-1 | 0.9908 | 1.0005 | 是 |
| formal-2 | 0.9929 | 1.0052 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.804 / 0.818 通过 | 0.813 / 0.824 通过 |
| cista:mutate | 1.05 | 0.705 / 0.714 通过 | 0.708 / 0.717 通过 |
| cista:emit_after | 0.2 | 0.151 / 0.154 通过 | 0.155 / 0.158 通过 |
| flatbuffers:read_sum | 1.05 | 0.623 / 0.633 通过 | 0.624 / 0.633 通过 |
| flatbuffers:mutate | 1.05 | 0.610 / 0.622 通过 | 0.608 / 0.621 通过 |
| flatbuffers:emit_after | 0.2 | 0.141 / 0.144 通过 | 0.145 / 0.148 通过 |
| capnproto:read_sum | 1.05 | 0.213 / 0.216 通过 | 0.213 / 0.216 通过 |
| capnproto:mutate | 1.05 | 0.251 / 0.254 通过 | 0.248 / 0.251 通过 |
| capnproto:emit_after | 0.2 | 0.370 / 0.378 **不通过** | 0.372 / 0.378 **不通过** |
| cereal:read_sum | 1.05 | 0.867 / 0.882 通过 | 0.876 / 0.889 通过 |
| cereal:mutate | 1.05 | 1.016 / 1.032 通过 | 1.013 / 1.031 通过 |
| cereal:emit_after | 0.2 | 0.196 / 0.200 **不通过** | 0.202 / 0.205 **不通过** |
| 通过数 head | | 10/12 | 10/12 |
| 通过数 anchor（同批） | | 10/12 | 10/12 |
| 通过数 r3（同批） | | 9/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（cleanup） | 无效（retire_modified_state, cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2249 [0.2170, 0.2331] | 0.5043 [0.4864, 0.5230] | 0.3706 [0.3579, 0.3837] | 0.4103 [0.3961, 0.4250] |
| formal-2 | 0.2478 [0.2392, 0.2567] | 0.5547 [0.5344, 0.5759] | 0.4066 [0.3924, 0.4214] | 0.4535 [0.4376, 0.4700] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0049）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9439 [0.9326, 0.9553] | 是 |
| B | 0.9724 [0.9628, 0.9821] | 是 |
| C | 0.9922 [0.9797, 1.0049] | 是 |
| D | 0.9939 [0.9710, 1.0173] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9439 [0.9326, 0.9553] / 0.9439 [0.9329, 0.9550] | 0.9724 [0.9628, 0.9821] / 0.9724 [0.9634, 0.9816] | 0.9922 [0.9797, 1.0049] / 0.9922 [0.9792, 1.0054] | 0.9939 [0.9710, 1.0173] / 0.9939 [0.9700, 1.0184] | head A 0.997 B 1.007 C 1.003 D 1.018；anchor A 0.990 B 0.987 C 0.995 D 0.982 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9439 [0.9101, 0.9790] | 0.9724 [0.9364, 1.0099] | 0.9922 [0.9630, 1.0223] | 0.9939 [0.9712, 1.0171] |
| 合并 | 0.9439 [0.9101, 0.9790] | 0.9724 [0.9364, 1.0099] | 0.9922 [0.9630, 1.0223] | 0.9939 [0.9712, 1.0171] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.240 / 0.242 (0.257) | 0.089 / 0.090 (0.097) | 0.317 / 0.319 (0.337) |
| B | 0.137 / 0.137 (0.142) | 0.070 / 0.071 (0.073) | 0.177 / 0.178 (0.183) |
| C | 0.106 / 0.108 (0.112) | 0.064 / 0.065 (0.067) | 0.241 / 0.245 (0.260) |
| D | 0.329 / 0.334 (0.367) | 0.511 / 0.526 (0.567) | 1.356 / 1.384 (1.410) |

## 4. 环境

- 开跑条件 formal-1：2026-10-26T21:22:12+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.82, quiet_s=240.9, screen={'WindowServer': 4.0, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-10-26T21:34:15+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.78, quiet_s=240.9, screen={'WindowServer': 4.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-10-26T21:45:28+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=2.21, quiet_s=241.0, screen={'WindowServer': 3.3, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-10-26T21:26:13+00:00 → 2026-10-26T21:34:15+00:00，load [1.82275390625, 2.103515625, 2.02490234375] → [2.38916015625, 2.3203125, 2.15771484375]，CPU 采样 {'samples': 237, 'other_busy_samples': 7, 'compile_samples': 0, 'other_busy_top': {'XprotectService': 2, 'searchpartyd': 2, 'Claude Helper (Renderer)': 2, 'Codex (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.75}
  - 计时期间屏幕采样：0/121 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.0, 'UURemoteServer': 0.4, 'parsecd': 0.0}；远程桌面活动（≥5%）0/121 次采样，只记录；WindowServer ≥20% 0/121 次采样，有效
- formal-2：2026-10-26T21:38:16+00:00 → 2026-10-26T21:45:27+00:00，load [1.78076171875, 2.03515625, 2.0615234375] → [2.38818359375, 2.12841796875, 2.060546875]，CPU 采样 {'samples': 212, 'other_busy_samples': 11, 'compile_samples': 0, 'other_busy_top': {'trustd': 5, 'Claude Helper (Renderer)': 4, 'runningboardd': 1, 'PerfPowerServices': 1, 'Codex (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.42}
  - 计时期间屏幕采样：0/109 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/109 次采样，只记录；WindowServer ≥20% 0/109 次采样，有效
- slg 计时期间屏幕采样：0/16 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.8, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/16 次采样，只记录；WindowServer ≥20% 0/16 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.086，mutate 0.081，emit_after 0.100；mutate 按副本 0.9974 / 1.0063 / 1.0190
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.076，mutate 0.079，emit_after 0.107；mutate 按副本 1.0116 / 1.0145 / 1.0062
- host macOS-26.3-arm64-arm-64bit；冻结 2026-10-26T21:22:12+00:00
- 二进制 sha256：head `7b075f544f28be2d…`；head_aa `7b075f544f28be2d…`；anchor `2ddce61e21b9f3b0…`；r3 `6d4a33ba1f8547dc…`；cista `13bf74996c1723ce…`；flatbuffers `684f7be86bd7b628…`；capnproto `5a7625c96f0a3e7b…`；cereal `c897ba0d1a11a0e8…`
- SLG 树 bench/slg of e46cf38 (v3 tree, git tree ffd88a16, unchanged since f8eebf4), used for every point（sha256 `2a010b04d53b4049727c4061b9ac750f26ff55bff2bb56da78a3f65c1c91081e`），slg_head `6b1c7dd602f90893…`，slg_anch `f801d7abd33f1685…`
- 准备检查：gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0；gate_sml_p4m exit 0；slg_verify_reuse_p4m exit 0；slg_verify_default_p4m exit 0
