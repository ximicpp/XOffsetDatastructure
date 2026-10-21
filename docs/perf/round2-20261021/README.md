# 第二轮性能优化存档（2026-10-21）

本目录只存档，不改变默认实现：默认仍为 W9a（`xoffsetdatastructure.hpp` `a1593020…`、`xoffset_virtual_memory.hpp` `5ce20e40…`）。

- `ROUND2_REPORT_20261021.md`：第二轮收尾报告（诊断、实现、筛选结果与否决原因）。
- `HISTORY_TABLE_20261021.md`：各代 XOffset 性能对照（同机数据 + 历史注释）。
- `PLAN_20261020.md`：审定后的总计划。
- `MUTATE_ANALYSIS_20261021.md`：修改阶段时间分析（逐阶段计时与剖析）。
- B3（push_back 容量内快路径）未入代码：前提“capacity_>0 ⇒ 已绑定且 arena 非空”被两个公开 API 反例推翻（对有存储的 vector 调 `bind(allocator_ref{})`；绑定到 `initialize_allocator_state(s, nullptr)` 创建的 state）。`beta-B3.patch` 只含固定当前首错误的测试。
- E1（输出前分段 `madvise`）按用户 Q1(b) 视为预触页，关闭；探针源码与数据在 `diagnostics/`。
- `candidates/`：候选补丁（均相对 W9a，可在 W9a 源树上 `patch -p1` 应用）与证明：
  - `w9a-vac.patch`：V-A（8 字节粒度位图，不对齐回退逐字节重校）+ V-C（合并范围检查），含差分测试。
  - `w9a-vb.patch`：V-B（校验遍历软件预取，`XOFFSET_VALIDATION_PREFETCH_DISTANCE`，默认 8；筛选用 4）。
  - `w9a-alpha.patch`（B1+B4+B5）、`alpha-B1/B4/B5.patch`、`w9a-alpha-PROOF.md`。
  - `w9a-beta.patch`（B2+A1+B6+B3 测试）、`beta-*.patch`、`w9a-beta-PROOF.md`。
- `screens/`：预注册筛选的冻结文件、原始数据、结果与脚本。
- `diagnostics/`：P1 探针与 31 轮探索数据。

所有候选都通过了功能门禁并与 W9a 逐字节一致，但均未通过预注册筛选，详见报告。
