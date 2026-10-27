# L2 Mac 门禁批次 2026-10-27-ckpt2-b50fc8a

- head `b50fc8abbbe05c2d152a8a473313975498f465b1`（CKPT2 head = next_cpp26 b50fc8a (#117 merged), build-r3d params）；anchor `f8eebf46565c746a69ed64dbe9e406546032f892`（f8eebf4 (INT1 merged, CKPT1 head), build-r3d params）
- second checkpoint (merge rules v2; 6 S class since 414dea4: #59 #91 #95 #104 #114 #117); anchor f8eebf4 (CKPT1_R2 with formal-3: pass); every first-parent merge in between, bisection points deferred; #116: sc_xoffset __text same; slg_bench __text same; -> not listed (inside the #117 range)
- **结论：fail**。正式部分 pass（合并 2 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `c1979649b396fe7398eb8b2a84f56473c84269ebc43bb45022e82347ad90cb84`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9927 [0.9853, 0.9993] | 0.9988 [0.9905, 1.0063] | 0.9974 [0.9861, 1.0075] | 1.0075 | pass | 1.0010 / 0.9955 / 0.9991 | 有效 |
| formal-1（单批，只报） | 0.9900 [0.9783, 1.0005] | 0.9914 [0.9800, 1.0015] | 0.9935 [0.9770, 1.0084] | 1.0084 | pass | 1.0046 / 1.0019 / 1.0021 | 有效 |
| formal-2（单批，只报） | 0.9954 [0.9861, 1.0038] | 1.0063 [0.9943, 1.0171] | 1.0013 [0.9856, 1.0154] | 1.0171 | pass | 0.9974 / 0.9890 / 0.9962 | **无效** |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9504 / 0.9936 / 1.0439 | 0.9478 / 1.0017 / 1.0473 | 0.9362 / 0.9969 / 1.0681 |
| formal-1 | 0.9466 / 0.9883 / 1.0457 | 0.9373 / 0.9930 / 1.0389 | 0.9297 / 0.9889 / 1.0687 |
| formal-2 | 0.9514 / 0.9948 / 1.0331 | 0.9564 / 1.0119 / 1.0546 | 0.9432 / 1.0008 / 1.0773 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0012 [0.9935, 1.0089] | 0.9927 [0.9838, 1.0017] | 0.9988 [0.9889, 1.0089] | 0.9974 [0.9839, 1.0111] | 0.9867 [0.9784, 0.9951] | 0.9942 [0.9879, 1.0005] |
| formal-1 head÷anchor | 1.0040 [0.9931, 1.0150] | 0.9900 [0.9759, 1.0044] | 0.9914 [0.9777, 1.0053] | 0.9935 [0.9736, 1.0139] | 0.9847 [0.9735, 0.9961] | 0.9909 [0.9813, 1.0006] |
| formal-2 head÷anchor | 0.9983 [0.9873, 1.0095] | 0.9954 [0.9842, 1.0068] | 1.0063 [0.9918, 1.0211] | 1.0013 [0.9823, 1.0207] | 0.9887 [0.9759, 1.0016] | 0.9975 [0.9892, 1.0059] |
| 合并 head÷r3 | 0.4947 [0.4916, 0.4978] | 0.9453 [0.9375, 0.9532] | 0.9848 [0.9750, 0.9947] | 0.9925 [0.9795, 1.0057] | 0.4977 [0.4938, 0.5015] | 0.8108 [0.8061, 0.8155] |
| formal-1 head÷r3 | 0.4963 [0.4916, 0.5011] | 0.9459 [0.9333, 0.9587] | 0.9764 [0.9627, 0.9903] | 0.9880 [0.9710, 1.0054] | 0.4988 [0.4934, 0.5042] | 0.8089 [0.8018, 0.8162] |
| formal-2 head÷r3 | 0.4931 [0.4890, 0.4971] | 0.9448 [0.9350, 0.9547] | 0.9933 [0.9794, 1.0075] | 0.9970 [0.9767, 1.0178] | 0.4965 [0.4909, 0.5022] | 0.8126 [0.8065, 0.8188] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9988 | 1.0057 | 是 |
| formal-1 | 0.9914 | 1.0007 | 是 |
| formal-2 | 1.0063 | 1.0163 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.809 / 0.821 通过 | 0.804 / 0.813 通过 |
| cista:mutate | 1.05 | 0.704 / 0.714 通过 | 0.713 / 0.723 通过 |
| cista:emit_after | 0.2 | 0.145 / 0.148 通过 | 0.145 / 0.147 通过 |
| flatbuffers:read_sum | 1.05 | 0.621 / 0.632 通过 | 0.636 / 0.645 通过 |
| flatbuffers:mutate | 1.05 | 0.616 / 0.627 通过 | 0.625 / 0.636 通过 |
| flatbuffers:emit_after | 0.2 | 0.136 / 0.139 通过 | 0.136 / 0.138 通过 |
| capnproto:read_sum | 1.05 | 0.209 / 0.212 通过 | 0.216 / 0.218 通过 |
| capnproto:mutate | 1.05 | 0.249 / 0.252 通过 | 0.253 / 0.255 通过 |
| capnproto:emit_after | 0.2 | 0.378 / 0.385 **不通过** | 0.385 / 0.391 **不通过** |
| cereal:read_sum | 1.05 | 0.869 / 0.881 通过 | 0.877 / 0.884 通过 |
| cereal:mutate | 1.05 | 1.019 / 1.034 通过 | 1.036 / 1.048 通过 |
| cereal:emit_after | 0.2 | 0.191 / 0.193 通过 | 0.189 / 0.192 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 10/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（cleanup） | 无效（cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2235 [0.2199, 0.2271] | 0.4982 [0.4906, 0.5059] | 0.3961 [0.3914, 0.4009] | 0.4047 [0.4011, 0.4082] |
| formal-2 | 0.2268 [0.2248, 0.2289] | 0.5069 [0.5019, 0.5120] | 0.4044 [0.3997, 0.4091] | 0.4097 [0.4055, 0.4139] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；2 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0216）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9905 [0.9802, 1.0009] | 是 |
| B | 0.9333 [0.9209, 0.9460] | 是 |
| C | 1.0001 [0.9791, 1.0216] | **否** |
| D | 1.0219 [1.0037, 1.0405] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9827 [0.9723, 0.9932] / 0.9827 [0.9717, 0.9938] | 0.9284 [0.9173, 0.9398] / 0.9284 [0.9181, 0.9389] | 1.0010 [0.9762, 1.0265] / 1.0010 [0.9756, 1.0272] | 1.0208 [0.9968, 1.0454] / 1.0208 [0.9953, 1.0470] | head A 0.998 B 1.020 C 1.011 D 0.986；anchor A 1.001 B 1.001 C 0.981 D 0.970 |
| slg-2 | 0.9983 [0.9805, 1.0165] / 0.9983 [0.9805, 1.0165] | 0.9383 [0.9108, 0.9666] / 0.9383 [0.9119, 0.9655] | 0.9992 [0.9728, 1.0263] / 0.9992 [0.9758, 1.0232] | 1.0231 [0.9885, 1.0588] / 1.0231 [0.9878, 1.0596] | head A 0.984 B 1.039 C 0.952 D 1.029；anchor A 0.999 B 0.995 C 1.013 D 0.996 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9827 [0.9579, 1.0082] | 0.9284 [0.9163, 0.9407] | 1.0010 [0.8929, 1.1223] | 1.0208 [0.9978, 1.0444] |
| slg-2 | 0.9983 [0.9528, 1.0460] | 0.9383 [0.9173, 0.9597] | 0.9992 [0.8945, 1.1161] | 1.0231 [0.9033, 1.1587] |
| 合并 | 0.9905 [0.9739, 1.0074] | 0.9333 [0.9238, 0.9430] | 1.0001 [0.9700, 1.0312] | 1.0219 [0.9973, 1.0472] |

进程数 head 12、anchor 12。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.254 / 0.257 (0.271) | 0.094 / 0.095 (0.100) | 0.332 / 0.336 (0.348) |
| B | 0.140 / 0.141 (0.144) | 0.072 / 0.073 (0.074) | 0.180 / 0.183 (0.185) |
| C | 0.107 / 0.110 (0.113) | 0.064 / 0.065 (0.067) | 0.245 / 0.248 (0.259) |
| D | 0.344 / 0.349 (0.388) | 0.530 / 0.538 (0.600) | 1.375 / 1.404 (1.501) |

## 4. 环境

- 开跑条件 formal-1：2026-10-27T00:23:13+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.71, quiet_s=241.0, screen={'WindowServer': 3.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-10-27T00:35:00+00:00 ok=True, quiet_s_required=180.0, waited_s=240.8, load1=1.81, quiet_s=240.8, screen={'WindowServer': 42.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=['WindowServer'], screen_gated=False
- 开跑条件 slg：2026-10-27T00:46:33+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.37, quiet_s=241.0, screen={'WindowServer': 3.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg-2：2026-10-27T00:51:31+00:00 ok=True, quiet_s_required=180.0, waited_s=240.8, load1=1.56, quiet_s=240.8, screen={'WindowServer': 41.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=['WindowServer'], screen_gated=False
- formal-1：2026-10-27T00:27:14+00:00 → 2026-10-27T00:35:00+00:00，load [1.712890625, 1.58056640625, 1.6337890625] → [2.47412109375, 2.03662109375, 1.82470703125]，CPU 采样 {'samples': 229, 'other_busy_samples': 21, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 16, 'XprotectService': 2, 'Claude Helper (Renderer)': 2, 'duetexpertd': 2, 'com.apple.AmbientDisplayAgent': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.48}
  - 计时期间屏幕采样：0/117 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 47.7, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/117 次采样，只记录；WindowServer ≥20% 14/117 次采样，有效
- formal-2：2026-10-27T00:39:01+00:00 → 2026-10-27T00:46:33+00:00，load [1.80712890625, 1.908203125, 1.81884765625] → [1.95751953125, 2.28076171875, 2.08837890625]，CPU 采样 {'samples': 222, 'other_busy_samples': 183, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 181, 'duetexpertd': 2, 'searchpartyd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 4.03}
  - 计时期间屏幕采样：0/114 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 42.4, 'UURemoteServer': 0.6, 'parsecd': 0.0}；远程桌面活动（≥5%）0/114 次采样，只记录；WindowServer ≥20% 92/114 次采样，有效
- slg 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 28.1, 'UURemoteServer': 0.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/15 次采样，只记录；WindowServer ≥20% 1/15 次采样，有效
- slg-2 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 41.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/15 次采样，只记录；WindowServer ≥20% 15/15 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.069，mutate 0.065，emit_after 0.094；mutate 按副本 1.0070 / 0.9956 / 1.0032
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.051，mutate 0.065，emit_after 0.087；mutate 按副本 0.9851 / 0.9892 / 0.9928
- host macOS-26.3-arm64-arm-64bit；冻结 2026-10-27T00:23:13+00:00
- 二进制 sha256：head `e4fdda91bd2eca69…`；head_aa `e4fdda91bd2eca69…`；anchor `13793b33f19ed144…`；r3 `6d4a33ba1f8547dc…`；cista `13bf74996c1723ce…`；flatbuffers `684f7be86bd7b628…`；capnproto `5a7625c96f0a3e7b…`；cereal `c897ba0d1a11a0e8…`
- SLG 树 bench/slg of b50fc8a (v3 tree, git tree ffd88a16, unchanged since f8eebf4), used for every point（sha256 `2a010b04d53b4049727c4061b9ac750f26ff55bff2bb56da78a3f65c1c91081e`），slg_head `7296dd67b5c24290…`，slg_anch `8ee1d0a98cd94164…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
