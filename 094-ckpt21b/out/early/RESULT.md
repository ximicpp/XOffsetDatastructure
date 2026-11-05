# L2 Mac 门禁批次 2026-11-05-ckpt21b-bf70e99

- head `bf70e9986e54d927686ac372e65c706b071a86b7`（CKPT21b head = PR #228 bf70e998 (trust tier P1: XReceiver admit, xoffset_receiver.hpp + XBuffer private admit loads; cd19776 + 1ae7aed merged with next_cpp26 51a4815b), build-r3d params）；anchor `51a4815b7802ec125e2752847b69fd2a87ab7259`（next_cpp26 51a4815b (3.3.0 release merged), build-r3d params）
- CKPT21b: L2 gate of PR #228 (class L, trust tier P1: new xoffset_receiver.hpp, private admit loads in xoffsetdatastructure.hpp; the benches do not call them) as merged with next_cpp26 51a4815b (bf70e998 vs 51a4815b, merge result vs mainline); merge tier of TEST_TIERS (core gate + G9R); each point built with its own bench/slg (both 23e86624); one deferred bisection point 1ae7aedd (#228 before the merge, report only); no report-only step
- **结论：n/a (identical binaries)**。正式部分 n/a（合并 0 批），SLG n/a。
- 环境失效规则（frozen by 性能优化 2026-10-23 14:51Z, not re…）：计时期间每 4 s 采一次屏幕负载，超限采样占比 >5% 即失效，同目录最多补跑一次。
- 8 个二进制在磁盘上用等长文件名（head→head, head_aa→hdaa, anchor→anch, r3→r3fz, cista→cist, flatbuffers→flat, capnproto→capn, cereal→cere）；raw 里的标签不变。每个二进制和 wire 各存 3 份独立文件，第 r 轮用第 r % 3 份（W0b 等长文件名后 A/A mutate 仍偏 1.2%，历次冻结之间在 -0.7% 到 +1.2% 之间变化，像是按文件的效应，轮换副本把它平均掉）。
- FREEZE.json sha256 `8d9c8c6bee78e46efd3fd584ef855ab34231ffae92ea3ca9647f0b0334e7182c`；mac_gate.py `fad50bd8854112ed351b2d21816d18e5e8553d1eca353afeb9dcc4481da212d5`；xc_ab.py `a95160a635e4d59f61cfaa66fdcbd31366a41a2ba645b1f9ddaf6164942abc60`

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

**正式部分不适用**：head 与 anchor 的 sc_xoffset 是同一个二进制（sha256 `8cd9767c692ea0bbf3c111f9f174909534b4f35aaa5a7324f0bca4d16cc8ca5a`），不计时，不参与判定。


## 3. SLG M

**SLG 不适用**：slg_head 与 slg_anch 是同一个二进制（sha256 `e0773338871b29f0d1f4428bae4bd49f113c1930ce65be1fc353c07dd736fca4`），不计时，不参与判定。


## 4. 环境

- host macOS-27.2-arm64-arm-64bit；冻结 2026-11-05T03:00:40+00:00
- 二进制 sha256：head `8cd9767c692ea0bb…`；head_aa `8cd9767c692ea0bb…`；anchor `8cd9767c692ea0bb…`；r3 `328d8872af63ecee…`；cista `1cffe48e6c9b3cc9…`；flatbuffers `51e053ddd6afe165…`；capnproto `e528cb3cbb42b497…`；cereal `4d65fc64843c0c46…`
- SLG 树 each point's own bench/slg (head git tree 23e86624, anchor 23e86624; bisection points their own)（sha256 `c60c7c18d80c929261e1eb847c940c8265ed98df984e3b8d9e97200a0764cf34`），slg_head `e0773338871b29f0…`，slg_anch `e0773338871b29f0…`
- 准备检查：gate_sml_head exit 0；slg_verify_reuse_head exit 0；slg_verify_default_head exit 0；gate_sml_anch exit 0；slg_verify_reuse_anch exit 0；slg_verify_default_anch exit 0
