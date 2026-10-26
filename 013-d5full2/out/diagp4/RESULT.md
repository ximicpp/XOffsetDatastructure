# L2 Mac 门禁批次 2026-10-26-diagp4-737f43c

- head `737f43c0b830dc0a9edc350b86d28b17db278778`（DIAG_P4 head = p2 = p1 +P4 (737f43c, tree = c47680cc)）；anchor `8cb6221304c90c1df7387b560e9c855a7a78861f`（DIAG_P4 anchor = p1 = f8eebf4 +#87 D5 (8cb6221, tree = #87 0e4c326d)）
- diagnostic, report only: P4 alone (p2 over p1), one SLG group for the stage table; the formal part is not timed
- **结论：fail**。正式部分 incomplete（合并 0 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `f16451c0aaa6d4085753f864db7eaa3f71b6f0fc1fe04f1071af01b99ed366b8`；mac_gate.py `3cc49a962d4611eadbc17f83ed3bdfe501fe7eb6be023eb3ccab90192b4c5911`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

## 1. 新口径：head÷anchor（门禁）

逐周期 log 比值，各批周期合并；单侧 95% 上界，Bonferroni k=3。三项最大上界 ≤1.02 通过，1.02–1.03 待定（诊断后加跑一批、三批合并再判，>1.02 即失败），>1.03 失败。A/A（head_aa÷head，同一统计量）每项双侧区间含 1 且偏差 ≤1%，否则无效。

| 范围 | read_sum | mutate | emit_after | 最大上界 | 判定 | A/A read_sum / mutate / emit_after | A/A |
| --- | --- | --- | --- | --- | --- | --- | --- |

区间列为 几何均值 [双侧下界, 单侧上界]。

逐周期 head÷anchor 比值的分位数（只报，看重尾是否由少数周期造成）：

| 范围 | read_sum p10 / 中位数 / p90 | mutate p10 / 中位数 / p90 | emit_after p10 / 中位数 / p90 |
| --- | --- | --- | --- |

### head÷anchor、head÷r3 其余指标（只报，双侧 95% k=12）

| 范围 | load | read_sum | mutate | emit_after | reload | total |
| --- | --- | --- | --- | --- | --- | --- |

head÷r3 是 G4 的数据（R3 冻结二进制 df876f6c），G4 判定归「性能优化」。

### 旧 norm 项（#30 等合并判据）

cereal:mutate norm head÷anchor（同轮里 cereal 约掉，等于 head÷anchor 的 mutate）。判据：合并后的几何均值 ≤1.016；单侧 95% 上界（k=2）只报。

| 范围 | 几何均值 | 单侧上界 | 几何均值 ≤门槛 |
| --- | --- | --- | --- |

## 2. 旧口径：ROUND3 12 项（照原样，逐批）

head÷对手，联合单侧 95% 上界 k=12。

| 项 | 门槛 |  |
| --- | --- |
| cista:read_sum | 1.05 |  |
| cista:mutate | 1.05 |  |
| cista:emit_after | 0.2 |  |
| flatbuffers:read_sum | 1.05 |  |
| flatbuffers:mutate | 1.05 |  |
| flatbuffers:emit_after | 0.2 |  |
| capnproto:read_sum | 1.05 |  |
| capnproto:mutate | 1.05 |  |
| capnproto:emit_after | 0.2 |  |
| cereal:read_sum | 1.05 |  |
| cereal:mutate | 1.05 |  |
| cereal:emit_after | 0.2 |  |
| 通过数 head | |  |
| 通过数 anchor（同批） | |  |
| 通过数 r3（同批） | |  |
| 8 阶段 A/A（旧规则，只报） | |  |

reload 辅助比值（head÷对手，双侧 k=8）：

| 批 | cista | flatbuffers | capnproto | cereal |
| --- | --- | --- | --- | --- |

## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：fail**（A、B、C 最大上界 1.0326）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9494 [0.9330, 0.9661] | 是 |
| B | 0.9695 [0.9556, 0.9837] | 是 |
| C | 1.0098 [0.9876, 1.0326] | **否** |
| D | 1.0250 [1.0084, 1.0418] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9494 [0.9330, 0.9661] / 0.9494 [0.9336, 0.9655] | 0.9695 [0.9556, 0.9837] / 0.9695 [0.9571, 0.9821] | 1.0098 [0.9876, 1.0326] / 1.0098 [0.9940, 1.0259] | 1.0250 [1.0084, 1.0418] / 1.0250 [1.0119, 1.0382] | head A 0.988 B 0.982 C 0.969 D 1.013；anchor A 0.994 B 0.994 C 0.975 D 1.017 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9494 [0.9179, 0.9821] | 0.9695 [0.9265, 1.0145] | 1.0098 [0.9543, 1.0686] | 1.0250 [0.9829, 1.0689] |
| 合并 | 0.9494 [0.9179, 0.9821] | 0.9695 [0.9265, 1.0145] | 1.0098 [0.9543, 1.0686] | 1.0250 [0.9829, 1.0689] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.241 / 0.244 (0.255) | 0.088 / 0.090 (0.093) | 0.313 / 0.319 (0.327) |
| B | 0.152 / 0.155 (0.160) | 0.078 / 0.079 (0.082) | 0.196 / 0.200 (0.205) |
| C | 0.105 / 0.108 (0.113) | 0.064 / 0.065 (0.069) | 0.242 / 0.250 (0.262) |
| D | 0.353 / 0.358 (0.387) | 0.539 / 0.550 (0.592) | 1.420 / 1.444 (1.490) |

## 4. 环境

- 开跑条件 slg：2026-10-26T18:07:15+00:00 ok=True, quiet_s_required=180.0, waited_s=241.0, load1=1.01, quiet_s=241.0, screen={'WindowServer': 1.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- slg 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 3.5, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/15 次采样，只记录；WindowServer ≥20% 0/15 次采样，有效
- host macOS-26.3-arm64-arm-64bit；冻结 2026-10-26T18:07:15+00:00
- 二进制 sha256：head `3ba5652fe506806f…`；head_aa `3ba5652fe506806f…`；anchor `3c08cd457c9be5d5…`；r3 `6d4a33ba1f8547dc…`；cista `13bf74996c1723ce…`；flatbuffers `684f7be86bd7b628…`；capnproto `5a7625c96f0a3e7b…`；cereal `c897ba0d1a11a0e8…`
- SLG 树 bench/slg of f8eebf4 (v3 tree, git tree ffd88a16), used for every point（sha256 `2a010b04d53b4049727c4061b9ac750f26ff55bff2bb56da78a3f65c1c91081e`），slg_head `564d14b029da187d…`，slg_anch `afad685321ab6337…`
- 准备检查：gate_sml_p2 exit 0；slg_verify_reuse_p2 exit 0；slg_verify_default_p2 exit 0；gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0
