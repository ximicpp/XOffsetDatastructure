# L2 Mac 门禁批次 2026-11-04-ckpt19b-6578f40

- head `6578f4039c5fe746430c2f0c3889bca59cf77e25`（CKPT19b head = PR #222 6578f403 (payload coloring 60288a20 + test_owner_map fix 209fe35 + 6578f403: a colored VM region no longer doubles its 2^k alignment; three commits on next_cpp26 fe8c0743), build-r3d params）；anchor `fe8c07437224d9462f407cd115eae18a3972a829`（next_cpp26 fe8c0743 (#191 merged), build-r3d params）
- CKPT19b: L2 gate of PR #222 at 6578f403 (class L; library change since CKPT19 in xoffset_virtual_memory.hpp: region alignment k from the reservation alone, region_start masks to the granule) vs its base next_cpp26 fe8c0743; merge tier of TEST_TIERS (core gate + G9R); each point built with its own bench/slg (both 23e86624); one deferred bisection point 60288a20 (CKPT19 head, report only); report-only step 5x: HFT 64 books (book_c.cpp) and the SLG demo cases
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `5a9bcc906363c56b124a3be44406cdbfc577386e2e3bc1812e86d5c8721740a9`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 1.0027 [0.9973, 1.0075] | 0.9951 [0.9875, 1.0019] | 0.9970 [0.9882, 1.0048] | 1.0075 | pass | 1.0010 / 0.9995 / 0.9993 | 有效 |
| formal-1（单批，只报） | 0.9989 [0.9914, 1.0055] | 0.9899 [0.9814, 0.9976] | 0.9921 [0.9810, 1.0020] | 1.0055 | pass | 1.0040 / 1.0048 / 0.9990 | 有效 |
| formal-2（单批，只报） | 1.0065 [0.9986, 1.0136] | 1.0003 [0.9875, 1.0117] | 1.0018 [0.9881, 1.0141] | 1.0141 | pass | 0.9980 / 0.9942 / 0.9996 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9757 / 1.0030 / 1.0336 | 0.9593 / 0.9940 / 1.0227 | 0.9636 / 0.9976 / 1.0302 |
| formal-1 | 0.9691 / 1.0005 / 1.0340 | 0.9530 / 0.9925 / 1.0205 | 0.9628 / 0.9957 / 1.0291 |
| formal-2 | 0.9861 / 1.0042 / 1.0340 | 0.9646 / 0.9959 / 1.0273 | 0.9624 / 1.0027 / 1.0309 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0048 [0.9993, 1.0103] | 1.0027 [0.9962, 1.0092] | 0.9951 [0.9859, 1.0044] | 0.9970 [0.9865, 1.0076] | 0.9972 [0.9871, 1.0075] | 0.9999 [0.9929, 1.0070] |
| formal-1 head÷anchor | 1.0064 [0.9983, 1.0146] | 0.9989 [0.9898, 1.0080] | 0.9899 [0.9796, 1.0004] | 0.9921 [0.9787, 1.0057] | 0.9920 [0.9860, 0.9980] | 0.9957 [0.9879, 1.0036] |
| formal-2 head÷anchor | 1.0031 [0.9954, 1.0109] | 1.0065 [0.9969, 1.0162] | 1.0003 [0.9849, 1.0159] | 1.0018 [0.9853, 1.0186] | 1.0025 [0.9828, 1.0226] | 1.0041 [0.9923, 1.0161] |
| 合并 head÷r3 | 0.9337 [0.9290, 0.9384] | 0.9613 [0.9546, 0.9680] | 0.9393 [0.9303, 0.9483] | 0.8441 [0.8363, 0.8519] | 0.8978 [0.8895, 0.9063] | 0.9393 [0.9326, 0.9460] |
| formal-1 head÷r3 | 0.9348 [0.9277, 0.9420] | 0.9585 [0.9500, 0.9671] | 0.9320 [0.9224, 0.9416] | 0.8454 [0.8357, 0.8552] | 0.8943 [0.8896, 0.8990] | 0.9356 [0.9286, 0.9426] |
| formal-2 head÷r3 | 0.9325 [0.9262, 0.9389] | 0.9640 [0.9535, 0.9747] | 0.9466 [0.9315, 0.9619] | 0.8427 [0.8302, 0.8555] | 0.9014 [0.8851, 0.9180] | 0.9429 [0.9315, 0.9546] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9951 | 1.0014 | 是 |
| formal-1 | 0.9899 | 0.9970 | 是 |
| formal-2 | 1.0003 | 1.0108 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.799 / 0.809 通过 | 0.807 / 0.817 通过 |
| cista:mutate | 1.05 | 0.672 / 0.679 通过 | 0.678 / 0.688 通过 |
| cista:emit_after | 0.2 | 0.116 / 0.117 通过 | 0.117 / 0.119 通过 |
| flatbuffers:read_sum | 1.05 | 0.613 / 0.622 通过 | 0.630 / 0.638 通过 |
| flatbuffers:mutate | 1.05 | 0.608 / 0.619 通过 | 0.627 / 0.638 通过 |
| flatbuffers:emit_after | 0.2 | 0.112 / 0.112 通过 | 0.113 / 0.114 通过 |
| capnproto:read_sum | 1.05 | 0.193 / 0.194 通过 | 0.192 / 0.193 通过 |
| capnproto:mutate | 1.05 | 0.233 / 0.235 通过 | 0.232 / 0.236 通过 |
| capnproto:emit_after | 0.2 | 0.318 / 0.320 **不通过** | 0.320 / 0.324 **不通过** |
| cereal:read_sum | 1.05 | 0.847 / 0.856 通过 | 0.848 / 0.856 通过 |
| cereal:mutate | 1.05 | 1.002 / 1.013 通过 | 1.002 / 1.016 通过 |
| cereal:emit_after | 0.2 | 0.156 / 0.157 通过 | 0.157 / 0.159 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 10/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 有效 | 有效 |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.1780 [0.1773, 0.1788] | 0.4296 [0.4267, 0.4325] | 0.3253 [0.3238, 0.3267] | 0.3371 [0.3356, 0.3386] |
| formal-2 | 0.1789 [0.1761, 0.1818] | 0.4273 [0.4190, 0.4358] | 0.3252 [0.3201, 0.3304] | 0.3379 [0.3325, 0.3434] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0142）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9935 [0.9731, 1.0142] | 是 |
| B | 0.9920 [0.9763, 1.0080] | 是 |
| C | 0.9877 [0.9714, 1.0043] | 是 |
| D | 1.0008 [0.9823, 1.0196] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9935 [0.9731, 1.0142] / 0.9935 [0.9795, 1.0076] | 0.9920 [0.9763, 1.0080] / 0.9920 [0.9832, 1.0010] | 0.9877 [0.9714, 1.0043] / 0.9877 [0.9752, 1.0004] | 1.0008 [0.9823, 1.0196] / 1.0008 [0.9841, 1.0177] | head A 0.974 B 0.981 C 0.985 D 1.019；anchor A 0.997 B 0.980 C 0.975 D 1.003 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9935 [0.9331, 1.0577] | 0.9920 [0.9886, 0.9955] | 0.9877 [0.9568, 1.0197] | 1.0008 [0.9677, 1.0349] |
| 合并 | 0.9935 [0.9331, 1.0577] | 0.9920 [0.9886, 0.9955] | 0.9877 [0.9568, 1.0197] | 1.0008 [0.9677, 1.0349] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.224 / 0.229 (0.242) | 0.092 / 0.095 (0.100) | 0.364 / 0.371 (0.389) |
| B | 0.159 / 0.161 (0.164) | 0.076 / 0.077 (0.078) | 0.289 / 0.295 (0.299) |
| C | 0.113 / 0.115 (0.120) | 0.067 / 0.069 (0.072) | 0.340 / 0.346 (0.360) |
| D | 0.251 / 0.256 (0.280) | 0.354 / 0.361 (0.390) | 0.952 / 0.978 (0.982) |

## 4. 环境

- 开跑条件 formal-1：2026-11-04T20:30:33+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=0.94, quiet_s=241.1, screen={'WindowServer': 1.4, 'UURemoteServer': 1.5, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-04T20:41:37+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.04, quiet_s=241.2, screen={'WindowServer': 1.0, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-04T20:54:00+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.55, quiet_s=241.1, screen={'WindowServer': 4.3, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-04T20:34:34+00:00 → 2026-11-04T20:41:37+00:00，load [0.9375, 1.3994140625, 1.80029296875] → [1.67822265625, 1.671875, 1.78271484375]，CPU 采样 {'samples': 208, 'other_busy_samples': 3, 'compile_samples': 0, 'other_busy_top': {'modelcatalogd': 1, 'stickersd': 1, 'corespotlightd': 1, 'com.apple.geod': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.15}
  - 计时期间屏幕采样：0/106 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.0, 'UURemoteServer': 1.8, 'parsecd': 0.0}；远程桌面活动（≥5%）0/106 次采样，只记录；WindowServer ≥20% 0/106 次采样，有效
- formal-2：2026-11-04T20:45:38+00:00 → 2026-11-04T20:54:00+00:00，load [1.03955078125, 1.4072265625, 1.63671875] → [1.8818359375, 1.73046875, 1.68359375]，CPU 采样 {'samples': 247, 'other_busy_samples': 2, 'compile_samples': 0, 'other_busy_top': {'mds_stores': 1, 'UURemote': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.04}
  - 计时期间屏幕采样：0/126 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.0, 'UURemoteServer': 1.7, 'parsecd': 0.2}；远程桌面活动（≥5%）0/126 次采样，只记录；WindowServer ≥20% 0/126 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.3, 'UURemoteServer': 0.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.041，mutate 0.050，emit_after 0.053；mutate 按副本 1.0085 / 1.0001 / 1.0060
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.042，mutate 0.070，emit_after 0.073；mutate 按副本 0.9997 / 1.0001 / 0.9828
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-04T20:30:32+00:00
- 二进制 sha256：head `37cc5f068a1b4699…`；head_aa `37cc5f068a1b4699…`；anchor `242c1888c3c0301a…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 23e86624, anchor 23e86624; bisection points their own)（sha256 `c60c7c18d80c929261e1eb847c940c8265ed98df984e3b8d9e97200a0764cf34`），slg_head `5e6d1aeeb78ac31b…`，slg_anch `2f74a89b48f0d469…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
