# L2 Mac 门禁批次 2026-11-04-ckpt14-6ec08cc

- head `6ec08cc6ba8e837925f008947aaa49b16e72c420`（CKPT14 head = PR #191 6ec08cc6 (delta sync, class L, reconciled with next_cpp26 eabb583b = #207 #208 #210: root cache kept across move, 40-byte hot run at the start of XBufferCore; XBufferCore 208, XBuffer 224 bytes), build-r3d params）；anchor `eabb583b033d9dba9b30fbdb1ffc66b4bc8f953a`（next_cpp26 eabb583b (#210; current PR #191 base), build-r3d params）
- CKPT14: L2 gate of PR #191 (class L, delta sync) reconciled with the current next_cpp26 (6ec08cc6 vs eabb583b, merge result vs mainline as GATE_D4), merge tier of TEST_TIERS (core gate + G9R); anchor eabb583b; each point built with its own bench/slg; one deferred bisection point 7157488f (report only)
- **结论：fail**。正式部分 pass（合并 2 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `5b08b38eb1d4a7789608bffb7d6bf2f6b5c9ca6eeec55bb748e60a8e15623b22`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 1.0025 [0.9894, 1.0144] | 1.0010 [0.9890, 1.0118] | 1.0036 [0.9917, 1.0143] | 1.0144 | pass | 0.9975 / 0.9966 / 1.0057 | 有效 |
| formal-1（单批，只报） | 0.9993 [0.9787, 1.0178] | 0.9948 [0.9744, 1.0132] | 1.0044 [0.9868, 1.0203] | 1.0203 | pending | 1.0003 / 1.0018 / 1.0116 | **无效** |
| formal-2（单批，只报） | 1.0058 [0.9888, 1.0211] | 1.0073 [0.9942, 1.0189] | 1.0028 [0.9863, 1.0177] | 1.0211 | pending | 0.9946 / 0.9915 / 0.9998 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9435 / 1.0027 / 1.0760 | 0.9544 / 1.0028 / 1.0544 | 0.9432 / 0.9991 / 1.0668 |
| formal-1 | 0.9380 / 1.0025 / 1.0733 | 0.9486 / 0.9979 / 1.0631 | 0.9469 / 0.9987 / 1.0651 |
| formal-2 | 0.9378 / 1.0074 / 1.0784 | 0.9557 / 1.0069 / 1.0504 | 0.9413 / 0.9992 / 1.0677 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9928 [0.9740, 1.0119] | 1.0025 [0.9867, 1.0186] | 1.0010 [0.9865, 1.0157] | 1.0036 [0.9893, 1.0182] | 1.0131 [0.9952, 1.0313] | 1.0026 [0.9884, 1.0171] |
| formal-1 head÷anchor | 0.9851 [0.9493, 1.0222] | 0.9993 [0.9745, 1.0247] | 0.9948 [0.9702, 1.0201] | 1.0044 [0.9831, 1.0262] | 1.0149 [0.9881, 1.0425] | 0.9992 [0.9755, 1.0235] |
| formal-2 head÷anchor | 1.0005 [0.9887, 1.0126] | 1.0058 [0.9853, 1.0268] | 1.0073 [0.9916, 1.0232] | 1.0028 [0.9828, 1.0232] | 1.0112 [0.9866, 1.0365] | 1.0061 [0.9894, 1.0231] |
| 合并 head÷r3 | 0.9318 [0.9238, 0.9399] | 0.9551 [0.9429, 0.9675] | 0.9493 [0.9391, 0.9596] | 0.8419 [0.8299, 0.8540] | 0.9127 [0.9002, 0.9254] | 0.9392 [0.9296, 0.9488] |
| formal-1 head÷r3 | 0.9301 [0.9169, 0.9435] | 0.9511 [0.9348, 0.9677] | 0.9458 [0.9309, 0.9610] | 0.8348 [0.8211, 0.8488] | 0.9206 [0.9043, 0.9372] | 0.9370 [0.9240, 0.9502] |
| formal-2 head÷r3 | 0.9335 [0.9239, 0.9432] | 0.9591 [0.9403, 0.9782] | 0.9527 [0.9383, 0.9674] | 0.8489 [0.8290, 0.8694] | 0.9049 [0.8858, 0.9244] | 0.9413 [0.9269, 0.9560] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 1.0010 | 1.0109 | 是 |
| formal-1 | 0.9948 | 1.0117 | 是 |
| formal-2 | 1.0073 | 1.0180 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.768 / 0.780 通过 | 0.779 / 0.793 通过 |
| cista:mutate | 1.05 | 0.668 / 0.678 通过 | 0.678 / 0.687 通过 |
| cista:emit_after | 0.2 | 0.127 / 0.129 通过 | 0.131 / 0.133 通过 |
| flatbuffers:read_sum | 1.05 | 0.614 / 0.626 通过 | 0.622 / 0.634 通过 |
| flatbuffers:mutate | 1.05 | 0.604 / 0.617 通过 | 0.611 / 0.621 通过 |
| flatbuffers:emit_after | 0.2 | 0.120 / 0.122 通过 | 0.122 / 0.124 通过 |
| capnproto:read_sum | 1.05 | 0.201 / 0.204 通过 | 0.202 / 0.204 通过 |
| capnproto:mutate | 1.05 | 0.231 / 0.234 通过 | 0.232 / 0.235 通过 |
| capnproto:emit_after | 0.2 | 0.304 / 0.311 **不通过** | 0.302 / 0.308 **不通过** |
| cereal:read_sum | 1.05 | 0.829 / 0.844 通过 | 0.833 / 0.851 通过 |
| cereal:mutate | 1.05 | 0.981 / 0.995 通过 | 0.984 / 1.000 通过 |
| cereal:emit_after | 0.2 | 0.165 / 0.168 通过 | 0.170 / 0.172 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 有效 | 无效（cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2142 [0.2072, 0.2214] | 0.5245 [0.5058, 0.5439] | 0.3565 [0.3468, 0.3665] | 0.4115 [0.3974, 0.4261] |
| formal-2 | 0.2248 [0.2174, 0.2324] | 0.5411 [0.5224, 0.5605] | 0.3685 [0.3568, 0.3806] | 0.4321 [0.4176, 0.4470] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0464）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 1.0188 [0.9919, 1.0464] | **否** |
| B | 1.0038 [0.9858, 1.0222] | **否** |
| C | 0.9991 [0.9672, 1.0321] | **否** |
| D | 0.9897 [0.9680, 1.0120] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 1.0188 [0.9919, 1.0464] / 1.0188 [1.0093, 1.0283] | 1.0038 [0.9858, 1.0222] / 1.0038 [0.9961, 1.0116] | 0.9991 [0.9672, 1.0321] / 0.9991 [0.9840, 1.0144] | 0.9897 [0.9680, 1.0120] / 0.9897 [0.9690, 1.0109] | head A 0.964 B 0.979 C 0.956 D 1.029；anchor A 0.973 B 0.980 C 0.976 D 1.011 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 1.0188 [0.9955, 1.0426] | 1.0038 [0.9861, 1.0218] | 0.9991 [0.9471, 1.0540] | 0.9897 [0.9367, 1.0458] |
| 合并 | 1.0188 [0.9955, 1.0426] | 1.0038 [0.9861, 1.0218] | 0.9991 [0.9471, 1.0540] | 0.9897 [0.9367, 1.0458] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.236 / 0.242 (0.258) | 0.099 / 0.104 (0.110) | 0.389 / 0.399 (0.416) |
| B | 0.160 / 0.164 (0.164) | 0.077 / 0.079 (0.080) | 0.295 / 0.299 (0.299) |
| C | 0.118 / 0.123 (0.127) | 0.070 / 0.073 (0.076) | 0.356 / 0.368 (0.380) |
| D | 0.244 / 0.248 (0.276) | 0.352 / 0.358 (0.396) | 0.940 / 0.984 (0.948) |

## 4. 环境

- 开跑条件 formal-1：2026-11-04T02:28:39+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.31, quiet_s=241.1, screen={'WindowServer': 4.7, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-04T02:39:48+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.61, quiet_s=241.1, screen={'WindowServer': 5.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-04T02:51:42+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=2.02, quiet_s=241.1, screen={'WindowServer': 4.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-04T02:32:40+00:00 → 2026-11-04T02:39:47+00:00，load [1.3056640625, 1.7568359375, 2.0078125] → [2.0166015625, 1.99755859375, 2.01806640625]，CPU 采样 {'samples': 210, 'other_busy_samples': 16, 'compile_samples': 0, 'other_busy_top': {'Autoupdate': 7, 'ChatGPT': 3, 'spindump': 3, 'launchd': 2, 'modelcatalogd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.5}
  - 计时期间屏幕采样：0/107 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 6.0, 'UURemoteServer': 2.2, 'parsecd': 0.0}；远程桌面活动（≥5%）0/107 次采样，只记录；WindowServer ≥20% 0/107 次采样，有效
- formal-2：2026-11-04T02:43:49+00:00 → 2026-11-04T02:51:42+00:00，load [1.6103515625, 1.75439453125, 1.8984375] → [1.958984375, 1.9658203125, 1.91748046875]，CPU 采样 {'samples': 233, 'other_busy_samples': 6, 'compile_samples': 0, 'other_busy_top': {'Claude Helper (Renderer)': 4, 'Codex (Renderer)': 2}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.65}
  - 计时期间屏幕采样：0/119 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 6.4, 'UURemoteServer': 1.4, 'parsecd': 0.0}；远程桌面活动（≥5%）0/119 次采样，只记录；WindowServer ≥20% 0/119 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 7.7, 'UURemoteServer': 0.9, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.088，mutate 0.076，emit_after 0.111；mutate 按副本 1.0051 / 1.0100 / 0.9903
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.088，mutate 0.072，emit_after 0.095；mutate 按副本 0.9847 / 0.9810 / 1.0089
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-04T02:28:39+00:00
- 二进制 sha256：head `bc19859aed125e83…`；head_aa `bc19859aed125e83…`；anchor `b325298ef9e33efe…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree b68e63e9, anchor b68e63e9; bisection points their own)（sha256 `7e8b6e8cc7a6e58b45635c467db092e54a156eae841d29d4727cf2add62c2999`），slg_head `eab7101808a2a1cd…`，slg_anch `6bdfd811a671f335…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
