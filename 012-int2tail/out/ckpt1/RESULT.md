# L2 Mac 门禁批次 2026-10-26-ckpt1-f8eebf4

- head `f8eebf46565c746a69ed64dbe9e406546032f892`（CKPT1 head = next_cpp26 f8eebf4 (INT1 merged), build-r3d params）；anchor `414dea49870365fa016d65e267cb6530ce40f7c4`（414dea4 (#81 merge, CKPT1 anchor), build-r3d params）
- first checkpoint (merge rules v2): every first-parent merge 414dea4..f8eebf4, 0, S and L classes, including INT1; bisection points deferred
- **结论：invalid**。正式部分 invalid（合并 1 批），SLG 未跑。
- formal-2 **环境失效**，未计入合并：16/107 次采样超限（14.9%，上限 5%），按进程 {'WindowServer': 16}，最高 pcpu {'WindowServer': 46.7, 'UURemoteServer': 0.0, 'parsecd': 0.1}；远程桌面活动（≥5%）0/107 次采样，只记录
- formal-3 **环境失效**，未计入合并：46/117 次采样超限（39.3%，上限 5%），按进程 {'WindowServer': 46}，最高 pcpu {'WindowServer': 45.5, 'UURemoteServer': 0.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/117 次采样，只记录；它是 formal-2 的补跑
- **正式部分 invalid：formal-3（替换 formal-2）环境仍失效：停下报告**
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `96c2b49b0ffe15b114c8db00b174d637b705dd8e98768cf272f94ecd6188b1bf`；mac_gate.py `d3b2fa67368047e12c7115dc0ec46e4c5bd425aa6ec04bf42b31711444e14a5f`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |
| formal-1（单批，只报） | 1.0013 [0.9909, 1.0106] | 1.0014 [0.9912, 1.0106] | 0.9519 [0.9372, 0.9650] | 1.0106 | pass | 0.9980 / 0.9983 / 0.9912 | 有效 |
| formal-2（环境失效，未计入） | 1.0035 [0.9926, 1.0132] | 1.0080 [0.9947, 1.0199] | 0.9499 [0.9343, 0.9638] | 1.0199 | pass | 1.0014 / 1.0025 / 1.0107 | **无效** |
| formal-3（环境失效，未计入） | 1.0137 [0.9948, 1.0307] | 1.0197 [0.9918, 1.0451] | 0.9575 [0.9270, 0.9853] | 1.0451 | fail | 0.9848 / 0.9839 / 0.9897 | **无效** |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |
| formal-1 | 0.9584 / 1.0026 / 1.0485 | 0.9617 / 1.0010 / 1.0457 | 0.8830 / 0.9574 / 1.0112 |
| formal-2 | 0.9619 / 1.0052 / 1.0517 | 0.9584 / 1.0076 / 1.0514 | 0.9040 / 0.9523 / 1.0101 |
| formal-3 | 0.9573 / 1.0087 / 1.0736 | 0.9507 / 1.0099 / 1.0701 | 0.8997 / 0.9538 / 1.0463 |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |
| formal-1 head÷anchor | 1.0023 [0.9941, 1.0106] | 1.0013 [0.9888, 1.0141] | 1.0014 [0.9891, 1.0139] | 0.9519 [0.9342, 0.9699] | 1.0133 [1.0005, 1.0262] | 0.9999 [0.9901, 1.0099] |
| formal-2 head÷anchor | 0.9941 [0.9824, 1.0059] | 1.0035 [0.9903, 1.0168] | 1.0080 [0.9919, 1.0243] | 0.9499 [0.9311, 0.9690] | 1.0069 [0.9916, 1.0225] | 1.0011 [0.9904, 1.0119] |
| formal-3 head÷anchor | 0.9930 [0.9748, 1.0114] | 1.0137 [0.9909, 1.0370] | 1.0197 [0.9860, 1.0546] | 0.9575 [0.9207, 0.9958] | 1.0190 [0.9858, 1.0534] | 1.0114 [0.9869, 1.0365] |
| formal-1 head÷r3 | 0.4951 [0.4912, 0.4991] | 0.9646 [0.9527, 0.9766] | 0.9836 [0.9735, 0.9938] | 0.9842 [0.9665, 1.0022] | 0.5373 [0.5268, 0.5480] | 0.8177 [0.8100, 0.8254] |
| formal-2 head÷r3 | 0.4936 [0.4891, 0.4981] | 0.9581 [0.9425, 0.9740] | 0.9839 [0.9687, 0.9994] | 0.9837 [0.9678, 0.9999] | 0.5133 [0.5036, 0.5231] | 0.8140 [0.8041, 0.8240] |
| formal-3 head÷r3 | 0.4950 [0.4891, 0.5010] | 0.9617 [0.9413, 0.9825] | 1.0087 [0.9772, 1.0412] | 1.0020 [0.9751, 1.0297] | 0.5223 [0.5051, 0.5401] | 0.8277 [0.8099, 0.8459] |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |
| formal-1 | 1.0014 | 1.0098 | 是 |
| formal-2 | 1.0080 | 1.0189 | 是 |
| formal-3 | 1.0197 | 1.0430 | 否 |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 | formal-1 | formal-2 | formal-3 |
| --- | --- | --- | --- | --- |
| cista:read_sum | 1.05 | 0.835 / 0.847 通过 | 0.825 / 0.838 通过 | 0.821 / 0.836 通过 |
| cista:mutate | 1.05 | 0.711 / 0.718 通过 | 0.712 / 0.723 通过 | 0.728 / 0.750 通过 |
| cista:emit_after | 0.2 | 0.147 / 0.150 通过 | 0.145 / 0.147 通过 | 0.151 / 0.154 通过 |
| flatbuffers:read_sum | 1.05 | 0.650 / 0.660 通过 | 0.633 / 0.643 通过 | 0.641 / 0.656 通过 |
| flatbuffers:mutate | 1.05 | 0.639 / 0.649 通过 | 0.616 / 0.628 通过 | 0.634 / 0.655 通过 |
| flatbuffers:emit_after | 0.2 | 0.139 / 0.141 通过 | 0.137 / 0.138 通过 | 0.141 / 0.144 通过 |
| capnproto:read_sum | 1.05 | 0.205 / 0.208 通过 | 0.208 / 0.211 通过 | 0.221 / 0.225 通过 |
| capnproto:mutate | 1.05 | 0.249 / 0.252 通过 | 0.252 / 0.255 通过 | 0.259 / 0.266 通过 |
| capnproto:emit_after | 0.2 | 0.353 / 0.360 **不通过** | 0.363 / 0.370 **不通过** | 0.386 / 0.396 **不通过** |
| cereal:read_sum | 1.05 | 0.890 / 0.899 通过 | 0.882 / 0.894 通过 | 0.885 / 0.904 通过 |
| cereal:mutate | 1.05 | 1.038 / 1.049 通过 | 1.038 / 1.052 **不通过** | 1.029 / 1.085 **不通过** |
| cereal:emit_after | 0.2 | 0.194 / 0.197 通过 | 0.191 / 0.194 通过 | 0.193 / 0.200 通过 |
| 通过数 head | | 11/12 | 10/12 | 10/12 |
| 通过数 anchor（同批） | | 10/12 | 10/12 | 9/12 |
| 通过数 r3（同批） | | 10/12 | 10/12 | 10/12 |
| 8 阶段 A/A（旧规则，只报） | | 无效（cleanup） | 无效（cleanup） | 无效（mutate, retire_modified_state, cleanup） |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |
| formal-1 | 0.2564 [0.2483, 0.2649] | 0.5768 [0.5580, 0.5962] | 0.4257 [0.4119, 0.4399] | 0.4708 [0.4557, 0.4864] |
| formal-2 | 0.2365 [0.2300, 0.2433] | 0.5287 [0.5139, 0.5438] | 0.3955 [0.3851, 0.4062] | 0.4299 [0.4172, 0.4429] |
| formal-3 | 0.2499 [0.2389, 0.2614] | 0.5574 [0.5327, 0.5832] | 0.4329 [0.4171, 0.4493] | 0.4510 [0.4305, 0.4725] |

## 4. 环境

- 开跑条件 formal-1：2026-10-26T13:30:47+00:00 ok=True, quiet_s_required=180.0, waited_s=711.0, load1=1.46, quiet_s=710.9, screen={'WindowServer': 10.6, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[]
- 开跑条件 formal-2：2026-10-26T13:50:59+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.23, quiet_s=241.0, screen={'WindowServer': 2.6, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[]
- 开跑条件 formal-3：2026-10-26T14:02:06+00:00 ok=True, quiet_s_required=180.0, waited_s=4279.4, load1=2.27, quiet_s=4279.4, screen={'WindowServer': 3.9, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[]
- formal-1：2026-10-26T13:42:38+00:00 → 2026-10-26T13:50:59+00:00，load [1.45654296875, 1.70166015625, 1.71875] → [2.0576171875, 1.9365234375, 1.83837890625]，CPU 采样 {'samples': 247, 'other_busy_samples': 24, 'compile_samples': 0, 'other_busy_top': {'mobileassetd': 17, 'duetexpertd': 2, 'opendirectoryd': 2, 'searchpartyd': 1, 'Codex (Renderer)': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.32}
  - 计时期间屏幕采样：0/126 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 11.0, 'UURemoteServer': 0.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/126 次采样，只记录，有效
- formal-2：2026-10-26T13:55:00+00:00 → 2026-10-26T14:02:06+00:00，load [1.22998046875, 1.5537109375, 1.68994140625] → [2.337890625, 1.95703125, 1.814453125]，CPU 采样 {'samples': 209, 'other_busy_samples': 30, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 29, 'searchpartyd': 1, 'duetexpertd': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.37}
  - 计时期间屏幕采样：16/107 次采样超限（14.9%，上限 5%），按进程 {'WindowServer': 16}，最高 pcpu {'WindowServer': 46.7, 'UURemoteServer': 0.0, 'parsecd': 0.1}；远程桌面活动（≥5%）0/107 次采样，只记录，**失效**
- formal-3：2026-10-26T15:13:25+00:00 → 2026-10-26T15:21:09+00:00，load [2.26806640625, 1.923828125, 1.8505859375] → [2.2744140625, 2.07275390625, 1.9208984375]，CPU 采样 {'samples': 228, 'other_busy_samples': 89, 'compile_samples': 0, 'other_busy_top': {'WindowServer': 87, 'com.apple.AmbientDisplayAgent': 1, 'WallpaperAerialsExtension': 1}, 'chrome_busy_mean': 0, 'chrome_busy_max': 0, 'load1_max': 2.47}
  - 计时期间屏幕采样：46/117 次采样超限（39.3%，上限 5%），按进程 {'WindowServer': 46}，最高 pcpu {'WindowServer': 45.5, 'UURemoteServer': 0.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/117 次采样，只记录，**失效**
- formal-1 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.056，mutate 0.055，emit_after 0.085；mutate 按副本 0.9967 / 0.9959 / 1.0023
- formal-2 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.069，mutate 0.084，emit_after 0.084；mutate 按副本 1.0094 / 1.0060 / 0.9922
- formal-3 A/A 逐轮 log 比值 sd（噪声；历史安静批次 mutate 约 0.06–0.08）：read_sum 0.098，mutate 0.151，emit_after 0.119；mutate 按副本 0.9759 / 0.9805 / 0.9954
- host macOS-26.3-arm64-arm-64bit；冻结 2026-10-26T13:30:47+00:00
- 二进制 sha256：head `13793b33f19ed144…`；head_aa `13793b33f19ed144…`；anchor `0492f332857b93f0…`；r3 `6d4a33ba1f8547dc…`；cista `13bf74996c1723ce…`；flatbuffers `684f7be86bd7b628…`；capnproto `5a7625c96f0a3e7b…`；cereal `c897ba0d1a11a0e8…`
- SLG 树 bench/slg of f8eebf4 (v3 tree, git tree ffd88a16), used for every point（sha256 `2a010b04d53b4049727c4061b9ac750f26ff55bff2bb56da78a3f65c1c91081e`），slg_head `8ee1d0a98cd94164…`，slg_anch `5da23b484d7648bc…`
- 准备检查：gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0；gate_sml_ref exit 0；slg_verify_reuse_ref exit 0；slg_verify_default_ref exit 0
