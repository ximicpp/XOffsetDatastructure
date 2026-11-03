# L2 Mac 门禁批次 2026-11-03-ckpt10-3e02607

- head `3e026071a28e0032801666edc8ccdb5f82f19e39`（CKPT10 head = PR #191 3e026071 (delta sync, class L: xoffset_delta.hpp, XBuffer touch log, free-table generation, cached root follows move/swap; merged with next_cpp26 cc5b28ee), build-r3d params）；anchor `cc5b28eed7184138f7755da72f73a78e89848929`（next_cpp26 cc5b28ee (#202; PR #191 base, contains #193 #194), build-r3d params）
- CKPT10: L2 gate of PR #191 (class L, delta sync) at 3e026071, merge tier of TEST_TIERS (core gate + G9R); anchor cc5b28ee; each point built with its own bench/slg; one deferred bisection point (built only when a failed L2 bisects to it)
- **结论：fail**。正式部分 pass（合并 2 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `6a97aa472371fb8783fc606a78a35adbbbe92494353d48e81d04d78c789d9056`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9974 [0.9900, 1.0041] | 1.0003 [0.9929, 1.0069] | 1.0017 [0.9908, 1.0115] | 1.0115 | pass | 1.0027 / 1.0018 / 1.0054 | 有效 |
| formal-1（单批，只报） | 1.0044 [0.9944, 1.0132] | 1.0053 [0.9953, 1.0143] | 0.9929 [0.9793, 1.0052] | 1.0143 | pass | 0.9986 / 0.9945 / 1.0074 | 有效 |
| formal-2（单批，只报） | 0.9905 [0.9795, 1.0003] | 0.9953 [0.9844, 1.0051] | 1.0106 [0.9935, 1.0259] | 1.0259 | pending | 1.0067 / 1.0092 / 1.0035 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9524 / 0.9970 / 1.0403 | 0.9557 / 1.0017 / 1.0400 | 0.9420 / 0.9980 / 1.0663 |
| formal-1 | 0.9685 / 0.9998 / 1.0447 | 0.9677 / 1.0043 / 1.0475 | 0.9360 / 0.9896 / 1.0532 |
| formal-2 | 0.9470 / 0.9921 / 1.0304 | 0.9498 / 1.0001 / 1.0345 | 0.9532 / 1.0038 / 1.0793 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0050 [0.9977, 1.0124] | 0.9974 [0.9885, 1.0064] | 1.0003 [0.9914, 1.0092] | 1.0017 [0.9886, 1.0150] | 1.0009 [0.9889, 1.0129] | 0.9991 [0.9921, 1.0062] |
| formal-1 head÷anchor | 1.0045 [0.9939, 1.0153] | 1.0044 [0.9924, 1.0165] | 1.0053 [0.9932, 1.0176] | 0.9929 [0.9764, 1.0097] | 0.9982 [0.9793, 1.0174] | 1.0028 [0.9932, 1.0125] |
| formal-2 head÷anchor | 1.0055 [0.9950, 1.0161] | 0.9905 [0.9772, 1.0040] | 0.9953 [0.9821, 1.0087] | 1.0106 [0.9900, 1.0316] | 1.0036 [0.9882, 1.0191] | 0.9955 [0.9849, 1.0062] |
| 合并 head÷r3 | 0.9351 [0.9283, 0.9419] | 0.9446 [0.9348, 0.9546] | 0.9446 [0.9353, 0.9540] | 0.8286 [0.8194, 0.8378] | 0.9438 [0.9327, 0.9551] | 0.9367 [0.9292, 0.9442] |
| formal-1 head÷r3 | 0.9336 [0.9239, 0.9435] | 0.9461 [0.9319, 0.9605] | 0.9463 [0.9329, 0.9598] | 0.8270 [0.8138, 0.8405] | 0.9530 [0.9363, 0.9701] | 0.9388 [0.9275, 0.9502] |
| formal-2 head÷r3 | 0.9366 [0.9268, 0.9465] | 0.9431 [0.9289, 0.9576] | 0.9430 [0.9294, 0.9567] | 0.8301 [0.8171, 0.8434] | 0.9347 [0.9200, 0.9496] | 0.9346 [0.9243, 0.9450] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 1.0003 | 1.0064 | 是 |
| formal-1 | 1.0053 | 1.0135 | 是 |
| formal-2 | 0.9953 | 1.0043 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.777 / 0.787 通过 | 0.776 / 0.786 通过 |
| cista:mutate | 1.05 | 0.677 / 0.685 通过 | 0.675 / 0.683 通过 |
| cista:emit_after | 0.2 | 0.124 / 0.126 通过 | 0.127 / 0.129 通过 |
| flatbuffers:read_sum | 1.05 | 0.633 / 0.645 通过 | 0.607 / 0.618 通过 |
| flatbuffers:mutate | 1.05 | 0.610 / 0.621 通过 | 0.599 / 0.611 通过 |
| flatbuffers:emit_after | 0.2 | 0.118 / 0.119 通过 | 0.119 / 0.121 通过 |
| capnproto:read_sum | 1.05 | 0.211 / 0.214 通过 | 0.200 / 0.201 通过 |
| capnproto:mutate | 1.05 | 0.235 / 0.238 通过 | 0.231 / 0.234 通过 |
| capnproto:emit_after | 0.2 | 0.306 / 0.313 **不通过** | 0.297 / 0.302 **不通过** |
| cereal:read_sum | 1.05 | 0.848 / 0.862 通过 | 0.830 / 0.844 通过 |
| cereal:mutate | 1.05 | 0.991 / 1.006 通过 | 0.980 / 1.004 通过 |
| cereal:emit_after | 0.2 | 0.161 / 0.164 通过 | 0.164 / 0.168 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 10/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（cleanup） | 有效 |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2226 [0.2165, 0.2288] | 0.5419 [0.5263, 0.5579] | 0.3777 [0.3676, 0.3881] | 0.4277 [0.4157, 0.4402] |
| formal-2 | 0.2266 [0.2208, 0.2325] | 0.5503 [0.5357, 0.5654] | 0.3744 [0.3647, 0.3843] | 0.4335 [0.4200, 0.4473] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0484）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 1.0276 [1.0072, 1.0484] | **否** |
| B | 0.9990 [0.9900, 1.0081] | 是 |
| C | 1.0002 [0.9806, 1.0202] | **否** |
| D | 1.0236 [1.0044, 1.0432] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 1.0276 [1.0072, 1.0484] / 1.0276 [1.0105, 1.0450] | 0.9990 [0.9900, 1.0081] / 0.9990 [0.9929, 1.0052] | 1.0002 [0.9806, 1.0202] / 1.0002 [0.9868, 1.0139] | 1.0236 [1.0044, 1.0432] / 1.0236 [1.0051, 1.0425] | head A 0.990 B 0.987 C 0.972 D 1.007；anchor A 0.994 B 1.000 C 0.993 D 1.021 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 1.0276 [1.0116, 1.0438] | 0.9990 [0.9780, 1.0206] | 1.0002 [0.9786, 1.0223] | 1.0236 [1.0156, 1.0317] |
| 合并 | 1.0276 [1.0116, 1.0438] | 0.9990 [0.9780, 1.0206] | 1.0002 [0.9786, 1.0223] | 1.0236 [1.0156, 1.0317] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.243 / 0.246 (0.259) | 0.099 / 0.101 (0.106) | 0.391 / 0.398 (0.412) |
| B | 0.178 / 0.180 (0.182) | 0.079 / 0.080 (0.082) | 0.300 / 0.303 (0.306) |
| C | 0.122 / 0.125 (0.130) | 0.071 / 0.072 (0.076) | 0.347 / 0.355 (0.371) |
| D | 0.260 / 0.265 (0.293) | 0.362 / 0.368 (0.405) | 0.980 / 1.013 (1.009) |

## 4. 环境

- 开跑条件 formal-1：2026-11-03T15:26:28+00:00 ok=True, quiet_s_required=180.0, waited_s=275.9, load1=2.3, quiet_s=275.9, screen={'WindowServer': 41.0, 'UURemoteServer': 17.5, 'parsecd': 0.0}, remote_active=['UURemoteServer'], screen_over=['WindowServer'], screen_gated=False
- 开跑条件 formal-2：2026-11-03T15:39:06+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.43, quiet_s=241.0, screen={'WindowServer': 4.4, 'UURemoteServer': 0.1, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-03T15:50:02+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.66, quiet_s=241.1, screen={'WindowServer': 4.6, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-03T15:31:04+00:00 → 2026-11-03T15:39:06+00:00，load [2.30029296875, 3.45068359375, 3.03466796875] → [2.83544921875, 2.83349609375, 2.88525390625]，CPU 采样 {'samples': 237, 'other_busy_samples': 89, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 80, 'UURemoteServer': 9, 'Claude Helper (Renderer)': 6, 'mediaanalysisd': 2, 'duetexpertd': 2}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 3.68}
  - 计时期间屏幕采样：0/121 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 68.4, 'UURemoteServer': 43.5, 'parsecd': 1.5}；远程桌面活动（≥5%）110/121 次采样，只记录；WindowServer ≥20% 40/121 次采样，有效
- formal-2：2026-11-03T15:43:07+00:00 → 2026-11-03T15:50:02+00:00，load [1.4287109375, 1.99951171875, 2.48974609375] → [2.1123046875, 2.1806640625, 2.3876953125]，CPU 采样 {'samples': 204, 'other_busy_samples': 4, 'compile_samples': 0, 'other_busy_top': {'mediaanalysisd': 1, 'launchd': 1, 'deleted': 1, 'Electron': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.65}
  - 计时期间屏幕采样：0/104 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 6.1, 'UURemoteServer': 0.8, 'parsecd': 0.0}；远程桌面活动（≥5%）0/104 次采样，只记录；WindowServer ≥20% 0/104 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.6, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.070，mutate 0.059，emit_after 0.083；mutate 按副本 1.0030 / 0.9840 / 0.9966
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.073，mutate 0.070，emit_after 0.097；mutate 按副本 1.0062 / 1.0122 / 1.0093
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-03T15:26:28+00:00
- 二进制 sha256：head `a0760ce639e2b77e…`；head_aa `a0760ce639e2b77e…`；anchor `d8fd30e4cf7e03af…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 98bfe299, anchor 98bfe299; bisection points their own)（sha256 `ce789818e1542dfc55b9d943781e2609811d3adedbee8959b0f4aeb71af70b28`），slg_head `743b33409a9ecd34…`，slg_anch `8f5d78f0dcd4be27…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
