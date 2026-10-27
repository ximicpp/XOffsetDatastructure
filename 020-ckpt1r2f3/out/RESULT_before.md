# L2 Mac 门禁批次 2026-10-26-ckpt1r2-f8eebf4

- head `f8eebf46565c746a69ed64dbe9e406546032f892`（CKPT1 head = next_cpp26 f8eebf4 (INT1 merged), build-r3d params）；anchor `414dea49870365fa016d65e267cb6530ce40f7c4`（414dea4 (#81 merge, CKPT1 anchor), build-r3d params）
- first checkpoint (merge rules v2), rerun of INT2's CKPT1 (invalid by the old screen rule): every first-parent merge 414dea4..f8eebf4, 0, S and L classes, including INT1; bisection points deferred
- **结论：invalid**。正式部分 invalid（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `202cc249b8f99bdf678b0f1e8da65c2e82f626ce972f6d8b95d7589ed7ff5175`；mac_gate.py `3cc49a962d4611eadbc17f83ed3bdfe501fe7eb6be023eb3ccab90192b4c5911`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9940 [0.9842, 1.0028] | 1.0044 [0.9946, 1.0133] | 0.9493 [0.9364, 0.9610] | 1.0133 | invalid | 1.0127 / 1.0080 / 1.0026 | **无效** |
| formal-1（单批，只报） | 0.9917 [0.9784, 1.0036] | 1.0039 [0.9904, 1.0160] | 0.9476 [0.9276, 0.9656] | 1.0160 | pass | 1.0169 / 1.0086 / 1.0014 | **无效** |
| formal-2（单批，只报） | 0.9963 [0.9814, 1.0098] | 1.0049 [0.9902, 1.0182] | 0.9511 [0.9341, 0.9664] | 1.0182 | pass | 1.0086 / 1.0074 / 1.0038 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9382 / 0.9930 / 1.0597 | 0.9492 / 1.0066 / 1.0659 | 0.8755 / 0.9527 / 1.0290 |
| formal-1 | 0.9363 / 1.0000 / 1.0488 | 0.9494 / 1.0072 / 1.0689 | 0.8499 / 0.9564 / 1.0276 |
| formal-2 | 0.9367 / 0.9918 / 1.0751 | 0.9482 / 1.0059 / 1.0662 | 0.8882 / 0.9481 / 1.0360 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9987 [0.9911, 1.0063] | 0.9940 [0.9822, 1.0060] | 1.0044 [0.9926, 1.0164] | 0.9493 [0.9338, 0.9651] | 1.0141 [1.0023, 1.0262] | 0.9969 [0.9898, 1.0041] |
| formal-1 head÷anchor | 0.9961 [0.9859, 1.0063] | 0.9917 [0.9756, 1.0080] | 1.0039 [0.9876, 1.0205] | 0.9476 [0.9235, 0.9723] | 1.0180 [1.0018, 1.0344] | 0.9957 [0.9862, 1.0052] |
| formal-2 head÷anchor | 1.0012 [0.9897, 1.0129] | 0.9963 [0.9783, 1.0147] | 1.0049 [0.9871, 1.0231] | 0.9511 [0.9306, 0.9721] | 1.0103 [0.9924, 1.0285] | 0.9981 [0.9870, 1.0094] |
| 合并 head÷r3 | 0.4935 [0.4882, 0.4988] | 0.9422 [0.9311, 0.9535] | 0.9796 [0.9677, 0.9916] | 0.9893 [0.9725, 1.0064] | 0.4999 [0.4946, 0.5052] | 0.8089 [0.8024, 0.8154] |
| formal-1 head÷r3 | 0.4912 [0.4818, 0.5007] | 0.9375 [0.9211, 0.9541] | 0.9802 [0.9628, 0.9979] | 0.9869 [0.9636, 1.0109] | 0.4966 [0.4888, 0.5044] | 0.8058 [0.7960, 0.8157] |
| formal-2 head÷r3 | 0.4958 [0.4908, 0.5008] | 0.9470 [0.9315, 0.9628] | 0.9789 [0.9620, 0.9961] | 0.9916 [0.9666, 1.0173] | 0.5033 [0.4960, 0.5106] | 0.8120 [0.8032, 0.8209] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 1.0044 | 1.0125 | 是 |
| formal-1 | 1.0039 | 1.0150 | 是 |
| formal-2 | 1.0049 | 1.0171 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.801 / 0.811 通过 | 0.797 / 0.809 通过 |
| cista:mutate | 1.05 | 0.712 / 0.721 通过 | 0.708 / 0.717 通过 |
| cista:emit_after | 0.2 | 0.145 / 0.148 通过 | 0.146 / 0.148 通过 |
| flatbuffers:read_sum | 1.05 | 0.612 / 0.625 通过 | 0.614 / 0.625 通过 |
| flatbuffers:mutate | 1.05 | 0.607 / 0.621 通过 | 0.606 / 0.621 通过 |
| flatbuffers:emit_after | 0.2 | 0.136 / 0.138 通过 | 0.137 / 0.140 通过 |
| capnproto:read_sum | 1.05 | 0.210 / 0.214 通过 | 0.210 / 0.213 通过 |
| capnproto:mutate | 1.05 | 0.252 / 0.256 通过 | 0.249 / 0.252 通过 |
| capnproto:emit_after | 0.2 | 0.388 / 0.395 **不通过** | 0.386 / 0.393 **不通过** |
| cereal:read_sum | 1.05 | 0.865 / 0.879 通过 | 0.862 / 0.877 通过 |
| cereal:mutate | 1.05 | 1.030 / 1.046 通过 | 1.023 / 1.039 通过 |
| cereal:emit_after | 0.2 | 0.190 / 0.193 通过 | 0.191 / 0.194 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 10/12 | 10/12 |
| 通过数 r3（同批） | | 10/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（retire_modified_state, cleanup） | 无效（cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2262 [0.2238, 0.2287] | 0.5065 [0.4999, 0.5132] | 0.4036 [0.3967, 0.4107] | 0.4092 [0.4042, 0.4143] |
| formal-2 | 0.2265 [0.2240, 0.2290] | 0.5089 [0.5026, 0.5153] | 0.4046 [0.4001, 0.4092] | 0.4097 [0.4047, 0.4147] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 0.9568）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.8315 [0.8186, 0.8446] | 是 |
| B | 0.9223 [0.9133, 0.9313] | 是 |
| C | 0.9394 [0.9224, 0.9568] | 是 |
| D | 0.9824 [0.9618, 1.0034] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.8315 [0.8186, 0.8446] / 0.8315 [0.8196, 0.8435] | 0.9223 [0.9133, 0.9313] / 0.9223 [0.9135, 0.9311] | 0.9394 [0.9224, 0.9568] / 0.9394 [0.9242, 0.9549] | 0.9824 [0.9618, 1.0034] / 0.9824 [0.9604, 1.0049] | head A 1.014 B 1.012 C 0.989 D 1.012；anchor A 0.997 B 0.999 C 0.982 D 0.987 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.8315 [0.8083, 0.8552] | 0.9223 [0.8994, 0.9458] | 0.9394 [0.9129, 0.9667] | 0.9824 [0.9043, 1.0672] |
| 合并 | 0.8315 [0.8083, 0.8552] | 0.9223 [0.8994, 0.9458] | 0.9394 [0.9129, 0.9667] | 0.9824 [0.9043, 1.0672] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.255 / 0.258 (0.268) | 0.096 / 0.097 (0.101) | 0.332 / 0.337 (0.345) |
| B | 0.148 / 0.150 (0.154) | 0.077 / 0.078 (0.080) | 0.192 / 0.194 (0.200) |
| C | 0.105 / 0.107 (0.112) | 0.063 / 0.064 (0.067) | 0.240 / 0.244 (0.258) |
| D | 0.341 / 0.348 (0.377) | 0.521 / 0.533 (0.567) | 1.373 / 1.422 (1.448) |

## 4. 环境

- 开跑条件 formal-1：2026-10-26T20:01:01+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.74, quiet_s=241.0, screen={'WindowServer': 4.3, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-10-26T20:12:52+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=2.08, quiet_s=240.9, screen={'WindowServer': 4.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-10-26T20:24:15+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.82, quiet_s=240.9, screen={'WindowServer': 3.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-10-26T20:05:02+00:00 → 2026-10-26T20:12:52+00:00，load [1.7431640625, 1.88671875, 2.03369140625] → [2.3603515625, 2.263671875, 2.16162109375]，CPU 采样 {'samples': 231, 'other_busy_samples': 8, 'compile_samples': 0, 'other_busy_top': {'Claude Helper (Renderer)': 5, 'XprotectService': 1, 'PerfPowerServices': 1, 'mds_stores': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.9}
  - 计时期间屏幕采样：0/118 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.3, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/118 次采样，只记录；WindowServer ≥20% 0/118 次采样，有效
- formal-2：2026-10-26T20:16:53+00:00 → 2026-10-26T20:24:15+00:00，load [2.0751953125, 2.2197265625, 2.17333984375] → [2.78271484375, 2.58251953125, 2.3525390625]，CPU 采样 {'samples': 217, 'other_busy_samples': 7, 'compile_samples': 0, 'other_busy_top': {'Claude Helper (Renderer)': 4, 'Codex (Renderer)': 2, 'searchpartyd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 3.22}
  - 计时期间屏幕采样：0/111 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/111 次采样，只记录；WindowServer ≥20% 0/111 次采样，有效
- slg 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.6, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/15 次采样，只记录；WindowServer ≥20% 0/15 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.106，mutate 0.119，emit_after 0.121；mutate 按副本 1.0041 / 1.0313 / 0.9909
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.078，mutate 0.086，emit_after 0.103；mutate 按副本 1.0064 / 0.9967 / 1.0191
- host macOS-26.3-arm64-arm-64bit；冻结 2026-10-26T20:01:01+00:00
- 二进制 sha256：head `13793b33f19ed144…`；head_aa `13793b33f19ed144…`；anchor `0492f332857b93f0…`；r3 `6d4a33ba1f8547dc…`；cista `13bf74996c1723ce…`；flatbuffers `684f7be86bd7b628…`；capnproto `5a7625c96f0a3e7b…`；cereal `c897ba0d1a11a0e8…`
- SLG 树 bench/slg of f8eebf4 (v3 tree, git tree ffd88a16), used for every point（sha256 `2a010b04d53b4049727c4061b9ac750f26ff55bff2bb56da78a3f65c1c91081e`），slg_head `8ee1d0a98cd94164…`，slg_anch `5da23b484d7648bc…`
- 准备检查：gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0；gate_sml_ref exit 0；slg_verify_reuse_ref exit 0；slg_verify_default_ref exit 0
