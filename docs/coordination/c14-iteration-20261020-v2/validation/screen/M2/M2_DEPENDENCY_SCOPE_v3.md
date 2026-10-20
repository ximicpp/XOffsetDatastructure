# M2 screen metadata v3：source-ready，未准入执行

新入口仅为本目录 `M2_SOURCE_READY_v3.json` → `M2_REQUEST_DRAFT_v3.json`、`M2_TOOL_MANIFEST_v3.json`、`M2_BARE_PROFILE_OBSERVATION_v3.json` 与 `M2_RESOURCE_CAPTURE_PLAN_v3.json`。没有运行旧 publisher；没有覆写共享 CURRENT_BARE_PROFILE_OBSERVATION、SOURCE_READY_v2、旧M2 metadata、R1工具/输入/freeze/144原始证据或最终disposition。新 producer `tools/.../screen/m2/prepare_metadata_v3.py` 用独占写入和路径检查，仅能写本目录新 `M2_*_v3.json` 文件。

现有 M2 九份执行文件无需修改。`screen.py` 保持59,817B / SHA256 `9dc7c7808c3cb8e5fc9c77ed5ed4cf34e6f51b82a5ca490c6751ba17a7b07aaf`；runtime manifest与v2逐项一致。新 producer不在运行worker的显式tools依赖表中，作为source-only metadata来源另外列出。旧 `publish_revision_v2.py` 同时遍历R1/M2并写共享索引，因此不能用于本次独立更新。

`screen.py` 仅读自己的campaign/core/protocol拷贝、M2 frozen design、共享已冻结统计/随机次序review、原Documents runner/Python以及未来config明确绑定的source/profile/runtime/input/gate文件；不导入R1 worker或其计时样本。`execute.py`、`audit_job.py`、`finalize.py`和host STATE均为独立M2目录/输出，未来仍须新M2 quiet/host review、freshplan/held handles/token、独立freeze SHA；不得复用R1 PIN、实际guard或PID。原全局queue.lock仍是唯一重负载租约。

实际 own profile为 `/root/xoffset-c14-iteration/20261020/v2/validation/m2-business-v1/profile-normal-v2.json`，SHA `2cd7e9b61d59157e85cbf5e9da98948efc6bc5f6c05fd11bb3d7f28f0bca8846`；对应binary SHA `f155d5fd360eea38f38b99a7ca45c63a4342c1a136d5ad5b260f57f22c83710e`。effective argv包含裸-O3/-DNDEBUG与P2996 flags，无-UNDEBUG/SAN/diagnostic/forced include；driver与原control逐字节同，adapter仅允许library_id/source_content_sha256两literal差分。actual compiled154、production14、compiled-source binding与profile14 digest全部一致，主头SHA `3e347612c1a62299bb910c68dd6b1411663f3165e18e298a51e7ba2e062c201f`，library为 `xoffset-c14-m2-exact-owns-v1`。legacy commit仍仅benchmark base metadata。无需额外裸构建。

已完成 own full73+73、SML80+80、V1/AB/C只作为精确现有事实引用。**十一项typed gate在v3中全部保持pending**，后续须逐项绑定M2真实完整结果、独立机制/source/cost接受与最终package/shared-runtime/lifecycle证明；未借R1通过，不以仅有裸profile或static4→1代替机制准入。

资源capture方案只复用 parent 已批准的同一WSL staged wire/fixed-M政策，已重核原/副本九组byte IDs；计入staging1.275457023s和retained62,218,240B。proposal最多control/candidate各一次完整裸run、child20/outer120、0 compile/0 warmup/0 retry/0推断样本。config、fresh M2 resource/power观察、exact request与独立capture admission均未生成。已有own functional M完整过程0.417081838s仅作为既有收据记录，不代表最终screen投影或比较收益；不能用R1资源run、C诊断/replay或旧失败前缀补M2证据。

未来M2正式统计仍为72pair/144child、公开seed平衡随机AB/BA36、主mutate、原八阶段+read_sum/total、原4主/36辅助族/df71/t常数；任何零/undefined阻止晋升，九辅助任一下界>1确认回退即否决，无删点/epsilon/替换/补样。v3 metrics/primary/auxiliary直接取M2 frozen design，未重新执行任何统计样本。

本次仅AST/既有文件hash/metadata一致性检查：0 compile、0 native/capture、0 queue、0 host plan/pause、0 freeze。共享两份旧索引/观察与选定R1冻结输入及九份worker前后hash相同；R1 raw144不重扫，写入隔离由本次明确M2-only路径保证。交付后停止编辑，等待独立审查。
