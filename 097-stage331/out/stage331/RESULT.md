# L2 Mac 门禁批次 2026-11-05-stage331-ee506d3

- head `ee506d3f54a979ededdd71cdc5cca5857ff38f11`（STAGE331 head = v3.3.1 ee506d3f (next_cpp26, release PR #238; #228 #223 #233 #221 #226 since v3.3.0), build-r3d params）；anchor `51a4815b7802ec125e2752847b69fd2a87ab7259`（v3.3.0 51a4815b (previous release tag), build-r3d params）
- STAGE331: the 3.3.1 post-release stage review (docs/RELEASING.md section 7: L2 rules, report only): v3.3.1 ee506d3f vs v3.3.0 51a4815b; merge tier of TEST_TIERS (core gate + G9R); every point built with the head's bench/slg (a0a379a8); one deferred bisection point #223 84243ba5 (1 step at most; cloud P2996: the bench binaries of #221 0723bc0b equal 84243ba5's and those of #226 eb4f0468 equal the head's, so the range (84243ba5, head] is #226); #228 d9eb9559 is not a point (bench binaries identical to v3.3.0, CKPT21b); #223 is a known item (CKPT20: A.req_encode +6.8%); no report-only step
- **结论：fail**。正式部分 pass（合并 2 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `4985248fbbb7796f8c671a46aca2186512a7f8d21fc18692735cf581306cf356`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **合并 2 批（判定）** | 0.9955 [0.9866, 1.0034] | 0.9998 [0.9924, 1.0065] | 0.9972 [0.9856, 1.0076] | 1.0076 | pass | 1.0002 / 0.9985 / 0.9993 | 有效 |
| formal-1（单批，只报） | 0.9925 [0.9827, 1.0012] | 0.9973 [0.9871, 1.0064] | 0.9936 [0.9768, 1.0087] | 1.0087 | pass | 1.0045 / 1.0012 / 1.0011 | 有效 |
| formal-2（单批，只报） | 0.9984 [0.9833, 1.0120] | 1.0024 [0.9913, 1.0123] | 1.0008 [0.9844, 1.0156] | 1.0156 | pass | 0.9960 / 0.9958 / 0.9975 | 有效 |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| 合并 | 0.9399 / 0.9976 / 1.0416 | 0.9571 / 0.9980 / 1.0497 | 0.9403 / 0.9950 / 1.0595 |
| formal-1 | 0.9453 / 0.9936 / 1.0316 | 0.9606 / 0.9977 / 1.0511 | 0.9410 / 0.9931 / 1.0489 |
| formal-2 | 0.9314 / 1.0035 / 1.0550 | 0.9547 / 1.0026 / 1.0509 | 0.9308 / 0.9963 / 1.0766 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| 合并 head÷anchor | 0.9931 [0.9774, 1.0090] | 0.9955 [0.9848, 1.0062] | 0.9998 [0.9909, 1.0089] | 0.9972 [0.9832, 1.0113] | 0.9945 [0.9807, 1.0085] | 0.9963 [0.9886, 1.0041] |
| formal-1 head÷anchor | 1.0036 [0.9924, 1.0150] | 0.9925 [0.9807, 1.0044] | 0.9973 [0.9850, 1.0097] | 0.9936 [0.9733, 1.0143] | 0.9968 [0.9806, 1.0134] | 0.9949 [0.9851, 1.0048] |
| formal-2 head÷anchor | 0.9826 [0.9535, 1.0126] | 0.9984 [0.9802, 1.0170] | 1.0024 [0.9890, 1.0160] | 1.0008 [0.9810, 1.0210] | 0.9922 [0.9693, 1.0157] | 0.9978 [0.9855, 1.0102] |
| 合并 head÷r3 | 0.9269 [0.9188, 0.9350] | 0.9443 [0.9340, 0.9547] | 0.9411 [0.9324, 0.9499] | 0.8404 [0.8292, 0.8517] | 0.9177 [0.9053, 0.9302] | 0.9330 [0.9263, 0.9398] |
| formal-1 head÷r3 | 0.9283 [0.9175, 0.9394] | 0.9372 [0.9269, 0.9476] | 0.9323 [0.9210, 0.9437] | 0.8375 [0.8224, 0.8529] | 0.9323 [0.9187, 0.9460] | 0.9291 [0.9208, 0.9376] |
| formal-2 head÷r3 | 0.9254 [0.9131, 0.9378] | 0.9515 [0.9335, 0.9698] | 0.9501 [0.9370, 0.9633] | 0.8433 [0.8262, 0.8606] | 0.9034 [0.8837, 0.9235] | 0.9369 [0.9261, 0.9478] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| **合并（判据）** | 0.9998 | 1.0059 | 是 |
| formal-1 | 0.9973 | 1.0056 | 是 |
| formal-2 | 1.0024 | 1.0115 | 是 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 |
| --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.786 / 0.794 通过 | 0.769 / 0.781 通过 |
| cista:mutate | 1.05 | 0.675 / 0.681 通过 | 0.675 / 0.685 通过 |
| cista:emit_after | 0.2 | 0.125 / 0.126 通过 | 0.127 / 0.129 通过 |
| flatbuffers:read_sum | 1.05 | 0.629 / 0.640 通过 | 0.594 / 0.605 通过 |
| flatbuffers:mutate | 1.05 | 0.594 / 0.607 通过 | 0.590 / 0.602 通过 |
| flatbuffers:emit_after | 0.2 | 0.118 / 0.120 通过 | 0.119 / 0.121 通过 |
| capnproto:read_sum | 1.05 | 0.214 / 0.217 通过 | 0.201 / 0.204 通过 |
| capnproto:mutate | 1.05 | 0.232 / 0.236 通过 | 0.233 / 0.235 通过 |
| capnproto:emit_after | 0.2 | 0.325 / 0.331 **不通过** | 0.319 / 0.326 **不通过** |
| cereal:read_sum | 1.05 | 0.851 / 0.864 通过 | 0.828 / 0.843 通过 |
| cereal:mutate | 1.05 | 0.969 / 0.993 通过 | 0.978 / 0.993 通过 |
| cereal:emit_after | 0.2 | 0.161 / 0.164 通过 | 0.165 / 0.167 通过 |
| 通过数 head | | 11/12 | 11/12 |
| 通过数 anchor（同批） | | 11/12 | 11/12 |
| 通过数 r3（同批） | | 10/12 | 11/12 |
| 8 阶段 A/A（旧规则，只报） | | 有效 | 无效（retire_modified_state, cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2084 [0.2048, 0.2120] | 0.5086 [0.5002, 0.5171] | 0.3644 [0.3579, 0.3711] | 0.3939 [0.3862, 0.4017] |
| formal-2 | 0.1988 [0.1939, 0.2038] | 0.4896 [0.4780, 0.5014] | 0.3377 [0.3301, 0.3454] | 0.3798 [0.3702, 0.3897] |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0365）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 1.0161 [0.9961, 1.0365] | **否** |
| B | 0.9955 [0.9774, 1.0140] | 是 |
| C | 0.9852 [0.9575, 1.0137] | 是 |
| D | 0.9666 [0.9323, 1.0021] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 1.0161 [0.9961, 1.0365] / 1.0161 [1.0019, 1.0305] | 0.9955 [0.9774, 1.0140] / 0.9955 [0.9868, 1.0044] | 0.9852 [0.9575, 1.0137] / 0.9852 [0.9651, 1.0057] | 0.9666 [0.9323, 1.0021] / 0.9666 [0.9380, 0.9960] | head A 0.982 B 0.979 C 0.965 D 1.020；anchor A 0.965 B 0.975 C 0.976 D 0.999 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 1.0161 [1.0065, 1.0258] | 0.9955 [0.9569, 1.0357] | 0.9852 [0.9499, 1.0218] | 0.9666 [0.9417, 0.9920] |
| 合并 | 1.0161 [1.0065, 1.0258] | 0.9955 [0.9569, 1.0357] | 0.9852 [0.9499, 1.0218] | 0.9666 [0.9417, 0.9920] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.233 / 0.236 (0.252) | 0.098 / 0.101 (0.107) | 0.382 / 0.389 (0.408) |
| B | 0.158 / 0.161 (0.162) | 0.077 / 0.078 (0.079) | 0.291 / 0.295 (0.297) |
| C | 0.112 / 0.115 (0.121) | 0.069 / 0.071 (0.076) | 0.350 / 0.360 (0.379) |
| D | 0.242 / 0.245 (0.270) | 0.348 / 0.354 (0.387) | 0.949 / 0.972 (0.997) |

## 4. 环境

- 开跑条件 formal-1：2026-11-05T05:42:39+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.5, quiet_s=241.1, screen={'WindowServer': 5.4, 'UURemoteServer': 13.7, 'parsecd': 0.0}, remote_active=['UURemoteServer'], screen_over=[], screen_gated=False
- 开跑条件 formal-2：2026-11-05T05:53:58+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.55, quiet_s=241.1, screen={'WindowServer': 5.8, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- 开跑条件 slg：2026-11-05T06:06:02+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=2.35, quiet_s=241.1, screen={'WindowServer': 4.4, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- formal-1：2026-11-05T05:46:40+00:00 → 2026-11-05T05:53:58+00:00，load [1.4951171875, 1.8955078125, 2.09912109375] → [3.38525390625, 2.54931640625, 2.2822265625]，CPU 采样 {'samples': 215, 'other_busy_samples': 69, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 65, 'Claude Helper (Renderer)': 3, 'airportd': 2, 'Codex (Renderer)': 2, 'duetexpertd': 2}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 3.65}
  - 计时期间屏幕采样：0/110 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 44.7, 'UURemoteServer': 32.9, 'parsecd': 0.0}；远程桌面活动（≥5%）106/110 次采样，只记录；WindowServer ≥20% 35/110 次采样，有效
- formal-2：2026-11-05T05:58:00+00:00 → 2026-11-05T06:06:02+00:00，load [1.54833984375, 2.08544921875, 2.14453125] → [2.22412109375, 2.208984375, 2.1865234375]，CPU 采样 {'samples': 237, 'other_busy_samples': 13, 'compile_samples': 0, 'other_busy_top': {'spindump': 3, 'Claude Helper (Renderer)': 3, 'duetexpertd': 2, 'Codex (Renderer)': 2, 'launchd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.99}
  - 计时期间屏幕采样：0/121 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 7.0, 'UURemoteServer': 1.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/121 次采样，只记录；WindowServer ≥20% 0/121 次采样，有效
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.4, 'UURemoteServer': 1.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.052，mutate 0.059，emit_after 0.086；mutate 按副本 1.0058 / 0.9945 / 1.0034
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.081，mutate 0.076，emit_after 0.096；mutate 按副本 0.9930 / 0.9974 / 0.9971
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-05T05:42:39+00:00
- 二进制 sha256：head `a394582b515d0ab1…`；head_aa `a394582b515d0ab1…`；anchor `8cd9767c692ea0bb…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 the head's bench/slg for every point (git tree a0a379a8; anchor and bisection points too)（sha256 `3b043a261907190be511f77fd87516c1cbda590097695534d9dd902b5518b346`），slg_head `c945740d34fbb608…`，slg_anch `305ea85839d95e1e…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
