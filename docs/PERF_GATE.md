# 性能回归门禁（指令数，L0）

**指令数只用于拦截回退，计时结论以 Mac 正式批次为准。** 任何"变快"或达标（G1–G4）的说法都不能拿这里的数字作证据。

设计依据：H3 设计 `beta2-20261022/H3_PERF_GATE_DESIGN_20261022.md` 第 5 节。本页是实现说明。

## 1. 它做什么

`.github/workflows/perf-gate.yml` 在 PR 指向 `next_cpp26`（忽略只改 `docs/**`、`*.md` 的 PR）、push 到 `next_cpp26` 和手动触发时运行，GCC 16 和 P2996 Clang 各一个作业，两个都判定：

1. 在同一个作业里导出三份库代码：**anchor**（`bench/perf/ANCHOR`）、**base**（PR 是 merge-base，push 是第一父提交）、**head**。
2. 三份都用 **head 里的基准代码**（手动回放时用所选分支的基准代码）（`bench/slg/`、`bench/formal/`）编译 `slg_bench` 和 `sc_xoffset`。base 或 anchor 用 head 的基准编不过（公开 API 改了）时，改用它自己那一版的基准，标"基准不同"并警告；仍编不过就标"不可比"，作业不失败，但这个 PR 合并前要跑一次 Mac L2。
3. 计数前先查正确性：SLG `--mode verify --scale S`（xoffset 与 cereal 一致，复用协议和逐条协议各一次）；正式 harness 用各自的 `encode` 生成 S、M 输入 wire，`validate` S 和 M。head 任何一项失败，门禁直接失败，不出数字。
4. 在 valgrind callgrind 下数每个阶段的指令数（Ir），`--collect-atstart=no`，只在被测阶段里打开收集。各阶段一个进程，`nproc` 个并行。
5. head 的一个 SLG 阶段和正式 mutate 各多跑一次，两次 Ir 必须逐位相同，否则门禁失败（结果不可信）。
6. 按第 3 节的门槛判定，作业摘要里一张表，JSON 产物 `perf-gate-<编译器>-<head sha>.json`（90 天）。不发 PR 评论。

不需要存基线，不需要 nightly：Ir 是确定的，三份每次现编现测，runner 镜像、valgrind 版本的变化自动抵消；代码不变时 Ir 不变，push 的结果已经覆盖每次合并。

## 2. 测什么

| 来源 | 设置 | 指标名 |
| --- | --- | --- |
| SLG | S 规模，seed 20261021，`SLG_REUSE=1`（口径 c），3 个周期，只收集第 1、2 周期 | 每个计时阶段 `slg.<键>`（如 `slg.A.apply`、`slg.S.ticks_middle`）；场景合计 `slg.A`…`slg.D` = 该场景各阶段之和 |
| 正式 harness | M fixture（`bench/formal/model/reference.py --scale M`，默认 seed） | `formal.load`、`formal.query_before`、`formal.mutate`、`formal.emit_after`、`formal.retire`、`formal.reload`、`formal.query_after`、`formal.cleanup`；`formal.read_sum` = query_before + query_after；`formal.total` 只报 |

SLG 只编 XOffset + cereal（cereal v1.3.2，`ebef1e92`，头文件）。M 规模的 SLG 在 valgrind 下会超出地址空间（512 × 256 MiB 预留），所以用 S。正式 harness 的 M 输入 wire 由每份二进制自己编码并记录 sha256；与 anchor 不同时摘要会写"M 输入 wire 变了"，格式改动要 susu 定。

## 3. 门槛

| 对象 | 警告 | 失败 |
| --- | --- | --- |
| 合计：`slg.A`–`slg.D`，`formal.read_sum`、`mutate`、`emit_after`、`load`、`reload`；head÷base | > 1.003 | > 1.010 |
| 单个阶段，占所在场景（正式为 `formal.total`）Ir ≥ 5%；head÷base | > 1.020 | > 1.050 |
| 占比 < 5% 的阶段，`formal.retire`、`formal.cleanup` | 只报（JSON） | — |
| 合计：head÷anchor，扣除 ledger 已放行的部分 | > 1.010 | > 1.020 |
| 任一合计 head÷base < 0.97 | 标"指令数下降"，提醒要 Mac 数据才能说变快 | — |

占比按 base 和 head 中较大的一边算。`slg.S.ticks_middle`（中间删除压力项）自成一个场景，按阶段门槛判定。

## 4. ledger（有意的回退）

`bench/perf/ledger.json`：

```json
{
  "schema": "xoffset-perf-ledger-v1",
  "entries": [
    {
      "id": "pr26-mutate",
      "pr": 26,
      "metric": "formal.mutate",
      "compiler": "*",
      "max_ratio": 1.03,
      "reason": "换回输出 −23%（emit_after），修改阶段多 2.4% 指令",
      "evidence": "m3v2_RESULT.md；Mac mutate 1.038",
      "approved_by": "susu（项目对话卡片，日期）"
    }
  ]
}
```

- `metric` 可用通配符（`slg.A.*`），`compiler` 是 `*`、`gcc16` 或 `p2996`，`max_ratio` > 1。所有字段必填，`id` 不能重复。
- **head÷base**：只有这个 PR 新加的条目（base 的 ledger 里没有的 `id`）能放行。超过失败门槛但不超过 `max_ratio` 的指标记为"已放行"，摘要里列出条目；超过 `max_ratio` 照样失败。
- **head÷anchor**：head 的 ledger 里所有匹配条目的 `max_ratio` 连乘，作为已放行的部分扣除后再比门槛。
- **批准**：场景合计（`slg.A`–`slg.D`、`formal.*`）≤ 2% 的条目由「性能优化」批准并写进 `approved_by`；超过 2% 或涉及安全与性能取舍的，要 susu 确认，`approved_by` 必须写明 susu，否则门禁直接失败。
- **anchor 前移**：tag 候选的 Mac L2 通过后，开一个只改 `bench/perf/ANCHOR` 的 PR，同时把旧条目移到 `bench/perf/ledger_archive.json`（门禁不读它）。

## 5. 看结果

- 作业摘要：第一行固定是上面的加粗句；然后是判定、失败/警告/说明列表、判定对象和超门槛阶段的表、各场景变化最大的阶段。
- 产物：`perf-gate-<编译器>-<sha>.json`（全部行，含只报的阶段）、`plan.json`、`build-*.json`、`ir-*.json`、构建日志。
- 变红怎么办：先看是哪个阶段；是意外就修；是有意的取舍就写 ledger 条目（见上），> 1% 的放行在合并前还要跑一次 Mac L2。

## 6. 手动触发与回放

Actions → "Perf gate (instruction counts)" → Run workflow，选一个带 `bench/` 的分支（通常是 next_cpp26），输入 `base`、`head`、`anchor`（可选，默认取该分支的 `bench/perf/ANCHOR`）、`compilers`。基准代码取自所选分支，所以可以回放 `bench/` 入库之前的提交，也可以给 Mac 候选预检。回放 GCC 16 支持合入之前的提交时，GCC 一侧不可比，只看 P2996。回放历史提交时把 `anchor` 设成 `base`，否则 head÷anchor 比的是一个更新的提交。

本地（需要 GCC 16 或 P2996 Clang、valgrind、python3）：

```bash
git submodule update --init external/typelayout
git clone https://github.com/USCiLab/cereal.git /tmp/cereal && git -C /tmp/cereal checkout ebef1e92
W=build/perf-gate
python3 tools/perf_gate/perf_gate.py prepare --work $W --base origin/next_cpp26 --head HEAD
python3 tools/perf_gate/perf_gate.py run --work $W --compiler gcc16 --cereal /tmp/cereal/include
python3 tools/perf_gate/test_perf_gate.py   # 判定逻辑自测，不需要编译器
```

`build` 的 `--cxx` 可以是一条命令（例如用 docker 包装的 g++），`--extra-flags` 追加编译选项（宿主 valgrind 跑容器里编的二进制时加 `-static-libstdc++ -static-libgcc`）。

## 7. 基准代码要提供的接口

门禁直接用编译命令构建（不走 CMake），对 `bench/` 的要求：

| 位置 | 要求 |
| --- | --- |
| `bench/slg/bench/slg_bench.cpp` | 入口；包含目录 `include`、`adapters`、`bench`；`-DSLG_WITH_XOFFSET=1 -DSLG_WITH_CEREAL=1` 能编 |
| `bench/slg/bench/runner.hpp` | `-DSLG_IR_COUNT=1` 时：`timed()` 对 `SLG_IR_KEYS`（逗号分隔）里**精确等于**键名的区间，或以 `*` 结尾的前缀条目，用 `CALLGRIND_TOGGLE_COLLECT` 开关收集；`SLG_IR_SKIP_CYCLES=N` 时第 0…N−1 周期不收集（主循环每周期设置 `slg::ir_cycle() = c`）。不定义 `SLG_IR_COUNT` 时不包含 valgrind 头文件、不改变计时代码 |
| `bench/slg` 的输出 | `--mode bench --out f.jsonl` 每行一个 `{"cycle","format","metric","ns"}`，门禁用它发现阶段名 |
| `bench/formal/driver.cpp` | `-DSC_IR_COUNT=1` 时按 `SC_IR_PHASE`（load、query_before、mutate、emit_after、retire、reload、query_after、cleanup）只收集一个阶段；`-DSC_ADAPTER_HEADER="adapters/xoffset/adapter.hpp" -DSC_ADAPTER_TYPE=sc::XOffsetAdapter`，包含目录 `include` 和根目录 |
| `bench/formal/model/reference.py` | `--scale S|M --out DIR` 生成 fixture（`input.canonical.bin`、`trace.bin`、`fixture.json`） |

精确匹配是必须的：前缀匹配下 `C.snapshot` 会把 `C.snapshot2` 也算进去。

## 8. 它抓不到什么

只改内存访问、不改指令数的变化：#27 让 Mac 上的正式修改快 3.0%，Ir 只变 −0.1%；新 M3 在 Mac 上让 B.season_edits 慢 2–3%，x86 Ir 却是 −5%。x86 与 Mac 的方向可能相反。所以 L0 只是合并前的过滤器，Mac L2（tag 前必跑、每周一次、ledger 放行 > 1% 的 PR 合并前）才出结论。

## 9. 历史回放（云端，GCC 16 x86-64，valgrind 3.22）

| 变化 | 结果 | 失败项 |
| --- | --- | --- |
| 旧 M3（b3a694e → ea885e5） | 失败 | formal.emit_after 1.305、formal.mutate 1.032、slg.A 1.024、A.apply 1.063、B.out_encode 1.104、B.back_encode 1.147、S.ticks_middle 1.058 |
| 新 M3（b3a694e → 70ee1c4） | 失败 | formal.mutate 1.056、slg.A 1.020、A.apply 1.071、S.ticks_middle 1.071；B.season_edits 0.950（Mac +2–3%，漏掉，平台差异） |
| #26（ea885e5 → 70ee1c4） | 失败 | formal.mutate 1.024（要 ledger 写明"换回输出 −23%"，emit_after 0.766） |
| #27（f0768ad → b3ab3a3） | 通过 | — |
| #29（b3ab3a3 → 30bb5ca） | 通过 | 说明：slg.A 0.956、formal.load 0.967 指令数下降 |

正式 harness 各阶段的 Ir 与设计探针逐位相同（例如旧 M3 emit_after 3,912,698、新 M3 mutate 43,729,847），M 输入 wire 都是 `2bf0c0717767f377…`。
