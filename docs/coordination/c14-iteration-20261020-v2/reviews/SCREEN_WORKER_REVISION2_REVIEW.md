# Screen 发布链 revision 2 窄复审

日期：2026-10-20（Asia/Shanghai）；审查者 `v2_stats_review`。本次只复核此前 `SCREEN_WORKER_REVIEW.md` 的两项发布阻断和新引用身份；未重做统计函数测试、运行编译/native、采样或读取capture时间作收益推断。

**结论：此前两项阻断均已修复。实际worker代码未变，新发布链通过元数据复审；实际window freeze、资源、host及执行准入仍归root独立终审。**

| 新文件 | bytes | SHA256 |
|---|---:|---|
| `validation/screen/SOURCE_READY_v2.json` | 5938 | `0643b3d884ae5711b2ddba0850130aad8603a0d3ca4b1062c635eb791d5de1f4` |
| `validation/screen/R1/REQUEST_DRAFT_v2.json` | 76054 | `b648308c3a8592fcfdd8f84f042862667a1e18cab39fe04736a26e6680987fcd` |
| `validation/screen/M2/REQUEST_DRAFT_v2.json` | 63330 | `5c94b0ddd3cf9522460b82355bd695ee6a9f330a64230629619b53927d3c76dc` |
| `validation/screen/R1/TOOL_MANIFEST_v2.json` | 4397 | `d9e560b4125ee640fdd86d9ec6433df91e1fa19130c8e508c1ac3f2294a14b12` |
| `validation/screen/M2/TOOL_MANIFEST_v2.json` | 4390 | `47d35ef36127c6ce8ee8e7252993b30a018f3cac23ad2bd7e2552e6a371af866` |

路径均相对 `docs/coordination/c14-iteration-20261020-v2/`。

核验结果：

- 新 SOURCE_READY 的两份request/manifest引用均与实际bytes/SHA一致；每份request的tool_manifest引用等于该新manifest引用；各manifest列出的9个实际工具逐一核对一致。
- R1 worker仍为59903 bytes/SHA `0ae713213e7f5cfeba5cba8cd7dded155fe44a3114fbcecabf30f23f6f651a21`；M2仍为59817 bytes/SHA `9dc7c7808c3cb8e5fc9c77ed5ed4cf34e6f51b82a5ca490c6751ba17a7b07aaf`，完全沿用此前实际代码审查结论，没有用旧manifest替代新代码。
- 两request的 `statistics.metrics`、`stages`、`primary`、`auxiliary`直接等于各自已审 design字段；完整randomization结构也与design相同。八阶段恢复为 `load,query_before,mutate,emit_after,retire_modified_state,reload,query_after,cleanup`，再加derived read_sum、total；没有validate/emit/release替代名。
- 原72对/144child、AB36/BA36、df71、四主/36辅助及临界值 `2.289930854179058`、`3.327935133019535`均保留；warmup/retry/replacement为零。zero与晋升文本未放宽，实际worker继续采用此前已测的“任一undefined不晋升、辅助下界>1否决”合取。
- 已核对新ready所引用的旧SOURCE_READY、两候选旧REQUEST_DRAFT/TOOL_MANIFEST仍为原bytes/SHA；旧发布失败事实保留，新revision没有覆盖或追认旧链通过。

R1另有独立准入的 `07ebe1dcc9933faff07a3fa438a4a484c1026be1db536043acc310781cb2bdfa` 资源capture请求。本次不审其执行结果，不把capture纳入144项screen或从其阶段时间解释收益；其实际资源账本由另行准入和审查负责。

本复审仅关闭两项发布阻断，不批准screen/Q32/F64，不证明R1/M2或组合性能达标。任何实际worker、schedule或新发布文件后续改变，都需按新身份复核受影响范围。
