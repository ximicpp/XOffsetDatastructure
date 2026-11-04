# 阶段复核 2026-11-04-ckpt16c-d540148 二分

失败指标（复核点 head÷anchor 合并几何均值）：slg:A 1.0094，slg:B 1.0016，slg:C 1.0020

判定：每步的 head÷anchor 按失败指标投影到复核点的回退上，f = Σ(m·r)/Σ(r²)；f ≥ 0.5 说明回退在这一点之前（含这一点），否则在这一点之后。最多 1 步计时。

| 点 | 提交 | 说明 | 同一二进制的后续提交 |
| --- | --- | --- | --- |
| 0 | 49ce3c68fa60 | anchor |  |
| 1 | 6ec08cc6ba8e | the CKPT14 head (with the cached-root carry, bench/slg b68e63e9; report only); the last range holds a8f12389 (carry dropped) and the merge |  |
| 2 | d5401486e5cc | head |  |

| 步 | 点 | 范围 (lo, hi] | f | 各指标 m/r | 方向 | 计时 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 6ec08cc the CKPT14 head (with the cached-root carry, bench/slg b68e63e9; report only); the last range holds a8f12389 (carry dropped) and the merge | (49ce3c6, d540148] | 1.86 | slg:A 2.05，slg:B -2.07，slg:C 0.02 | 回退在此点及之前 | bisect-1 |
|  |  |  |  | **不单调**：slg:A 此点及之前，slg:B 此点之后，slg:C 此点之后 |  |  |

**结论：不单调，不给单一来源。** 各失败指标在第一个不单调的步里各自的方向：

- slg:A：m/r 2.05，来源在 (49ce3c68fa60 anchor, 6ec08cc6ba8e the CKPT14 head (with the cached-root carry, bench/slg b68e63e9; report only); the last range holds a8f12389 (carry dropped) and the merge]
- slg:B：m/r -2.07，来源在 (6ec08cc6ba8e the CKPT14 head (with the cached-root carry, bench/slg b68e63e9; report only); the last range holds a8f12389 (carry dropped) and the merge, d5401486e5cc head]
- slg:C：m/r 0.02，来源在 (6ec08cc6ba8e the CKPT14 head (with the cached-root carry, bench/slg b68e63e9; report only); the last range holds a8f12389 (carry dropped) and the merge, d5401486e5cc head]

按 f 走到的点是 6ec08cc6ba8e（the CKPT14 head (with the cached-root carry, bench/slg b68e63e9; report only); the last range holds a8f12389 (carry dropped) and the merge），只作参考；交「性能优化」决定。
