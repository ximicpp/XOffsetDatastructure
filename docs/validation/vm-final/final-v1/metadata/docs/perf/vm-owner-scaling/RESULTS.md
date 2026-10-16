# 固定请求 owner 测量：唯一窗口与离线补证

2026-10-16。[统筹 D033](../../coordination/vm-followup/owner-measurement-acceptance.json)
已最终验收本轮 owner 研究，交接完成。D030 唯一 78 进程窗口全部执行成功；
D032 离线封包、完整搬迁审计和终态 validation 回执核验均通过，新增原生/构建/
测量窗口全部为 0。
**24 个 condition/phase cell 中 7 个合格，24 个相对 C 的比较中 2 个合格，
其中一个是 C/free 自比。唯一非自比合格项是 A/free 对 C/free；多线程比较均不确定。**
这是当前 F 的单窗、10% 粗粒度描述研究，不作 registry 因果、优化或采用结论。

## 精确执行与工作量

沿用 D027 已验收的原始 F `owner_scaling_timing`（build-v4），没有重编、重跑或
替换样本。每个 allocate/free 阶段固定 8192 请求，control=none：
A/B/C 分别为 8/64/256 owners、1 thread，D/E/F 为 256 owners、2/4/8 threads。
六个新进程 prerun 顺序 ABCDEF；六个测量 block 各按旋转顺序及精确逆序执行，
共 72 个测量进程、每条件 12 样本/六个配对。prerun 保留但不进入统计，也不
预热后续进程内部 registry/Core。四阶段 create/allocate/free/destroy 各自报告。

计时队列 run ID `e883a7e32e3440c1b996d4e77cd1c15a`；客户端时间
2026-10-16 01:49:28.778332–01:49:30.887336（Asia/Shanghai），2.108528333 秒、
退出 0、78/78 完成，清理 live/process-group/zombie 均为空。每 child 30 秒、
整窗 900 秒、证据 512 MiB 上限保持不变。原生 `timing_evidence=false` 未改写；
执行授权与逐 cell 资格由独立 wrapper 记录，不将原生声明改为 true。

## 全部 24 cell 资格

每 cell 的六个配对 `g_b=sqrt(t_b1*t_b2)`，AA、block dispersion 和三锚点漂移
均不大于 1.10 才合格。冻结审计器以整数产品精确判定边界；下表不按舍入值判定。

| 条件 / owners / threads | create | allocate | free | destroy |
|---|---|---|---|---|
| A / 8 / 1 | 不确定 | 合格 | 合格 | 不确定 |
| B / 64 / 1 | 合格 | 合格 | 不确定 | 合格 |
| C / 256 / 1 | 不确定 | 不确定 | 合格 | 不确定 |
| D / 256 / 2 | 合格 | 不确定 | 不确定 | 不确定 |
| E / 256 / 4 | 不确定 | 不确定 | 不确定 | 不确定 |
| F / 256 / 8 | 不确定 | 不确定 | 不确定 | 不确定 |

合格 cell 的批量中位数及对应归一化值如下；不确定 cell 的全部原始时间、检查与
统计继续完整保留在封包 summary，不删除、不替样、不筛异常值。

| Cell | 中位批量 ns | 归一化中位数 | 单位 |
|---|---:|---:|---|
| A/allocate | 115853.612 | 14.142287 | ns/request |
| A/free | 116834.193 | 14.261986 | ns/request |
| B/create | 361144.367 | 5642.880733 | ns/owner |
| B/allocate | 114779.712 | 14.011195 | ns/request |
| B/destroy | 115546.445 | 1805.413195 | ns/owner |
| C/free | 203831.736 | 24.881804 | ns/request |
| D/create | 1125488.214 | 4396.438337 | ns/owner |

## 全部 24 比较资格

参考条件为 C=256 owners/1 thread。每比较仅在 case 与 C 的对应 cell 都合格时
才合格；任何一端不合格均保留为不确定。

| 相对 C 的条件 | create | allocate | free | destroy |
|---|---|---|---|---|
| A | 不确定 | 不确定 | 合格 | 不确定 |
| B | 不确定 | 不确定 | 不确定 | 不确定 |
| C | 不确定 | 不确定 | 合格（自比） | 不确定 |
| D | 不确定 | 不确定 | 不确定 | 不确定 |
| E | 不确定 | 不确定 | 不确定 | 不确定 |
| F | 不确定 | 不确定 | 不确定 | 不确定 |

唯一非自比合格比较 A/free 对 C/free 的六 block 比值中位数为 `0.574711100`，
观察最小/最大为 `0.562113826` / `0.584546044`，六个方向均为 lower。
这只描述本窗主时钟区间的批量差异；C/free 自比比值为 1，不是研究收益。
所有六 block 比值、24 个 comparison（含不确定项）均在
[原始 summary](measurement-approval-supplement-v1/original-window/summary.json)保留。

比值在含边界 `[0.9,1.1]` 内为方向未定，block 方向不一致为 mixed；本窗 min/max
不是置信区间，没有 bootstrap、总体保证、2% 检测或尾延迟推断。owner 数还改变
每 owner 请求数、工作集和 Core 元数据；线程还改变调度、barrier 和记账，不能
将差异单独归因于哈希复杂度或锁。现有 main clock 包含 barrier、调度、指针/计数
记账及相应库工作，排除 thread startup、填充/验证、snapshot、join/output；
Core destroy 不销毁进程终生 registry bucket。它不是 pure registry cost、lookup
latency 或完整应用生命周期。

## 批准字节差异与完整补证

首次归档在批准副本 SHA 校验处退出 1，
`build/vm-owner-measurement/receipts/archive-v1/` 原样保留，原归档目标
`measurement-v1/` 未创建。冻结 collector 将批准对象重新按键排序；两份均 688 B，
严格解析对象相同，但原批准 SHA 与窗口副本不同。

D031/D032 指定的补证没有覆盖原窗口。
[完整封包](measurement-approval-supplement-v1/result.json)明确区分：

- `original-window/`：完整原窗口，批准副本 SHA
  `92058415a840fe800a7e519a3835461c18032f177e9cfe2a2d0e2743ef9d3aa1`。
- `derived-window/`：完整派生副本，唯一差异为 `approval.json` 使用原批准精确字节，
  SHA `0157b0d310d7388729f51a4031c6ca552ca32f9d15982972d7e0c5aebe1d9e12`。
- `differences.json` / `approval-proof.json`：逐项差异与严格等值、旧序列化规则证明。
- `timing-receipt/` 与 `failed-archive-receipt/`：两份完整回执；冻结 proposal、工具、
  producer 绑定和全部 78 份报告同时保留，原始/派生 summary 字节相同。

`derived_is_original_bytes=false`，不能称派生副本与原窗口逐字节相同。完整原始
inventory 首次在本次 D031 补证验证中生成，不冒充 D030 测量前已有的 authority pin。
原始输出、时序、marker、冻结工具/proposal 和失败回执未改写。

补证 validation run ID `f2cfcec9e94741adae7aa5899b891e83`，
2026-10-16 03:20:52.388868–03:20:54.147842（Asia/Shanghai），1.758525208 秒、
退出 0、清理为空。终态 receipt auditor 得到 `audit_passed` 与 `job_status=complete`；
其 request 与封包捕获 request 全等、精确 command 与 D032 一致，run ID 前后一致。
封包内部捕获的 running metadata 由这份外部终态回执闭合，不追改已封存封包。

完整搬迁审计覆盖最终 result/COMPLETE、两个窗口、全部冻结工具和两份回执，
`status=passed`、新增 native/build/window 为 0。封包 payload 698 文件、8,891,188 B
（result/COMPLETE 另计），未触及 512 MiB 预算；没有复制无关历史归档。

## SHA 与交接入口

| 项目 | SHA256 |
|---|---|
| D030 proposal | `8dde7bf6e5536704dc8921de4507564499e8a4e208a4fe0606fe599596268b9a` |
| 原始/派生 summary | `59e192b428848734d14f3f64676481d267f0744697a372c942101442da9d2340` |
| D032 supplement tool | `411d4e6054c18e09cd0a2f0d5caa8e972552376e1918856062941e039dc37698` |
| 最终封包 result（COMPLETE 同值） | `cbaf56a9e4ad79267c90cb2a0c9853d502f62e1cbb0903c8c1ed5492578ca1f7` |
| 完成 validation receipt manifest | `c90fab3be2d8f2f70932a1a876d952c27c5ee248b1438c8d6c6c1af94b4cbeac` |

原始 timing 回执：`build/vm-owner-measurement/receipts/window-v1/`。
补证终态回执：`build/vm-owner-measurement/receipts/approval-supplement-v1/`。
[D032 接受与执行权限](../../coordination/vm-followup/contracts/owner-measurement-v1/supplement-tool-acceptance.json)
与全部精确状态见 [handoff.json](handoff.json)。D033 已接受独立窗口统计、698 文件补证封包与完成回执，本轮 owner 研究
交付完成。原窗口、仅 approval.json 字节不同的派生副本及原失败回执继续完整保留。
最终 G4 由统筹管理；没有新增执行、审计、网络/default 测量或候选/生产采用授权。

D033 验收记录 SHA256：
`d87332b22406e9077ab68a44cb02ba14ac611a8fba78447c6498458bf584eddd`。
