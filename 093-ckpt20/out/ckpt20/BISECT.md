# 阶段复核 2026-11-05-ckpt20-9313428 二分

失败指标（复核点 head÷anchor 合并几何均值）：slg:A 1.0112

判定：每步的 head÷anchor 按失败指标投影到复核点的回退上，f = Σ(m·r)/Σ(r²)；f ≥ 0.5 说明回退在这一点之前（含这一点），否则在这一点之后。最多 1 步计时。

| 点 | 提交 | 说明 | 同一二进制的后续提交 |
| --- | --- | --- | --- |
| 0 | c9fae222bb86 | anchor |  |
| 1 | 16de37a521a9 | PR #223 merged with e7fd8cb0 (CKPT20 v1 head, bench/slg 23e86624; report only); the last range holds the merge of c9fae222 (3.3.0 release, #227 bench/slg only) |  |
| 2 | 9313428e633a | head |  |

| 步 | 点 | 范围 (lo, hi] | f | 各指标 m/r | 方向 | 计时 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 16de37a PR #223 merged with e7fd8cb0 (CKPT20 v1 head, bench/slg 23e86624; report only); the last range holds the merge of c9fae222 (3.3.0 release, #227 bench/slg only) | (c9fae22, 9313428] | 1.10 | slg:A 1.10 | 回退在此点及之前 | bisect-1 |

**结论：回退来自 16de37a521a9（PR #223 merged with e7fd8cb0 (CKPT20 v1 head, bench/slg 23e86624; report only); the last range holds the merge of c9fae222 (3.3.0 release, #227 bench/slg only)）**。
