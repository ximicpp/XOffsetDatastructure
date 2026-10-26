# 性能回归门禁（指令数，L0）

**指令数只用于拦截回退，计时结论以 Mac 正式批次为准。** 任何"变快"或达标（G1–G4）的说法都不能拿这里的数字作证据。

设计依据：H3 设计 `beta2-20261022/H3_PERF_GATE_DESIGN_20261022.md` 第 5 节。本页是实现说明。

## 1. 它做什么

`.github/workflows/perf-gate.yml` 在 PR 指向 `next_cpp26`（忽略只改 `docs/**`、`*.md` 的 PR）、push 到 `next_cpp26` 和手动触发时运行，GCC 16 和 P2996 Clang 各一个作业，两个都判定：

1. 在同一个作业里导出三份库代码：**anchor**（`bench/perf/ANCHOR`）、**base**（PR 是 merge-base，push 是第一父提交）、**head**。
2. 三份都用 **head 里的基准代码**（手动回放时用所选分支的基准代码）（`bench/slg/`、`bench/formal/`、`bench/reloc/`）编译 `slg_bench`、`sc_xoffset` 和 `reloc_bench`。base 或 anchor 用 head 的基准编不过（公开 API 改了）时，改用它自己那一版的基准，标"基准不同"并警告；仍编不过就标"不可比"，作业不失败，但这个 PR 合并前要跑一次 Mac L2。`reloc_bench` 是可选的：base 或 anchor 编不过它时只少 `reloc.*` 的比较（警告），head 编不过就是构建失败。
3. 计数前先查正确性：SLG `--mode verify --scale S`（xoffset 与 cereal 一致，复用协议和逐条协议各一次）；正式 harness 用各自的 `encode` 生成 S、M 输入 wire，`validate` S 和 M；`reloc_bench --mode verify`（每轮各方结果摘要一致，同时给出项名）。head 任何一项失败，门禁直接失败，不出数字。
4. 在 valgrind callgrind 下数每个阶段的指令数（Ir），`--collect-atstart=no`，只在被测阶段里打开收集。各阶段一个进程，`nproc` 个并行。
   - 每个进程都带 `GLIBC_TUNABLES=glibc.cpu.x86_rep_movsb_threshold=0x7fffffffffffffff`，关掉 glibc memcpy 的 rep movsb 路径。
   - 原因：valgrind 把 `rep movsb` 的每次迭代算一条指令，落在这条路径上的拷贝约每字节一条 Ir；同样的字节放在一次大拷贝里走的是向量循环。
   - 例子：#83 把 12 MiB 输出拆成 256 KiB 一块，formal.emit_after 的 Ir 就成了 5.96 倍，实际工作量并没有变。关掉这条路径后 memcpy 的 Ir 跟拷贝字节数走，#83 是 1.058 倍。
5. head 的一个 SLG 阶段、正式 mutate 和第一个 reloc 项各多跑一次，两次 Ir 必须逐位相同，否则门禁失败（结果不可信）。
6. 按第 3 节的门槛判定，作业摘要里一张表，JSON 产物 `perf-gate-<编译器>-<head sha>.json`（90 天）。不发 PR 评论。
7. 每个警告、失败或要 ledger 放行的指标，再从同一次 callgrind 输出列一张函数差异表（见第 5 节）。

不需要存基线，不需要 nightly：Ir 是确定的，三份每次现编现测，runner 镜像、valgrind 版本的变化自动抵消；代码不变时 Ir 不变，push 的结果已经覆盖每次合并。

## 2. 测什么

| 来源 | 设置 | 指标名 |
| --- | --- | --- |
| SLG | S 规模，seed 20261021，`SLG_REUSE=1`（口径 c），8 个周期，只收集第 4–7 周期（前几个周期还在预热，30bb5ca 的 A 场景第 4–7 周期每周期 Ir 只有第 1–2 周期的 74%；Mac 正式 SLG 量的是稳态） | 每个计时阶段 `slg.<键>`（如 `slg.A.apply`、`slg.S.ticks_middle`）；场景合计 `slg.A`…`slg.D` = 该场景各阶段之和 |
| 正式 harness | M fixture（`bench/formal/model/reference.py --scale M`，默认 seed） | `formal.load`、`formal.query_before`、`formal.mutate`、`formal.emit_after`、`formal.retire`、`formal.reload`、`formal.query_after`、`formal.cleanup`；`formal.read_sum` = query_before + query_after；`formal.total` 只报 |
| 重定位税（G9） | `bench/reloc`，每项一个进程：`--rounds 5 --warmup 1 --cases <项>`，`RELOC_IR_SKIP_ROUNDS=2`，只收 XOffset 一侧的计时区（第 2–4 轮） | 每项 `reloc.<项>`（如 `reloc.vec.push_back`、`reloc.map.insert.fresh`），包括只报告的 `*.reused`、`.plain` 和节点容器对照项 |

SLG 只编 XOffset + cereal（cereal v1.3.2，`ebef1e92`，头文件）。M 规模的 SLG 在 valgrind 下会超出地址空间（512 × 256 MiB 预留），所以用 S。正式 harness 的 M 输入 wire 由每份二进制自己编码并记录 sha256；与 anchor 不同时摘要会写"M 输入 wire 变了"，格式改动要 susu 定。

## 3. 门槛

| 对象 | 警告 | 失败 |
| --- | --- | --- |
| 合计：`slg.A`–`slg.D`，`formal.read_sum`、`mutate`、`emit_after`、`load`、`reload`；head÷base | > 1.003 | > 1.010 |
| 单个阶段，占所在场景（正式为 `formal.total`）Ir ≥ 5%；head÷base | > 1.020 | > 1.050 |
| 占比 < 5% 的阶段，`formal.retire`、`formal.cleanup` | 只报（JSON） | — |
| 合计：head÷anchor，扣除 ledger 已放行的部分 | > 1.010 | > 1.020 |
| 任一合计 head÷base < 0.97 | 标"指令数下降"，提醒要 Mac 数据才能说变快 | — |

占比按 base 和 head 中较大的一边算。`slg.S.ticks_middle`（中间删除压力项）自成一个场景，按阶段门槛判定。每个 `reloc.<项>` 也自成一个场景，按阶段门槛（> 1.02 警告，> 1.05 失败）判定；G9 自己的判定（XOffset ÷ std 的 UB ≤ 1.05）以 Mac 批次为准，门禁只拦 XOffset 一侧的指令数回退。

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
- `"record_only": true` 的条目只作记录（例如 Ir 看不到的收益），什么都不放行，也不需要 susu 批准；`max_ratio` 写当时观察到的比值。
- **head÷base**：只有这个 PR 新加的条目（base 的 ledger 里没有的 `id`）能放行。超过失败门槛但不超过 `max_ratio` 的指标记为"已放行"，摘要里列出条目；超过 `max_ratio` 照样失败。
- **head÷anchor**：head 的 ledger 里所有匹配条目的 `max_ratio` 连乘，作为已放行的部分扣除后再比门槛。
- **批准**：场景合计（`slg.A`–`slg.D`、`formal.*`）≤ 2% 的条目由「性能优化」批准并写进 `approved_by`；超过 2% 或涉及安全与性能取舍的，要 susu 确认，`approved_by` 必须写明 susu，否则门禁直接失败。
- **内联变化**：函数差异表标了"疑似内联变化"的回退，先试把新增的冷路径（错误分支、抛异常、少见情形）挪进一个 `[[gnu::noinline]]` 函数，让热函数保持原来的内联状态；挪了仍回退才写条目，`evidence` 里附上这张函数差异表（摘要里的表或 JSON 的 `functions`）和试过的改法。
- **anchor 前移**：tag 候选的 Mac L2 通过后，开一个只改 `bench/perf/ANCHOR` 的 PR，同时把旧条目移到 `bench/perf/ledger_archive.json`（门禁不读它；条目失效时也移过去，例如 `reloc-bench-added` 在门禁开始测 `reloc.*` 时移入）。

## 5. 看结果

- 作业摘要：第一行固定是上面的加粗句；然后是判定、失败/警告/说明列表、判定对象和超门槛阶段的表、各场景变化最大的阶段。
- 产物：`perf-gate-<编译器>-<sha>.json`（全部行，含只报的阶段）、`plan.json`、`build-*.json`、`ir-*.json`、构建日志。
- 函数差异表：每个警告、失败或"已放行"的指标，列出参照（head÷base 触发的是 base，只有 head÷anchor 触发的是 anchor）与 head 之间自身 Ir（即 `callgrind_annotate --inclusive=no`；GCC 的 `[clone …]` 后缀合并到原函数）变化最大的 10 个函数，摘要和 JSON（行的 `functions` 字段）都有。合计行与它唯一的阶段相同时只列一次。
- **疑似内联变化**：表里的函数满足下面任一条，就在表头和"说明"里标出：
  - 只在一边出现，并且它的某个调用者或被调用者反向变化，幅度至少是它的一半（被内联进那个函数或从中拆出）；
  - 一对调用者与被调用者自身 Ir 一升一降，两者之和的变化不到两者变化幅度之和的一半。

  两条都只算变化量至少占该指标总变化 10% 的函数。只在一边出现、却没有反向变化的伙伴，是新增或删除的代码；同名不同参数的是签名改了，都不标。**这个标记只是诊断，不放行**，也不改变判定：真回退里同样可能有内联移动（旧 M3 的 formal.mutate、slg.C、S.ticks_middle 都会被标），看到标记不能当成误报，放行仍然只靠 ledger。
- **分配器回收型**：被标的指标净增 Ir 里，`FreeTableRecycler::recycle` 的自身 Ir 增量至少占一半，就在表头和"说明"里标出，并按 malloc/free/realloc/calloc/new/delete（含 glibc 的 `_int_*`、`tcache*`）自身 Ir 是否下降分成两种：下降的是"分配器调用换成原地回收"（D2 型），没下降的是"要回收的 bin 变多"（COPS 型）。2026-10-26 用 Mac L2 校准时这两例 Ir 都上升（D2 的 slg.B.in_decode GCC 1.059、P2996 1.052；COPS 的 slg.B GCC 1.014），Mac 上却都变快（0.989；0.977），缓存模拟也没把 D2 翻过来。**这个标记同样只是诊断，不放行**，判定不变：这类 PR 本来就因为 Ir 警告或失败进 L 类，由 Mac L2 下结论。
- 变红怎么办：先看是哪个阶段、哪些函数；是意外就修；标了疑似内联的，先试把新增的冷路径挪进 `[[gnu::noinline]]` 函数（见第 4 节）；是有意的取舍就写 ledger 条目（见上），> 1% 的放行在合并前还要跑一次 Mac L2。

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
| `bench/reloc/reloc_bench.cpp` | 单文件入口，只需库的包含目录；`-DRELOC_IR_COUNT=1` 时只在 `RELOC_IR_KEYS`（精确名或 `*` 前缀）所选项的 XOffset 计时区收集，`RELOC_IR_SKIP_ROUNDS=N` 时第 0…N−1 轮和热身轮不收集；`--cases <项>` 只跑这一项；`--mode verify --out f.jsonl` 校验并按行写出 `{"case","round",…}`，门禁用它发现项名 |

精确匹配是必须的：前缀匹配下 `C.snapshot` 会把 `C.snapshot2` 也算进去。

## 8. 它抓不到什么

只改内存访问、不改指令数的变化：#27 让 Mac 上的正式修改快 3.0%，Ir 只变 −0.1%；新 M3 在 Mac 上让 B.season_edits 慢 2–3%，x86 Ir 却是 −5%。x86 与 Mac 的方向可能相反。2026-10-26 用 GCC 16 和 P2996 两种 Ir 重放 Mac L2 门禁对：分配器回收型改动（D2、COPS）两种 Ir 都与 Mac 反向；GCC 的内联决定翻转会让 GCC Ir 与 Mac 反向（W0b 的 formal.mutate：GCC 0.955、P2996 1.005、Mac 1.011）；方向一致时 Ir 与 Mac 的幅度也差 0.3–5 倍。所以 L0 只是合并前的过滤器，Mac L2（tag 前必跑、每周一次、ledger 放行 > 1% 的 PR 合并前）才出结论。

## 9. 历史回放（云端，GCC 16 x86-64，valgrind 3.22，SLG 第 4–7 周期，基准代码取自 next_cpp26 的 `bench/`）

| 变化 | 结果 | 失败项 |
| --- | --- | --- |
| 旧 M3（b3a694e → ea885e5） | 失败 | formal.emit_after 1.305、formal.mutate 1.030、slg.A 1.021、slg.C 1.012、A.apply 1.101、B.out_encode 1.130、B.back_encode 1.128、C.snapshot 1.052、C.snapshot2 1.053、S.ticks_middle 1.060 |
| 新 M3（b3a694e → 70ee1c4） | 失败 | formal.mutate 1.056、slg.A 1.017、A.apply 1.114、S.ticks_middle 1.073；B.season_edits 0.946（Mac +2–3%，漏掉，平台差异） |
| #26（ea885e5 → 70ee1c4） | 失败 | formal.mutate 1.026（要 ledger 写明"换回输出 −23%"，emit_after 0.766） |
| #27（f0768ad → b3ab3a3） | 通过 | — |
| #29（b3ab3a3 → 30bb5ca） | 失败，GCC 内联变化，预期结果 | slg.D 1.032、slg.B 1.014、A.req_decode 1.092（slg.A 0.951）。函数差异表标出疑似内联：`register_allocation` 调用 `register_owned_range`，D 里 −100,096 / +154,112。#29 给 `register_owned_range` 加了二分查找和抛异常的分支，GCC 16 不再把它的大部分内联进 `register_allocation`；Mac 的 Clang 没有这个变化，A–D 都变快（0.951 / 0.991 / 0.985 / 0.969）。把重叠检查挪进 `[[gnu::noinline]]` 冷函数后 D、B、C 回到 1.001 / 0.998 / 1.000（「性能门禁」验证，改库代码另行处理） |

正式 harness 不受窗口影响；入库版 harness 的 Ir 与设计探针只差几条指令（旧 M3 emit_after 3,912,706 对 3,912,698，新 M3 mutate 43,729,854 对 43,729,847），M 输入 wire 都是 `2bf0c0717767f377…`。
