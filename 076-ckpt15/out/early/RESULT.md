# L2 Mac 门禁批次 2026-11-04-ckpt15-daf56e8

- head `daf56e89cd4c4ec8a8d838e52846864e24bf82bd`（CKPT15 head = PR #213 daf56e89 (keep a reloaded buffer's free table instead of recycling it, 4a54734d, merged with next_cpp26 49ce3c68), build-r3d params）；anchor `49ce3c68fa60ff9b531f1fa482fc4fb4b8e65479`（next_cpp26 49ce3c68 (#212; mainline merged into PR #213), build-r3d params）
- CKPT15: L2 gate of PR #213 (class L, keep a reloaded buffer's free table) as merged with the current next_cpp26 (daf56e89 vs 49ce3c68, merge result vs mainline as GATE_D4), merge tier of TEST_TIERS (core gate + G9R); anchor 49ce3c68; each point built with its own bench/slg; one deferred bisection point 4a54734d (report only)
- **结论：pending**。正式部分 pending（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `28671be94ba7285dd00b31a604c49eca0f1e4e3bd622491d5bc0c776acd75567`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 1.0030 [0.9941, 1.0110] | 1.0007 [0.9929, 1.0077] | 1.0002 [0.9868, 1.0122] | 1.0122 | pending | 1.0003 / 1.0011 / 1.0033 | 有效 |
| formal-1（单批，只报） | 0.9945 [0.9841, 1.0038] | 0.9936 [0.9833, 1.0028] | 0.9897 [0.9739, 1.0038] | 1.0038 | pass | 1.0072 / 1.0056 / 1.0136 | **无效** |
| formal-2（单批，只报） | 1.0116 [0.9972, 1.0245] | 1.0079 [0.9962, 1.0183] | 1.0108 [0.9890, 1.0305] | 1.0305 | fail | 0.9935 / 0.9966 / 0.9931 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

**两批不一致，未合并判定（待定，先诊断再加跑一批）：** formal-1 vs formal-2 emit_after: 0.9897 vs 1.0108

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9598 / 1.0037 / 1.0422 | 0.9545 / 1.0031 / 1.0430 | 0.9299 / 1.0047 / 1.0835 |
| formal-1 | 0.9615 / 0.9940 / 1.0283 | 0.9514 / 0.9965 / 1.0334 | 0.9236 / 0.9923 / 1.0525 |
| formal-2 | 0.9525 / 1.0087 / 1.0606 | 0.9589 / 1.0113 / 1.0555 | 0.9244 / 1.0233 / 1.0941 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9934 [0.9861, 1.0006] | 1.0030 [0.9923, 1.0138] | 1.0007 [0.9913, 1.0101] | 1.0002 [0.9841, 1.0165] | 1.0095 [0.9979, 1.0213] | 1.0026 [0.9940, 1.0113] |
| formal-1 head÷anchor | 0.9922 [0.9823, 1.0021] | 0.9945 [0.9819, 1.0073] | 0.9936 [0.9812, 1.0062] | 0.9897 [0.9707, 1.0090] | 1.0108 [0.9979, 1.0239] | 0.9962 [0.9855, 1.0070] |
| formal-2 head÷anchor | 0.9946 [0.9836, 1.0056] | 1.0116 [0.9942, 1.0293] | 1.0079 [0.9938, 1.0221] | 1.0108 [0.9845, 1.0378] | 1.0082 [0.9884, 1.0285] | 1.0091 [0.9956, 1.0229] |
| 合并 head÷r3 | 0.9297 [0.9232, 0.9362] | 0.9550 [0.9443, 0.9658] | 0.9433 [0.9336, 0.9531] | 0.8419 [0.8294, 0.8546] | 0.9182 [0.9060, 0.9305] | 0.9383 [0.9301, 0.9465] |
| formal-1 head÷r3 | 0.9326 [0.9244, 0.9408] | 0.9674 [0.9538, 0.9813] | 0.9447 [0.9322, 0.9573] | 0.8275 [0.8142, 0.8411] | 0.8975 [0.8865, 0.9086] | 0.9402 [0.9295, 0.9511] |
| formal-2 head÷r3 | 0.9268 [0.9164, 0.9372] | 0.9428 [0.9271, 0.9587] | 0.9420 [0.9266, 0.9576] | 0.8566 [0.8360, 0.8777] | 0.9394 [0.9198, 0.9595] | 0.9363 [0.9235, 0.9492] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 1.0007 | 1.0071 | 是 |
| formal-1 | 0.9936 | 1.0020 | 是 |
| formal-2 | 1.0079 | 1.0174 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.816 / 0.828 通过 | 0.795 / 0.808 通过 |
| cista:mutate | 1.05 | 0.675 / 0.683 通过 | 0.681 / 0.691 通过 |
| cista:emit_after | 0.2 | 0.129 / 0.131 通过 | 0.129 / 0.131 通过 |
| flatbuffers:read_sum | 1.05 | 0.617 / 0.627 通过 | 0.645 / 0.659 通过 |
| flatbuffers:mutate | 1.05 | 0.611 / 0.623 通过 | 0.602 / 0.615 通过 |
| flatbuffers:emit_after | 0.2 | 0.122 / 0.124 通过 | 0.121 / 0.124 通过 |
| capnproto:read_sum | 1.05 | 0.195 / 0.197 通过 | 0.220 / 0.224 通过 |
| capnproto:mutate | 1.05 | 0.232 / 0.235 通过 | 0.234 / 0.237 通过 |
| capnproto:emit_after | 0.2 | 0.299 / 0.303 **不通过** | 0.326 / 0.333 **不通过** |
| cereal:read_sum | 1.05 | 0.845 / 0.857 通过 | 0.862 / 0.881 通过 |
| cereal:mutate | 1.05 | 0.989 / 1.002 通过 | 0.976 / 1.001 通过 |
| cereal:emit_after | 0.2 | 0.169 / 0.172 通过 | 0.166 / 0.171 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 有效 | 有效 |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2251 [0.2219, 0.2284] | 0.5450 [0.5357, 0.5545] | 0.3693 [0.3639, 0.3748] | 0.4327 [0.4263, 0.4392] |
| formal-2 | 0.2185 [0.2141, 0.2231] | 0.5297 [0.5178, 0.5420] | 0.3686 [0.3618, 0.3755] | 0.4162 [0.4072, 0.4254] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；2 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0120）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9938 [0.9828, 1.0050] | 是 |
| B | 1.0020 [0.9921, 1.0120] | 是 |
| C | 0.9941 [0.9794, 1.0091] | 是 |
| D | 0.9861 [0.9756, 0.9967] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9998 [0.9816, 1.0184] / 0.9998 [0.9856, 1.0142] | 1.0043 [0.9857, 1.0233] / 1.0043 [0.9918, 1.0170] | 0.9944 [0.9710, 1.0183] / 0.9944 [0.9722, 1.0170] | 0.9858 [0.9664, 1.0057] / 0.9858 [0.9685, 1.0034] | head A 0.965 B 0.979 C 0.959 D 0.970；anchor A 0.994 B 0.986 C 0.988 D 0.981 |
| slg-2 | 0.9879 [0.9716, 1.0044] / 0.9879 [0.9739, 1.0021] | 0.9996 [0.9863, 1.0132] / 0.9996 [0.9904, 1.0090] | 0.9939 [0.9705, 1.0178] / 0.9939 [0.9758, 1.0123] | 0.9863 [0.9709, 1.0020] / 0.9863 [0.9721, 1.0008] | head A 0.971 B 0.985 C 0.962 D 0.981；anchor A 0.984 B 0.993 C 0.973 D 0.986 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9998 [0.9744, 1.0260] | 1.0043 [0.9599, 1.0508] | 0.9944 [0.9779, 1.0111] | 0.9858 [0.9491, 1.0240] |
| slg-2 | 0.9879 [0.9589, 1.0177] | 0.9996 [0.9771, 1.0227] | 0.9939 [0.9592, 1.0298] | 0.9863 [0.9524, 1.0215] |
| 合并 | 0.9938 [0.9813, 1.0066] | 1.0020 [0.9915, 1.0126] | 0.9941 [0.9866, 1.0017] | 0.9861 [0.9763, 0.9959] |

进程数 head 12、anchor 12。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.234 / 0.236 (0.254) | 0.097 / 0.099 (0.109) | 0.379 / 0.383 (0.408) |
| B | 0.161 / 0.162 (0.164) | 0.077 / 0.077 (0.080) | 0.290 / 0.293 (0.298) |
| C | 0.125 / 0.127 (0.135) | 0.074 / 0.075 (0.080) | 0.372 / 0.378 (0.403) |
| D | 0.244 / 0.246 (0.279) | 0.354 / 0.356 (0.401) | 0.951 / 0.971 (1.025) |

## 4. 环境

- 开跑条件 formal-1：2026-11-04T05:14:16+00:00 ok=True, quiet_s_required=180.0, waited_s=241.3, load1=1.43, quiet_s=241.2, screen={'WindowServer': 1.3, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-04T05:25:19+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.6, quiet_s=241.1, screen={'WindowServer': 42.6, 'UURemoteServer': 16.9, 'parsecd': 0.0}, remote_active=['UURemoteServer'], screen_over=['WindowServer'], screen_gated=False
- 开跑条件 slg：2026-11-04T05:37:23+00:00 ok=True, quiet_s_required=180.0, waited_s=240.9, load1=2.24, quiet_s=240.9, screen={'WindowServer': 42.7, 'UURemoteServer': 15.5, 'parsecd': 0.0}, remote_active=['UURemoteServer'], screen_over=['WindowServer'], screen_gated=False
- 开跑条件 slg-2：2026-11-04T05:42:11+00:00 ok=True, quiet_s_required=180.0, waited_s=255.8, load1=2.86, quiet_s=255.8, screen={'WindowServer': 42.7, 'UURemoteServer': 18.2, 'parsecd': 0.0}, remote_active=['UURemoteServer'], screen_over=['WindowServer'], screen_gated=False
- formal-1：2026-11-04T05:18:17+00:00 → 2026-11-04T05:25:19+00:00，load [1.43359375, 1.55859375, 1.58935546875] → [1.748046875, 1.7021484375, 1.634765625]，CPU 采样 {'samples': 207, 'other_busy_samples': 1, 'compile_samples': 0, 'other_busy_top': {'Codex (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.1}
  - 计时期间屏幕采样：0/106 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.8, 'UURemoteServer': 1.8, 'parsecd': 0.0}；远程桌面活动（≥5%）0/106 次采样，只记录；WindowServer ≥20% 0/106 次采样，有效
- formal-2：2026-11-04T05:29:20+00:00 → 2026-11-04T05:37:23+00:00，load [1.595703125, 1.6162109375, 1.60986328125] → [2.63623046875, 2.755859375, 2.27001953125]，CPU 采样 {'samples': 238, 'other_busy_samples': 238, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 237, 'Claude Helper (Renderer)': 10, 'opendirectoryd': 1, 'Codex (Renderer)': 1, 'wechatwebdevtools Helper (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 4.58}
  - 计时期间屏幕采样：0/121 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 47.5, 'UURemoteServer': 19.5, 'parsecd': 0.0}；远程桌面活动（≥5%）121/121 次采样，只记录；WindowServer ≥20% 121/121 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 45.7, 'UURemoteServer': 18.9, 'parsecd': 0.0}；远程桌面活动（≥5%）13/13 次采样，只记录；WindowServer ≥20% 13/13 次采样，有效
- slg-2 计时期间屏幕采样：0/12 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 46.0, 'UURemoteServer': 18.2, 'parsecd': 0.0}；远程桌面活动（≥5%）12/12 次采样，只记录；WindowServer ≥20% 12/12 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.068，mutate 0.059，emit_after 0.098；mutate 按副本 1.0126 / 1.0103 / 0.9941
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.089，mutate 0.074，emit_after 0.108；mutate 按副本 0.9978 / 1.0055 / 0.9865
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-04T05:14:15+00:00
- 二进制 sha256：head `b749a0f4d98e9678…`；head_aa `b749a0f4d98e9678…`；anchor `b325298ef9e33efe…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 55ba4e84, anchor 55ba4e84; bisection points their own)（sha256 `efa09929732a456b5b8a9169ee03fbb3574f975bc54a60a8e4db024bf9c544e3`），slg_head `6d66e7e3dafdb293…`，slg_anch `4d8a2b8c6d246dc6…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
