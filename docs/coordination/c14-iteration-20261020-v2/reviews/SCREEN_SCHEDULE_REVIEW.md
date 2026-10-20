# R1 / M2 72对 schedule 独立元数据审查

审查日期：2026-10-20（Asia/Shanghai）；审查者 `v2_stats_review`。仅读取新设计及生成器，使用独立轻量 Python 重建次序、检查元数据和 SHA256；未调用生成器、编译、运行 native 或采样，未改写旧窗口。

**结论：下列精确版本的 seed、生成算法、完整配对次序和继承统计政策通过元数据审查。它们仍是 design-only，不能据此启动 screen；实际 worker、完整门禁、资源、host 与 freeze 准入均未由本次审查通过。**

## 固定设计及身份

| 角色 | R1 | M2 |
|---|---|---|
| 候选 | `r1-canonical-hotcold-v1` | `m2-exact-owns-v1` |
| 主目标 | `read_sum` | `mutate` |
| seed | `xoffset/c14-iteration-20261020-v2/screen/R1/read_sum/72pairs/v1` | `xoffset/c14-iteration-20261020-v2/screen/M2/mutate/72pairs/v1` |
| 设计文件 | `core/SCREEN_DESIGN_R1.json` | `core/SCREEN_DESIGN_M2.json` |
| bytes | 99923 | 99893 |
| 设计文件 SHA256 | `3c835b01d1b84dbd06a28d0660a932fc97b4148f68bc71703a674f146309410d` | `9b6737a6a0d4a1962c3183e4da995fb60ef1d9c0e15f2f158b63bd49bbf31151` |
| schedule SHA256 | `6fa34c0576a80a64557d37ec780bf396a7ffdbe042e506c4208cab8bc428bdb7` | `e8313c5e7639f5890c11ce4135ef5dd31122074373acd1eb56990dba277bb54f` |

生成器 `tools/c14_iteration_20261020_v2/core/prepare_screen_design.py` 为10522 bytes、SHA256 `2c271647f82898a0b4dc281e785fef831e1e636afe6b926d1542cf66646efeda`，与两个设计自带引用一致。设计记录元数据生成 runtime CPython3.11.7，明确不冒充最终 worker/runtime 身份。

两设计引用此前统计预审的9352 bytes/SHA256 `20ffbb9076c5bc4b9dedb6f36b04f7cff210c325fb1edbb007e26e99130692de`，与磁盘文件一致。本次不重做统计政策预审或154/14源码审查。

## 独立核验结果

- 从36个 AB、随后36个 BA 开始，i从71递减至1，使用 `SHA256(UTF8(seed)||00||uint64be(counter))`，counter从0每次递增；以 `x < floor(2^256/m)*m` 拒绝域外整数后取 `j=x%m` 并交换。生成器的拒绝操作只作用于确定性整数范围，不是执行或采样重试。
- 独立实现使用 `limit=2^256-(2^256 mod m)` 重建，两个设计各71次 digest，与设计的 `hash_draws` 和完整72项次序逐项相同。没有导入或执行原生成器。
- 每个设计恰有72个唯一 pair ID `p000..p071`，索引连续0..71；AB36对、BA36对。144个 child ordinal 连续0..143，每pair两个相邻ordinal，within_pair为0、1，pair引用与符号次序全部一致。
- 每个pair恰含一次control和一次candidate；A始终为独立 C14 control，B为对应候选，各72个child。统计方向不能改成 second/first。
- 对 `{pairs,children}` 按已声明 UTF8 JSON/sorted keys/compact separators/ensure_ascii=false/无尾换行重新编码，两项 schedule SHA256 与设计完全一致。
- 原四主/36辅助族、n72/df71、不回收M1失败与L1取消位置均保留。R1九辅助为八阶段加total；M2九辅助为除mutate外七阶段加read_sum、total。derived先在child内求和，native.total_ns另存。
- 合法零原样保留；任一指标无定义（含辅助）则不晋升；晋升还要求主上界严格<1、无辅助下界>1及全部事前工程/正确性/资源/恢复/审计门禁。不得加epsilon、删点、补样、换检验或合并R1/M2样本。临界值引用已审原证据，最终worker须绑定既有两个常数。

生成器对已存在设计采用拒绝覆盖，设计自述须保留版本；`READY.json` 明确它们是 metadata-only，两个设计均记 `screen_admitted=false`、`QF_admitted=false`，本次可见v2目录中没有实际screen freeze或样本文件。因此该完整次序已在准入/采样之前具名固定。本审查只证明当前设计事实，不据元数据声明推断所有系统执行历史；首个计时child前仍须将上述精确文件及digest纳入外部pin的实际freeze，之后禁止改seed、次序或替代样本。

## 不属于本次通过范围

尚未审查新的实际 worker 是否执行该 schedule、是否正确计算所有统计项，以及其运行时、二进制、输入、完整功能门禁、实际全进程预算/空间、原租约、host quiet和恢复协议。固定平衡随机顺序也不证明配对 log 值独立。

实际 freeze 独立终审必须逐项闭合上述条件并核对本次精确文件；任何设计修订另行版本化和审查。本次不批准screen/Q32/F64，不重开M1，也不建立R1/M2或组合性能收益。
