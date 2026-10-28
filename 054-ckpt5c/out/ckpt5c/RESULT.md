# L2 Mac 门禁批次 2026-10-28-ckpt5c-70a07a0

- head `70a07a059cd82417bb0bb0a69ca9dea91c3b21ff`（CKPT5c head = next_cpp26 70a07a05 = CKPT5 head (3.2.0 release checkpoint; since CKPT4 af601ea: S #167 #162 #169 #172 #177, 0 the rest), build-r3d params）；anchor `9ef5ce75d72a172660a97d9fb4e188da3945f6c6`（v3.1.0 9ef5ce75 (release 3.1.0, library = INT4 head e913d22), build-r3d params）
- CKPT5c: one more SLG group of CKPT5/CKPT5b (same binaries); the verdict is the three runs pooled in the cloud (POOL_RULE.md); this directory's own SLG line is report only
- **结论：fail**。正式部分 incomplete（合并 0 批），SLG fail。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `92cffd3e4b197dd59dc5149d05e68874a444cd803f81b70aad327718b43d8c60`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

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

**SLG 判定：fail**（A、B、C 最大上界 1.0318）

| 场景 | head÷anchor（合并） | ≤1.02 |
| --- | --- | --- |
| A | 0.9955 [0.9794, 1.0119] | 是 |
| B | 1.0006 [0.9849, 1.0164] | 是 |
| C | 1.0070 [0.9827, 1.0318] | **否** |
| D | 0.8057 [0.7943, 0.8172] | 只报 |

按组（只报）；趋势调整 = 每组内对进程序号做线性回归后的 head÷anchor，双侧 k=4，只用于诊断会话内漂移，不参与判定：

| 组 | A Welch / 趋势调整 | B Welch / 趋势调整 | C Welch / 趋势调整 | D Welch / 趋势调整 | 漂移（最后÷第一个进程）|
| --- | --- | --- | --- | --- | --- |
| slg | 0.9955 [0.9794, 1.0119] / 0.9955 [0.9820, 1.0092] | 1.0006 [0.9849, 1.0164] / 1.0006 [0.9893, 1.0119] | 1.0070 [0.9827, 1.0318] / 1.0070 [0.9885, 1.0258] | 0.8057 [0.7943, 0.8172] / 0.8057 [0.7936, 0.8179] | head A 0.992 B 0.982 C 0.983 D 1.007；anchor A 0.989 B 0.984 C 0.978 D 0.997 |

ABBA 配对（只报，不参与判定）：每 4 个连续进程一块，块内 head 均值减 anchor 均值，块间 t，双侧 95% k=4。点估计与 Welch 相同；每组只有 3 块，自由度 2，区间通常更宽：

| 组 | A | B | C | D |
| --- | --- | --- | --- | --- |
| slg | 0.9955 [0.9901, 1.0010] | 1.0006 [0.9803, 1.0212] | 1.0070 [0.9827, 1.0318] | 0.8057 [0.7780, 0.8343] |
| 合并 | 0.9955 [0.9901, 1.0010] | 1.0006 [0.9803, 1.0212] | 1.0070 [0.9827, 1.0318] | 0.8057 [0.7780, 0.8343] |

进程数 head 6、anchor 6。

### 旧口径 G1–G3（head 进程合并）

每个 head 进程一个几何均值比值，进程间 t，单侧 95%，k=12；括号里是第一个 head 进程按 analyze.py 逐周期的上界。A、B、C ≤0.50 为达标。

| 场景 | cereal | protobuf | flatbuffers |
| --- | --- | --- | --- |
| A | 0.242 / 0.247 (0.258) | 0.098 / 0.099 (0.105) | 0.329 / 0.336 (0.348) |
| B | 0.132 / 0.135 (0.136) | 0.069 / 0.070 (0.071) | 0.179 / 0.182 (0.184) |
| C | 0.103 / 0.107 (0.108) | 0.064 / 0.066 (0.067) | 0.255 / 0.262 (0.268) |
| D | 0.226 / 0.231 (0.252) | 0.346 / 0.352 (0.383) | 0.936 / 0.954 (0.988) |

## 4. 环境

- 开跑条件 slg：2026-10-28T20:04:36+00:00 ok=True, quiet_s_required=180.0, waited_s=241.1, load1=1.59, quiet_s=241.0, screen={'WindowServer': 1.2, 'UURemoteServer': 0.0, 'parsecd': 0.0}, remote_active=[], screen_over=[], screen_gated=False
- slg 计时期间屏幕采样：0/15 次采样超限（0.0%，上限 5%），按进程 无，最高 pcpu {'WindowServer': 2.5, 'UURemoteServer': 0.0, 'parsecd': 0.0}；远程桌面活动（≥5%）0/15 次采样，只记录；WindowServer ≥20% 0/15 次采样，有效
- host macOS-27.2-arm64-arm-64bit；冻结 2026-10-28T20:04:36+00:00
- 二进制 sha256：head `6eece9b8d14ba644…`；head_aa `6eece9b8d14ba644…`；anchor `934e7624616d9128…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 66ea9df7, anchor 0dd0a31e)（sha256 `a3a2c95f3b799021db93db5d4966039c95a187f44649488fb01882bbd3e2825c`），slg_head `ff454946684a14b2…`，slg_anch `d97b078c72258a3b…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
