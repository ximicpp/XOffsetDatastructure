# L2 Mac 门禁批次 2026-10-27-ckpt2b-3549272

- head `35492720b5981d9672c8b8f00037dcb687aaf9fd`（CKPT2b head = next_cpp26 3549272 (#119 merged), build-r3d params）；anchor `f8eebf46565c746a69ed64dbe9e406546032f892`（f8eebf4 (INT1 merged, CKPT1 head), build-r3d params）
- CKPT2b: second checkpoint again at the current head (CKPT2 b50fc8a fail by width, C 1.0001, not rerun); since f8eebf4: S #91 #95 #104 #114 #117, L #113 #93 #118, 0 #116 #119 and the tools/docs merges; anchor f8eebf4 (CKPT1_R2 with formal-3: pass; fixed by 性能优化 01:42Z); bisection points deferred
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `31151d2c378d5ccd7214ebb05399d995aca900e552ae36a58c0b7e6accf228ee`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9985 [0.9875, 1.0083] | 0.9955 [0.9858, 1.0042] | 0.9939 [0.9795, 1.0068] | 1.0083 | pass | 1.0006 / 1.0022 / 1.0002 | 有效 |
| formal-1（单批，只报） | 0.9992 [0.9829, 1.0139] | 0.9995 [0.9862, 1.0114] | 1.0007 [0.9791, 1.0202] | 1.0202 | pending | 1.0023 / 1.0030 / 0.9937 | 有效 |
| formal-2（单批，只报） | 0.9977 [0.9825, 1.0114] | 0.9915 [0.9769, 1.0046] | 0.9871 [0.9676, 1.0047] | 1.0114 | pass | 0.9989 / 1.0015 / 1.0067 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9335 / 0.9975 / 1.0616 | 0.9423 / 0.9954 / 1.0581 | 0.9078 / 1.0012 / 1.0782 |
| formal-1 | 0.9357 / 0.9972 / 1.0692 | 0.9502 / 0.9959 / 1.0576 | 0.9312 / 1.0101 / 1.0840 |
| formal-2 | 0.9260 / 0.9988 / 1.0574 | 0.9245 / 0.9928 / 1.0583 | 0.9016 / 0.9954 / 1.0728 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.8570 [0.8469, 0.8673] | 0.9985 [0.9853, 1.0119] | 0.9955 [0.9838, 1.0074] | 0.9939 [0.9766, 1.0115] | 0.9027 [0.8889, 0.9168] | 0.9747 [0.9659, 0.9835] |
| formal-1 head÷anchor | 0.8610 [0.8490, 0.8732] | 0.9992 [0.9795, 1.0193] | 0.9995 [0.9835, 1.0158] | 1.0007 [0.9747, 1.0274] | 0.9005 [0.8807, 0.9208] | 0.9764 [0.9628, 0.9903] |
| formal-2 head÷anchor | 0.8530 [0.8364, 0.8700] | 0.9977 [0.9793, 1.0165] | 0.9915 [0.9739, 1.0095] | 0.9871 [0.9635, 1.0113] | 0.9049 [0.8849, 0.9254] | 0.9730 [0.9614, 0.9847] |
| 合并 head÷r3 | 0.4245 [0.4201, 0.4290] | 0.9491 [0.9376, 0.9608] | 0.9727 [0.9568, 0.9889] | 0.9922 [0.9746, 1.0102] | 0.4608 [0.4544, 0.4674] | 0.7935 [0.7863, 0.8007] |
| formal-1 head÷r3 | 0.4259 [0.4204, 0.4316] | 0.9495 [0.9324, 0.9670] | 0.9769 [0.9595, 0.9945] | 1.0051 [0.9786, 1.0323] | 0.4616 [0.4525, 0.4708] | 0.7953 [0.7860, 0.8047] |
| formal-2 head÷r3 | 0.4231 [0.4160, 0.4304] | 0.9487 [0.9326, 0.9652] | 0.9685 [0.9413, 0.9966] | 0.9795 [0.9560, 1.0036] | 0.4601 [0.4506, 0.4699] | 0.7917 [0.7804, 0.8032] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9955 | 1.0035 | 是 |
| formal-1 | 0.9995 | 1.0104 | 是 |
| formal-2 | 0.9915 | 1.0035 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.804 / 0.818 通过 | 0.807 / 0.820 通过 |
| cista:mutate | 1.05 | 0.706 / 0.715 通过 | 0.704 / 0.714 通过 |
| cista:emit_after | 0.2 | 0.150 / 0.153 通过 | 0.146 / 0.149 通过 |
| flatbuffers:read_sum | 1.05 | 0.617 / 0.629 通过 | 0.615 / 0.626 通过 |
| flatbuffers:mutate | 1.05 | 0.612 / 0.625 通过 | 0.606 / 0.618 通过 |
| flatbuffers:emit_after | 0.2 | 0.141 / 0.144 通过 | 0.138 / 0.140 通过 |
| capnproto:read_sum | 1.05 | 0.212 / 0.215 通过 | 0.211 / 0.214 通过 |
| capnproto:mutate | 1.05 | 0.248 / 0.251 通过 | 0.247 / 0.250 通过 |
| capnproto:emit_after | 0.2 | 0.380 / 0.389 **不通过** | 0.381 / 0.388 **不通过** |
| cereal:read_sum | 1.05 | 0.865 / 0.880 通过 | 0.874 / 0.891 通过 |
| cereal:mutate | 1.05 | 1.017 / 1.035 通过 | 1.018 / 1.037 通过 |
| cereal:emit_after | 0.2 | 0.195 / 0.198 通过 | 0.191 / 0.194 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 10/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（cleanup） | 无效（cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2113 [0.2061, 0.2165] | 0.4713 [0.4600, 0.4829] | 0.3557 [0.3474, 0.3641] | 0.3822 [0.3725, 0.3922] |
| formal-2 | 0.2095 [0.2055, 0.2136] | 0.4663 [0.4571, 0.4756] | 0.3654 [0.3588, 0.3722] | 0.3788 [0.3714, 0.3865] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0143）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9468 [0.9265, 0.9674] | 是 |
| B | 0.9113 [0.9052, 0.9175] | 是 |
| C | 0.9983 [0.9826, 1.0143] | 是 |
| D | 1.0142 [0.9945, 1.0342] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9468 [0.9265, 0.9674] / 0.9468 [0.9261, 0.9678] | 0.9113 [0.9052, 0.9175] / 0.9113 [0.9052, 0.9175] | 0.9983 [0.9826, 1.0143] / 0.9983 [0.9819, 1.0150] | 1.0142 [0.9945, 1.0342] / 1.0142 [0.9943, 1.0344] | head A 1.001 B 0.997 C 1.016 D 1.029；anchor A 1.005 B 1.005 C 1.007 D 1.014 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9468 [0.9137, 0.9810] | 0.9113 [0.8977, 0.9251] | 0.9983 [0.9825, 1.0143] | 1.0142 [0.9405, 1.0936] |
| 合并 | 0.9468 [0.9137, 0.9810] | 0.9113 [0.8977, 0.9251] | 0.9983 [0.9825, 1.0143] | 1.0142 [0.9405, 1.0936] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.242 / 0.248 (0.258) | 0.089 / 0.091 (0.094) | 0.315 / 0.325 (0.329) |
| B | 0.136 / 0.137 (0.143) | 0.070 / 0.071 (0.074) | 0.176 / 0.177 (0.184) |
| C | 0.106 / 0.107 (0.111) | 0.063 / 0.064 (0.066) | 0.238 / 0.242 (0.248) |
| D | 0.340 / 0.348 (0.371) | 0.520 / 0.531 (0.573) | 1.361 / 1.391 (1.420) |

## 4. 环境

- 开跑条件 formal-1：2026-10-27T02:00:11+00:00 ok=True, quiet_s_required=180.0, waited_s=240.9, load1=1.32, quiet_s=240.9, screen={'WindowServer': 4.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-10-27T02:11:58+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.85, quiet_s=240.9, screen={'WindowServer': 3.8, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-10-27T02:23:10+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.29, quiet_s=240.9, screen={'WindowServer': 3.6, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-10-27T02:04:12+00:00 → 2026-10-27T02:11:58+00:00，load [1.3203125, 1.640625, 1.73193359375] → [2.03662109375, 1.9755859375, 1.85009765625]，CPU 采样 {'samples': 229, 'other_busy_samples': 3, 'compile_samples': 0, 'other_busy_top': {'XprotectService': 2, 'Claude Helper (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.48}
  - 计时期间屏幕采样：0/117 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/117 次采样，只记录；WindowServer ≥20% 0/117 次采样，有效
- formal-2：2026-10-27T02:15:59+00:00 → 2026-10-27T02:23:10+00:00，load [1.85498046875, 1.8115234375, 1.7978515625] → [2.15478515625, 1.93115234375, 1.84326171875]，CPU 采样 {'samples': 212, 'other_busy_samples': 3, 'compile_samples': 0, 'other_busy_top': {'Claude Helper (Renderer)': 2, 'searchpartyd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.26}
  - 计时期间屏幕采样：0/108 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.1, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/108 次采样，只记录；WindowServer ≥20% 0/108 次采样，有效
- slg 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.6, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/15 次采样，只记录；WindowServer ≥20% 0/15 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.087，mutate 0.085，emit_after 0.113；mutate 按副本 0.9918 / 1.0166 / 1.0006
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.092，mutate 0.078，emit_after 0.109；mutate 按副本 1.0127 / 0.9898 / 1.0021
- host macOS-26.3-arm64-arm-64bit；冻结 2026-10-27T02:00:11+00:00
- 二进制 sha256：head `3340584675ac4ab9…`；head_aa `3340584675ac4ab9…`；anchor `13793b33f19ed144…`；r3 `6d4a33ba1f8547dc…`；cista `13bf74996c1723ce…`；flatbuffers `684f7be86bd7b628…`；capnproto `5a7625c96f0a3e7b…`；cereal `c897ba0d1a11a0e8…`
- SLG 树 bench/slg of 3549272 (v3 tree, git tree ffd88a16, unchanged since f8eebf4), used for every point（sha256 `2a010b04d53b4049727c4061b9ac750f26ff55bff2bb56da78a3f65c1c91081e`），slg_head `cd5eac056a30d4f0…`，slg_anch `8ee1d0a98cd94164…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
