# L2 Mac 门禁批次 2026-11-04-rel330-059b6a5

- head `059b6a5ffdc8da9c25424a0c932f780e97a43d18`（REL330 head = 3.3.0 candidate C = next_cpp26 059b6a5f (#214 #219 #220 #224 #225 on top of #191/#213; frozen for the release PR), build-r3d params）；anchor `23c6e2065063bd656e9e04239e7918bf355a0588`（v3.2.1 23c6e206 (tag; library = the 3.2.0 checkpoint CKPT5, release baseline per RELEASING 7), build-r3d params）
- REL330: the 3.3.0 release checkpoint (RELEASING.md section 2, MERGE_RULES_V2 section 2; doubles as the stage review, 性能优化 15:22Z 11-04): candidate C vs v3.2.1 23c6e206; core gate + G9R + report-only 5f demo cases and 5a align64 pair; every point built with the head's bench/slg (23e86624); deferred bisection points #213 1c528d67, #191 fe8c0743, #219 e13245fa, #214 e7fd8cb0 (3 steps at most)
- **结论：pass**。正式部分 pass（合并 2 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `93e7cc5043d5842471d95f47381ffe9c277f073d469e6859fe00dcbf2c06408b`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 1.0010 [0.9915, 1.0095] | 0.9742 [0.9662, 0.9814] | 0.9771 [0.9655, 0.9876] | 1.0095 | pass | 0.9975 / 0.9967 / 0.9994 | 有效 |
| formal-1（单批，只报） | 0.9979 [0.9844, 1.0099] | 0.9722 [0.9618, 0.9814] | 0.9747 [0.9584, 0.9893] | 1.0099 | pass | 0.9984 / 1.0020 / 1.0079 | 有效 |
| formal-2（单批，只报） | 1.0042 [0.9905, 1.0166] | 0.9763 [0.9637, 0.9876] | 0.9795 [0.9624, 0.9949] | 1.0166 | pass | 0.9966 / 0.9914 / 0.9910 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9507 / 1.0005 / 1.0602 | 0.9291 / 0.9738 / 1.0286 | 0.9087 / 0.9751 / 1.0461 |
| formal-1 | 0.9498 / 0.9896 / 1.0628 | 0.9279 / 0.9736 / 1.0190 | 0.9066 / 0.9719 / 1.0405 |
| formal-2 | 0.9510 / 1.0069 / 1.0548 | 0.9325 / 0.9738 / 1.0323 | 0.9101 / 0.9768 / 1.0466 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0066 [1.0000, 1.0133] | 1.0010 [0.9896, 1.0126] | 0.9742 [0.9646, 0.9840] | 0.9771 [0.9631, 0.9913] | 0.9495 [0.9348, 0.9645] | 0.9861 [0.9774, 0.9949] |
| formal-1 head÷anchor | 1.0078 [0.9973, 1.0183] | 0.9979 [0.9816, 1.0144] | 0.9722 [0.9597, 0.9848] | 0.9747 [0.9550, 0.9948] | 0.9474 [0.9268, 0.9685] | 0.9836 [0.9715, 0.9959] |
| formal-2 head÷anchor | 1.0055 [0.9970, 1.0141] | 1.0042 [0.9876, 1.0212] | 0.9763 [0.9611, 0.9917] | 0.9795 [0.9589, 1.0006] | 0.9516 [0.9298, 0.9739] | 0.9886 [0.9757, 1.0017] |
| 合并 head÷r3 | 0.9340 [0.9272, 0.9409] | 0.9529 [0.9416, 0.9642] | 0.9502 [0.9408, 0.9598] | 0.8367 [0.8240, 0.8496] | 0.9060 [0.8922, 0.9200] | 0.9375 [0.9294, 0.9457] |
| formal-1 head÷r3 | 0.9371 [0.9268, 0.9475] | 0.9491 [0.9332, 0.9653] | 0.9430 [0.9318, 0.9542] | 0.8243 [0.8083, 0.8406] | 0.9101 [0.8889, 0.9319] | 0.9341 [0.9231, 0.9453] |
| formal-2 head÷r3 | 0.9310 [0.9216, 0.9404] | 0.9566 [0.9403, 0.9733] | 0.9576 [0.9423, 0.9731] | 0.8492 [0.8299, 0.8690] | 0.9019 [0.8836, 0.9206] | 0.9409 [0.9286, 0.9533] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9742 | 0.9808 | 是 |
| formal-1 | 0.9722 | 0.9807 | 是 |
| formal-2 | 0.9763 | 0.9867 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.766 / 0.785 通过 | 0.777 / 0.788 通过 |
| cista:mutate | 1.05 | 0.674 / 0.684 通过 | 0.679 / 0.688 通过 |
| cista:emit_after | 0.2 | 0.130 / 0.132 通过 | 0.132 / 0.134 通过 |
| flatbuffers:read_sum | 1.05 | 0.617 / 0.627 通过 | 0.622 / 0.633 通过 |
| flatbuffers:mutate | 1.05 | 0.605 / 0.614 通过 | 0.612 / 0.624 通过 |
| flatbuffers:emit_after | 0.2 | 0.122 / 0.124 通过 | 0.124 / 0.126 通过 |
| capnproto:read_sum | 1.05 | 0.204 / 0.207 通过 | 0.204 / 0.207 通过 |
| capnproto:mutate | 1.05 | 0.233 / 0.235 通过 | 0.233 / 0.236 通过 |
| capnproto:emit_after | 0.2 | 0.301 / 0.306 **不通过** | 0.309 / 0.314 **不通过** |
| cereal:read_sum | 1.05 | 0.836 / 0.849 通过 | 0.836 / 0.851 通过 |
| cereal:mutate | 1.05 | 0.983 / 0.995 通过 | 0.984 / 1.000 通过 |
| cereal:emit_after | 0.2 | 0.168 / 0.171 通过 | 0.171 / 0.174 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（retire_modified_state, cleanup） | 无效（retire_modified_state） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2379 [0.2321, 0.2439] | 0.5840 [0.5698, 0.5986] | 0.3921 [0.3839, 0.4004] | 0.4609 [0.4506, 0.4715] |
| formal-2 | 0.2408 [0.2364, 0.2453] | 0.5826 [0.5707, 0.5948] | 0.3953 [0.3874, 0.4035] | 0.4634 [0.4540, 0.4730] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 0.9842）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9585 [0.9408, 0.9765] | 是 |
| B | 0.9221 [0.9079, 0.9365] | 是 |
| C | 0.9591 [0.9347, 0.9842] | 是 |
| D | 1.0067 [0.9756, 1.0387] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9585 [0.9408, 0.9765] / 0.9585 [0.9460, 0.9711] | 0.9221 [0.9079, 0.9365] / 0.9221 [0.9089, 0.9355] | 0.9591 [0.9347, 0.9842] / 0.9591 [0.9372, 0.9816] | 1.0067 [0.9756, 1.0387] / 1.0067 [0.9798, 1.0342] | head A 0.970 B 0.973 C 0.990 D 1.007；anchor A 0.995 B 1.008 C 0.976 D 1.058 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9585 [0.9077, 1.0121] | 0.9221 [0.8837, 0.9622] | 0.9591 [0.9148, 1.0056] | 1.0067 [0.9443, 1.0732] |
| 合并 | 0.9585 [0.9077, 1.0121] | 0.9221 [0.8837, 0.9622] | 0.9591 [0.9148, 1.0056] | 1.0067 [0.9443, 1.0732] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.235 / 0.241 (0.256) | 0.097 / 0.099 (0.105) | 0.378 / 0.387 (0.401) |
| B | 0.161 / 0.165 (0.169) | 0.077 / 0.079 (0.081) | 0.293 / 0.296 (0.302) |
| C | 0.120 / 0.124 (0.129) | 0.071 / 0.073 (0.077) | 0.359 / 0.368 (0.385) |
| D | 0.253 / 0.256 (0.286) | 0.346 / 0.351 (0.385) | 0.941 / 0.976 (0.978) |

## 4. 环境

- 开跑条件 formal-1：2026-11-04T16:24:35+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=2.47, quiet_s=241.1, screen={'WindowServer': 5.0, 'UURemoteServer': 0.8, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-04T16:36:00+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=2.14, quiet_s=241.2, screen={'WindowServer': 8.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-04T16:47:53+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.51, quiet_s=241.1, screen={'WindowServer': 4.9, 'UURemoteServer': 0.6, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-04T16:28:36+00:00 → 2026-11-04T16:36:00+00:00，load [2.46533203125, 2.18359375, 2.31982421875] → [2.19091796875, 2.5517578125, 2.515625]，CPU 采样 {'samples': 218, 'other_busy_samples': 6, 'compile_samples': 0, 'other_busy_top': {'Claude Helper (Renderer)': 3, 'contactsd': 1, 'runningboardd': 1, 'PerfPowerServices': 1, 'launchd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 6.04}
  - 计时期间屏幕采样：0/112 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 7.1, 'UURemoteServer': 1.5, 'parsecd': 0.0}；远程桌面活动（≥5%）0/112 次采样，只记录；WindowServer ≥20% 0/112 次采样，有效
- formal-2：2026-11-04T16:40:01+00:00 → 2026-11-04T16:47:53+00:00，load [2.14404296875, 2.29833984375, 2.3974609375] → [2.7119140625, 2.3505859375, 2.3486328125]，CPU 采样 {'samples': 232, 'other_busy_samples': 6, 'compile_samples': 0, 'other_busy_top': {'Claude Helper (Renderer)': 3, 'Codex (Renderer)': 2, 'mds_stores': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.86}
  - 计时期间屏幕采样：0/118 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 8.2, 'UURemoteServer': 1.3, 'parsecd': 0.2}；远程桌面活动（≥5%）0/118 次采样，只记录；WindowServer ≥20% 0/118 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.8, 'UURemoteServer': 1.9, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.077，mutate 0.064，emit_after 0.114；mutate 按副本 1.0083 / 1.0007 / 0.9971
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.080，mutate 0.078，emit_after 0.115；mutate 按副本 0.9886 / 0.9823 / 1.0033
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-04T16:24:35+00:00
- 二进制 sha256：head `8cd9767c692ea0bb…`；head_aa `8cd9767c692ea0bb…`；anchor `6eece9b8d14ba644…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 the head's bench/slg for every point (git tree 23e86624; anchor and bisection points too)（sha256 `c60c7c18d80c929261e1eb847c940c8265ed98df984e3b8d9e97200a0764cf34`），slg_head `e0773338871b29f0…`，slg_anch `b15a09e2f7fda9e4…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
