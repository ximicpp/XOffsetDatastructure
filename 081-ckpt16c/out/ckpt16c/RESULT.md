# L2 Mac 门禁批次 2026-11-04-ckpt16c-d540148

- head `d5401486e5cc2da95c495c3d4f1699850ce1fd53`（CKPT16C head = PR #191 d5401486 (delta sync without the cached-root carry, a8f12389 merged with next_cpp26 49ce3c68, plus: loading holes always marks the free table stale), build-r3d params）；anchor `49ce3c68fa60ff9b531f1fa482fc4fb4b8e65479`（next_cpp26 49ce3c68 (#212; PR #213 not merged), build-r3d params）
- CKPT16C: L2 gate of PR #191 (class L, delta sync) at d5401486 = 58779139 + the hole-load stale mark (replaces CKPT16A 58779139), after dropping the cached-root carry (a8f12389; 性能优化 05:14Z option a), merge result vs mainline; merge tier of TEST_TIERS (core gate + G9R); anchor 49ce3c68; each point built with its own bench/slg; one deferred bisection point 6ec08cc6 (the CKPT14 head, with the carry; report only)
- **结论：fail**。正式部分 pass（合并 2 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `d541e0b5320c2d89e7811d530949f7cc9d8305c9cc018a8252ec1b49c7e21d4f`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9980 [0.9899, 1.0051] | 1.0000 [0.9896, 1.0093] | 0.9996 [0.9874, 1.0105] | 1.0105 | pass | 1.0067 / 1.0022 / 0.9972 | 有效 |
| formal-1（单批，只报） | 0.9963 [0.9835, 1.0078] | 0.9987 [0.9829, 1.0128] | 1.0018 [0.9825, 1.0192] | 1.0192 | pass | 1.0127 / 1.0086 / 1.0015 | **无效** |
| formal-2（单批，只报） | 0.9996 [0.9896, 1.0086] | 1.0013 [0.9873, 1.0138] | 0.9973 [0.9820, 1.0112] | 1.0138 | pass | 1.0007 / 0.9959 / 0.9928 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9523 / 1.0005 / 1.0490 | 0.9422 / 1.0013 / 1.0556 | 0.9217 / 1.0029 / 1.0661 |
| formal-1 | 0.9444 / 0.9952 / 1.0528 | 0.9341 / 1.0009 / 1.0637 | 0.9139 / 1.0033 / 1.0878 |
| formal-2 | 0.9581 / 1.0027 / 1.0427 | 0.9486 / 1.0018 / 1.0536 | 0.9247 / 1.0029 / 1.0613 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9921 [0.9782, 1.0062] | 0.9980 [0.9883, 1.0077] | 1.0000 [0.9875, 1.0126] | 0.9996 [0.9849, 1.0144] | 1.0081 [0.9935, 1.0228] | 0.9993 [0.9911, 1.0075] |
| formal-1 head÷anchor | 0.9945 [0.9824, 1.0068] | 0.9963 [0.9808, 1.0121] | 0.9987 [0.9797, 1.0180] | 1.0018 [0.9785, 1.0257] | 1.0074 [0.9833, 1.0322] | 0.9984 [0.9859, 1.0111] |
| formal-2 head÷anchor | 0.9897 [0.9643, 1.0158] | 0.9996 [0.9875, 1.0119] | 1.0013 [0.9845, 1.0185] | 0.9973 [0.9788, 1.0163] | 1.0087 [0.9914, 1.0263] | 1.0001 [0.9891, 1.0112] |
| 合并 head÷r3 | 0.9327 [0.9252, 0.9403] | 0.9405 [0.9320, 0.9492] | 0.9358 [0.9241, 0.9477] | 0.8390 [0.8264, 0.8518] | 0.9214 [0.9059, 0.9371] | 0.9303 [0.9226, 0.9380] |
| formal-1 head÷r3 | 0.9308 [0.9196, 0.9422] | 0.9450 [0.9304, 0.9597] | 0.9337 [0.9126, 0.9554] | 0.8428 [0.8217, 0.8645] | 0.9058 [0.8808, 0.9315] | 0.9299 [0.9165, 0.9434] |
| formal-2 head÷r3 | 0.9346 [0.9242, 0.9450] | 0.9361 [0.9266, 0.9457] | 0.9379 [0.9270, 0.9490] | 0.8352 [0.8205, 0.8501] | 0.9372 [0.9202, 0.9545] | 0.9307 [0.9226, 0.9388] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 1.0000 | 1.0085 | 是 |
| formal-1 | 0.9987 | 1.0116 | 是 |
| formal-2 | 1.0013 | 1.0128 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.781 / 0.794 通过 | 0.791 / 0.797 通过 |
| cista:mutate | 1.05 | 0.676 / 0.688 通过 | 0.682 / 0.690 通过 |
| cista:emit_after | 0.2 | 0.129 / 0.132 通过 | 0.125 / 0.127 通过 |
| flatbuffers:read_sum | 1.05 | 0.627 / 0.639 通过 | 0.626 / 0.640 通过 |
| flatbuffers:mutate | 1.05 | 0.597 / 0.611 通过 | 0.590 / 0.607 通过 |
| flatbuffers:emit_after | 0.2 | 0.121 / 0.123 通过 | 0.117 / 0.119 通过 |
| capnproto:read_sum | 1.05 | 0.212 / 0.216 通过 | 0.219 / 0.221 通过 |
| capnproto:mutate | 1.05 | 0.231 / 0.234 通过 | 0.233 / 0.237 通过 |
| capnproto:emit_after | 0.2 | 0.320 / 0.328 **不通过** | 0.322 / 0.331 **不通过** |
| cereal:read_sum | 1.05 | 0.844 / 0.863 通过 | 0.843 / 0.880 通过 |
| cereal:mutate | 1.05 | 0.980 / 0.994 通过 | 0.985 / 0.999 通过 |
| cereal:emit_after | 0.2 | 0.167 / 0.171 通过 | 0.162 / 0.164 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（retire_modified_state, cleanup） | 有效 |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2177 [0.2101, 0.2256] | 0.5285 [0.5092, 0.5485] | 0.3618 [0.3484, 0.3758] | 0.4136 [0.3977, 0.4301] |
| formal-2 | 0.2116 [0.2072, 0.2161] | 0.5148 [0.5043, 0.5254] | 0.3618 [0.3541, 0.3698] | 0.4019 [0.3927, 0.4112] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0352）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 1.0094 [0.9843, 1.0352] | **否** |
| B | 1.0016 [0.9815, 1.0220] | **否** |
| C | 1.0020 [0.9720, 1.0330] | **否** |
| D | 0.9899 [0.9658, 1.0146] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 1.0094 [0.9843, 1.0352] / 1.0094 [0.9842, 1.0354] | 1.0016 [0.9815, 1.0220] / 1.0016 [0.9805, 1.0231] | 1.0020 [0.9720, 1.0330] / 1.0020 [0.9743, 1.0306] | 0.9899 [0.9658, 1.0146] / 0.9899 [0.9671, 1.0132] | head A 0.984 B 0.991 C 0.970 D 1.022；anchor A 1.009 B 1.008 C 0.976 D 1.031 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 1.0094 [0.9729, 1.0474] | 1.0016 [0.9422, 1.0647] | 1.0020 [0.9293, 1.0805] | 0.9899 [0.9280, 1.0560] |
| 合并 | 1.0094 [0.9729, 1.0474] | 1.0016 [0.9422, 1.0647] | 1.0020 [0.9293, 1.0805] | 0.9899 [0.9280, 1.0560] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.236 / 0.243 (0.254) | 0.099 / 0.100 (0.106) | 0.377 / 0.398 (0.406) |
| B | 0.160 / 0.162 (0.163) | 0.076 / 0.077 (0.079) | 0.286 / 0.301 (0.295) |
| C | 0.123 / 0.128 (0.129) | 0.073 / 0.075 (0.077) | 0.364 / 0.380 (0.385) |
| D | 0.248 / 0.257 (0.282) | 0.351 / 0.358 (0.395) | 0.928 / 0.957 (0.976) |

## 4. 环境

- 开跑条件 formal-1：2026-11-04T09:11:45+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.81, quiet_s=241.1, screen={'WindowServer': 4.8, 'UURemoteServer': 0.2, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-04T09:24:08+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=2.09, quiet_s=240.9, screen={'WindowServer': 44.7, 'UURemoteServer': 21.5, 'parsecd': 0.0}, remote_active=['UURemoteServer'], screen_over=['WindowServer'], screen_gated=False
- 开跑条件 slg：2026-11-04T09:35:15+00:00 ok=True, quiet_s_required=180.0, waited_s=240.9, load1=2.44, quiet_s=240.9, screen={'WindowServer': 42.9, 'UURemoteServer': 17.5, 'parsecd': 0.0}, remote_active=['UURemoteServer'], screen_over=['WindowServer'], screen_gated=False
- formal-1：2026-11-04T09:15:46+00:00 → 2026-11-04T09:24:08+00:00，load [1.810546875, 2.255859375, 2.21484375] → [2.76904296875, 2.47607421875, 2.30517578125]，CPU 采样 {'samples': 246, 'other_busy_samples': 129, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 123, 'Claude Helper (Renderer)': 15, 'UURemoteServer': 3, 'launchd': 2, 'Codex (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 3.01}
  - 计时期间屏幕采样：0/126 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 47.1, 'UURemoteServer': 33.8, 'parsecd': 0.0}；远程桌面活动（≥5%）64/126 次采样，只记录；WindowServer ≥20% 64/126 次采样，有效
- formal-2：2026-11-04T09:28:09+00:00 → 2026-11-04T09:35:15+00:00，load [2.08642578125, 2.3759765625, 2.310546875] → [4.482421875, 3.20263671875, 2.68505859375]，CPU 采样 {'samples': 210, 'other_busy_samples': 162, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 154, 'Claude Helper (Renderer)': 5, 'Codex (Renderer)': 3, 'duetexpertd': 2, 'corespotlightd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 4.48}
  - 计时期间屏幕采样：0/107 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 46.5, 'UURemoteServer': 21.5, 'parsecd': 0.0}；远程桌面活动（≥5%）107/107 次采样，只记录；WindowServer ≥20% 79/107 次采样，有效
- slg 计时期间屏幕采样：0/14 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 46.1, 'UURemoteServer': 54.7, 'parsecd': 0.0}；远程桌面活动（≥5%）12/14 次采样，只记录；WindowServer ≥20% 14/14 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.111，mutate 0.105，emit_after 0.111；mutate 按副本 0.9992 / 1.0098 / 1.0169
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.053，mutate 0.059，emit_after 0.074；mutate 按副本 0.9984 / 1.0001 / 0.9892
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-04T09:11:45+00:00
- 二进制 sha256：head `2ec85d62848f173e…`；head_aa `2ec85d62848f173e…`；anchor `b325298ef9e33efe…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 55ba4e84, anchor 55ba4e84; bisection points their own)（sha256 `efa09929732a456b5b8a9169ee03fbb3574f975bc54a60a8e4db024bf9c544e3`），slg_head `ab1d2d4e207a5a40…`，slg_anch `4d8a2b8c6d246dc6…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
