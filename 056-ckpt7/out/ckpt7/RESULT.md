# L2 Mac 门禁批次 2026-11-03-ckpt7-c4925fa

- head `c4925fa7e7eb7406136685a2943af1ce1e9de96c`（CKPT7 head = class 1 integration c4925fa7 (claude/class1-integration-20261102, class L: member swap, reload zero watermark, D6 adaptive step, inline free bins, cached free table, prefetch_for_update, bench A adapter), build-r3d params）；anchor `c9fbaea2e980f6f708df615181ced209d653dc0f`（next_cpp26 c9fbaea2 (#190; library headers byte-identical to v3.2.1 / 3.2.0), build-r3d params）
- CKPT7: L2 gate of the class 1 migration integration (class L, before merge); anchor c9fbaea2 (library = v3.2.1); each point built with its own bench/slg; bisection points prebuilt; 5f demo case set report only
- **结论：fail**。正式部分 fail（合并 3 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `1a5a0eeb6396f0db55cbe2f2ea800ed2f661740ca6dfe10e36d21329e958271a`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 3 批（判定）** | 0.9993 [0.9938, 1.0043] | 0.9956 [0.9896, 1.0009] | 1.0122 [1.0028, 1.0206] | 1.0206 | fail | 0.9989 / 0.9993 / 0.9950 | 有效 |
| formal-1（单批，只报） | 0.9965 [0.9868, 1.0052] | 0.9939 [0.9837, 1.0030] | 1.0166 [0.9988, 1.0326] | 1.0326 | fail | 0.9983 / 0.9955 / 0.9971 | 有效 |
| formal-2（单批，只报） | 1.0069 [0.9971, 1.0157] | 1.0007 [0.9902, 1.0101] | 1.0167 [1.0014, 1.0303] | 1.0303 | fail | 0.9972 / 1.0005 / 0.9925 | 有效 |
| formal-3（单批，只报） | 0.9946 [0.9852, 1.0030] | 0.9921 [0.9813, 1.0017] | 1.0035 [0.9871, 1.0182] | 1.0182 | pass | 1.0011 / 1.0019 / 0.9954 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9694 / 0.9995 / 1.0340 | 0.9539 / 0.9967 / 1.0437 | 0.9558 / 1.0126 / 1.0751 |
| formal-1 | 0.9689 / 0.9942 / 1.0304 | 0.9482 / 0.9937 / 1.0404 | 0.9566 / 1.0108 / 1.0787 |
| formal-2 | 0.9712 / 1.0081 / 1.0449 | 0.9583 / 1.0047 / 1.0494 | 0.9560 / 1.0231 / 1.0701 |
| formal-3 | 0.9620 / 0.9944 / 1.0244 | 0.9539 / 0.9915 / 1.0444 | 0.9481 / 0.9998 / 1.0768 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0015 [0.9966, 1.0064] | 0.9993 [0.9927, 1.0060] | 0.9956 [0.9884, 1.0028] | 1.0122 [1.0010, 1.0236] | 0.9813 [0.9750, 0.9877] | 0.9967 [0.9911, 1.0023] |
| formal-1 head÷anchor | 1.0034 [0.9944, 1.0125] | 0.9965 [0.9848, 1.0084] | 0.9939 [0.9815, 1.0064] | 1.0166 [0.9951, 1.0385] | 0.9875 [0.9769, 0.9982] | 0.9961 [0.9866, 1.0058] |
| formal-2 head÷anchor | 0.9998 [0.9912, 1.0086] | 1.0069 [0.9951, 1.0189] | 1.0007 [0.9881, 1.0136] | 1.0167 [0.9983, 1.0354] | 0.9808 [0.9699, 0.9919] | 1.0018 [0.9915, 1.0122] |
| formal-3 head÷anchor | 1.0012 [0.9931, 1.0095] | 0.9946 [0.9833, 1.0061] | 0.9921 [0.9790, 1.0053] | 1.0035 [0.9838, 1.0237] | 0.9757 [0.9639, 0.9876] | 0.9922 [0.9826, 1.0020] |
| 合并 head÷r3 | 0.9268 [0.9224, 0.9311] | 0.9670 [0.9604, 0.9737] | 0.9707 [0.9634, 0.9781] | 0.8558 [0.8458, 0.8659] | 0.9144 [0.9094, 0.9195] | 0.9503 [0.9448, 0.9559] |
| formal-1 head÷r3 | 0.9288 [0.9211, 0.9365] | 0.9657 [0.9548, 0.9767] | 0.9719 [0.9578, 0.9862] | 0.8507 [0.8309, 0.8710] | 0.9172 [0.9082, 0.9262] | 0.9502 [0.9408, 0.9597] |
| formal-2 head÷r3 | 0.9272 [0.9202, 0.9343] | 0.9757 [0.9648, 0.9867] | 0.9762 [0.9641, 0.9885] | 0.8661 [0.8495, 0.8830] | 0.9143 [0.9054, 0.9234] | 0.9565 [0.9471, 0.9659] |
| formal-3 head÷r3 | 0.9244 [0.9161, 0.9328] | 0.9598 [0.9470, 0.9728] | 0.9641 [0.9518, 0.9765] | 0.8506 [0.8347, 0.8669] | 0.9118 [0.9028, 0.9209] | 0.9444 [0.9341, 0.9548] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9956 | 1.0005 | 是 |
| formal-1 | 0.9939 | 1.0023 | 是 |
| formal-2 | 1.0007 | 1.0093 | 是 |
| formal-3 | 0.9921 | 1.0009 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 | formal-3 |
| --- | --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.819 / 0.830 通过 | 0.827 / 0.838 通过 | 0.816 / 0.826 通过 |
| cista:mutate | 1.05 | 0.697 / 0.706 通过 | 0.702 / 0.710 通过 | 0.693 / 0.701 通过 |
| cista:emit_after | 0.2 | 0.133 / 0.136 通过 | 0.135 / 0.137 通过 | 0.133 / 0.135 通过 |
| flatbuffers:read_sum | 1.05 | 0.626 / 0.636 通过 | 0.629 / 0.641 通过 | 0.622 / 0.631 通过 |
| flatbuffers:mutate | 1.05 | 0.632 / 0.643 通过 | 0.631 / 0.644 通过 | 0.628 / 0.640 通过 |
| flatbuffers:emit_after | 0.2 | 0.126 / 0.128 通过 | 0.127 / 0.129 通过 | 0.125 / 0.127 通过 |
| capnproto:read_sum | 1.05 | 0.198 / 0.200 通过 | 0.199 / 0.201 通过 | 0.197 / 0.199 通过 |
| capnproto:mutate | 1.05 | 0.241 / 0.244 通过 | 0.241 / 0.244 通过 | 0.240 / 0.242 通过 |
| capnproto:emit_after | 0.2 | 0.307 / 0.314 **不通过** | 0.311 / 0.316 **不通过** | 0.307 / 0.311 **不通过** |
| cereal:read_sum | 1.05 | 0.859 / 0.867 通过 | 0.859 / 0.870 通过 | 0.851 / 0.860 通过 |
| cereal:mutate | 1.05 | 1.027 / 1.041 通过 | 1.025 / 1.038 通过 | 1.023 / 1.036 通过 |
| cereal:emit_after | 0.2 | 0.176 / 0.180 通过 | 0.178 / 0.180 通过 | 0.175 / 0.178 通过 |
| 通过数 head | | 11/12 | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 9/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（cleanup） | 无效（retire_modified_state, cleanup） | 有效 |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2412 [0.2393, 0.2430] | 0.5880 [0.5823, 0.5937] | 0.3945 [0.3908, 0.3982] | 0.4637 [0.4598, 0.4677] |
| formal-2 | 0.2371 [0.2332, 0.2411] | 0.5764 [0.5658, 0.5872] | 0.3893 [0.3829, 0.3958] | 0.4561 [0.4480, 0.4644] |
| formal-3 | 0.2378 [0.2358, 0.2398] | 0.5808 [0.5746, 0.5871] | 0.3909 [0.3874, 0.3945] | 0.4570 [0.4529, 0.4611] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0100）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9862 [0.9656, 1.0072] | 是 |
| B | 0.9637 [0.9257, 1.0033] | 是 |
| C | 0.9895 [0.9695, 1.0100] | 是 |
| D | 1.0085 [0.9943, 1.0228] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9862 [0.9656, 1.0072] / 0.9862 [0.9703, 1.0023] | 0.9637 [0.9257, 1.0033] / 0.9637 [0.9256, 1.0034] | 0.9895 [0.9695, 1.0100] / 0.9895 [0.9825, 0.9966] | 1.0085 [0.9943, 1.0228] / 1.0085 [0.9934, 1.0237] | head A 0.964 B 0.988 C 0.967 D 1.015；anchor A 1.003 B 0.940 C 0.983 D 0.994 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9862 [0.9173, 1.0603] | 0.9637 [0.8469, 1.0966] | 0.9895 [0.9574, 1.0227] | 1.0085 [0.9820, 1.0357] |
| 合并 | 0.9862 [0.9173, 1.0603] | 0.9637 [0.8469, 1.0966] | 0.9895 [0.9574, 1.0227] | 1.0085 [0.9820, 1.0357] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.235 / 0.239 (0.252) | 0.096 / 0.099 (0.105) | 0.324 / 0.333 (0.346) |
| B | 0.128 / 0.133 (0.132) | 0.067 / 0.070 (0.069) | 0.175 / 0.182 (0.180) |
| C | 0.102 / 0.104 (0.109) | 0.064 / 0.065 (0.068) | 0.254 / 0.261 (0.273) |
| D | 0.226 / 0.229 (0.252) | 0.352 / 0.357 (0.392) | 0.940 / 0.962 (0.976) |

## 4. 环境

- 开跑条件 formal-1：2026-11-03T00:16:21+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.27, quiet_s=241.2, screen={'WindowServer': 1.3, 'UURemoteServer': 0.6, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-03T00:27:25+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.57, quiet_s=241.2, screen={'WindowServer': 1.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-3：2026-11-03T00:38:47+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=0.93, quiet_s=241.1, screen={'WindowServer': 1.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-03T00:49:45+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.18, quiet_s=241.2, screen={'WindowServer': 1.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-03T00:20:22+00:00 → 2026-11-03T00:27:25+00:00，load [1.27392578125, 1.55029296875, 1.6650390625] → [2.14453125, 1.7626953125, 1.70166015625]，CPU 采样 {'samples': 207, 'other_busy_samples': 5, 'compile_samples': 0, 'other_busy_top': {'launchd': 3, 'deleted': 1, 'mds_stores': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.14}
  - 计时期间屏幕采样：0/106 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.1, 'UURemoteServer': 5.5, 'parsecd': 0.0}；远程桌面活动（≥5%）1/106 次采样，只记录；WindowServer ≥20% 0/106 次采样，有效
- formal-2：2026-11-03T00:31:26+00:00 → 2026-11-03T00:38:47+00:00，load [1.5703125, 1.55615234375, 1.609375] → [1.62939453125, 1.63720703125, 1.640625]，CPU 采样 {'samples': 216, 'other_busy_samples': 20, 'compile_samples': 0, 'other_busy_top': {'signpost_reporter': 19, 'modelcatalogd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.09}
  - 计时期间屏幕采样：0/111 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.0, 'UURemoteServer': 2.2, 'parsecd': 0.0}；远程桌面活动（≥5%）0/111 次采样，只记录；WindowServer ≥20% 0/111 次采样，有效
- formal-3：2026-11-03T00:42:49+00:00 → 2026-11-03T00:49:45+00:00，load [0.9345703125, 1.29345703125, 1.48095703125] → [1.67431640625, 1.6572265625, 1.58837890625]，CPU 采样 {'samples': 204, 'other_busy_samples': 7, 'compile_samples': 0, 'other_busy_top': {'launchd': 4, 'mediaanalysisd-access': 2, 'Codex (Renderer)': 1, 'mds_stores': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.01}
  - 计时期间屏幕采样：0/105 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.0, 'UURemoteServer': 1.4, 'parsecd': 0.0}；远程桌面活动（≥5%）0/105 次采样，只记录；WindowServer ≥20% 0/105 次采样，有效
- slg 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 1.2, 'UURemoteServer': 0.4, 'parsecd': 0.0}；远程桌面活动（≥5%）0/15 次采样，只记录；WindowServer ≥20% 0/15 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.055，mutate 0.061，emit_after 0.103；mutate 按副本 0.9842 / 1.0012 / 1.0010
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.065，mutate 0.061，emit_after 0.101；mutate 按副本 1.0086 / 1.0045 / 0.9885
- formal-3 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.045，mutate 0.054，emit_after 0.071；mutate 按副本 1.0127 / 0.9975 / 0.9956
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-03T00:16:21+00:00
- 二进制 sha256：head `9dd79d23bc87b7cc…`；head_aa `9dd79d23bc87b7cc…`；anchor `6eece9b8d14ba644…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree f24c253a, anchor 4180ba9b; bisection points their own)（sha256 `f4de6283f6a04ddff84619363f7a3a21917bced75e6bc9a81fc6dd77f8eb98ad`），slg_head `4b3a166e05793c3e…`，slg_anch `19978229bfc1997e…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
