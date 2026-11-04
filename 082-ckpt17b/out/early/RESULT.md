# L2 Mac 门禁批次 2026-11-04-ckpt17b-8a4bca9

- head `8a4bca988938d32a1f84b46b82881b76c0f59bfe`（CKPT17b head = PR #214 8a4bca98 (XMap/XSet: append a key past the last one without a search, 8a9286c1, merged with next_cpp26 49ce3c68), build-r3d params）；anchor `49ce3c68fa60ff9b531f1fa482fc4fb4b8e65479`（next_cpp26 49ce3c68 (#212; mainline merged into PR #214), build-r3d params）
- CKPT17b: L2 gate of PR #214 (class L, XMap/XSet sequential append fast path) as merged with next_cpp26 (8a4bca98 vs 49ce3c68, merge result vs mainline as GATE_D4), merge tier of TEST_TIERS (core gate + G9R: random-key insert cost, reloc set/map.insert); anchor 49ce3c68; each point built with its own bench/slg; one deferred bisection point 8a9286c1 (report only)
- **结论：pass**。正式部分 n/a（合并 0 批），SLG pass。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `def1642e999387ae03d2cf806cf295c4cfeb2e103235186e0a8717db27abef3b`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

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

**正式部分不适用**：head 与 anchor 的 sc_xoffset 是同一个二进制（sha256 `b325298ef9e33efe5ee1f2fd80f411997c7f5266c084a076af182d3157b022ca`），不计时，不参与判定。


## 3. SLG M（head、anchor 交替进程 ABBA×3，SLG_REUSE=1，四家；1 组合并）

### 新口径 head÷anchor

每个进程的值 = 第 1–40 周期 log(xoffset 场景合计 ÷ 同周期三家几何均值) 的均值；Welch t，双侧 95%，k=4，各组进程合并。A、B、C 最大上界 ≤1.02 通过；1.02–1.03 待定，加跑一组 ABBA×3、两组合并后 ≤1.02 通过、否则失败；>1.03 失败。D 只报。

**SLG 判定：pass**（A、B、C 最大上界 1.0151）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9936 [0.9757, 1.0119] | 是 |
| B | 0.9990 [0.9832, 1.0151] | 是 |
| C | 0.9939 [0.9751, 1.0131] | 是 |
| D | 0.9948 [0.9690, 1.0213] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9936 [0.9757, 1.0119] / 0.9936 [0.9770, 1.0105] | 0.9990 [0.9832, 1.0151] / 0.9990 [0.9870, 1.0113] | 0.9939 [0.9751, 1.0131] / 0.9939 [0.9758, 1.0125] | 0.9948 [0.9690, 1.0213] / 0.9948 [0.9693, 1.0209] | head A 0.981 B 0.985 C 1.006 D 0.995；anchor A 0.987 B 0.992 C 0.985 D 1.031 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9936 [0.9349, 1.0560] | 0.9990 [0.9795, 1.0189] | 0.9939 [0.9482, 1.0419] | 0.9948 [0.9184, 1.0776] |
| 合并 | 0.9936 [0.9349, 1.0560] | 0.9990 [0.9795, 1.0189] | 0.9939 [0.9482, 1.0419] | 0.9948 [0.9184, 1.0776] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.237 / 0.240 (0.257) | 0.096 / 0.100 (0.107) | 0.381 / 0.387 (0.408) |
| B | 0.162 / 0.165 (0.166) | 0.077 / 0.079 (0.080) | 0.293 / 0.298 (0.299) |
| C | 0.119 / 0.121 (0.125) | 0.070 / 0.072 (0.074) | 0.355 / 0.365 (0.370) |
| D | 0.250 / 0.254 (0.280) | 0.349 / 0.358 (0.396) | 0.930 / 0.957 (0.997) |

## 4. 环境

- 开跑条件 slg：2026-11-04T10:24:17+00:00 ok=True, quiet_s_required=180.0, waited_s=241.2, load1=1.68, quiet_s=241.1, screen={'WindowServer': 4.8, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- slg 计时期间屏幕采样：0/13 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 4.9, 'UURemoteServer': 0.1, 'parsecd': 0.0}；远程桌面活动（≥5%）0/13 次采样，只记录；WindowServer ≥20% 0/13 次采样，有效
- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-04T10:24:17+00:00
- 二进制 sha256：head `b325298ef9e33efe…`；head_aa `b325298ef9e33efe…`；anchor `b325298ef9e33efe…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 55ba4e84, anchor 55ba4e84; bisection points their own)（sha256 `efa09929732a456b5b8a9169ee03fbb3574f975bc54a60a8e4db024bf9c544e3`），slg_head `14ba77d3b709d31d…`，slg_anch `4d8a2b8c6d246dc6…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
