# L2 Mac 门禁批次 2026-11-05-ckpt20-9313428

- head `9313428e633aac1e9c2e0a57ab03a17cc718f244`（CKPT20 head = PR #223 9313428e (root<T>(): inline cache hit, out-of-line root_checked fills the cache; 16de37a5 merged with next_cpp26 c9fae222), build-r3d params）；anchor `c9fae222bb86ef829f9cf5b7a7c241e6978e93f6`（next_cpp26 c9fae222 (3.3.0 and #227 merged), build-r3d params）
- CKPT20: L2 gate of PR #223 (class L, root cache refill in xoffsetdatastructure.hpp root<T>() / cache_root, one guard in xoffset_delta.hpp) as merged with next_cpp26 c9fae222 (9313428e vs c9fae222, merge result vs mainline); merge tier of TEST_TIERS (core gate + G9R); each point built with its own bench/slg (both 03557f54); one deferred bisection point 16de37a5 (#223 merged with e7fd8cb0, bench/slg 23e86624, report only); report-only step 5x: HFT 64 books (book_c.cpp, access root and ptr)
- **结论：fail**。正式部分 pass（合并 2 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `5a757324e8a7ea334fa867696b73bfaf4292a006aa96c0e6789e5cf1c406ac1b`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 1.0007 [0.9896, 1.0108] | 1.0005 [0.9886, 1.0111] | 1.0048 [0.9904, 1.0179] | 1.0179 | pass | 0.9966 / 0.9940 / 0.9960 | 有效 |
| formal-1（单批，只报） | 1.0066 [0.9871, 1.0243] | 0.9991 [0.9795, 1.0167] | 1.0002 [0.9802, 1.0183] | 1.0243 | pending | 0.9919 / 0.9948 / 0.9922 | 有效 |
| formal-2（单批，只报） | 0.9949 [0.9836, 1.0050] | 1.0019 [0.9878, 1.0144] | 1.0095 [0.9880, 1.0288] | 1.0288 | pending | 1.0012 / 0.9931 / 0.9998 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9476 / 0.9989 / 1.0513 | 0.9488 / 0.9928 / 1.0544 | 0.9263 / 1.0077 / 1.0904 |
| formal-1 | 0.9453 / 1.0004 / 1.0558 | 0.9428 / 0.9983 / 1.0367 | 0.9250 / 1.0048 / 1.0739 |
| formal-2 | 0.9508 / 0.9940 / 1.0451 | 0.9503 / 0.9915 / 1.0658 | 0.9201 / 1.0175 / 1.1101 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 1.0055 [0.9897, 1.0216] | 1.0007 [0.9873, 1.0144] | 1.0005 [0.9862, 1.0149] | 1.0048 [0.9874, 1.0225] | 1.0054 [0.9878, 1.0234] | 1.0019 [0.9897, 1.0142] |
| formal-1 head÷anchor | 1.0117 [0.9830, 1.0412] | 1.0066 [0.9830, 1.0308] | 0.9991 [0.9755, 1.0233] | 1.0002 [0.9761, 1.0250] | 1.0158 [0.9865, 1.0459] | 1.0063 [0.9840, 1.0291] |
| formal-2 head÷anchor | 0.9993 [0.9848, 1.0141] | 0.9949 [0.9812, 1.0088] | 1.0019 [0.9849, 1.0191] | 1.0095 [0.9836, 1.0360] | 0.9952 [0.9749, 1.0161] | 0.9975 [0.9868, 1.0083] |
| 合并 head÷r3 | 0.9301 [0.9111, 0.9494] | 0.9454 [0.9266, 0.9646] | 0.9328 [0.9007, 0.9661] | 0.8420 [0.8265, 0.8577] | 0.9172 [0.8953, 0.9396] | 0.9295 [0.9048, 0.9548] |
| formal-1 head÷r3 | 0.9291 [0.8948, 0.9647] | 0.9556 [0.9307, 0.9812] | 0.9437 [0.9182, 0.9700] | 0.8428 [0.8255, 0.8603] | 0.9200 [0.8898, 0.9513] | 0.9392 [0.9150, 0.9640] |
| formal-2 head÷r3 | 0.9310 [0.9135, 0.9488] | 0.9353 [0.9067, 0.9647] | 0.9220 [0.8632, 0.9849] | 0.8412 [0.8148, 0.8684] | 0.9144 [0.8818, 0.9482] | 0.9198 [0.8766, 0.9652] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 1.0005 | 1.0103 | 是 |
| formal-1 | 0.9991 | 1.0153 | 是 |
| formal-2 | 1.0019 | 1.0134 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.780 / 0.797 通过 | 0.787 / 0.802 通过 |
| cista:mutate | 1.05 | 0.675 / 0.688 通过 | 0.680 / 0.698 通过 |
| cista:emit_after | 0.2 | 0.128 / 0.131 通过 | 0.129 / 0.132 通过 |
| flatbuffers:read_sum | 1.05 | 0.609 / 0.626 通过 | 0.634 / 0.646 通过 |
| flatbuffers:mutate | 1.05 | 0.593 / 0.613 通过 | 0.602 / 0.615 通过 |
| flatbuffers:emit_after | 0.2 | 0.120 / 0.123 通过 | 0.121 / 0.124 通过 |
| capnproto:read_sum | 1.05 | 0.204 / 0.210 通过 | 0.222 / 0.225 通过 |
| capnproto:mutate | 1.05 | 0.234 / 0.240 通过 | 0.236 / 0.241 通过 |
| capnproto:emit_after | 0.2 | 0.312 / 0.318 **不通过** | 0.328 / 0.335 **不通过** |
| cereal:read_sum | 1.05 | 0.827 / 0.851 通过 | 0.857 / 0.872 通过 |
| cereal:mutate | 1.05 | 0.971 / 1.000 通过 | 0.978 / 1.005 通过 |
| cereal:emit_after | 0.2 | 0.165 / 0.169 通过 | 0.166 / 0.170 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 9/12 | 9/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（retire_modified_state, cleanup） | 无效（retire_modified_state, cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2068 [0.1984, 0.2154] | 0.5068 [0.4859, 0.5287] | 0.3383 [0.3249, 0.3523] | 0.3916 [0.3743, 0.4097] |
| formal-2 | 0.2178 [0.2120, 0.2237] | 0.5359 [0.5204, 0.5519] | 0.3654 [0.3572, 0.3739] | 0.4141 [0.4027, 0.4259] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；2 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0302）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 1.0112 [0.9966, 1.0260] | **否** |
| B | 0.9893 [0.9783, 1.0004] | 是 |
| C | 0.9982 [0.9672, 1.0302] | **否** |
| D | 1.0066 [0.9944, 1.0189] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 1.0055 [0.9841, 1.0274] / 1.0055 [0.9890, 1.0223] | 0.9899 [0.9695, 1.0108] / 0.9899 [0.9763, 1.0038] | 0.9987 [0.9723, 1.0257] / 0.9987 [0.9860, 1.0115] | 1.0066 [0.9840, 1.0297] / 1.0066 [0.9835, 1.0302] | head A 0.996 B 0.996 C 0.968 D 1.011；anchor A 0.972 B 0.966 C 0.953 D 0.992 |
| slg-2 | 1.0169 [0.9892, 1.0453] / 1.0169 [1.0066, 1.0273] | 0.9886 [0.9708, 1.0068] / 0.9886 [0.9794, 0.9980] | 0.9977 [0.9707, 1.0256] / 0.9977 [0.9773, 1.0186] | 1.0066 [0.9880, 1.0255] / 1.0066 [0.9959, 1.0174] | head A 0.956 B 0.973 C 0.966 D 1.020；anchor A 0.959 B 0.973 C 0.956 D 1.036 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 1.0055 [0.9469, 1.0677] | 0.9899 [0.9410, 1.0415] | 0.9987 [0.9904, 1.0070] | 1.0066 [0.9285, 1.0912] |
| slg-2 | 1.0169 [0.9998, 1.0342] | 0.9886 [0.9585, 1.0197] | 0.9977 [0.9191, 1.0832] | 1.0066 [0.9744, 1.0399] |
| 合并 | 1.0112 [0.9958, 1.0268] | 0.9893 [0.9780, 1.0007] | 0.9982 [0.9825, 1.0142] | 1.0066 [0.9899, 1.0236] |

进程数 head 12、anchor 12。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.240 / 0.242 (0.257) | 0.098 / 0.100 (0.110) | 0.384 / 0.388 (0.408) |
| B | 0.161 / 0.162 (0.166) | 0.077 / 0.077 (0.080) | 0.290 / 0.293 (0.304) |
| C | 0.123 / 0.127 (0.137) | 0.072 / 0.074 (0.081) | 0.363 / 0.372 (0.415) |
| D | 0.250 / 0.251 (0.281) | 0.349 / 0.353 (0.397) | 0.940 / 0.952 (1.004) |

## 4. 环境

- 开跑条件 formal-1：2026-11-05T01:15:12+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=2.06, quiet_s=241.1, screen={'WindowServer': 4.9, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-05T01:26:29+00:00 ok=True, quiet_s_required=180.0, waited_s=245.5, load1=2.82, quiet_s=245.5, screen={'WindowServer': 45.0, 'UURemoteServer': 16.1, 'parsecd': 0.0}, remote_active=['UURemoteServer'], screen_over=['WindowServer'], screen_gated=False
- 开跑条件 slg：2026-11-05T01:38:49+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=2.68, quiet_s=241.0, screen={'WindowServer': 43.4, 'UURemoteServer': 16.3, 'parsecd': 0.0}, remote_active=['UURemoteServer'], screen_over=['WindowServer'], screen_gated=False
- 开跑条件 slg-2：2026-11-05T01:43:37+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.74, quiet_s=240.9, screen={'WindowServer': 43.5, 'UURemoteServer': 1.0, 'parsecd': 0.0}, remote_active=[], screen_over=['WindowServer'], screen_gated=False
- formal-1：2026-11-05T01:19:13+00:00 → 2026-11-05T01:26:29+00:00，load [2.056640625, 2.42919921875, 2.43115234375] → [2.61865234375, 2.29638671875, 2.34228515625]，CPU 采样 {'samples': 215, 'other_busy_samples': 38, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 32, 'Claude Helper (Renderer)': 5, 'spindump': 2, 'UURemoteServer': 2, 'duetexpertd': 2}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.68}
  - 计时期间屏幕采样：0/110 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 46.6, 'UURemoteServer': 34.6, 'parsecd': 0.0}；远程桌面活动（≥5%）18/110 次采样，只记录；WindowServer ≥20% 18/110 次采样，有效
- formal-2：2026-11-05T01:30:35+00:00 → 2026-11-05T01:38:48+00:00，load [2.81884765625, 2.623046875, 2.46533203125] → [2.22265625, 2.4638671875, 2.4677734375]，CPU 采样 {'samples': 243, 'other_busy_samples': 133, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 128, 'Claude Helper (Renderer)': 8, 'PerfPowerServices': 4, 'Codex (Renderer)': 2, 'duetexpertd': 2}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 3.37}
  - 计时期间屏幕采样：0/124 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 46.7, 'UURemoteServer': 22.8, 'parsecd': 0.0}；远程桌面活动（≥5%）124/124 次采样，只记录；WindowServer ≥20% 66/124 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 45.6, 'UURemoteServer': 16.6, 'parsecd': 0.0}；远程桌面活动（≥5%）13/13 次采样，只记录；WindowServer ≥20% 13/13 次采样，有效
- slg-2 计时期间屏幕采样：0/12 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 44.3, 'UURemoteServer': 1.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/12 次采样，只记录；WindowServer ≥20% 10/12 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.112，mutate 0.120，emit_after 0.110；mutate 按副本 0.9743 / 1.0020 / 1.0085
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.101，mutate 0.093，emit_after 0.120；mutate 按副本 0.9986 / 0.9946 / 0.9861
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-05T01:15:12+00:00
- 二进制 sha256：head `809ff91d87da94f3…`；head_aa `809ff91d87da94f3…`；anchor `8cd9767c692ea0bb…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 03557f54, anchor 03557f54; bisection points their own)（sha256 `3dda371aa1f09c5d65021b00697510d4618e4db93e32c7d9af2a16d5bfd26d96`），slg_head `79ae571ad9440345…`，slg_anch `fd008adaa52c68df…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
